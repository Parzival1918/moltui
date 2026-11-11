#include "moltui/io/loader.hpp"
#include "moltui/core/elements.hpp"

#include <gemmi/model.hpp>
#include <gemmi/mmread.hpp>
#include <gemmi/cif.hpp>
#include <gemmi/smcif.hpp>
#include <gemmi/small.hpp>

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace moltui::io {

core::Structure StructureLoader::load_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::string filename = path.filename().string();
    std::string upper_filename = filename;
    std::transform(upper_filename.begin(), upper_filename.end(), upper_filename.begin(), ::toupper);

    if (ext == ".xyz") {
        return load_xyz(path);
    } else if (ext == ".cif") {
        try {
            return load_cif_small(path);
        } catch (...) {
            return load_pdb_or_mmcif(path);
        }
    } else if (ext == ".pdb" || ext == ".ent" || ext == ".mmcif") {
        return load_pdb_or_mmcif(path);
    } else if (ext == ".res" || ext == ".ins") {
        try {
            return load_cif_small(path);
        } catch (...) {
            return load_pdb_or_mmcif(path);
        }
    } else if (upper_filename.find("POSCAR") != std::string::npos || upper_filename.find("CONTCAR") != std::string::npos) {
        return load_poscar(path);
    }

    // Default fallback: attempt Gemmi read_structure_file, then XYZ
    try {
        return load_pdb_or_mmcif(path);
    } catch (...) {
        return load_xyz(path);
    }
}

core::Structure StructureLoader::load_pdb_or_mmcif(const std::filesystem::path& path) {
    core::Structure s;
    s.title = path.stem().string();

    gemmi::Structure st = gemmi::read_structure_file(path.string());
    
    // Cell info
    if (st.cell.is_crystal()) {
        core::UnitCell uc;
        uc.a = st.cell.a;
        uc.b = st.cell.b;
        uc.c = st.cell.c;
        uc.alpha = st.cell.alpha;
        uc.beta = st.cell.beta;
        uc.gamma = st.cell.gamma;
        uc.space_group = st.spacegroup_hm;
        uc.update_matrices();
        s.unit_cell = uc;
    }

    // Trajectory frames if multiple models
    if (st.models.size() > 1) {
        for (const auto& model : st.models) {
            std::vector<glm::vec3> frame_pos;
            for (const auto& chain : model.chains) {
                for (const auto& res : chain.residues) {
                    for (const auto& atom : res.atoms) {
                        frame_pos.push_back(glm::vec3(atom.pos.x, atom.pos.y, atom.pos.z));
                    }
                }
            }
            s.trajectory_frames.push_back(frame_pos);
        }
    }

    // Populate primary model atoms
    if (!st.models.empty()) {
        const auto& model = st.models[0];
        size_t id = 0;
        for (const auto& chain : model.chains) {
            for (const auto& res : chain.residues) {
                for (const auto& atom : res.atoms) {
                    core::Atom a;
                    a.id = id++;
                    a.element = atom.element.name();
                    a.label = atom.name;
                    a.pos = glm::vec3(atom.pos.x, atom.pos.y, atom.pos.z);
                    a.occupancy = static_cast<float>(atom.occ);
                    a.b_factor = static_cast<float>(atom.b_iso);
                    a.charge = static_cast<float>(atom.charge);
                    a.chain_id = chain.name;
                    a.residue_name = res.name;
                    a.residue_seq = res.seqid.num.value;

                    if (s.unit_cell.has_value()) {
                        a.frac_pos = s.unit_cell->cartesian_to_fractional(a.pos);
                    }

                    s.atoms.push_back(a);
                }
            }
        }
    }

    s.compute_bonds();
    return s;
}

core::Structure StructureLoader::load_cif_small(const std::filesystem::path& path) {
    core::Structure s;
    s.title = path.stem().string();

    gemmi::cif::Document doc = gemmi::cif::read_file(path.string());
    if (doc.blocks.empty()) {
        throw std::runtime_error("Empty CIF file");
    }

    gemmi::SmallStructure small_st = gemmi::make_small_structure_from_block(doc.blocks[0]);

    core::UnitCell uc;
    uc.a = small_st.cell.a;
    uc.b = small_st.cell.b;
    uc.c = small_st.cell.c;
    uc.alpha = small_st.cell.alpha;
    uc.beta = small_st.cell.beta;
    uc.gamma = small_st.cell.gamma;
    uc.space_group = small_st.spacegroup_hm;
    uc.update_matrices();
    s.unit_cell = uc;

    // Expand symmetry equivalent positions across the full unit cell
    std::vector<gemmi::SmallStructure::Site> full_sites = small_st.get_all_unit_cell_sites();
    if (full_sites.empty()) {
        full_sites = small_st.sites;
    }

    size_t id = 0;
    for (const auto& site : full_sites) {
        core::Atom a;
        a.id = id++;
        a.element = site.element.name();
        a.label = site.label;
        a.frac_pos = glm::vec3(site.fract.x, site.fract.y, site.fract.z);
        a.pos = uc.fractional_to_cartesian(a.frac_pos);
        a.occupancy = static_cast<float>(site.occ);
        a.b_factor = static_cast<float>(site.u_iso * 78.9568); // U_iso to B_iso approx
        s.atoms.push_back(a);
    }

    s.compute_bonds();
    return s;
}

core::Structure StructureLoader::load_xyz(const std::filesystem::path& path) {
    core::Structure s;
    s.title = path.stem().string();

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip leading whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;

        std::istringstream iss(line.substr(start));
        size_t natoms = 0;
        if (!(iss >> natoms)) continue;

        // Comment line / title
        std::string comment;
        if (!std::getline(file, comment)) break;

        std::vector<glm::vec3> frame_positions;
        std::vector<core::Atom> frame_atoms;

        for (size_t i = 0; i < natoms; ++i) {
            if (!std::getline(file, line)) break;
            std::istringstream atom_iss(line);
            std::string elem;
            float x, y, z;
            if (atom_iss >> elem >> x >> y >> z) {
                core::Atom a;
                a.id = i;
                a.element = elem;
                a.label = elem + std::to_string(i + 1);
                a.pos = glm::vec3(x, y, z);
                frame_positions.push_back(a.pos);
                frame_atoms.push_back(a);
            }
        }

        if (s.atoms.empty()) {
            s.atoms = std::move(frame_atoms);
        }
        s.trajectory_frames.push_back(std::move(frame_positions));
    }

    s.compute_bonds();
    return s;
}

core::Structure StructureLoader::load_poscar(const std::filesystem::path& path) {
    core::Structure s;
    s.title = path.stem().string();

    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open POSCAR file: " + path.string());
    }

    std::string line;
    // Line 1: Title
    std::getline(file, line);
    if (!line.empty()) s.title = line;

    // Line 2: Universal scaling factor
    double scale = 1.0;
    file >> scale;

    // Lines 3-5: Lattice vectors a, b, c
    glm::dvec3 va, vb, vc;
    file >> va.x >> va.y >> va.z;
    file >> vb.x >> vb.y >> vb.z;
    file >> vc.x >> vc.y >> vc.z;

    va *= scale;
    vb *= scale;
    vc *= scale;

    double len_a = glm::length(va);
    double len_b = glm::length(vb);
    double len_c = glm::length(vc);

    double alpha = std::acos(glm::dot(vb, vc) / (len_b * len_c)) * (180.0 / 3.141592653589793);
    double beta  = std::acos(glm::dot(va, vc) / (len_a * len_c)) * (180.0 / 3.141592653589793);
    double gamma = std::acos(glm::dot(va, vb) / (len_a * len_b)) * (180.0 / 3.141592653589793);

    core::UnitCell uc;
    uc.a = len_a;
    uc.b = len_b;
    uc.c = len_c;
    uc.alpha = alpha;
    uc.beta = beta;
    uc.gamma = gamma;
    uc.space_group = "P 1";
    uc.update_matrices();
    s.unit_cell = uc;

    // Line 6 & 7: Element species and counts
    std::getline(file, line); // consume newline
    std::getline(file, line);
    std::istringstream iss_species(line);
    std::vector<std::string> species;
    std::string sp;
    while (iss_species >> sp) {
        species.push_back(sp);
    }

    std::getline(file, line);
    std::istringstream iss_counts(line);
    std::vector<size_t> counts;
    size_t count;
    while (iss_counts >> count) {
        counts.push_back(count);
    }

    // Direct / Cartesian mode
    std::getline(file, line);
    bool is_direct = (line.empty() || line[0] == 'D' || line[0] == 'd');

    size_t id = 0;
    for (size_t s_idx = 0; s_idx < species.size(); ++s_idx) {
        std::string elem = species[s_idx];
        size_t n = (s_idx < counts.size()) ? counts[s_idx] : 0;
        for (size_t k = 0; k < n; ++k) {
            float x, y, z;
            file >> x >> y >> z;
            core::Atom a;
            a.id = id++;
            a.element = elem;
            a.label = elem + std::to_string(id);
            if (is_direct) {
                a.frac_pos = glm::vec3(x, y, z);
                a.pos = uc.fractional_to_cartesian(a.frac_pos);
            } else {
                a.pos = glm::vec3(x, y, z) * static_cast<float>(scale);
                a.frac_pos = uc.cartesian_to_fractional(a.pos);
            }
            s.atoms.push_back(a);
        }
    }

    s.compute_bonds();
    return s;
}

} // namespace moltui::io
