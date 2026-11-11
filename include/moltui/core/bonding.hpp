#pragma once

#include "moltui/core/atom.hpp"
#include "moltui/core/structure.hpp"
#include <vector>

namespace moltui::core {

class BondPerceiver {
public:
    static std::vector<Bond> compute_bonds(
        const std::vector<Atom>& atoms,
        const std::optional<UnitCell>& cell = std::nullopt,
        float tolerance = 0.45f
    );
};

} // namespace moltui::core
