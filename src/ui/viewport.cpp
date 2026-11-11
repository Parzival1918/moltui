#include "moltui/ui/viewport.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/mouse.hpp>
#include <ftxui/component/event.hpp>
#include <algorithm>

namespace moltui::ui {

namespace {

class MolViewportNode : public ftxui::Node {
public:
    MolViewportNode(
        core::Structure& structure,
        render::Camera& camera,
        render::Rasterizer& rasterizer,
        render::Framebuffer& fb,
        int& render_width,
        int& render_height
    ) : structure_(structure),
        camera_(camera),
        rasterizer_(rasterizer),
        fb_(fb),
        render_width_(render_width),
        render_height_(render_height) {}

    void ComputeRequirement() override {
        requirement_.min_x = 20;
        requirement_.min_y = 10;
        requirement_.flex_grow_x = 1;
        requirement_.flex_grow_y = 1;
        requirement_.flex_shrink_x = 1;
        requirement_.flex_shrink_y = 1;
    }

    void Render(ftxui::Screen& screen) override {
        int w = std::max(1, box_.x_max - box_.x_min + 1);
        int h = std::max(1, box_.y_max - box_.y_min + 1);

        // 2 vertical pixels per terminal character cell using half-blocks
        int pixel_w = w;
        int pixel_h = h * 2;

        render_width_ = pixel_w;
        render_height_ = pixel_h;

        if (fb_.width() != pixel_w || fb_.height() != pixel_h) {
            fb_.resize(pixel_w, pixel_h);
        }
        camera_.set_viewport(pixel_w, pixel_h);

        rasterizer_.render(structure_, camera_, fb_);

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
};

} // namespace

MolViewport::MolViewport(
    core::Structure& structure,
    render::Camera& camera,
    render::Rasterizer& rasterizer,
    AtomPickCallback on_pick
) : structure_(structure),
    camera_(camera),
    rasterizer_(rasterizer),
    on_pick_(std::move(on_pick)),
    fb_(80, 40) {}

ftxui::Element MolViewport::OnRender() {
    return std::make_shared<MolViewportNode>(
        structure_,
        camera_,
        rasterizer_,
        fb_,
        render_width_,
        render_height_
    ) | ftxui::reflect(box_);
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

                if (mouse.shift) {
                    camera_.pan(dx * 4.0f, -dy * 4.0f);
                } else {
                    camera_.rotate(dx * 8.0f, dy * 8.0f);
                }
                return true;
            } else if (mouse.motion == ftxui::Mouse::Released) {
                if (is_dragging_ && mouse.x == last_mouse_x_ && mouse.y == last_mouse_y_) {
                    // Click without drag: pick atom
                    if (box_.x_min <= mouse.x && mouse.x <= box_.x_max &&
                        box_.y_min <= mouse.y && mouse.y <= box_.y_max) {
                        int px = mouse.x - box_.x_min;
                        int py = (mouse.y - box_.y_min) * 2;
                        int atom_id = fb_.get_atom_id(px, py);
                        if (atom_id < 0 && py + 1 < fb_.height()) {
                            atom_id = fb_.get_atom_id(px, py + 1);
                        }
                        if (on_pick_) {
                            on_pick_(atom_id);
                        }
                    }
                }
                is_dragging_ = false;
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
                camera_.pan(dx * 4.0f, -dy * 4.0f);
                return true;
            } else if (mouse.motion == ftxui::Mouse::Released) {
                is_panning_ = false;
                return true;
            }
        } else if (mouse.button == ftxui::Mouse::WheelUp) {
            camera_.zoom(0.92f);
            return true;
        } else if (mouse.button == ftxui::Mouse::WheelDown) {
            camera_.zoom(1.08f);
            return true;
        }
    }

    return false;
}

void MolViewport::trigger_rerender() {
    // Handled in main loop
}

} // namespace moltui::ui
