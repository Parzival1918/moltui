#include "moltui/render/rasterizer.hpp"
#include "moltui/core/elements.hpp"
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace moltui::render {

namespace {

const glm::vec3 kLightDir = glm::normalize(glm::vec3(0.35f, 0.55f, 0.75f));

inline glm::u8vec3 to_u8vec3(const glm::vec3& v) {
    return glm::u8vec3(
        static_cast<uint8_t>(std::clamp(v.r, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(v.g, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(v.b, 0.0f, 1.0f) * 255.0f)
    );
}

} // namespace

Rasterizer::Rasterizer(const RasterizerSettings& settings) : settings_(settings) {}

void Rasterizer::render(
    const core::Structure& structure,
    const Camera& camera,
    Framebuffer& target
) {
    target.clear(settings_.background_color);

    if (settings_.show_unit_cell && structure.unit_cell.has_value()) {
        render_unit_cell(*structure.unit_cell, camera, target);
        if (settings_.miller_plane.has_value()) {
            render_miller_plane(*structure.unit_cell, *settings_.miller_plane, camera, target);
        }
    }

    if (settings_.show_bonds && settings_.style != RenderStyle::SpaceFilling) {
        render_bonds(structure, camera, target);
    }

    if (settings_.style != RenderStyle::Wireframe) {
        render_atoms(structure, camera, target);
    }
}

void Rasterizer::render_atoms(
    const core::Structure& structure,
    const Camera& camera,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    int fb_w = fb.width();
    int fb_h = fb.height();
    float fov_half_tan = std::tan(glm::radians(22.5f)); // 45 / 2

    float base_scale = settings_.sphere_scale;
    if (settings_.style == RenderStyle::SpaceFilling) {
        base_scale = 1.0f;
    } else if (settings_.style == RenderStyle::Licorice) {
        base_scale = 0.16f;
    }

    for (const auto& atom : structure.atoms) {
        if (!atom.visible) continue;

        glm::vec4 cam_pos = view * glm::vec4(atom.pos, 1.0f);
        if (cam_pos.z >= -0.1f) continue; // Behind camera

        float depth_center = -cam_pos.z;
        glm::vec3 screen_center = camera.world_to_screen(atom.pos);
        if (screen_center.z < 0.0f) continue;

        float radius_world = (settings_.style == RenderStyle::SpaceFilling)
            ? (core::ElementTable::get_vdw_radius(atom.element) * base_scale)
            : (core::ElementTable::get_covalent_radius(atom.element) * base_scale);

        // Screen radius in pixels
        float screen_r = (radius_world * fb_h) / (2.0f * fov_half_tan * depth_center);
        if (screen_r < 0.8f) screen_r = 0.8f;

        int min_x = std::max(0, static_cast<int>(std::floor(screen_center.x - screen_r)));
        int max_x = std::min(fb_w - 1, static_cast<int>(std::ceil(screen_center.x + screen_r)));
        int min_y = std::max(0, static_cast<int>(std::floor(screen_center.y - screen_r)));
        int max_y = std::min(fb_h - 1, static_cast<int>(std::ceil(screen_center.y + screen_r)));

        glm::vec3 base_color = core::ElementTable::get_color(atom.element);
        if (atom.selected) {
            base_color = glm::mix(base_color, glm::vec3(1.0f, 0.85f, 0.1f), 0.6f);
        }

        float r_sq = screen_r * screen_r;
        float inv_r = 1.0f / screen_r;

        for (int y = min_y; y <= max_y; ++y) {
            float dy = static_cast<float>(y) - screen_center.y;
            float dy_sq = dy * dy;

            for (int x = min_x; x <= max_x; ++x) {
                float dx = static_cast<float>(x) - screen_center.x;
                float d_sq = dx * dx + dy_sq;
                if (d_sq > r_sq) continue;

                float nx = dx * inv_r;
                float ny = -dy * inv_r; // Invert y for screen coord
                float nz = std::sqrt(std::max(0.0f, 1.0f - (nx * nx + ny * ny)));

                // Depth at sphere surface in camera space
                float pixel_depth = depth_center - (radius_world * nz);

                // Lighting
                glm::vec3 normal(nx, ny, nz);
                float diff = std::max(0.0f, glm::dot(normal, kLightDir));
                
                // Specular reflection
                glm::vec3 refl = 2.0f * diff * normal - kLightDir;
                float spec = std::pow(std::max(0.0f, refl.z), 18.0f) * 0.45f;

                glm::vec3 lit_color = base_color * (0.35f + 0.65f * diff) + glm::vec3(spec);
                if (atom.selected && d_sq > (r_sq * 0.75f)) {
                    // Golden selection outline
                    lit_color = glm::vec3(1.0f, 0.88f, 0.2f);
                }

                fb.test_and_set(x, y, pixel_depth, to_u8vec3(lit_color), static_cast<int>(atom.id));
            }
        }
    }
}

void Rasterizer::render_bonds(
    const core::Structure& structure,
    const Camera& camera,
    Framebuffer& fb
) {
    float res_scale = std::max(1.0f, static_cast<float>(fb.height()) / 120.0f);
    float base_thickness = (settings_.style == RenderStyle::Licorice) ? 2.5f : 1.8f;
    float thickness = base_thickness * res_scale;

    for (const auto& bond : structure.bonds) {
        if (bond.atom1 >= structure.atoms.size() || bond.atom2 >= structure.atoms.size()) continue;
        const auto& a1 = structure.atoms[bond.atom1];
        const auto& a2 = structure.atoms[bond.atom2];
        if (!a1.visible || !a2.visible) continue;

        glm::vec3 p1 = a1.pos;
        glm::vec3 p2 = a2.pos;

        // If bond crosses periodic boundary and has cell_offset
        if (bond.cell_offset != glm::ivec3(0) && structure.unit_cell.has_value()) {
            glm::vec3 shift_cart = structure.unit_cell->fractional_to_cartesian(glm::vec3(bond.cell_offset));
            p2 = a2.pos + shift_cart;
        }

        glm::vec3 mid = 0.5f * (p1 + p2);
        glm::u8vec3 col1 = to_u8vec3(core::ElementTable::get_color(a1.element) * 0.85f);
        glm::u8vec3 col2 = to_u8vec3(core::ElementTable::get_color(a2.element) * 0.85f);

        draw_line_3d(p1, mid, col1, camera, fb, thickness);
        draw_line_3d(mid, p2, col2, camera, fb, thickness);
    }
}

void Rasterizer::draw_line_3d(
    const glm::vec3& p1_world,
    const glm::vec3& p2_world,
    const glm::u8vec3& color,
    const Camera& camera,
    Framebuffer& fb,
    float thickness
) {
    glm::mat4 view = camera.view_matrix();
    glm::vec4 c1 = view * glm::vec4(p1_world, 1.0f);
    glm::vec4 c2 = view * glm::vec4(p2_world, 1.0f);

    if (c1.z >= -0.1f && c2.z >= -0.1f) return;

    glm::vec3 s1 = camera.world_to_screen(p1_world);
    glm::vec3 s2 = camera.world_to_screen(p2_world);

    if (s1.z < 0.0f || s2.z < 0.0f) return;

    float dx = s2.x - s1.x;
    float dy = s2.y - s1.y;
    float dist = std::hypot(dx, dy);
    if (dist < 0.5f) return;

    int steps = static_cast<int>(std::ceil(dist));
    float step_x = dx / steps;
    float step_y = dy / steps;
    float d1 = -c1.z;
    float d2 = -c2.z;
    float step_depth = (d2 - d1) / steps;

    int half_t = static_cast<int>(thickness * 0.5f);

    for (int i = 0; i <= steps; ++i) {
        int cx = static_cast<int>(std::round(s1.x + i * step_x));
        int cy = static_cast<int>(std::round(s1.y + i * step_y));
        float depth = d1 + i * step_depth;

        for (int ox = -half_t; ox <= half_t; ++ox) {
            for (int oy = -half_t; oy <= half_t; ++oy) {
                fb.test_and_set(cx + ox, cy + oy, depth, color, -1);
            }
        }
    }
}

void Rasterizer::render_unit_cell(
    const core::UnitCell& cell,
    const Camera& camera,
    Framebuffer& fb
) {
    // 8 fractional corners
    std::array<glm::vec3, 8> corners;
    for (int i = 0; i < 8; ++i) {
        glm::vec3 frac(
            (i & 1) ? 1.0f : 0.0f,
            (i & 2) ? 1.0f : 0.0f,
            (i & 4) ? 1.0f : 0.0f
        );
        corners[i] = cell.fractional_to_cartesian(frac);
    }

    glm::u8vec3 cell_color(110, 140, 175); // Subtle cyan/slate

    // 12 edges
    const std::array<std::pair<int, int>, 12> edges = {{
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, // along a
        {0, 2}, {1, 3}, {4, 6}, {5, 7}, // along b
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // along c
    }};

    float res_scale = std::max(1.0f, static_cast<float>(fb.height()) / 200.0f);

    for (const auto& [i1, i2] : edges) {
        draw_line_3d(corners[i1], corners[i2], cell_color, camera, fb, 1.0f * res_scale);
    }

    // Distinguish a, b, c axes from origin (0,0,0) with RGB colors
    glm::vec3 origin = corners[0];
    draw_line_3d(origin, corners[1], glm::u8vec3(230, 60, 60), camera, fb, 1.5f * res_scale);  // a axis: Red
    draw_line_3d(origin, corners[2], glm::u8vec3(60, 210, 60), camera, fb, 1.5f * res_scale);  // b axis: Green
    draw_line_3d(origin, corners[4], glm::u8vec3(60, 130, 240), camera, fb, 1.5f * res_scale); // c axis: Blue
}

void Rasterizer::render_miller_plane(
    const core::UnitCell& cell,
    const glm::ivec3& hkl,
    const Camera& camera,
    Framebuffer& fb
) {
    if (hkl == glm::ivec3(0)) return;

    // Standard h*x + k*y + l*z = 1 intercepts
    std::vector<glm::vec3> pts;
    if (hkl.x != 0) pts.push_back(cell.fractional_to_cartesian(glm::vec3(1.0f / hkl.x, 0.0f, 0.0f)));
    if (hkl.y != 0) pts.push_back(cell.fractional_to_cartesian(glm::vec3(0.0f, 1.0f / hkl.y, 0.0f)));
    if (hkl.z != 0) pts.push_back(cell.fractional_to_cartesian(glm::vec3(0.0f, 0.0f, 1.0f / hkl.z)));

    if (pts.size() >= 2) {
        glm::u8vec3 plane_color(240, 200, 60);
        for (size_t i = 0; i < pts.size(); ++i) {
            size_t next = (i + 1) % pts.size();
            draw_line_3d(pts[i], pts[next], plane_color, camera, fb, 1.5f);
        }
    }
}

} // namespace moltui::render
