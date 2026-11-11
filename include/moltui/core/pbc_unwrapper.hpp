#pragma once

#include "moltui/core/structure.hpp"

namespace moltui::core {

class PbcUnwrapper {
public:
    // Unwraps split molecules so they form continuous, unbroken entities across periodic boundaries
    static void unwrap(Structure& structure);

    // Wraps all atom fractional coordinates back into the primary [0, 1) unit cell
    static void wrap_to_unit_cell(Structure& structure);
};

} // namespace moltui::core
