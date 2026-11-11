#pragma once

#include "moltui/core/structure.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/rasterizer.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace moltui::render {

std::optional<RenderStyle> parse_render_style(std::string_view name);
std::optional<RenderShading> parse_render_shading(std::string_view name);

// Computes tight framing for the structure in the current camera orientation.
void fit_camera_to_structure(
    const core::Structure& structure,
    Camera& camera,
    const RasterizerSettings& settings,
    bool reset_pan_offset = true
);

// Aims camera along a Cartesian or crystallographic direction expression:
// x, y, z, a, b, c, a*, b*, c* (with optional +/-), [u v w], or (h k l).
bool aim_camera_along(
    const core::Structure& structure,
    Camera& camera,
    std::string_view expr,
    std::string* out_label = nullptr,
    std::string* out_error = nullptr
);

} // namespace moltui::render
