#pragma once

#include "moltui/core/structure.hpp"

namespace moltui::core {

class PbcUnwrapper {
public:
    // Returns true if periodic bonding forms an infinite network/framework/salt
    static bool bonding_percolates(const Structure& structure);

    // Unwraps split molecules so they form continuous, unbroken entities across periodic boundaries
    static void unwrap(Structure& structure);

    // Wraps all atom fractional coordinates back into the primary [0, 1) unit cell (deduplicating images)
    static void wrap_to_unit_cell(Structure& structure);

    // For percolating network/ionic crystals, replicates boundary atoms onto [0, 1] faces/corners
    static void complete_network_boundaries(Structure& structure);
};

} // namespace moltui::core
