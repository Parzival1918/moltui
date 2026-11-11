#include "moltui/ui/app.hpp"
#include "moltui/core/elements.hpp"
#include "moltui/core/pbc_unwrapper.hpp"
#include "moltui/io/loader.hpp"
#include "moltui/render/view_utils.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <cmath>
#include <chrono>
#include <thread>

namespace moltui::ui {

namespace {

constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

double calculate_angle(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
    glm::vec3 v1 = glm::normalize(p1 - p2);
    glm::vec3 v2 = glm::normalize(p3 - p2);
    float dot_val = std::clamp(glm::dot(v1, v2), -1.0f, 1.0f);
    return std::acos(dot_val) * kRadToDeg;
}

double calculate_dihedral(
    const glm::vec3& p1,
    const glm::vec3& p2,
    const glm::vec3& p3,
    const glm::vec3& p4
) {
    glm::vec3 b1 = p2 - p1;
    glm::vec3 b2 = p3 - p2;
    glm::vec3 b3 = p4 - p3;

    glm::vec3 b2_norm = glm::normalize(b2);
    glm::vec3 n1 = glm::cross(b1, b2);
    glm::vec3 n2 = glm::cross(b2, b3);

    glm::vec3 m1 = glm::cross(n1, b2_norm);
    float x = glm::dot(n1, n2);
    float y = glm::dot(m1, n2);

    return std::atan2(y, x) * kRadToDeg;
}

} // namespace

App::App(core::Structure structure, render::TerminalGraphicsMode graphics_mode)
    : App(std::vector<core::Structure>{std::move(structure)}, graphics_mode) {}

App::App(std::vector<core::Structure> structures, render::TerminalGraphicsMode graphics_mode)
    : ensemble_(std::move(structures)),
      graphics_mode_(graphics_mode) {
    if (ensemble_.empty()) {
        ensemble_.emplace_back();
    }
    structure_ = ensemble_[0];
    original_structure_ = structure_;
    is_unwrapped_ = structure_.is_molecular_crystal;
    setup_camera();
}

void App::setup_camera() {
    const auto& bounds = structure_.get_bounds();
    camera_.reset(structure_.center_of_mass(), bounds.radius);
    active_axis_key_.clear();
    active_axis_flipped_ = false;
    refit_requested_ = true;
    refit_reset_pan_ = true;
}

void App::on_atom_picked(int atom_id) {
    if (atom_id < 0 || static_cast<size_t>(atom_id) >= structure_.atoms.size()) {
        // Clicked on background: clear selections if not empty
        if (!selected_atom_ids_.empty()) {
            for (size_t id : selected_atom_ids_) {
                if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
            }
            selected_atom_ids_.clear();
            status_message_ = "Selections cleared.";
        }
        return;
    }

    size_t id = static_cast<size_t>(atom_id);
    auto it = std::find(selected_atom_ids_.begin(), selected_atom_ids_.end(), id);

    if (it != selected_atom_ids_.end()) {
        // Deselect
        structure_.atoms[id].selected = false;
        selected_atom_ids_.erase(it);
    } else {
        // Cap selection at 4 atoms (for dihedral)
        if (selected_atom_ids_.size() >= 4) {
            structure_.atoms[selected_atom_ids_.front()].selected = false;
            selected_atom_ids_.erase(selected_atom_ids_.begin());
        }
        structure_.atoms[id].selected = true;
        selected_atom_ids_.push_back(id);
    }

    status_message_ = format_measurements();
}

std::string App::format_measurements() const {
    if (selected_atom_ids_.empty()) {
        return "No atoms selected. Click atoms to measure distance/angle/dihedral.";
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);

    if (selected_atom_ids_.size() == 1) {
        const auto& a = structure_.atoms[selected_atom_ids_[0]];
        ss << "Atom #" << (a.id + 1) << " " << a.element;
        if (!a.label.empty() && a.label != a.element) ss << " (" << a.label << ")";
        ss << " | Pos: (" << a.pos.x << ", " << a.pos.y << ", " << a.pos.z << ") Å";
        if (structure_.unit_cell.has_value()) {
            ss << " | Frac: (" << a.frac_pos.x << ", " << a.frac_pos.y << ", " << a.frac_pos.z << ")";
        }
        if (a.b_factor > 0.0f) ss << " | B: " << a.b_factor;
        if (a.charge != 0.0f) ss << " | q: " << a.charge;
    } else if (selected_atom_ids_.size() == 2) {
        const auto& a1 = structure_.atoms[selected_atom_ids_[0]];
        const auto& a2 = structure_.atoms[selected_atom_ids_[1]];
        float d = glm::length(a2.pos - a1.pos);
        ss << "Distance [" << a1.element << (a1.id + 1) << " - "
           << a2.element << (a2.id + 1) << "]: " << d << " Å";
    } else if (selected_atom_ids_.size() == 3) {
        const auto& a1 = structure_.atoms[selected_atom_ids_[0]];
        const auto& a2 = structure_.atoms[selected_atom_ids_[1]];
        const auto& a3 = structure_.atoms[selected_atom_ids_[2]];
        double angle = calculate_angle(a1.pos, a2.pos, a3.pos);
        ss << "Angle [" << a1.element << (a1.id + 1) << " - "
           << a2.element << (a2.id + 1) << " - "
           << a3.element << (a3.id + 1) << "]: " << angle << "°";
    } else if (selected_atom_ids_.size() == 4) {
        const auto& a1 = structure_.atoms[selected_atom_ids_[0]];
        const auto& a2 = structure_.atoms[selected_atom_ids_[1]];
        const auto& a3 = structure_.atoms[selected_atom_ids_[2]];
        const auto& a4 = structure_.atoms[selected_atom_ids_[3]];
        double dih = calculate_dihedral(a1.pos, a2.pos, a3.pos, a4.pos);
        ss << "Dihedral [" << a1.element << (a1.id + 1) << " - "
           << a2.element << (a2.id + 1) << " - "
           << a3.element << (a3.id + 1) << " - "
           << a4.element << (a4.id + 1) << "]: " << dih << "°";
    }

    return ss.str();
}

std::string App::format_structure_info() const {
    std::ostringstream ss;
    if (ensemble_.size() > 1) {
        ss << "[" << (ensemble_index_ + 1) << "/" << ensemble_.size() << "] ";
    } else if (structure_.num_frames() > 1) {
        ss << "[" << (structure_.current_frame + 1) << "/" << structure_.num_frames() << "] ";
    }
    ss << structure_.title << " | " << structure_.atoms.size() << " atoms, "
       << structure_.bonds.size() << " bonds";

    if (structure_.unit_cell.has_value()) {
        const auto& uc = *structure_.unit_cell;
        ss << " | SG: " << uc.space_group << " (" << std::fixed << std::setprecision(2)
           << uc.a << ", " << uc.b << ", " << uc.c << " Å; "
           << uc.alpha << "°, " << uc.beta << "°, " << uc.gamma << "°)";
    }

    return ss.str();
}

void App::toggle_unwrap() {
    if (!structure_.unit_cell.has_value()) {
        status_message_ = "Structure is not periodic (no unit cell).";
        return;
    }

    is_unwrapped_ = !is_unwrapped_;
    if (is_unwrapped_) {
        structure_.unwrap_pbc();
        status_message_ = "Unwrapped molecules across periodic boundaries.";
    } else {
        if (structure_.is_molecular_crystal) {
            structure_.wrap_pbc();
            status_message_ = "Wrapped coordinates back into primary unit cell.";
        } else {
            core::PbcUnwrapper::complete_network_boundaries(structure_);
            status_message_ = "Completed periodic network boundaries.";
        }
    }
}

void App::cycle_style() {
    auto& s = rasterizer_.settings().style;
    if (s == render::RenderStyle::BallAndStick) {
        s = render::RenderStyle::SpaceFilling;
        status_message_ = "Style: Space-filling (VDW)";
    } else if (s == render::RenderStyle::SpaceFilling) {
        s = render::RenderStyle::Licorice;
        status_message_ = "Style: Licorice";
    } else if (s == render::RenderStyle::Licorice) {
        s = render::RenderStyle::Wireframe;
        status_message_ = "Style: Wireframe";
    } else {
        s = render::RenderStyle::BallAndStick;
        status_message_ = "Style: Ball-and-stick";
    }
}

void App::toggle_shading() {
    auto& sh = rasterizer_.settings().shading;
    if (sh == render::RenderShading::Lit) {
        sh = render::RenderShading::Flat;
        status_message_ = "Shading: Flat (rim)";
    } else {
        sh = render::RenderShading::Lit;
        status_message_ = "Shading: Lit (Blinn-Phong)";
    }
}

void App::step_ensemble(int delta) {
    if (ensemble_.size() > 1) {
        size_t n = ensemble_.size();
        ensemble_index_ = (ensemble_index_ + n + static_cast<size_t>(delta % static_cast<int>(n))) % n;
        structure_ = ensemble_[ensemble_index_];
        original_structure_ = structure_;
        selected_atom_ids_.clear();
        is_unwrapped_ = structure_.is_molecular_crystal;
        // Preserve orientation and zoom while recentering rotation pivot on the new structure's COM
        camera_.set_target(structure_.center_of_mass());
        status_message_ = "Structure " + std::to_string(ensemble_index_ + 1) + "/" +
                          std::to_string(n) + ": " + structure_.title;
        return;
    }

    if (structure_.num_frames() > 1) {
        size_t n = structure_.num_frames();
        size_t next = (structure_.current_frame + n + static_cast<size_t>(delta % static_cast<int>(n))) % n;
        structure_.set_frame(next);
        status_message_ = "Frame " + std::to_string(structure_.current_frame + 1) + "/" +
                          std::to_string(n);
    }
}

bool App::aim_along(std::string_view expr) {
    std::string label;
    std::string err;
    if (!render::aim_camera_along(structure_, camera_, expr, &label, &err)) {
        status_message_ = err;
        return false;
    }
    last_aimed_orientation_ = camera_.orientation();
    refit_requested_ = true;
    refit_reset_pan_ = false;
    status_message_ = "View along " + label;
    return true;
}

void App::set_initial_frame(size_t one_based_idx) {
    if (one_based_idx == 0) return;
    if (ensemble_.size() > 1) {
        size_t idx = std::min(one_based_idx - 1, ensemble_.size() - 1);
        ensemble_index_ = idx;
        structure_ = ensemble_[idx];
        original_structure_ = structure_;
        is_unwrapped_ = structure_.is_molecular_crystal;
        setup_camera();
    } else if (structure_.num_frames() > 1) {
        size_t idx = std::min(one_based_idx - 1, structure_.num_frames() - 1);
        structure_.set_frame(idx);
        setup_camera();
    }
}

void App::set_initial_unwrap(bool unwrap) {
    if (unwrap && structure_.is_periodic() && !is_unwrapped_) {
        structure_.unwrap_pbc();
        is_unwrapped_ = true;
        setup_camera();
    }
}

void App::set_initial_wrap(bool wrap) {
    if (wrap && structure_.is_periodic() && is_unwrapped_) {
        structure_.wrap_pbc();
        is_unwrapped_ = false;
        setup_camera();
    }
}

bool App::set_initial_view(std::string_view expr) {
    return aim_along(expr);
}

void App::set_initial_rotation(float dx, float dy) {
    camera_.rotate(dx, dy);
    active_axis_key_.clear();
}

void App::set_initial_pan(float dx, float dy) {
    initial_pan_dx_ = dx;
    initial_pan_dy_ = dy;
}

void App::set_initial_zoom_mag(float mag) {
    if (mag > 0.0f) {
        initial_zoom_mag_ = mag;
    }
}

void App::select_atoms(const std::vector<size_t>& one_based_ids) {
    for (size_t id : selected_atom_ids_) {
        if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
    }
    selected_atom_ids_.clear();
    for (size_t one_based : one_based_ids) {
        if (one_based >= 1 && one_based <= structure_.atoms.size()) {
            size_t id = one_based - 1;
            if (std::find(selected_atom_ids_.begin(), selected_atom_ids_.end(), id) == selected_atom_ids_.end()) {
                if (selected_atom_ids_.size() >= 4) break;
                structure_.atoms[id].selected = true;
                selected_atom_ids_.push_back(id);
            }
        }
    }
    if (!selected_atom_ids_.empty()) {
        status_message_ = format_measurements();
    }
}

void App::aim_axis_shortcut(char key) {
    std::string key_str(1, key);
    bool same_axis = (active_axis_key_ == key_str) &&
                     (std::abs(glm::dot(camera_.orientation(), last_aimed_orientation_)) > 0.999f);
    if (same_axis) {
        active_axis_flipped_ = !active_axis_flipped_;
    } else {
        active_axis_key_ = key_str;
        active_axis_flipped_ = false;
    }

    std::string prefix = active_axis_flipped_ ? "-" : "+";
    if (key == 'x' || key == 'y' || key == 'z') {
        aim_along(prefix + key_str);
        return;
    }

    const bool has_cell = structure_.unit_cell.has_value();
    const char* lattice_names[3] = {"a", "b", "c"};
    const char* cart_names[3] = {"x", "y", "z"};
    int idx = (key == '1') ? 0 : (key == '2' ? 1 : 2);
    aim_along(prefix + (has_cell ? lattice_names[idx] : cart_names[idx]));
}

void App::execute_command(const std::string& cmd_line) {
    if (cmd_line.empty()) return;
    std::istringstream iss(cmd_line);
    std::string cmd_raw;
    iss >> cmd_raw;
    std::string cmd = cmd_raw;
    for (char& ch : cmd) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }

    auto parse_bool_arg = [&](bool current) -> std::optional<bool> {
        std::string arg;
        if (!(iss >> arg)) {
            return !current;
        }
        for (char& ch : arg) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (arg == "on" || arg == "true" || arg == "1" || arg == "yes") return true;
        if (arg == "off" || arg == "false" || arg == "0" || arg == "no") return false;
        return std::nullopt;
    };

    if (cmd == "q" || cmd == "quit") {
        quit_requested_ = true;
    } else if (cmd == "help" || cmd == "?") {
        auto val = parse_bool_arg(show_help_);
        if (val.has_value()) {
            show_help_ = *val;
        } else {
            show_help_ = !show_help_;
        }
        status_message_ = show_help_ ? "Help opened." : "Help closed.";
    } else if (cmd == "load") {
        std::string path;
        if (iss >> path) {
            try {
                auto loaded = io::StructureLoader::load_structures(path);
                if (!loaded.empty()) {
                    ensemble_ = std::move(loaded);
                    ensemble_index_ = 0;
                    structure_ = ensemble_[0];
                    original_structure_ = structure_;
                    selected_atom_ids_.clear();
                    is_unwrapped_ = structure_.is_molecular_crystal;
                    setup_camera();
                    status_message_ = "Loaded " + path + " (" + std::to_string(ensemble_.size()) +
                                      (ensemble_.size() == 1 ? " structure)" : " structures)");
                }
            } catch (const std::exception& e) {
                status_message_ = "Error loading file: " + std::string(e.what());
            }
        } else {
            status_message_ = "Usage: :load <filepath>";
        }
    } else if (cmd == "cell" || cmd == "supercell") {
        int nx = 1, ny = 1, nz = 1;
        if (iss >> nx >> ny >> nz) {
            if (original_structure_.unit_cell.has_value()) {
                structure_ = original_structure_.generate_supercell(nx, ny, nz);
                selected_atom_ids_.clear();
                is_unwrapped_ = structure_.is_molecular_crystal;
                setup_camera();
                status_message_ = "Generated supercell: " + std::to_string(nx) + "x" +
                                  std::to_string(ny) + "x" + std::to_string(nz);
            } else {
                status_message_ = "Structure has no unit cell.";
            }
        } else {
            status_message_ = "Usage: :cell <nx> <ny> <nz>";
        }
    } else if (cmd == "unwrap") {
        if (!is_unwrapped_) toggle_unwrap();
    } else if (cmd == "wrap") {
        if (is_unwrapped_) toggle_unwrap();
    } else if (cmd == "next" || cmd == "n") {
        step_ensemble(1);
    } else if (cmd == "prev" || cmd == "p") {
        step_ensemble(-1);
    } else if (cmd == "frame") {
        size_t idx = 0;
        if (iss >> idx && idx >= 1) {
            if (ensemble_.size() > 1) {
                if (idx <= ensemble_.size()) {
                    step_ensemble(static_cast<int>(idx - 1) - static_cast<int>(ensemble_index_));
                } else {
                    status_message_ = "Structure index out of range (1-" + std::to_string(ensemble_.size()) + ").";
                }
            } else if (structure_.num_frames() > 1) {
                if (idx <= structure_.num_frames()) {
                    structure_.set_frame(idx - 1);
                    status_message_ = "Frame " + std::to_string(idx) + "/" + std::to_string(structure_.num_frames());
                } else {
                    status_message_ = "Frame index out of range (1-" + std::to_string(structure_.num_frames()) + ").";
                }
            } else {
                status_message_ = "Only 1 frame/structure loaded.";
            }
        } else {
            status_message_ = "Usage: :frame <1-based-index>";
        }
    } else if (cmd == "play") {
        if (structure_.num_frames() > 1) {
            is_playing_ = true;
            status_message_ = "Trajectory playing...";
        } else {
            status_message_ = "Single-frame structure (nothing to play).";
        }
    } else if (cmd == "pause") {
        is_playing_ = false;
        status_message_ = "Trajectory paused.";
    } else if (cmd == "view" || cmd == "aim") {
        std::string rest;
        std::getline(iss, rest);
        aim_along(rest);
    } else if (cmd == "x" || cmd == "+x" || cmd == "-x" ||
               cmd == "y" || cmd == "+y" || cmd == "-y" ||
               cmd == "z" || cmd == "+z" || cmd == "-z" ||
               cmd == "a" || cmd == "+a" || cmd == "-a" ||
               cmd == "b" || cmd == "+b" || cmd == "-b" ||
               cmd == "c" || cmd == "+c" || cmd == "-c" ||
               cmd == "a*" || cmd == "+a*" || cmd == "-a*" ||
               cmd == "b*" || cmd == "+b*" || cmd == "-b*" ||
               cmd == "c*" || cmd == "+c*" || cmd == "-c*") {
        aim_along(cmd);
    } else if (cmd == "rotate" || cmd == "rot") {
        float dx = 0.0f, dy = 0.0f;
        if (iss >> dx >> dy) {
            camera_.rotate(dx, dy);
            active_axis_key_.clear();
            status_message_ = "Rotated view.";
        } else {
            status_message_ = "Usage: :rotate <dx> <dy>";
        }
    } else if (cmd == "pan" || cmd == "move" || cmd == "displace") {
        float dx = 0.0f, dy = 0.0f;
        if (iss >> dx >> dy) {
            camera_.pan(dx, dy);
            status_message_ = "Displaced structure.";
        } else {
            status_message_ = "Usage: :pan <dx> <dy>";
        }
    } else if (cmd == "zoom") {
        float factor = 1.0f;
        if (iss >> factor && factor > 0.0f) {
            camera_.zoom(factor);
            status_message_ = "Zoomed.";
        } else {
            status_message_ = "Usage: :zoom <factor> (e.g. 0.8 to zoom in, 1.2 out)";
        }
    } else if (cmd == "style") {
        std::string mode;
        if (iss >> mode) {
            if (auto parsed = render::parse_render_style(mode)) {
                rasterizer_.settings().style = *parsed;
                status_message_ = "Style updated to " + mode;
            } else {
                status_message_ = "Unknown style '" + mode + "' (use ball, vdw, licorice, wire).";
            }
        } else {
            cycle_style();
        }
    } else if (cmd == "shading") {
        std::string mode;
        if (iss >> mode) {
            if (auto parsed = render::parse_render_shading(mode)) {
                rasterizer_.settings().shading = *parsed;
                status_message_ = (*parsed == render::RenderShading::Flat)
                    ? "Shading: Flat (rim)"
                    : "Shading: Lit (Blinn-Phong)";
            } else {
                status_message_ = "Unknown shading '" + mode + "' (use lit or flat).";
            }
        } else {
            toggle_shading();
        }
    } else if (cmd == "ortho") {
        camera_.orthographic = true;
        status_message_ = "Projection: Orthographic";
    } else if (cmd == "persp" || cmd == "perspective") {
        camera_.orthographic = false;
        status_message_ = "Projection: Perspective";
    } else if (cmd == "proj" || cmd == "projection") {
        std::string arg;
        if (iss >> arg) {
            for (char& ch : arg) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            camera_.orthographic = (arg != "persp" && arg != "perspective");
        } else {
            camera_.orthographic = !camera_.orthographic;
        }
        status_message_ = camera_.orthographic ? "Projection: Orthographic" : "Projection: Perspective";
    } else if (cmd == "draft" || cmd == "adaptive") {
        auto val = parse_bool_arg(adaptive_quality_);
        adaptive_quality_ = val.value_or(!adaptive_quality_);
        status_message_ = adaptive_quality_ ? "Adaptive draft quality: ON" : "Adaptive draft quality: OFF";
    } else if (cmd == "mode" || cmd == "render") {
        std::string mode_str;
        if (iss >> mode_str) {
            auto parsed = render::ProtocolRenderer::parse_mode(mode_str);
            if (parsed.has_value()) {
                graphics_mode_ = *parsed;
                status_message_ = std::string("Render mode: ") + render::ProtocolRenderer::mode_name(graphics_mode_);
            } else {
                status_message_ = "Unknown render mode '" + mode_str + "' (use auto, kitty, half-blocks, ascii).";
            }
        } else {
            status_message_ = std::string("Current render mode: ") + render::ProtocolRenderer::mode_name(graphics_mode_);
        }
    } else if (cmd == "unitcell" || cmd == "showcell") {
        auto val = parse_bool_arg(rasterizer_.settings().show_unit_cell);
        rasterizer_.settings().show_unit_cell = val.value_or(!rasterizer_.settings().show_unit_cell);
        status_message_ = rasterizer_.settings().show_unit_cell ? "Unit cell ON" : "Unit cell OFF";
    } else if (cmd == "bonds") {
        auto val = parse_bool_arg(rasterizer_.settings().show_bonds);
        rasterizer_.settings().show_bonds = val.value_or(!rasterizer_.settings().show_bonds);
        status_message_ = rasterizer_.settings().show_bonds ? "Bonds ON" : "Bonds OFF";
    } else if (cmd == "hkl") {
        std::string first;
        if (iss >> first) {
            if (first == "off" || first == "clear" || first == "none") {
                rasterizer_.settings().miller_plane = std::nullopt;
                status_message_ = "Miller plane cleared.";
            } else {
                try {
                    int h = std::stoi(first);
                    int k = 0, l = 0;
                    if (iss >> k >> l) {
                        rasterizer_.settings().miller_plane = glm::ivec3(h, k, l);
                        status_message_ = "Miller plane (" + std::to_string(h) + " " +
                                          std::to_string(k) + " " + std::to_string(l) + ") displayed.";
                    } else {
                        status_message_ = "Usage: :hkl <h> <k> <l> | :hkl off";
                    }
                } catch (...) {
                    status_message_ = "Usage: :hkl <h> <k> <l> | :hkl off";
                }
            }
        } else {
            status_message_ = "Usage: :hkl <h> <k> <l> | :hkl off";
        }
    } else if (cmd == "measure" || cmd == "select") {
        std::vector<size_t> ids;
        size_t one_based = 0;
        while (iss >> one_based) {
            if (one_based >= 1 && one_based <= structure_.atoms.size()) {
                ids.push_back(one_based - 1);
            }
        }
        if (ids.empty()) {
            status_message_ = format_measurements();
        } else {
            for (size_t id : selected_atom_ids_) {
                if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
            }
            selected_atom_ids_.clear();
            for (size_t id : ids) {
                if (selected_atom_ids_.size() >= 4) break;
                if (std::find(selected_atom_ids_.begin(), selected_atom_ids_.end(), id) == selected_atom_ids_.end()) {
                    structure_.atoms[id].selected = true;
                    selected_atom_ids_.push_back(id);
                }
            }
            status_message_ = format_measurements();
        }
    } else if (cmd == "reset" || cmd == "r") {
        setup_camera();
        status_message_ = "Camera view reset.";
    } else if (cmd == "clear") {
        for (size_t id : selected_atom_ids_) {
            if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
        }
        selected_atom_ids_.clear();
        rasterizer_.settings().miller_plane = std::nullopt;
        status_message_ = "Selections and planes cleared.";
    } else {
        status_message_ = "Unknown command: " + cmd_raw + " (type :help for reference)";
    }
}

void App::run() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    constexpr uint32_t kKittyImageId = 1;
    bool kitty_displayed = false;
    bool kitty_image_uploaded = false;
    bool sync_open = false;
    bool kitty_post_queued = false;

    auto write_seq = [](std::string_view seq) {
        std::cout.write(seq.data(), static_cast<std::streamsize>(seq.size()));
        std::cout.flush();
    };

    std::shared_ptr<MolViewport> viewport;

    auto delete_kitty = [&]() {
        if (kitty_displayed || kitty_image_uploaded) {
            write_seq(render::ProtocolRenderer::delete_kitty_image(kKittyImageId));
            kitty_displayed = false;
            kitty_image_uploaded = false;
        }
    };

    auto sync_kitty_overlay = [&]() {
        kitty_post_queued = false;
        if (graphics_mode_ != render::TerminalGraphicsMode::KittyGraphics) {
            if (sync_open) {
                write_seq("\x1b[?2026l");
                sync_open = false;
            }
            return;
        }
        if (show_help_) {
            delete_kitty();
            viewport->clear_kitty_pending();
        } else if (viewport->consume_viewport_drawn()) {
            bool needs_upload = viewport->consume_kitty_image_dirty() || !kitty_image_uploaded;
            std::string cmd = needs_upload
                ? viewport->build_kitty_frame(kKittyImageId)
                : viewport->build_kitty_place(kKittyImageId);
            if (!cmd.empty()) {
                write_seq(cmd);
                kitty_displayed = true;
                if (needs_upload) {
                    kitty_image_uploaded = true;
                }
            }
        }
        if (sync_open) {
            write_seq("\x1b[?2026l");
            sync_open = false;
        }
    };

    std::mutex timer_mtx;
    std::condition_variable timer_cv;
    bool timer_stop = false;
    bool refine_pending = false;
    auto refine_deadline = std::chrono::steady_clock::now();

    auto trigger_draft_interaction = [&]() {
        if (!adaptive_quality_) return;
        viewport->set_draft_mode(true);
        {
            std::lock_guard<std::mutex> lk(timer_mtx);
            refine_pending = true;
            refine_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(180);
        }
        timer_cv.notify_one();
    };

    std::thread refine_thread([&]() {
        std::unique_lock<std::mutex> lk(timer_mtx);
        while (!timer_stop) {
            if (!refine_pending) {
                timer_cv.wait(lk, [&]() { return timer_stop || refine_pending; });
                continue;
            }
            if (timer_cv.wait_until(lk, refine_deadline, [&]() { return timer_stop; })) {
                break;
            }
            if (refine_pending && std::chrono::steady_clock::now() >= refine_deadline) {
                refine_pending = false;
                lk.unlock();
                screen.Post([&]() {
                    if (viewport->draft_mode()) {
                        viewport->set_draft_mode(false);
                    }
                });
                screen.PostEvent(ftxui::Event::Custom);
                lk.lock();
            }
        }
    });

    viewport = std::make_shared<MolViewport>(
        structure_,
        camera_,
        rasterizer_,
        [this](int atom_id) { on_atom_picked(atom_id); },
        graphics_mode_,
        [&]() {
            if (graphics_mode_ == render::TerminalGraphicsMode::KittyGraphics) {
                if (!sync_open) {
                    write_seq("\x1b[?2026h");
                    sync_open = true;
                }
                if (!kitty_post_queued) {
                    kitty_post_queued = true;
                    screen.Post(sync_kitty_overlay);
                }
            }
        },
        trigger_draft_interaction
    );
    viewport->set_adaptive_quality(adaptive_quality_);
    if (initial_zoom_mag_ != 1.0f || initial_pan_dx_ != 0.0f || initial_pan_dy_ != 0.0f) {
        viewport->set_post_fit_hook([this]() {
            if (initial_zoom_mag_ != 1.0f) {
                camera_.zoom(1.0f / initial_zoom_mag_);
            }
            if (initial_pan_dx_ != 0.0f || initial_pan_dy_ != 0.0f) {
                camera_.pan(initial_pan_dx_, initial_pan_dy_);
            }
        });
    }

    auto component = ftxui::CatchEvent(viewport, [&](ftxui::Event event) {
        if (command_mode_) {
            if (event == ftxui::Event::Escape) {
                command_mode_ = false;
                command_buffer_.clear();
                return true;
            }
            if (event == ftxui::Event::Return) {
                command_mode_ = false;
                if (!command_buffer_.empty()) {
                    if (command_history_.empty() || command_history_.back() != command_buffer_) {
                        command_history_.push_back(command_buffer_);
                    }
                }
                execute_command(command_buffer_);
                command_buffer_.clear();
                if (viewport->graphics_mode() != graphics_mode_) {
                    if (viewport->graphics_mode() == render::TerminalGraphicsMode::KittyGraphics) {
                        delete_kitty();
                    }
                    viewport->set_graphics_mode(graphics_mode_);
                }
                viewport->set_adaptive_quality(adaptive_quality_);
                if (refit_requested_) {
                    viewport->request_refit(refit_reset_pan_);
                    refit_requested_ = false;
                    refit_reset_pan_ = true;
                } else {
                    viewport->mark_scene_dirty();
                }
                if (quit_requested_) {
                    screen.Exit();
                }
                return true;
            }
            if (event == ftxui::Event::ArrowUp) {
                if (!command_history_.empty() && history_cursor_ > 0) {
                    if (history_cursor_ == command_history_.size()) {
                        history_draft_ = command_buffer_;
                    }
                    --history_cursor_;
                    command_buffer_ = command_history_[history_cursor_];
                }
                return true;
            }
            if (event == ftxui::Event::ArrowDown) {
                if (history_cursor_ < command_history_.size()) {
                    ++history_cursor_;
                    command_buffer_ = (history_cursor_ == command_history_.size())
                        ? history_draft_
                        : command_history_[history_cursor_];
                }
                return true;
            }
            if (event == ftxui::Event::Backspace) {
                if (!command_buffer_.empty()) command_buffer_.pop_back();
                return true;
            }
            if (event.is_character()) {
                command_buffer_ += event.character();
                return true;
            }
            return true;
        }

        // Hotkeys in normal mode
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            screen.Exit();
            return true;
        }
        if (event == ftxui::Event::Character(':')) {
            command_mode_ = true;
            command_buffer_.clear();
            history_cursor_ = command_history_.size();
            history_draft_.clear();
            return true;
        }
        if (event == ftxui::Event::Character('x') ||
            event == ftxui::Event::Character('y') ||
            event == ftxui::Event::Character('z') ||
            event == ftxui::Event::Character('1') ||
            event == ftxui::Event::Character('2') ||
            event == ftxui::Event::Character('3')) {
            aim_axis_shortcut(event.character()[0]);
            if (refit_requested_) {
                viewport->request_refit(refit_reset_pan_);
                refit_requested_ = false;
                refit_reset_pan_ = true;
            } else {
                viewport->mark_scene_dirty();
            }
            return true;
        }
        if (event == ftxui::Event::Character('d')) {
            adaptive_quality_ = !adaptive_quality_;
            viewport->set_adaptive_quality(adaptive_quality_);
            status_message_ = adaptive_quality_ ? "Adaptive draft quality: ON" : "Adaptive draft quality: OFF";
            return true;
        }
        if (event == ftxui::Event::Character('r')) {
            setup_camera();
            viewport->request_refit();
            refit_requested_ = false;
            status_message_ = "Camera reset.";
            return true;
        }
        if (event == ftxui::Event::Character('u')) {
            toggle_unwrap();
            viewport->mark_scene_dirty();
            return true;
        }
        if (event == ftxui::Event::Character('s')) {
            cycle_style();
            viewport->mark_scene_dirty();
            return true;
        }
        if (event == ftxui::Event::Character('f')) {
            toggle_shading();
            viewport->mark_scene_dirty();
            return true;
        }
        if (event == ftxui::Event::Character('o')) {
            camera_.orthographic = !camera_.orthographic;
            viewport->mark_scene_dirty();
            status_message_ = camera_.orthographic ? "Projection: Orthographic" : "Projection: Perspective";
            return true;
        }
        if (event == ftxui::Event::Character('c')) {
            rasterizer_.settings().show_unit_cell = !rasterizer_.settings().show_unit_cell;
            viewport->mark_scene_dirty();
            status_message_ = rasterizer_.settings().show_unit_cell ? "Unit cell ON" : "Unit cell OFF";
            return true;
        }
        if (event == ftxui::Event::Character('b')) {
            rasterizer_.settings().show_bonds = !rasterizer_.settings().show_bonds;
            viewport->mark_scene_dirty();
            status_message_ = rasterizer_.settings().show_bonds ? "Bonds ON" : "Bonds OFF";
            return true;
        }
        if (event == ftxui::Event::Character('m')) {
            for (size_t id : selected_atom_ids_) {
                if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
            }
            selected_atom_ids_.clear();
            viewport->mark_scene_dirty();
            status_message_ = "Selections cleared.";
            return true;
        }
        if (event == ftxui::Event::Character('?') || event == ftxui::Event::F1) {
            show_help_ = !show_help_;
            return true;
        }
        if (event == ftxui::Event::Character(']') || event == ftxui::Event::Character('n')) {
            step_ensemble(1);
            viewport->mark_scene_dirty();
            return true;
        }
        if (event == ftxui::Event::Character('[') || event == ftxui::Event::Character('p')) {
            step_ensemble(-1);
            viewport->mark_scene_dirty();
            return true;
        }
        if (event == ftxui::Event::Character(' ')) {
            if (structure_.num_frames() > 1) {
                is_playing_ = !is_playing_;
                status_message_ = is_playing_ ? "Trajectory playing..." : "Trajectory paused.";
            }
            return true;
        }

        // Camera keyboard controls: hjkl rotate around COM, Shift+hjkl displace on screen
        if (event == ftxui::Event::Character('h')) { camera_.rotate(-6.0f, 0.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('l')) { camera_.rotate(6.0f, 0.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('j')) { camera_.rotate(0.0f, 6.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('k')) { camera_.rotate(0.0f, -6.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('H')) { camera_.pan(-12.0f, 0.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('L')) { camera_.pan(12.0f, 0.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('J')) { camera_.pan(0.0f, -12.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('K')) { camera_.pan(0.0f, 12.0f); viewport->mark_scene_dirty(); trigger_draft_interaction(); return true; }
        if (event == ftxui::Event::Character('+') || event == ftxui::Event::Character('=')) {
            camera_.zoom(0.9f);
            viewport->mark_scene_dirty();
            trigger_draft_interaction();
            return true;
        }
        if (event == ftxui::Event::Character('-') || event == ftxui::Event::Character('_')) {
            camera_.zoom(1.1f);
            viewport->mark_scene_dirty();
            trigger_draft_interaction();
            return true;
        }

        return false;
    });

    auto renderer = ftxui::Renderer(component, [&] {
        using namespace ftxui;

        // Title Bar
        Elements title_items = {
            text(" MolTUI ") | bold | bgcolor(Color::RGB(41, 98, 255)) | color(Color::White),
            separator(),
            text(" " + format_structure_info() + " ") | color(Color::RGB(180, 200, 220)) | flex_shrink,
            filler()
        };
        if (structure_.unit_cell.has_value() && rasterizer_.settings().show_unit_cell) {
            title_items.push_back(hbox({
                text(" Cell: "),
                text("■a ") | bold | color(Color::RGB(228, 86, 86)),
                text("■b ") | bold | color(Color::RGB(96, 196, 112)),
                text("■c ") | bold | color(Color::RGB(104, 150, 244))
            }) | color(Color::RGB(160, 175, 200)));
        }
        title_items.push_back(
            text(std::string(" [") + render::ProtocolRenderer::mode_name(graphics_mode_) + "] ") | bold |
            color(Color::RGB(140, 200, 255))
        );
        title_items.push_back(
            text(is_unwrapped_ ? " [UNWRAPPED PBC] " : " [UNIT CELL] ") | bold |
            color(is_unwrapped_ ? Color::RGB(80, 230, 120) : Color::RGB(240, 200, 60))
        );
        auto title_bar = hbox(std::move(title_items)) | bgcolor(Color::RGB(20, 24, 34));

        // Bottom Bar / Command Bar
        Element bottom_bar;
        if (command_mode_) {
            bottom_bar = hbox({
                text(":") | bold | color(Color::RGB(255, 200, 40)),
                text(command_buffer_) | bold | color(Color::White),
                text("█") | blink | color(Color::RGB(255, 200, 40))
            }) | bgcolor(Color::RGB(30, 34, 46));
        } else {
            bottom_bar = hbox({
                text(" " + status_message_ + " ") | color(Color::RGB(220, 225, 235)),
                filler(),
                text(" [s]tyle [f]shading [d]raft [o]rtho [u]nwrap [c]ell [?]help [q]uit ") |
                    color(Color::RGB(120, 140, 160))
            }) | bgcolor(Color::RGB(24, 28, 40));
        }

        auto layout = vbox({
            title_bar,
            viewport->Render() | flex,
            bottom_bar
        });

        if (show_help_) {
            auto help_box = clear_under(
                window(
                    text(" MolTUI Quick Reference ") | bold | color(Color::White),
                    vbox({
                        text(" Mouse Controls ") | bold | color(Color::RGB(255, 200, 40)),
                        text("   Left-Click + Drag    : Rotate around center of mass") | color(Color::RGB(190, 220, 255)),
                        text("   Right-Click / Shift  : Displace structure (screen pan)") | color(Color::RGB(190, 220, 255)),
                        text("   Scroll Wheel         : Zoom in / out") | color(Color::RGB(190, 220, 255)),
                        text("   Left-Click on Atom   : Pick atom for measurements") | color(Color::RGB(190, 220, 255)),
                        separator() | color(Color::RGB(60, 70, 95)),
                        text(" Keybindings ") | bold | color(Color::RGB(255, 200, 40)),
                        text("   h / j / k / l        : Rotate around COM (left/down/up/right)") | color(Color::RGB(240, 220, 120)),
                        text("   Shift + h/j/k/l      : Displace structure (pan left/down/up/right)") | color(Color::RGB(240, 220, 120)),
                        text("   x / y / z            : View along Cartesian x / y / z (repeat flips)") | color(Color::RGB(240, 220, 120)),
                        text("   1 / 2 / 3            : View along lattice a / b / c (or x/y/z)") | color(Color::RGB(240, 220, 120)),
                        text("   + / -                : Zoom in / out") | color(Color::RGB(240, 220, 120)),
                        text("   s / f / d / o        : Style / flat shading / draft motion / ortho") | color(Color::RGB(200, 215, 235)),
                        text("   u                    : Toggle PBC unwrapping (Make Whole)") | color(Color::RGB(120, 240, 140)),
                        text("   c / b                : Toggle unit cell / bonds") | color(Color::RGB(200, 215, 235)),
                        text("   m                    : Clear active measurements") | color(Color::RGB(200, 215, 235)),
                        text("   r                    : Reset camera view") | color(Color::RGB(200, 215, 235)),
                        text("   Space / [ / ] / n/p  : Play/pause, prev/next frame or structure") | color(Color::RGB(200, 215, 235)),
                        text("   :                    : Commands (Up/Down history: :view, :x/:a, :style,") | color(Color::RGB(255, 180, 100)),
                        text("                          :shading, :mode, :bonds, :unitcell, :cell, :hkl, :q)") | color(Color::RGB(255, 180, 100)),
                        text("   ?                    : Toggle this help") | color(Color::RGB(200, 215, 235)),
                        text("   q / Esc              : Quit") | color(Color::RGB(255, 110, 110)),
                    }) | bgcolor(Color::RGB(20, 24, 36))
                ) | bgcolor(Color::RGB(20, 24, 36)) | color(Color::RGB(120, 150, 195))
            ) | center;

            return dbox({layout, help_box});
        }

        return layout;
    });

    // Switch from all-motion mouse tracking (?1003h) to button-event tracking (?1002h)
    // once FTXUI installs its terminal hooks, preventing hover events from causing redraws.
    screen.Post([&]() {
        write_seq("\x1b[?1003l\x1b[?1002h");
    });

    screen.Loop(renderer);

    {
        std::lock_guard<std::mutex> lk(timer_mtx);
        timer_stop = true;
    }
    timer_cv.notify_one();
    refine_thread.join();

    if (sync_open) {
        write_seq("\x1b[?2026l");
        sync_open = false;
    }
    write_seq("\x1b[?1002l");
    delete_kitty();
}

} // namespace moltui::ui
