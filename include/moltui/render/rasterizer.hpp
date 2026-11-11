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

enum class RenderShading {
    Lit,   // Blinn-Phong with bright-color specular damping
    Flat   // Rim-darkened flat shading
};

struct RasterizerSettings {
    RenderStyle style{RenderStyle::BallAndStick};
    RenderShading shading{RenderShading::Lit};
    bool depth_cue{true};
    bool show_unit_cell{true};
    bool show_bonds{true};
    bool show_labels{false};
    float sphere_scale{1.0f};
    float bond_radius{0.15f};
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

    struct DepthRange {
        float near_depth{1.0f};
        float far_depth{20.0f};
    };

    DepthRange compute_depth_range(const core::Structure& structure, const Camera& camera) const;

    void render_atoms(
        const core::Structure& structure,
        const Camera& camera,
        const DepthRange& depth_range,
        Framebuffer& fb
    );
    void render_bonds(
        const core::Structure& structure,
        const Camera& camera,
        const DepthRange& depth_range,
        Framebuffer& fb
    );
    void render_unit_cell(
        const core::Structure& structure,
        const core::UnitCell& cell,
        const Camera& camera,
        Framebuffer& fb
    );
    void render_miller_plane(const core::UnitCell& cell, const glm::ivec3& hkl, const Camera& camera, Framebuffer& fb);

    void draw_sphere_3d(
        const glm::vec3& center_world,
        float radius_world,
        const glm::vec3& base_color,
        bool selected,
        int atom_id,
        const Camera& camera,
        const DepthRange& depth_range,
        bool use_depth_cue,
        Framebuffer& fb
    );

    void draw_cylinder_3d(
        const glm::vec3& p1_world,
        const glm::vec3& p2_world,
        float radius_world,
        const glm::vec3& color1,
        const glm::vec3& color2,
        const Camera& camera,
        const DepthRange& depth_range,
        bool use_depth_cue,
        Framebuffer& fb
    );

    void draw_cone_3d(
        const glm::vec3& base_world,
        const glm::vec3& tip_world,
        float base_radius_world,
        const glm::vec3& color,
        const Camera& camera,
        const DepthRange& depth_range,
        bool use_depth_cue,
        Framebuffer& fb
    );

    void draw_hairline_3d(
        const glm::vec3& p1_world,
        const glm::vec3& p2_world,
        const glm::u8vec3& color,
        float alpha,
        const Camera& camera,
        Framebuffer& fb
    );
};

} // namespace moltui::render
