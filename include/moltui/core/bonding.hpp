#pragma once

#include "moltui/core/atom.hpp"
#include "moltui/core/structure.hpp"
#include <vector>

namespace moltui::core {

class BondPerceiver {
public:
    static glm::ivec3 minimum_image_shift(
        const glm::vec3& frac_i,
        const glm::vec3& frac_j,
        const UnitCell& cell
    );

    static std::vector<Bond> compute_bonds(
        const std::vector<Atom>& atoms,
        const std::optional<UnitCell>& cell = std::nullopt,
        float tolerance = 0.28f,
        bool use_pbc = true
    );
};

} // namespace moltui::core
