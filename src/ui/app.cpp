#include "moltui/ui/app.hpp"
#include "moltui/core/elements.hpp"
#include "moltui/io/loader.hpp"

#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <glm/geometric.hpp>
#include <iomanip>
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

App::App(core::Structure structure)
    : structure_(std::move(structure)),
      original_structure_(structure_) {
    setup_camera();
}

void App::setup_camera() {
    const auto& bounds = structure_.get_bounds();
    camera_.reset(bounds.center, bounds.radius);
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
    ss << structure_.title << " | " << structure_.atoms.size() << " atoms, "
       << structure_.bonds.size() << " bonds";

    if (structure_.unit_cell.has_value()) {
        const auto& uc = *structure_.unit_cell;
        ss << " | SG: " << uc.space_group << " (" << std::fixed << std::setprecision(2)
           << uc.a << ", " << uc.b << ", " << uc.c << " Å; "
           << uc.alpha << "°, " << uc.beta << "°, " << uc.gamma << "°)";
    }

    if (structure_.num_frames() > 1) {
        ss << " | Frame " << (structure_.current_frame + 1) << "/" << structure_.num_frames();
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
        structure_.wrap_pbc();
        status_message_ = "Wrapped coordinates back into primary unit cell.";
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

void App::execute_command(const std::string& cmd_line) {
    if (cmd_line.empty()) return;
    std::istringstream iss(cmd_line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "q" || cmd == "quit") {
        std::exit(0);
    } else if (cmd == "load") {
        std::string path;
        if (iss >> path) {
            try {
                auto new_st = io::StructureLoader::load_file(path);
                structure_ = new_st;
                original_structure_ = new_st;
                selected_atom_ids_.clear();
                is_unwrapped_ = false;
                setup_camera();
                status_message_ = "Loaded " + path;
            } catch (const std::exception& e) {
                status_message_ = "Error loading file: " + std::string(e.what());
            }
        }
    } else if (cmd == "cell" || cmd == "supercell") {
        int nx = 1, ny = 1, nz = 1;
        if (iss >> nx >> ny >> nz) {
            if (original_structure_.unit_cell.has_value()) {
                structure_ = original_structure_.generate_supercell(nx, ny, nz);
                selected_atom_ids_.clear();
                setup_camera();
                status_message_ = "Generated supercell: " + std::to_string(nx) + "x" +
                                  std::to_string(ny) + "x" + std::to_string(nz);
            } else {
                status_message_ = "Structure has no unit cell.";
            }
        }
    } else if (cmd == "unwrap") {
        if (!is_unwrapped_) toggle_unwrap();
    } else if (cmd == "wrap") {
        if (is_unwrapped_) toggle_unwrap();
    } else if (cmd == "style") {
        std::string mode;
        if (iss >> mode) {
            if (mode == "vdw" || mode == "space") {
                rasterizer_.settings().style = render::RenderStyle::SpaceFilling;
            } else if (mode == "licorice" || mode == "stick") {
                rasterizer_.settings().style = render::RenderStyle::Licorice;
            } else if (mode == "wire" || mode == "wireframe") {
                rasterizer_.settings().style = render::RenderStyle::Wireframe;
            } else {
                rasterizer_.settings().style = render::RenderStyle::BallAndStick;
            }
            status_message_ = "Style updated to " + mode;
        }
    } else if (cmd == "hkl") {
        int h = 0, k = 0, l = 0;
        if (iss >> h >> k >> l) {
            rasterizer_.settings().miller_plane = glm::ivec3(h, k, l);
            status_message_ = "Miller plane (" + std::to_string(h) + " " +
                              std::to_string(k) + " " + std::to_string(l) + ") displayed.";
        }
    } else if (cmd == "reset" || cmd == "r") {
        setup_camera();
        status_message_ = "Camera view reset.";
    } else if (cmd == "clear" || cmd == "c") {
        for (size_t id : selected_atom_ids_) {
            if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
        }
        selected_atom_ids_.clear();
        rasterizer_.settings().miller_plane = std::nullopt;
        status_message_ = "Selections and planes cleared.";
    } else {
        status_message_ = "Unknown command: " + cmd;
    }
}

void App::run() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto viewport = std::make_shared<MolViewport>(
        structure_,
        camera_,
        rasterizer_,
        [this](int atom_id) { on_atom_picked(atom_id); }
    );

    auto component = ftxui::CatchEvent(viewport, [&](ftxui::Event event) {
        if (command_mode_) {
            if (event == ftxui::Event::Escape) {
                command_mode_ = false;
                command_buffer_.clear();
                return true;
            }
            if (event == ftxui::Event::Return) {
                command_mode_ = false;
                execute_command(command_buffer_);
                command_buffer_.clear();
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
            return true;
        }
        if (event == ftxui::Event::Character('r')) {
            setup_camera();
            status_message_ = "Camera reset.";
            return true;
        }
        if (event == ftxui::Event::Character('u')) {
            toggle_unwrap();
            return true;
        }
        if (event == ftxui::Event::Character('s')) {
            cycle_style();
            return true;
        }
        if (event == ftxui::Event::Character('c')) {
            rasterizer_.settings().show_unit_cell = !rasterizer_.settings().show_unit_cell;
            status_message_ = rasterizer_.settings().show_unit_cell ? "Unit cell ON" : "Unit cell OFF";
            return true;
        }
        if (event == ftxui::Event::Character('b')) {
            rasterizer_.settings().show_bonds = !rasterizer_.settings().show_bonds;
            status_message_ = rasterizer_.settings().show_bonds ? "Bonds ON" : "Bonds OFF";
            return true;
        }
        if (event == ftxui::Event::Character('m')) {
            for (size_t id : selected_atom_ids_) {
                if (id < structure_.atoms.size()) structure_.atoms[id].selected = false;
            }
            selected_atom_ids_.clear();
            status_message_ = "Selections cleared.";
            return true;
        }
        if (event == ftxui::Event::Character('h') || event == ftxui::Event::Character('?')) {
            show_help_ = !show_help_;
            return true;
        }
        if (event == ftxui::Event::Character(']')) {
            if (structure_.num_frames() > 1) {
                structure_.set_frame((structure_.current_frame + 1) % structure_.num_frames());
                status_message_ = "Frame " + std::to_string(structure_.current_frame + 1) + "/" +
                                  std::to_string(structure_.num_frames());
            }
            return true;
        }
        if (event == ftxui::Event::Character('[')) {
            if (structure_.num_frames() > 1) {
                size_t prev = (structure_.current_frame == 0) ? (structure_.num_frames() - 1) : (structure_.current_frame - 1);
                structure_.set_frame(prev);
                status_message_ = "Frame " + std::to_string(structure_.current_frame + 1) + "/" +
                                  std::to_string(structure_.num_frames());
            }
            return true;
        }
        if (event == ftxui::Event::Character(' ')) {
            if (structure_.num_frames() > 1) {
                is_playing_ = !is_playing_;
                status_message_ = is_playing_ ? "Trajectory playing..." : "Trajectory paused.";
            }
            return true;
        }

        // Camera keyboard controls
        if (event == ftxui::Event::Character('h')) { camera_.rotate(-2.0f, 0.0f); return true; }
        if (event == ftxui::Event::Character('l')) { camera_.rotate(2.0f, 0.0f); return true; }
        if (event == ftxui::Event::Character('j')) { camera_.rotate(0.0f, 2.0f); return true; }
        if (event == ftxui::Event::Character('k')) { camera_.rotate(0.0f, -2.0f); return true; }
        if (event == ftxui::Event::Character('+') || event == ftxui::Event::Character('=')) {
            camera_.zoom(0.9f);
            return true;
        }
        if (event == ftxui::Event::Character('-') || event == ftxui::Event::Character('_')) {
            camera_.zoom(1.1f);
            return true;
        }

        return false;
    });

    auto renderer = ftxui::Renderer(component, [&] {
        using namespace ftxui;

        // Title Bar
        auto title_bar = hbox({
            text(" MolTUI ") | bold | bgcolor(Color::RGB(41, 98, 255)) | color(Color::White),
            separator(),
            text(" " + format_structure_info() + " ") | color(Color::RGB(180, 200, 220)),
            filler(),
            text(is_unwrapped_ ? " [UNWRAPPED PBC] " : " [UNIT CELL] ") | bold |
                color(is_unwrapped_ ? Color::RGB(80, 230, 120) : Color::RGB(240, 200, 60))
        }) | bgcolor(Color::RGB(20, 24, 34));

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
                text(" [u]nwrap [s]tyle [c]ell [m]easure [r]eset [h]elp [:]cmd [q]uit ") |
                    color(Color::RGB(120, 140, 160))
            }) | bgcolor(Color::RGB(24, 28, 40));
        }

        auto layout = vbox({
            title_bar,
            viewport->Render() | flex,
            bottom_bar
        });

        if (show_help_) {
            auto help_box = window(
                text(" MolTUI Quick Reference ") | bold | color(Color::White),
                vbox({
                    text("Mouse:"),
                    text("  Left-Click + Drag   : Trackball 3D rotation") | color(Color::RGB(180, 220, 255)),
                    text("  Right-Click / Shift : Screen pan") | color(Color::RGB(180, 220, 255)),
                    text("  Scroll Wheel        : Zoom in / out") | color(Color::RGB(180, 220, 255)),
                    text("  Left-Click on Atom  : Pick atom for measurements") | color(Color::RGB(180, 220, 255)),
                    separator(),
                    text("Keybindings:"),
                    text("  u                   : Toggle periodic boundary unwrapping (Make Whole)") | color(Color::RGB(120, 240, 140)),
                    text("  s                   : Cycle representation (Ball&Stick, VDW, Licorice, Wire)") | color(Color::RGB(240, 220, 120)),
                    text("  c / b               : Toggle unit cell wireframe / bonds") | color(Color::RGB(200, 200, 200)),
                    text("  m                   : Clear measurements") | color(Color::RGB(200, 200, 200)),
                    text("  r                   : Reset camera view") | color(Color::RGB(200, 200, 200)),
                    text("  Space / [ / ]       : Trajectory play/pause, prev/next frame") | color(Color::RGB(200, 200, 200)),
                    text("  :                   : Enter command mode (:cell 2 2 2, :hkl 1 1 1, :load, :q)") | color(Color::RGB(255, 180, 100)),
                    text("  h / ?               : Toggle this help") | color(Color::RGB(200, 200, 200)),
                    text("  q / Esc             : Quit") | color(Color::RGB(255, 100, 100)),
                })
            ) | bgcolor(Color::RGB(16, 20, 30)) | border | center;

            return dbox({layout, help_box});
        }

        return layout;
    });

    screen.Loop(renderer);
}

} // namespace moltui::ui
