#include "moltui/ui/viewport.hpp"
#include "moltui/core/elements.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/event.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sys/ioctl.h>
#include <unistd.h>

namespace moltui::ui {

namespace {

std::pair<int, int> query_kitty_pixel_size(int cols, int rows, bool draft) {
    struct winsize ws{};
    double cell_w = 10.0;
    double cell_h = 20.0;

    bool got_size = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
                     ws.ws_col > 0 && ws.ws_row > 0 &&
                     ws.ws_xpixel > 0 && ws.ws_ypixel > 0);
    if (!got_size) {
        got_size = (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0 &&
                    ws.ws_col > 0 && ws.ws_row > 0 &&
                    ws.ws_xpixel > 0 && ws.ws_ypixel > 0);
    }

    if (got_size) {
        cell_w = static_cast<double>(ws.ws_xpixel) / ws.ws_col;
        cell_h = static_cast<double>(ws.ws_ypixel) / ws.ws_row;
    }

    int pixel_w = std::max(1, static_cast<int>(std::round(cols * cell_w)));
    int pixel_h = std::max(1, static_cast<int>(std::round(rows * cell_h)));

    if (draft) {
        pixel_w = std::max(1, static_cast<int>(std::round(pixel_w * 0.5)));
        pixel_h = std::max(1, static_cast<int>(std::round(pixel_h * 0.5)));
    }

    const int max_render_dim = draft ? 540 : 1920;
    int max_dim = std::max(pixel_w, pixel_h);
    if (max_dim > max_render_dim) {
        double scale = static_cast<double>(max_render_dim) / max_dim;
        pixel_w = std::max(1, static_cast<int>(std::round(pixel_w * scale)));
        pixel_h = std::max(1, static_cast<int>(std::round(pixel_h * scale)));
    }

    return {pixel_w, pixel_h};
}

void fit_camera_to_structure(
    const core::Structure& structure,
    render::Camera& camera,
    const render::RasterizerSettings& settings
) {
    std::vector<glm::vec3> pts;
    std::vector<float> radii;
    pts.reserve(structure.atoms.size() + 8);
    radii.reserve(structure.atoms.size() + 8);

    for (const auto& atom : structure.atoms) {
        if (!atom.visible) continue;
        float r = 0.25f;
        if (settings.style == render::RenderStyle::SpaceFilling) {
            r = core::ElementTable::get_vdw_radius(atom.element);
        } else if (settings.style == render::RenderStyle::BallAndStick) {
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
        camera.fit_extent(structure.center_of_mass(), pts, radii, false);
    }
}

class MolViewportNode : public ftxui::Node {
public:
    MolViewportNode(
        core::Structure& structure,
        render::Camera& camera,
        render::Rasterizer& rasterizer,
        render::Framebuffer& fb,
        int& render_width,
        int& render_height,
        render::TerminalGraphicsMode graphics_mode,
        bool& scene_dirty,
        bool& needs_initial_fit,
        bool use_draft,
        bool& viewport_drawn,
        bool& kitty_image_dirty,
        MolViewport::FrameRenderHook& on_frame_rendered
    ) : structure_(structure),
        camera_(camera),
        rasterizer_(rasterizer),
        fb_(fb),
        render_width_(render_width),
        render_height_(render_height),
        graphics_mode_(graphics_mode),
        scene_dirty_(scene_dirty),
        needs_initial_fit_(needs_initial_fit),
        use_draft_(use_draft),
        viewport_drawn_(viewport_drawn),
        kitty_image_dirty_(kitty_image_dirty),
        on_frame_rendered_(on_frame_rendered) {}

    void ComputeRequirement() override {
        requirement_.min_x = 20;
        requirement_.min_y = 10;
        requirement_.flex_grow_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_shrink_y = 1;
    }

    void Render(ftxui::Screen& screen) override {
        int cols = std::max(1, box_.x_max - box_.x_min + 1);
        int rows = std::max(1, box_.y_max - box_.y_min + 1);

        int pixel_w = cols;
        int pixel_h = rows * 2;
        if (graphics_mode_ == render::TerminalGraphicsMode::KittyGraphics) {
            auto [kw, kh] = query_kitty_pixel_size(cols, rows, use_draft_);
            pixel_w = kw;
            pixel_h = kh;
        }

        render_width_ = pixel_w;
        render_height_ = pixel_h;

        if (fb_.width() != pixel_w || fb_.height() != pixel_h) {
            fb_.resize(pixel_w, pixel_h);
            scene_dirty_ = true;
        }

        camera_.set_viewport(pixel_w, pixel_h);
        if (needs_initial_fit_) {
            fit_camera_to_structure(structure_, camera_, rasterizer_.settings());
            needs_initial_fit_ = false;
            scene_dirty_ = true;
        }

        if (scene_dirty_) {
            rasterizer_.render(structure_, camera_, fb_);
            scene_dirty_ = false;
            kitty_image_dirty_ = true;
        }

        if (graphics_mode_ == render::TerminalGraphicsMode::KittyGraphics) {
            for (int y = box_.y_min; y <= box_.y_max; ++y) {
                for (int x = box_.x_min; x <= box_.x_max; ++x) {
                    auto& cell = screen.PixelAt(x, y);
                    cell.character = " ";
                    cell.foreground_color = ftxui::Color::Default;
                    cell.background_color = ftxui::Color::Default;
                }
            }
            viewport_drawn_ = true;
            if (on_frame_rendered_) {
                on_frame_rendered_();
            }
            return;
        }

        if (graphics_mode_ == render::TerminalGraphicsMode::Ascii) {
            for (int y = box_.y_min; y <= box_.y_max; ++y) {
                int py_top = (y - box_.y_min) * 2;
                for (int x = box_.x_min; x <= box_.x_max; ++x) {
                    int px = x - box_.x_min;
                    char ch = render::ProtocolRenderer::sample_ascii_char(fb_, px, py_top);
                    auto& cell = screen.PixelAt(x, y);
                    cell.character = std::string(1, ch);
                    cell.foreground_color = ftxui::Color::Default;
                    cell.background_color = ftxui::Color::Default;
                }
            }
            return;
        }

        for (int y = box_.y_min; y <= box_.y_max; ++y) {
            int py_top = (y - box_.y_min) * 2;
            int py_bot = py_top + 1;

            for (int x = box_.x_min; x <= box_.x_max; ++x) {
                int px = x - box_.x_min;
                const auto& top = fb_.get_color(px, py_top);
                const auto& bot = (py_bot < pixel_h) ? fb_.get_color(px, py_bot) : glm::u8vec3(0);

                auto& cell = screen.PixelAt(x, y);
                cell.character = "▀";
                cell.foreground_color = ftxui::Color::RGB(top.r, top.g, top.b);
                cell.background_color = ftxui::Color::RGB(bot.r, bot.g, bot.b);
            }
        }
    }

private:
    core::Structure& structure_;
    render::Camera& camera_;
    render::Rasterizer& rasterizer_;
    render::Framebuffer& fb_;
    int& render_width_;
    int& render_height_;
    render::TerminalGraphicsMode graphics_mode_;
    bool& scene_dirty_;
    bool& needs_initial_fit_;
    bool use_draft_;
    bool& viewport_drawn_;
    bool& kitty_image_dirty_;
    MolViewport::FrameRenderHook& on_frame_rendered_;
};

} // namespace

MolViewport::MolViewport(
    core::Structure& structure,
    render::Camera& camera,
    render::Rasterizer& rasterizer,
    AtomPickCallback on_pick,
    render::TerminalGraphicsMode graphics_mode,
    FrameRenderHook on_frame_rendered,
    InteractionHook on_interaction
) : structure_(structure),
    camera_(camera),
    rasterizer_(rasterizer),
    on_pick_(std::move(on_pick)),
    graphics_mode_(graphics_mode),
    on_frame_rendered_(std::move(on_frame_rendered)),
    on_interaction_(std::move(on_interaction)),
    fb_(80, 40) {}

ftxui::Element MolViewport::OnRender() {
    return std::make_shared<MolViewportNode>(
        structure_,
        camera_,
        rasterizer_,
        fb_,
        render_width_,
        render_height_,
        graphics_mode_,
        scene_dirty_,
        needs_initial_fit_,
        adaptive_quality_ && draft_mode_,
        viewport_drawn_,
        kitty_image_dirty_,
        on_frame_rendered_
    ) | ftxui::reflect(box_);
}

void MolViewport::set_adaptive_quality(bool enabled) {
    if (adaptive_quality_ == enabled) return;
    adaptive_quality_ = enabled;
    if (!adaptive_quality_ && draft_mode_) {
        draft_mode_ = false;
    }
    scene_dirty_ = true;
}

void MolViewport::set_draft_mode(bool draft) {
    bool effective = adaptive_quality_ && draft;
    if (draft_mode_ == effective) return;
    draft_mode_ = effective;
    scene_dirty_ = true;
}

bool MolViewport::consume_viewport_drawn() {
    if (!viewport_drawn_) {
        return false;
    }
    viewport_drawn_ = false;
    return true;
}

bool MolViewport::consume_kitty_image_dirty() {
    if (!kitty_image_dirty_) {
        return false;
    }
    kitty_image_dirty_ = false;
    return true;
}

void MolViewport::clear_kitty_pending() {
    viewport_drawn_ = false;
}

std::string MolViewport::wrap_kitty_cursor(const std::string& payload) const {
    if (payload.empty() || box_.x_max < box_.x_min || box_.y_max < box_.y_min) {
        return "";
    }
    int ansi_row = box_.y_min + 1;
    int ansi_col = box_.x_min + 1;

    std::string out;
    out.reserve(payload.size() + 32);
    out += "\x1b[s\x1b[" + std::to_string(ansi_row) + ";" + std::to_string(ansi_col) + "H";
    out += payload;
    out += "\x1b[u";
    return out;
}

std::string MolViewport::build_kitty_frame(uint32_t image_id) const {
    if (box_.x_max < box_.x_min || box_.y_max < box_.y_min) {
        return "";
    }
    int cols = box_.x_max - box_.x_min + 1;
    int rows = box_.y_max - box_.y_min + 1;
    return wrap_kitty_cursor(render::ProtocolRenderer::render_kitty(fb_, image_id, cols, rows));
}

std::string MolViewport::build_kitty_place(uint32_t image_id) const {
    if (box_.x_max < box_.x_min || box_.y_max < box_.y_min) {
        return "";
    }
    int cols = box_.x_max - box_.x_min + 1;
    int rows = box_.y_max - box_.y_min + 1;
    return wrap_kitty_cursor(render::ProtocolRenderer::render_kitty_place(image_id, cols, rows));
}

bool MolViewport::OnEvent(ftxui::Event event) {
    if (event.is_mouse()) {
        const auto& mouse = event.mouse();
        
        if (mouse.button == ftxui::Mouse::Left) {
            if (mouse.motion == ftxui::Mouse::Pressed) {
                last_mouse_x_ = mouse.x;
                last_mouse_y_ = mouse.y;
                is_dragging_ = true;
                return true;
            } else if (mouse.motion == ftxui::Mouse::Moved && is_dragging_) {
                float dx = static_cast<float>(mouse.x - last_mouse_x_);
                float dy = static_cast<float>(mouse.y - last_mouse_y_);
                last_mouse_x_ = mouse.x;
                last_mouse_y_ = mouse.y;

                if (dx != 0.0f || dy != 0.0f) {
                    if (mouse.shift) {
                        camera_.pan(dx * 4.0f, -dy * 4.0f);
                    } else {
                        camera_.rotate(dx * 8.0f, dy * 8.0f);
                    }
                    scene_dirty_ = true;
                    if (on_interaction_) {
                        on_interaction_();
                    }
                }
                return true;
            } else if (mouse.motion == ftxui::Mouse::Released) {
                if (is_dragging_ && mouse.x == last_mouse_x_ && mouse.y == last_mouse_y_) {
                    // Click without drag: pick atom
                    if (box_.x_min <= mouse.x && mouse.x <= box_.x_max &&
                        box_.y_min <= mouse.y && mouse.y <= box_.y_max) {
                        int atom_id = -1;
                        if (graphics_mode_ == render::TerminalGraphicsMode::KittyGraphics) {
                            int cols = std::max(1, box_.x_max - box_.x_min + 1);
                            int rows = std::max(1, box_.y_max - box_.y_min + 1);
                            int cx = mouse.x - box_.x_min;
                            int cy = mouse.y - box_.y_min;
                            int x0 = (cx * fb_.width()) / cols;
                            int x1 = std::max(x0 + 1, ((cx + 1) * fb_.width()) / cols);
                            int y0 = (cy * fb_.height()) / rows;
                            int y1 = std::max(y0 + 1, ((cy + 1) * fb_.height()) / rows);
                            int mid_x = (x0 + x1) / 2;
                            int mid_y = (y0 + y1) / 2;
                            atom_id = fb_.get_atom_id(mid_x, mid_y);
                            if (atom_id < 0) {
                                int best_dist = std::numeric_limits<int>::max();
                                for (int py = y0; py < y1; ++py) {
                                    for (int px = x0; px < x1; ++px) {
                                        int id = fb_.get_atom_id(px, py);
                                        if (id >= 0) {
                                            int dx = px - mid_x;
                                            int dy = py - mid_y;
                                            int dist = dx * dx + dy * dy;
                                            if (dist < best_dist) {
                                                best_dist = dist;
                                                atom_id = id;
                                            }
                                        }
                                    }
                                }
                            }
                        } else {
                            int px = mouse.x - box_.x_min;
                            int py = (mouse.y - box_.y_min) * 2;
                            atom_id = fb_.get_atom_id(px, py);
                            if (atom_id < 0 && py + 1 < fb_.height()) {
                                atom_id = fb_.get_atom_id(px, py + 1);
                            }
                        }
                        if (on_pick_) {
                            on_pick_(atom_id);
                            scene_dirty_ = true;
                        }
                    }
                }
                is_dragging_ = false;
                if (draft_mode_) {
                    set_draft_mode(false);
                }
                return true;
            }
        } else if (mouse.button == ftxui::Mouse::Right) {
            if (mouse.motion == ftxui::Mouse::Pressed) {
                last_mouse_x_ = mouse.x;
                last_mouse_y_ = mouse.y;
                is_panning_ = true;
                return true;
            } else if (mouse.motion == ftxui::Mouse::Moved && is_panning_) {
                float dx = static_cast<float>(mouse.x - last_mouse_x_);
                float dy = static_cast<float>(mouse.y - last_mouse_y_);
                last_mouse_x_ = mouse.x;
                last_mouse_y_ = mouse.y;
                if (dx != 0.0f || dy != 0.0f) {
                    camera_.pan(dx * 4.0f, -dy * 4.0f);
                    scene_dirty_ = true;
                    if (on_interaction_) {
                        on_interaction_();
                    }
                }
                return true;
            } else if (mouse.motion == ftxui::Mouse::Released) {
                is_panning_ = false;
                if (draft_mode_) {
                    set_draft_mode(false);
                }
                return true;
            }
        } else if (mouse.button == ftxui::Mouse::WheelUp) {
            camera_.zoom(0.92f);
            scene_dirty_ = true;
            if (on_interaction_) {
                on_interaction_();
            }
            return true;
        } else if (mouse.button == ftxui::Mouse::WheelDown) {
            camera_.zoom(1.08f);
            scene_dirty_ = true;
            if (on_interaction_) {
                on_interaction_();
            }
            return true;
        }
    }

    return false;
}

void MolViewport::trigger_rerender() {
    // Handled in main loop
}

} // namespace moltui::ui
