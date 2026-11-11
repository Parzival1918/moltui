#include "moltui/render/rasterizer.hpp"
#include "moltui/core/elements.hpp"
#include <glm/geometric.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace moltui::render {

namespace {

// Upper-left slightly toward viewer (chmpy.tui convention)
const glm::vec3 kLightDir = glm::normalize(glm::vec3(-0.45f, 0.55f, 0.70f));
const glm::vec3 kHalfVec = glm::normalize(kLightDir + glm::vec3(0.0f, 0.0f, 1.0f));

const std::array<glm::vec3, 3> kAxisColors = {{
    glm::vec3(228.0f / 255.0f,  86.0f / 255.0f,  86.0f / 255.0f), // a: muted red
    glm::vec3( 96.0f / 255.0f, 196.0f / 255.0f, 112.0f / 255.0f), // b: muted green
    glm::vec3(104.0f / 255.0f, 150.0f / 255.0f, 244.0f / 255.0f)  // c: muted blue
}};

inline glm::u8vec3 to_u8vec3(const glm::vec3& v) {
    return glm::u8vec3(
        static_cast<uint8_t>(std::clamp(v.r, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(v.g, 0.0f, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::clamp(v.b, 0.0f, 1.0f) * 255.0f)
    );
}

inline glm::vec3 shade_surface(
    const glm::vec3& normal,
    const glm::vec3& base_color,
    RenderShading shading
) {
    if (shading == RenderShading::Flat) {
        float rim = std::clamp((normal.z - 0.15f) / 0.35f, 0.0f, 1.0f);
        return base_color * (0.55f + 0.45f * rim);
    }

    float lam = std::max(0.0f, glm::dot(normal, kLightDir));
    float spec = std::pow(std::max(0.0f, glm::dot(normal, kHalfVec)), 18.0f);
    // Damp specular highlights on already-bright colors (e.g. white H) so shading stays readable
    float lum = 0.299f * base_color.r + 0.587f * base_color.g + 0.114f * base_color.b;
    float spec_strength = 0.45f * (1.0f - 0.75f * std::clamp(lum, 0.0f, 1.0f));
    return base_color * (0.30f + 0.70f * lam) + glm::vec3(spec_strength * spec);
}

inline glm::vec3 apply_depth_cue(
    const glm::vec3& rgb,
    float depth,
    float near_depth,
    float far_depth,
    const glm::vec3& bg_float,
    bool enabled
) {
    if (!enabled) return rgb;
    float span = std::max(far_depth - near_depth, 1.0f);
    float f = std::clamp((far_depth - depth) / span, 0.0f, 1.0f);
    float cue = 0.35f + 0.65f * f;
    return glm::mix(bg_float, rgb, cue);
}

} // namespace

Rasterizer::Rasterizer(const RasterizerSettings& settings) : settings_(settings) {}

Rasterizer::DepthRange Rasterizer::compute_depth_range(
    const core::Structure& structure,
    const Camera& camera
) const {
    glm::mat4 view = camera.view_matrix();
    float min_d = 1e9f;
    float max_d = -1e9f;

    for (const auto& atom : structure.atoms) {
        if (!atom.visible) continue;
        glm::vec4 cam_pos = view * glm::vec4(atom.pos, 1.0f);
        float d = -cam_pos.z;
        min_d = std::min(min_d, d);
        max_d = std::max(max_d, d);
    }

    if (min_d > max_d) {
        return {1.0f, 20.0f};
    }
    return {min_d, max_d + 1.0f};
}

void Rasterizer::render(
    const core::Structure& structure,
    const Camera& camera,
    Framebuffer& target
) {
    target.clear(settings_.background_color);

    DepthRange depth_range = compute_depth_range(structure, camera);

    render_atoms(structure, camera, depth_range, target);

    if (settings_.show_bonds && settings_.style != RenderStyle::SpaceFilling) {
        render_bonds(structure, camera, depth_range, target);
    }

    if (settings_.show_unit_cell && structure.unit_cell.has_value()) {
        render_unit_cell(structure, *structure.unit_cell, camera, target);
        if (settings_.miller_plane.has_value()) {
            render_miller_plane(*structure.unit_cell, *settings_.miller_plane, camera, target);
        }
    }
}

void Rasterizer::render_atoms(
    const core::Structure& structure,
    const Camera& camera,
    const DepthRange& depth_range,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    const bool periodic = structure.is_periodic();

    struct AtomDrawItem {
        size_t index;
        float depth;
    };
    std::vector<AtomDrawItem> items;
    items.reserve(structure.atoms.size());

    for (size_t i = 0; i < structure.atoms.size(); ++i) {
        if (!structure.atoms[i].visible) continue;
        glm::vec4 cam_pos = view * glm::vec4(structure.atoms[i].pos, 1.0f);
        items.push_back({i, -cam_pos.z});
    }

    // Back-to-front so anti-aliased sphere rims blend smoothly over occluded atoms
    std::sort(items.begin(), items.end(), [](const AtomDrawItem& a, const AtomDrawItem& b) {
        return a.depth > b.depth;
    });

    for (const auto& item : items) {
        const auto& atom = structure.atoms[item.index];
        float radius_world = 0.0f;
        switch (settings_.style) {
            case RenderStyle::SpaceFilling:
                radius_world = core::ElementTable::get_vdw_radius(atom.element) * settings_.sphere_scale;
                break;
            case RenderStyle::Licorice:
                radius_world = settings_.bond_radius;
                break;
            case RenderStyle::Wireframe:
                radius_world = 0.055f;
                break;
            case RenderStyle::BallAndStick:
            default:
                radius_world = core::ElementTable::get_ball_stick_radius(atom.element) *
                               (periodic ? 1.05f : 0.90f) * settings_.sphere_scale;
                break;
        }

        glm::vec3 base_color = core::ElementTable::get_color(atom.element);
        if (atom.selected) {
            base_color = glm::mix(base_color, glm::vec3(1.0f, 0.85f, 0.1f), 0.6f);
        }

        draw_sphere_3d(
            atom.pos,
            radius_world,
            base_color,
            atom.selected,
            static_cast<int>(atom.id),
            camera,
            depth_range,
            settings_.depth_cue,
            fb
        );
    }
}

void Rasterizer::draw_sphere_3d(
    const glm::vec3& center_world,
    float radius_world,
    const glm::vec3& base_color,
    bool selected,
    int atom_id,
    const Camera& camera,
    const DepthRange& depth_range,
    bool use_depth_cue,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    int fb_w = fb.width();
    int fb_h = fb.height();
    glm::vec4 cam_pos = view * glm::vec4(center_world, 1.0f);
    if (!camera.orthographic && cam_pos.z >= -0.1f) return;

    float depth_center = -cam_pos.z;
    glm::vec3 screen_center = camera.world_to_screen(center_world);

    float screen_r = 0.0f;
    if (camera.orthographic) {
        screen_r = radius_world * camera.pixels_per_world();
    } else {
        float fov_half_tan = std::tan(glm::radians(camera.fov_degrees() * 0.5f));
        screen_r = (radius_world * fb_h) / (2.0f * fov_half_tan * depth_center);
    }
    if (screen_r < 0.6f) screen_r = 0.6f;

    int min_x = std::max(0, static_cast<int>(std::floor(screen_center.x - screen_r - 1.0f)));
    int max_x = std::min(fb_w - 1, static_cast<int>(std::ceil(screen_center.x + screen_r + 1.0f)));
    int min_y = std::max(0, static_cast<int>(std::floor(screen_center.y - screen_r - 1.0f)));
    int max_y = std::min(fb_h - 1, static_cast<int>(std::ceil(screen_center.y + screen_r + 1.0f)));
    if (min_x > max_x || min_y > max_y) return;

    glm::vec3 bg_float(
        settings_.background_color.r / 255.0f,
        settings_.background_color.g / 255.0f,
        settings_.background_color.b / 255.0f
    );

    float inv_r = 1.0f / screen_r;
    float outer_r = screen_r + 0.5f;
    float outer_r_sq = outer_r * outer_r;

    for (int y = min_y; y <= max_y; ++y) {
        float dy = (static_cast<float>(y) + 0.5f) - screen_center.y;
        float dy_sq = dy * dy;

        for (int x = min_x; x <= max_x; ++x) {
            float dx = (static_cast<float>(x) + 0.5f) - screen_center.x;
            float d_sq = dx * dx + dy_sq;
            if (d_sq > outer_r_sq) continue;

            float d = std::sqrt(d_sq);
            float cov = std::clamp(outer_r - d, 0.0f, 1.0f);
            if (cov <= 0.001f) continue;

            float nx = dx * inv_r;
            float ny = -dy * inv_r; // Invert y for screen coord
            float r2_norm = nx * nx + ny * ny;
            if (r2_norm > 0.999f) {
                float s = std::sqrt(0.999f / r2_norm);
                nx *= s;
                ny *= s;
                r2_norm = 0.999f;
            }
            float nz = std::sqrt(std::max(0.0f, 1.0f - r2_norm));

            float pixel_depth = depth_center - (radius_world * nz);
            glm::vec3 normal(nx, ny, nz);

            glm::vec3 lit_color = shade_surface(normal, base_color, settings_.shading);
            lit_color = apply_depth_cue(
                lit_color, pixel_depth, depth_range.near_depth, depth_range.far_depth,
                bg_float, use_depth_cue
            );

            if (selected && d > (screen_r * 0.84f)) {
                lit_color = glm::vec3(1.0f, 0.88f, 0.2f);
            }

            fb.blend_and_set(x, y, pixel_depth, to_u8vec3(lit_color), cov, atom_id);
        }
    }
}

void Rasterizer::render_bonds(
    const core::Structure& structure,
    const Camera& camera,
    const DepthRange& depth_range,
    Framebuffer& fb
) {
    float cyl_radius = settings_.bond_radius;
    if (settings_.style == RenderStyle::Licorice) {
        cyl_radius = settings_.bond_radius;
    } else if (settings_.style == RenderStyle::Wireframe) {
        cyl_radius = 0.045f;
    }

    glm::mat4 view = camera.view_matrix();

    struct BondDrawItem {
        glm::vec3 p1;
        glm::vec3 p2;
        glm::vec3 c1;
        glm::vec3 c2;
        float depth;
    };
    std::vector<BondDrawItem> items;
    items.reserve(structure.bonds.size());

    for (const auto& bond : structure.bonds) {
        if (bond.atom1 >= structure.atoms.size() || bond.atom2 >= structure.atoms.size()) continue;
        const auto& a1 = structure.atoms[bond.atom1];
        const auto& a2 = structure.atoms[bond.atom2];
        if (!a1.visible || !a2.visible) continue;

        glm::vec3 p1 = a1.pos;
        glm::vec3 p2 = a2.pos;
        if (bond.cell_offset != glm::ivec3(0) && structure.unit_cell.has_value()) {
            glm::vec3 shift_cart = structure.unit_cell->fractional_to_cartesian(glm::vec3(bond.cell_offset));
            p2 = a2.pos + shift_cart;
        }

        glm::vec3 mid = 0.5f * (p1 + p2);
        glm::vec4 cam_mid = view * glm::vec4(mid, 1.0f);

        glm::vec3 c1 = core::ElementTable::get_color(a1.element) * 0.85f;
        glm::vec3 c2 = core::ElementTable::get_color(a2.element) * 0.85f;
        items.push_back({p1, p2, c1, c2, -cam_mid.z});
    }

    std::sort(items.begin(), items.end(), [](const BondDrawItem& a, const BondDrawItem& b) {
        return a.depth > b.depth;
    });

    for (const auto& item : items) {
        draw_cylinder_3d(
            item.p1, item.p2, cyl_radius, item.c1, item.c2,
            camera, depth_range, settings_.depth_cue, fb
        );
    }
}

void Rasterizer::draw_cylinder_3d(
    const glm::vec3& p1_world,
    const glm::vec3& p2_world,
    float radius_world,
    const glm::vec3& color1,
    const glm::vec3& color2,
    const Camera& camera,
    const DepthRange& depth_range,
    bool use_depth_cue,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    int fb_w = fb.width();
    int fb_h = fb.height();

    glm::vec3 A = glm::vec3(view * glm::vec4(p1_world, 1.0f));
    glm::vec3 B = glm::vec3(view * glm::vec4(p2_world, 1.0f));
    if (!camera.orthographic && A.z >= -0.1f && B.z >= -0.1f) return;

    glm::vec3 V = B - A;
    float len = glm::length(V);
    if (len < 1e-5f) return;
    glm::vec3 a_hat = V / len;

    glm::vec3 s1 = camera.world_to_screen(p1_world);
    glm::vec3 s2 = camera.world_to_screen(p2_world);

    float px_per_world = camera.pixels_per_world();
    float fov_half_tan = std::tan(glm::radians(camera.fov_degrees() * 0.5f));
    float px_per_y_unit = fb_h / (2.0f * fov_half_tan);

    float r_px = 0.0f;
    if (camera.orthographic) {
        r_px = radius_world * px_per_world;
    } else {
        float min_depth = std::max(0.1f, std::min(-A.z, -B.z));
        r_px = (radius_world * px_per_y_unit) / min_depth;
    }
    if (r_px < 0.6f) r_px = 0.6f;

    int min_x = std::max(0, static_cast<int>(std::floor(std::min(s1.x, s2.x) - r_px - 1.0f)));
    int max_x = std::min(fb_w - 1, static_cast<int>(std::ceil(std::max(s1.x, s2.x) + r_px + 1.0f)));
    int min_y = std::max(0, static_cast<int>(std::floor(std::min(s1.y, s2.y) - r_px - 1.0f)));
    int max_y = std::min(fb_h - 1, static_cast<int>(std::ceil(std::max(s1.y, s2.y) + r_px + 1.0f)));
    if (min_x > max_x || min_y > max_y) return;

    glm::vec3 bg_float(
        settings_.background_color.r / 255.0f,
        settings_.background_color.g / 255.0f,
        settings_.background_color.b / 255.0f
    );

    float r_sq = radius_world * radius_world;
    float half_w = fb_w * 0.5f;
    float half_h = fb_h * 0.5f;

    for (int y = min_y; y <= max_y; ++y) {
        float py = static_cast<float>(y) + 0.5f;
        for (int x = min_x; x <= max_x; ++x) {
            float px = static_cast<float>(x) + 0.5f;

            glm::vec3 O(0.0f);
            glm::vec3 D(0.0f, 0.0f, -1.0f);
            if (camera.orthographic) {
                O = glm::vec3((px - half_w) / px_per_world, (half_h - py) / px_per_world, 0.0f);
            } else {
                D = glm::normalize(glm::vec3((px - half_w) / px_per_y_unit, (half_h - py) / px_per_y_unit, -1.0f));
            }

            float D_dot_a = glm::dot(D, a_hat);
            glm::vec3 D_perp = D - D_dot_a * a_hat;
            float a_coef = glm::dot(D_perp, D_perp);
            if (a_coef < 1e-6f) continue; // End-on cylinder; endpoint spheres cover it

            glm::vec3 W = O - A;
            float W_dot_a = glm::dot(W, a_hat);
            glm::vec3 W_perp = W - W_dot_a * a_hat;

            float b_coef = 2.0f * glm::dot(W_perp, D_perp);
            float ww_perp = glm::dot(W_perp, W_perp);
            float c_coef = ww_perp - r_sq;

            float s_closest = -b_coef / (2.0f * a_coef);
            float d_perp_sq = std::max(0.0f, ww_perp - (b_coef * b_coef) / (4.0f * a_coef));
            float d_perp = std::sqrt(d_perp_sq);

            float disc = b_coef * b_coef - 4.0f * a_coef * c_coef;
            float s = (disc >= 0.0f)
                ? ((-b_coef - std::sqrt(disc)) / (2.0f * a_coef))
                : s_closest;

            float t = (W_dot_a + s * D_dot_a) / len;
            if (t < 0.0f || t > 1.0f) continue;

            glm::vec3 P = O + s * D;
            float depth = -P.z;
            if (!camera.orthographic && depth <= 0.1f) continue;

            float ppw = camera.orthographic ? px_per_world : (px_per_y_unit / depth);
            float cov = std::clamp((radius_world - d_perp) * ppw + 0.5f, 0.0f, 1.0f);
            if (cov <= 0.001f) continue;

            glm::vec3 rad_vec = (P - A) - (t * V);
            float rad_len = glm::length(rad_vec);
            glm::vec3 normal = (rad_len > 1e-6f) ? (rad_vec / rad_len) : glm::vec3(0.0f, 0.0f, 1.0f);

            const glm::vec3& seg_color = (t < 0.5f) ? color1 : color2;
            glm::vec3 lit_color = shade_surface(normal, seg_color, settings_.shading);
            lit_color = apply_depth_cue(
                lit_color, depth, depth_range.near_depth, depth_range.far_depth,
                bg_float, use_depth_cue
            );

            fb.blend_and_set(x, y, depth, to_u8vec3(lit_color), cov, -1);
        }
    }
}

void Rasterizer::draw_cone_3d(
    const glm::vec3& base_world,
    const glm::vec3& tip_world,
    float base_radius_world,
    const glm::vec3& color,
    const Camera& camera,
    const DepthRange& depth_range,
    bool use_depth_cue,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    int fb_w = fb.width();
    int fb_h = fb.height();

    glm::vec3 A = glm::vec3(view * glm::vec4(base_world, 1.0f));
    glm::vec3 B = glm::vec3(view * glm::vec4(tip_world, 1.0f));
    if (!camera.orthographic && A.z >= -0.1f && B.z >= -0.1f) return;

    glm::vec3 V = B - A;
    float len = glm::length(V);
    if (len < 1e-5f || base_radius_world < 1e-5f) return;

    glm::vec3 u_hat = -V / len; // From apex B toward base A
    float cos2 = (len * len) / (len * len + base_radius_world * base_radius_world);

    glm::vec3 s_base = camera.world_to_screen(base_world);
    glm::vec3 s_tip = camera.world_to_screen(tip_world);

    float px_per_world = camera.pixels_per_world();
    float fov_half_tan = std::tan(glm::radians(camera.fov_degrees() * 0.5f));
    float px_per_y_unit = fb_h / (2.0f * fov_half_tan);

    float r_px = camera.orthographic
        ? (base_radius_world * px_per_world)
        : ((base_radius_world * px_per_y_unit) / std::max(0.1f, std::min(-A.z, -B.z)));
    r_px = std::max(r_px, 1.0f);

    int min_x = std::max(0, static_cast<int>(std::floor(std::min(s_base.x - r_px, s_tip.x) - 1.5f)));
    int max_x = std::min(fb_w - 1, static_cast<int>(std::ceil(std::max(s_base.x + r_px, s_tip.x) + 1.5f)));
    int min_y = std::max(0, static_cast<int>(std::floor(std::min(s_base.y - r_px, s_tip.y) - 1.5f)));
    int max_y = std::min(fb_h - 1, static_cast<int>(std::ceil(std::max(s_base.y + r_px, s_tip.y) + 1.5f)));
    if (min_x > max_x || min_y > max_y) return;

    glm::vec3 bg_float(
        settings_.background_color.r / 255.0f,
        settings_.background_color.g / 255.0f,
        settings_.background_color.b / 255.0f
    );
    float half_w = fb_w * 0.5f;
    float half_h = fb_h * 0.5f;
    float r_sq = base_radius_world * base_radius_world;

    auto eval_ray = [&](const glm::vec3& O, const glm::vec3& D, float& out_depth, glm::vec3& out_normal) -> bool {
        glm::vec3 W = O - B;
        float du = glm::dot(D, u_hat);
        float wu = glm::dot(W, u_hat);
        float dd = glm::dot(D, D);
        float wd = glm::dot(W, D);
        float ww = glm::dot(W, W);

        float a_coef = du * du - cos2 * dd;
        float b_coef = 2.0f * (du * wu - cos2 * wd);
        float c_coef = wu * wu - cos2 * ww;

        bool hit = false;
        float best_s = 1e30f;
        glm::vec3 best_n(0.0f, 0.0f, 1.0f);

        auto try_side_root = [&](float s) {
            if (s <= 0.0f || s >= best_s) return;
            float h = wu + s * du;
            if (h < 0.0f || h > len) return;
            glm::vec3 P = O + s * D;
            if (!camera.orthographic && -P.z <= 0.1f) return;
            glm::vec3 M = P - B;
            glm::vec3 r_perp = M - h * u_hat;
            float r_len = glm::length(r_perp);
            best_s = s;
            hit = true;
            best_n = (r_len > 1e-6f)
                ? glm::normalize((r_perp / r_len) * len - u_hat * base_radius_world)
                : -u_hat;
        };

        if (std::abs(a_coef) < 1e-6f) {
            if (std::abs(b_coef) > 1e-6f) {
                try_side_root(-c_coef / b_coef);
            }
        } else {
            float disc = b_coef * b_coef - 4.0f * a_coef * c_coef;
            if (disc >= 0.0f) {
                float sd = std::sqrt(disc);
                try_side_root((-b_coef - sd) / (2.0f * a_coef));
                try_side_root((-b_coef + sd) / (2.0f * a_coef));
            }
        }

        // Circular base cap at A
        if (std::abs(du) > 1e-6f) {
            float s_cap = glm::dot(A - O, u_hat) / du;
            if (s_cap > 0.0f && s_cap < best_s) {
                glm::vec3 P_cap = O + s_cap * D;
                if ((camera.orthographic || -P_cap.z > 0.1f) &&
                    glm::dot(P_cap - A, P_cap - A) <= r_sq) {
                    best_s = s_cap;
                    hit = true;
                    best_n = u_hat;
                }
            }
        }

        if (!hit) return false;
        out_depth = -(O.z + best_s * D.z);
        out_normal = best_n;
        return true;
    };

    constexpr std::array<std::pair<float, float>, 4> kSubOffsets = {{
        {0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}
    }};

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            int hits = 0;
            float depth_sum = 0.0f;
            glm::vec3 normal_sum(0.0f);

            for (const auto& [ox, oy] : kSubOffsets) {
                float sx = static_cast<float>(x) + ox;
                float sy = static_cast<float>(y) + oy;
                glm::vec3 O(0.0f);
                glm::vec3 D(0.0f, 0.0f, -1.0f);
                if (camera.orthographic) {
                    O = glm::vec3((sx - half_w) / px_per_world, (half_h - sy) / px_per_world, 0.0f);
                } else {
                    D = glm::normalize(glm::vec3((sx - half_w) / px_per_y_unit, (half_h - sy) / px_per_y_unit, -1.0f));
                }

                float d = 0.0f;
                glm::vec3 n(0.0f);
                if (eval_ray(O, D, d, n)) {
                    ++hits;
                    depth_sum += d;
                    normal_sum += n;
                }
            }

            if (hits == 0) continue;
            float cov = static_cast<float>(hits) * 0.25f;
            float avg_depth = depth_sum / static_cast<float>(hits);
            float n_len = glm::length(normal_sum);
            glm::vec3 avg_normal = (n_len > 1e-6f) ? (normal_sum / n_len) : glm::vec3(0.0f, 0.0f, 1.0f);

            glm::vec3 lit_color = shade_surface(avg_normal, color, settings_.shading);
            lit_color = apply_depth_cue(
                lit_color, avg_depth, depth_range.near_depth, depth_range.far_depth,
                bg_float, use_depth_cue
            );
            fb.blend_and_set(x, y, avg_depth, to_u8vec3(lit_color), cov, -1);
        }
    }
}

void Rasterizer::draw_hairline_3d(
    const glm::vec3& p1_world,
    const glm::vec3& p2_world,
    const glm::u8vec3& color,
    float alpha,
    const Camera& camera,
    Framebuffer& fb
) {
    glm::mat4 view = camera.view_matrix();
    glm::vec4 c1 = view * glm::vec4(p1_world, 1.0f);
    glm::vec4 c2 = view * glm::vec4(p2_world, 1.0f);
    if (!camera.orthographic && c1.z >= -0.1f && c2.z >= -0.1f) return;

    glm::vec3 s1 = camera.world_to_screen(p1_world);
    glm::vec3 s2 = camera.world_to_screen(p2_world);

    float dx = s2.x - s1.x;
    float dy = s2.y - s1.y;
    float dist = std::hypot(dx, dy);
    if (dist < 0.5f) return;

    int steps = static_cast<int>(std::ceil(dist * 2.0f));
    float d1 = -c1.z;
    float d2 = -c2.z;

    int last_x = -1;
    int last_y = -1;
    for (int i = 0; i <= steps; ++i) {
        float u = static_cast<float>(i) / static_cast<float>(steps);
        int cx = static_cast<int>(std::floor(s1.x + u * dx));
        int cy = static_cast<int>(std::floor(s1.y + u * dy));
        if (cx == last_x && cy == last_y) continue;
        last_x = cx;
        last_y = cy;

        float depth = d1 + u * (d2 - d1);
        fb.blend_and_set(cx, cy, depth, color, alpha, -1);
    }
}

void Rasterizer::render_unit_cell(
    const core::Structure& structure,
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

    // Subtle chmpy.tui-style translucent cell edges (respecting depth buffer)
    glm::u8vec3 cell_color(170, 180, 205);
    const std::array<std::pair<int, int>, 12> edges = {{
        {0, 1}, {2, 3}, {4, 5}, {6, 7}, // along a
        {0, 2}, {1, 3}, {4, 6}, {5, 7}, // along b
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // along c
    }};

    for (const auto& [i1, i2] : edges) {
        draw_hairline_3d(corners[i1], corners[i2], cell_color, 0.45f, camera, fb);
    }

    // Solid 3D a, b, c vector arrows (cylinder shaft + cone arrowhead), unfaded by depth cueing
    float span = structure.get_bounds().radius;
    float shaft_r = std::max(0.06f, span * 0.011f);
    float head_r = shaft_r * 2.5f;
    float head_len = head_r * 2.6f;
    DepthRange unused_range{1.0f, 20.0f};

    glm::vec3 origin = corners[0];
    const std::array<glm::vec3, 3> tips = {{corners[1], corners[2], corners[4]}};
    for (size_t axis_idx = 0; axis_idx < 3; ++axis_idx) {
        glm::vec3 vec = tips[axis_idx] - origin;
        float len = glm::length(vec);
        if (len < 1e-4f) continue;
        glm::vec3 dir = vec / len;
        float cur_head_len = std::min(head_len, len * 0.35f);
        float cur_head_r = std::min(head_r, cur_head_len * 0.45f);
        glm::vec3 shaft_end = tips[axis_idx] - dir * cur_head_len;
        if (len > cur_head_len) {
            draw_cylinder_3d(
                origin, shaft_end, shaft_r,
                kAxisColors[axis_idx], kAxisColors[axis_idx],
                camera, unused_range, false, fb
            );
        }
        draw_cone_3d(
            shaft_end, tips[axis_idx], cur_head_r, kAxisColors[axis_idx],
            camera, unused_range, false, fb
        );
    }
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
            draw_hairline_3d(pts[i], pts[next], plane_color, 0.85f, camera, fb);
        }
    }
}

} // namespace moltui::render
