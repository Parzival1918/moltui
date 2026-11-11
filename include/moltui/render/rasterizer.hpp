#pragma once

#include "moltui/core/structure.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
#include <optional>

namespace moltui::render {

enum class RenderStyle {
    BallAndStick,
    SpaceFilling,  // VDW
    Licorice,
    Wireframe
};

struct RasterizerSettings {
    RenderStyle style{RenderStyle::BallAndStick};
    bool show_unit_cell{true};
    bool show_bonds{true};
    bool show_labels{false};
    float sphere_scale{0.32f};
    float bond_radius{0.10f};
    glm::u8vec3 background_color{16, 18, 26};
    std::optional<glm::ivec3> miller_plane{std::nullopt};
};

class Rasterizer {
public:
    explicit Rasterizer(const RasterizerSettings& settings = {});

    void render(
        const core::Structure& structure,
        const Camera& camera,
        Framebuffer& target
    );

    RasterizerSettings& settings() { return settings_; }
    const RasterizerSettings& settings() const { return settings_; }

private:
    RasterizerSettings settings_;

    void render_atoms(const core::Structure& structure, const Camera& camera, Framebuffer& fb);
    void render_bonds(const core::Structure& structure, const Camera& camera, Framebuffer& fb);
    void render_unit_cell(const core::UnitCell& cell, const Camera& camera, Framebuffer& fb);
    void render_miller_plane(const core::UnitCell& cell, const glm::ivec3& hkl, const Camera& camera, Framebuffer& fb);

    void draw_line_3d(
        const glm::vec3& p1_world,
        const glm::vec3& p2_world,
        const glm::u8vec3& color,
        const Camera& camera,
        Framebuffer& fb,
        float thickness = 1.0f
    );
};

} // namespace moltui::render
