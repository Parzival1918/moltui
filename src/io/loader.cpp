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
    auto list = load_structures(path);
    if (list.empty()) {
        throw std::runtime_error("No structures found in file: " + path.string());
    }
    return std::move(list.front());
}

std::vector<core::Structure> StructureLoader::load_structures(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("File does not exist: " + path.string());
    }

    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    std::string filename = path.filename().string();
    std::string upper_filename = filename;
    std::transform(upper_filename.begin(), upper_filename.end(), upper_filename.begin(), ::toupper);

    if (ext == ".xyz") {
        return {load_xyz(path)};
    } else if (ext == ".cif" || ext == ".res" || ext == ".ins") {
        try {
            auto blocks = load_cif_small_all(path);
            if (!blocks.empty()) {
                return blocks;
            }
        } catch (...) {
        }
        return {load_pdb_or_mmcif(path)};
    } else if (ext == ".pdb" || ext == ".ent" || ext == ".mmcif") {
        return {load_pdb_or_mmcif(path)};
    } else if (upper_filename.find("POSCAR") != std::string::npos || upper_filename.find("CONTCAR") != std::string::npos) {
        return {load_poscar(path)};
    }

    // Default fallback: attempt Gemmi read_structure_file, then XYZ
    try {
        return {load_pdb_or_mmcif(path)};
    } catch (...) {
        return {load_xyz(path)};
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
        uc.space_group = st.spacegroup_hm.empty() ? "P 1" : st.spacegroup_hm;
        if (const gemmi::SpaceGroup* sg = st.find_spacegroup()) {
            uc.space_group_number = sg->number;
            uc.hall_symbol = sg->hall;
            uc.crystal_system = sg->crystal_system_str();
            uc.point_group = sg->point_group_hm();
            for (const auto& op : sg->operations().all_ops_sorted()) {
                uc.symops.push_back(op.triplet());
            }
        } else {
            uc.symops.push_back("x,y,z");
        }
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

                    a.asym_site_index = static_cast<int>(a.id);
                    s.atoms.push_back(a);
                }
            }
        }
        if (s.unit_cell.has_value()) {
            s.unit_cell->asym_atom_count = s.atoms.size();
        }
    }

    s.compute_bonds(0.28f, false);
    return s;
}

std::vector<core::Structure> StructureLoader::load_cif_small_all(const std::filesystem::path& path) {
    gemmi::cif::Document doc = gemmi::cif::read_file(path.string());
    if (doc.blocks.empty()) {
        throw std::runtime_error("Empty CIF file");
    }

    std::vector<core::Structure> structures;
    structures.reserve(doc.blocks.size());

    for (const auto& block : doc.blocks) {
        gemmi::SmallStructure small_st = gemmi::make_small_structure_from_block(block);
        if (small_st.sites.empty()) {
            continue;
        }

        core::Structure s;
        s.title = block.name.empty() ? path.stem().string() : block.name;

        core::UnitCell uc;
        uc.a = small_st.cell.a;
        uc.b = small_st.cell.b;
        uc.c = small_st.cell.c;
        uc.alpha = small_st.cell.alpha;
        uc.beta = small_st.cell.beta;
        uc.gamma = small_st.cell.gamma;
        uc.space_group = small_st.spacegroup_hm.empty() ? "P 1" : small_st.spacegroup_hm;

        std::vector<gemmi::Op> ops;
        ops.push_back(gemmi::Op::identity());
        const gemmi::SpaceGroup* sg = small_st.spacegroup;
        if (!sg && !small_st.spacegroup_hm.empty()) {
            sg = gemmi::find_spacegroup_by_name(small_st.spacegroup_hm,
                                                small_st.cell.alpha,
                                                small_st.cell.gamma);
        }
        if (sg) {
            if (small_st.spacegroup_hm.empty()) {
                uc.space_group = sg->xhm();
            }
            uc.space_group_number = sg->number;
            uc.hall_symbol = sg->hall;
            uc.crystal_system = sg->crystal_system_str();
            uc.point_group = sg->point_group_hm();
            for (const auto& op : sg->operations().all_ops_sorted()) {
                if (op != gemmi::Op::identity()) {
                    ops.push_back(op);
                }
            }
        } else if (!small_st.symops.empty()) {
            for (const auto& op_str : small_st.symops) {
                gemmi::Op op = gemmi::parse_triplet(op_str);
                if (op != gemmi::Op::identity()) {
                    ops.push_back(op);
                }
            }
        }

        uc.symops.reserve(ops.size());
        for (const auto& op : ops) {
            uc.symops.push_back(op.triplet());
        }
        uc.asym_atom_count = small_st.sites.size();
        uc.update_matrices();
        s.unit_cell = uc;

        constexpr double SPECIAL_POS_TOL = 0.4;
        std::vector<gemmi::Fractional> site_fracs;
        size_t id = 0;
        for (size_t asym_idx = 0; asym_idx < small_st.sites.size(); ++asym_idx) {
            const auto& site = small_st.sites[asym_idx];
            site_fracs.clear();
            for (size_t op_idx = 0; op_idx < ops.size(); ++op_idx) {
                auto xyz = ops[op_idx].apply_to_xyz({site.fract.x, site.fract.y, site.fract.z});
                gemmi::Fractional fpos(xyz[0], xyz[1], xyz[2]);
                bool duplicate = false;
                for (const auto& prev : site_fracs) {
                    if (small_st.cell.distance_sq(fpos, prev) < gemmi::sq(SPECIAL_POS_TOL)) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;
                site_fracs.push_back(fpos);

                core::Atom a;
                a.id = id++;
                a.element = site.element.name();
                a.label = site.label;
                a.frac_pos = glm::vec3(fpos.x, fpos.y, fpos.z);
                a.pos = uc.fractional_to_cartesian(a.frac_pos);
                a.occupancy = static_cast<float>(site.occ);
                a.b_factor = static_cast<float>(site.u_iso * 78.9568); // U_iso to B_iso approx
                a.symop_index = static_cast<int>(op_idx);
                a.asym_site_index = static_cast<int>(asym_idx);
                s.atoms.push_back(std::move(a));
            }
        }

        s.prepare_crystal_display();
        structures.push_back(std::move(s));
    }

    if (structures.empty()) {
        throw std::runtime_error("No atomic sites in CIF file");
    }
    return structures;
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

    s.compute_bonds(0.28f, false);
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
    uc.space_group_number = 1;
    uc.crystal_system = "triclinic";
    uc.point_group = "1";
    uc.symops = {"x,y,z"};
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
            a.asym_site_index = static_cast<int>(a.id);
            s.atoms.push_back(a);
        }
    }

    s.unit_cell->asym_atom_count = s.atoms.size();
    s.prepare_crystal_display();
    return s;
}

} // namespace moltui::io
