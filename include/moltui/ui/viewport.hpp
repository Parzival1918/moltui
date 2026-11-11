#pragma once

#include "moltui/core/structure.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/render/rasterizer.hpp"

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/box.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace moltui::ui {

class MolViewport : public ftxui::ComponentBase {
public:
    using AtomPickCallback = std::function<void(int atom_id)>;
    using FrameRenderHook = std::function<void()>;
    using InteractionHook = std::function<void()>;

    MolViewport(
        core::Structure& structure,
        render::Camera& camera,
        render::Rasterizer& rasterizer,
        AtomPickCallback on_pick = nullptr,
        render::TerminalGraphicsMode graphics_mode = render::TerminalGraphicsMode::HalfBlockAnsi,
        FrameRenderHook on_frame_rendered = nullptr,
        InteractionHook on_interaction = nullptr
    );

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;

    void trigger_rerender();

    render::TerminalGraphicsMode graphics_mode() const { return graphics_mode_; }
    void set_graphics_mode(render::TerminalGraphicsMode mode);
    void mark_scene_dirty() { scene_dirty_ = true; }
    void request_refit(bool reset_pan = true) {
        needs_initial_fit_ = true;
        refit_reset_pan_ = reset_pan;
        scene_dirty_ = true;
    }
    void set_adaptive_quality(bool enabled);
    bool adaptive_quality() const { return adaptive_quality_; }
    void set_draft_mode(bool draft);
    bool draft_mode() const { return draft_mode_; }
    bool consume_viewport_drawn();
    bool consume_kitty_image_dirty();
    void clear_kitty_pending();
    std::string build_kitty_frame(uint32_t image_id) const;
    std::string build_kitty_place(uint32_t image_id) const;

private:
    std::string wrap_kitty_cursor(const std::string& payload) const;

    core::Structure& structure_;
    render::Camera& camera_;
    render::Rasterizer& rasterizer_;
    AtomPickCallback on_pick_;
    render::TerminalGraphicsMode graphics_mode_;
    FrameRenderHook on_frame_rendered_;
    InteractionHook on_interaction_;

    render::Framebuffer fb_;
    ftxui::Box box_;
    int last_mouse_x_{0};
    int last_mouse_y_{0};
    bool is_dragging_{false};
    bool is_panning_{false};
    int render_width_{80};
    int render_height_{40};
    bool scene_dirty_{true};
    bool needs_initial_fit_{true};
    bool refit_reset_pan_{true};
    bool adaptive_quality_{true};
    bool draft_mode_{false};
    bool viewport_drawn_{false};
    bool kitty_image_dirty_{true};
};

} // namespace moltui::ui
