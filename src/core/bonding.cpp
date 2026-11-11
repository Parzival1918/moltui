#include "moltui/core/bonding.hpp"
#include "moltui/core/elements.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

namespace moltui::core {

namespace {

struct GridCoord {
    int x, y, z;
    bool operator==(const GridCoord& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct GridCoordHash {
    size_t operator()(const GridCoord& c) const {
        size_t h1 = std::hash<int>()(c.x);
        size_t h2 = std::hash<int>()(c.y);
        size_t h3 = std::hash<int>()(c.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

int get_max_valence(std::string_view elem, std::string_view partner) {
    if (elem == "H") return 1;
    if (elem == "O" || elem == "S") return 3; // allow hydronium/sulfonium
    if (elem == "N" || elem == "P") return 4; // quaternary ammonium/phosphine
    if (elem == "C" || elem == "Si") return 4;
    if (elem == "F" || elem == "Cl" || elem == "Br" || elem == "I") {
        // Terminal covalent halogen when bonded to a non-metal; higher coordination in ionic salts
        return ElementTable::is_nonmetal(partner) ? 1 : 12;
    }
    return 12; // metals/coordination complexes
}

struct CandidateBond {
    size_t atom1;
    size_t atom2;
    float dist;
    glm::ivec3 cell_offset;
};

} // namespace

glm::ivec3 BondPerceiver::minimum_image_shift(
    const glm::vec3& frac_i,
    const glm::vec3& frac_j,
    const UnitCell& cell
) {
    glm::vec3 diff = frac_j - frac_i;
    glm::ivec3 base(
        static_cast<int>(std::round(diff.x)),
        static_cast<int>(std::round(diff.y)),
        static_cast<int>(std::round(diff.z))
    );

    glm::ivec3 best_s = base;
    float best_dist_sq = 1e30f;

    for (int kx = -1; kx <= 1; ++kx) {
        for (int ky = -1; ky <= 1; ++ky) {
            for (int kz = -1; kz <= 1; ++kz) {
                glm::ivec3 s = base + glm::ivec3(kx, ky, kz);
                glm::vec3 cart = cell.fractional_to_cartesian(diff - glm::vec3(s));
                float d2 = glm::dot(cart, cart);
                if (d2 < best_dist_sq) {
                    best_dist_sq = d2;
                    best_s = s;
                }
            }
        }
    }

    return best_s;
}

std::vector<Bond> BondPerceiver::compute_bonds(
    const std::vector<Atom>& atoms,
    const std::optional<UnitCell>& cell,
    float tolerance,
    bool use_pbc
) {
    std::vector<Bond> final_bonds;
    if (atoms.size() < 2) return final_bonds;

    const float cell_size = 3.5f;
    std::unordered_map<GridCoord, std::vector<size_t>, GridCoordHash> grid;

    for (size_t i = 0; i < atoms.size(); ++i) {
        if (!atoms[i].visible) continue;
        GridCoord gc{
            static_cast<int>(std::floor(atoms[i].pos.x / cell_size)),
            static_cast<int>(std::floor(atoms[i].pos.y / cell_size)),
            static_cast<int>(std::floor(atoms[i].pos.z / cell_size))
        };
        grid[gc].push_back(i);
    }

    const float min_dist_sq = 0.45f * 0.45f;
    std::vector<CandidateBond> candidates;

    for (size_t i = 0; i < atoms.size(); ++i) {
        if (!atoms[i].visible) continue;
        const auto& a1 = atoms[i];
        float r1 = ElementTable::get_covalent_radius(a1.element);

        GridCoord gc{
            static_cast<int>(std::floor(a1.pos.x / cell_size)),
            static_cast<int>(std::floor(a1.pos.y / cell_size)),
            static_cast<int>(std::floor(a1.pos.z / cell_size))
        };

        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    GridCoord neighbor_gc{gc.x + dx, gc.y + dy, gc.z + dz};
                    auto it = grid.find(neighbor_gc);
                    if (it == grid.end()) continue;

                    for (size_t j : it->second) {
                        if (j <= i) continue; // avoid duplicate pairs
                        const auto& a2 = atoms[j];
                        float r2 = ElementTable::get_covalent_radius(a2.element);
                        float max_dist = r1 + r2 + tolerance;
                        float max_dist_sq = max_dist * max_dist;

                        glm::vec3 diff = a2.pos - a1.pos;
                        float dist_sq = glm::dot(diff, diff);

                        if (dist_sq >= min_dist_sq && dist_sq <= max_dist_sq) {
                            candidates.push_back({i, j, std::sqrt(dist_sq), {0, 0, 0}});
                        }
                    }
                }
            }
        }
    }

    // In periodic crystals, check for periodic boundary crossing bonds
    if (use_pbc && cell.has_value() && atoms.size() < 2500) {
        for (size_t i = 0; i < atoms.size(); ++i) {
            if (!atoms[i].visible) continue;
            float r1 = ElementTable::get_covalent_radius(atoms[i].element);
            for (size_t j = i + 1; j < atoms.size(); ++j) {
                if (!atoms[j].visible) continue;
                float r2 = ElementTable::get_covalent_radius(atoms[j].element);
                float max_dist = r1 + r2 + tolerance;

                glm::ivec3 pbc_shift = minimum_image_shift(atoms[i].frac_pos, atoms[j].frac_pos, *cell);
                if (pbc_shift == glm::ivec3(0)) continue; // already handled by spatial grid

                glm::vec3 min_image_frac = (atoms[j].frac_pos - glm::vec3(pbc_shift)) - atoms[i].frac_pos;
                glm::vec3 min_image_cart = cell->fractional_to_cartesian(min_image_frac);
                float dist = glm::length(min_image_cart);

                if (dist >= 0.45f && dist <= max_dist) {
                    // cell_offset shifts atom2 into atom1's periodic neighborhood
                    candidates.push_back({i, j, dist, -pbc_shift});
                }
            }
        }
    }

    // Sort candidate bonds by distance (shortest/strongest bonds first)
    std::sort(candidates.begin(), candidates.end(), [](const CandidateBond& a, const CandidateBond& b) {
        return a.dist < b.dist;
    });

    // Enforce valence limits to prevent spurious cross-ring connections
    std::vector<int> valence_counts(atoms.size(), 0);
    for (const auto& cb : candidates) {
        int max_v1 = get_max_valence(atoms[cb.atom1].element, atoms[cb.atom2].element);
        int max_v2 = get_max_valence(atoms[cb.atom2].element, atoms[cb.atom1].element);

        if (valence_counts[cb.atom1] < max_v1 && valence_counts[cb.atom2] < max_v2) {
            valence_counts[cb.atom1]++;
            valence_counts[cb.atom2]++;
            final_bonds.push_back(Bond{cb.atom1, cb.atom2, 1.0f, cb.cell_offset});
        }
    }

    return final_bonds;
}

} // namespace moltui::core
