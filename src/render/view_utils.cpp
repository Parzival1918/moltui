#include "moltui/render/view_utils.hpp"
#include "moltui/core/elements.hpp"
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <vector>

namespace moltui::render {

namespace {

std::string to_lower(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (unsigned char ch : sv) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

std::string_view trim_sv(std::string_view sv) {
    size_t start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    size_t end = sv.find_last_not_of(" \t\r\n");
    return sv.substr(start, end - start + 1);
}

} // namespace

std::optional<RenderStyle> parse_render_style(std::string_view name) {
    std::string key = to_lower(trim_sv(name));
    if (key == "ball" || key == "ball-and-stick" || key == "ballstick") {
        return RenderStyle::BallAndStick;
    }
    if (key == "vdw" || key == "space" || key == "space-filling") {
        return RenderStyle::SpaceFilling;
    }
    if (key == "licorice" || key == "stick") {
        return RenderStyle::Licorice;
    }
    if (key == "wire" || key == "wireframe") {
        return RenderStyle::Wireframe;
    }
    return std::nullopt;
}

std::optional<RenderShading> parse_render_shading(std::string_view name) {
    std::string key = to_lower(trim_sv(name));
    if (key == "lit" || key == "phong" || key == "blinn-phong") {
        return RenderShading::Lit;
    }
    if (key == "flat" || key == "rim") {
        return RenderShading::Flat;
    }
    return std::nullopt;
}

std::optional<ColorMode> parse_color_mode(std::string_view name) {
    std::string key = to_lower(trim_sv(name));
    if (key == "element" || key == "cpk") {
        return ColorMode::Element;
    }
    if (key == "symop" || key == "sym" || key == "symmetry" || key == "operation") {
        return ColorMode::SymmetryOp;
    }
    if (key == "asym" || key == "site" || key == "asym-site" || key == "asymmetric") {
        return ColorMode::AsymmetricSite;
    }
    if (key == "mol" || key == "molecule" || key == "fragment") {
        return ColorMode::Molecule;
    }
    return std::nullopt;
}

void fit_camera_to_structure(
    const core::Structure& structure,
    Camera& camera,
    const RasterizerSettings& settings,
    bool reset_pan_offset
) {
    std::vector<glm::vec3> pts;
    std::vector<float> radii;
    pts.reserve(structure.atoms.size() + 8);
    radii.reserve(structure.atoms.size() + 8);

    for (const auto& atom : structure.atoms) {
        if (!Rasterizer::is_atom_displayed(structure, atom, settings)) continue;
        float r = 0.25f;
        if (settings.style == RenderStyle::SpaceFilling) {
            r = core::ElementTable::get_vdw_radius(atom.element);
        } else if (settings.style == RenderStyle::BallAndStick) {
            r = core::ElementTable::get_ball_stick_radius(atom.element) *
                (structure.is_periodic() ? 1.05f : 0.90f);
        } else {
            r = settings.bond_radius;
        }
        pts.push_back(atom.pos);
        radii.push_back(r);
    }

    if (settings.show_unit_cell && structure.unit_cell.has_value()) {
        for (int i = 0; i < 8; ++i) {
            glm::vec3 frac(
                (i & 1) ? 1.0f : 0.0f,
                (i & 2) ? 1.0f : 0.0f,
                (i & 4) ? 1.0f : 0.0f
            );
            pts.push_back(structure.unit_cell->fractional_to_cartesian(frac));
            radii.push_back(0.20f);
        }
    }

    if (!pts.empty()) {
        camera.fit_extent(structure.center_of_mass(), pts, radii, false, reset_pan_offset);
    }
}

bool aim_camera_along(
    const core::Structure& structure,
    Camera& camera,
    std::string_view expr,
    std::string* out_label,
    std::string* out_error
) {
    std::string raw = to_lower(trim_sv(expr));
    if (raw.empty()) {
        if (out_error) *out_error = "Usage: :view <x|y|z|a|b|c|a*|b*|c*|[uvw]|(hkl)>";
        return false;
    }

    float sign = 1.0f;
    if (raw.front() == '-') {
        sign = -1.0f;
        raw.erase(raw.begin());
    } else if (raw.front() == '+') {
        raw.erase(raw.begin());
    }

    glm::vec3 view_dir(0.0f);
    glm::vec3 up_hint(0.0f, 1.0f, 0.0f);
    std::string label;
    bool is_crystal_vec = false;
    bool is_reciprocal = false;

    auto set_cartesian = [&](int axis) {
        glm::vec3 d(0.0f);
        d[axis] = sign;
        view_dir = d;
        if (axis == 1) {
            up_hint = (sign > 0.0f) ? glm::vec3(0.0f, 0.0f, -1.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            up_hint = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        const char* names[3] = {"x", "y", "z"};
        label = std::string(sign < 0.0f ? "-" : "+") + names[axis];
    };

    if (raw == "x") {
        set_cartesian(0);
    } else if (raw == "y") {
        set_cartesian(1);
    } else if (raw == "z") {
        set_cartesian(2);
    } else if (raw == "a" || raw == "b" || raw == "c" ||
               raw == "a*" || raw == "b*" || raw == "c*") {
        if (!structure.unit_cell.has_value()) {
            if (out_error) *out_error = "Structure is not periodic (no unit cell). Use x, y, or z.";
            return false;
        }
        is_crystal_vec = true;
        is_reciprocal = (raw.size() == 2 && raw[1] == '*');
        int idx = (raw[0] == 'a') ? 0 : (raw[0] == 'b' ? 1 : 2);
        glm::vec3 indices(0.0f);
        indices[idx] = sign;
        const auto& cell = *structure.unit_cell;
        view_dir = is_reciprocal
            ? (glm::transpose(cell.frac_matrix) * indices)
            : (cell.orth_matrix * indices);
        label = std::string(sign < 0.0f ? "-" : "+") + raw;
    } else {
        std::string inner = raw;
        if (inner.size() >= 2 && inner.front() == '(' && inner.back() == ')') {
            is_reciprocal = true;
            inner = inner.substr(1, inner.size() - 2);
        } else if (inner.size() >= 2 && inner.front() == '[' && inner.back() == ']') {
            inner = inner.substr(1, inner.size() - 2);
        }
        for (char& ch : inner) {
            if (ch == ',' || ch == ';') ch = ' ';
        }
        std::istringstream vss(inner);
        float i0 = 0.0f, i1 = 0.0f, i2 = 0.0f;
        if (!(vss >> i0 >> i1 >> i2)) {
            std::vector<int> parsed;
            for (size_t pos = 0; pos < inner.size();) {
                if (std::isspace(static_cast<unsigned char>(inner[pos]))) {
                    ++pos;
                    continue;
                }
                int s = 1;
                if (inner[pos] == '-') {
                    s = -1;
                    ++pos;
                } else if (inner[pos] == '+') {
                    ++pos;
                }
                if (pos >= inner.size() || !std::isdigit(static_cast<unsigned char>(inner[pos]))) {
                    parsed.clear();
                    break;
                }
                parsed.push_back(s * (inner[pos] - '0'));
                ++pos;
            }
            if (parsed.size() != 3) {
                if (out_error) *out_error = "Invalid view direction: " + std::string(expr);
                return false;
            }
            i0 = static_cast<float>(parsed[0]);
            i1 = static_cast<float>(parsed[1]);
            i2 = static_cast<float>(parsed[2]);
        }

        glm::vec3 indices = sign * glm::vec3(i0, i1, i2);
        if (glm::length(indices) < 1e-6f) {
            if (out_error) *out_error = "View direction must be non-zero.";
            return false;
        }
        if (structure.unit_cell.has_value()) {
            is_crystal_vec = true;
            const auto& cell = *structure.unit_cell;
            view_dir = is_reciprocal
                ? (glm::transpose(cell.frac_matrix) * indices)
                : (cell.orth_matrix * indices);
        } else {
            view_dir = indices;
            up_hint = (std::abs(indices.x) < 1e-4f && std::abs(indices.z) < 1e-4f)
                ? glm::vec3(0.0f, 0.0f, (indices.y > 0.0f) ? -1.0f : 1.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
        }
        label = std::string(trim_sv(expr));
    }

    if (is_crystal_vec && structure.unit_cell.has_value()) {
        const auto& M = structure.unit_cell->orth_matrix;
        glm::vec3 w = glm::normalize(view_dir);
        const int order[3] = {1, 0, 2};
        float best_cos = 1e9f;
        for (int idx : order) {
            glm::vec3 vec = M[idx];
            float len = glm::length(vec);
            if (len < 1e-6f) continue;
            float c = std::abs(glm::dot(vec / len, w));
            if (c < best_cos - 1e-4f) {
                best_cos = c;
                up_hint = vec / len;
            }
        }
    }

    camera.look_along(view_dir, up_hint);
    if (out_label) *out_label = label;
    return true;
}

} // namespace moltui::render
