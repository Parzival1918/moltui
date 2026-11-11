#include "moltui/core/bonding.hpp"
#include "moltui/core/elements.hpp"
#include <unordered_map>
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

} // namespace

std::vector<Bond> BondPerceiver::compute_bonds(
    const std::vector<Atom>& atoms,
    const std::optional<UnitCell>& cell,
    float tolerance
) {
    std::vector<Bond> bonds;
    if (atoms.size() < 2) return bonds;

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

    const float min_dist_sq = 0.4f * 0.4f;

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
                            bonds.push_back(Bond{i, j, 1.0f, {0, 0, 0}});
                        }
                    }
                }
            }
        }
    }

    // In periodic crystals, if requested, check for periodic boundary crossing bonds
    if (cell.has_value() && bonds.empty() && atoms.size() < 2000) {
        // Fallback check for periodic image neighbors if atoms are close across unit cell boundary
        for (size_t i = 0; i < atoms.size(); ++i) {
            float r1 = ElementTable::get_covalent_radius(atoms[i].element);
            for (size_t j = i + 1; j < atoms.size(); ++j) {
                float r2 = ElementTable::get_covalent_radius(atoms[j].element);
                float max_dist = r1 + r2 + tolerance;

                glm::vec3 frac_diff = atoms[j].frac_pos - atoms[i].frac_pos;
                // Minimum image convention
                glm::vec3 pbc_shift = glm::round(frac_diff);
                glm::vec3 min_image_frac = frac_diff - pbc_shift;
                glm::vec3 min_image_cart = cell->fractional_to_cartesian(min_image_frac);
                float dist = glm::length(min_image_cart);

                if (dist >= 0.4f && dist <= max_dist) {
                    glm::ivec3 cell_offset(
                        static_cast<int>(pbc_shift.x),
                        static_cast<int>(pbc_shift.y),
                        static_cast<int>(pbc_shift.z)
                    );
                    bonds.push_back(Bond{i, j, 1.0f, cell_offset});
                }
            }
        }
    }

    return bonds;
}

} // namespace moltui::core
