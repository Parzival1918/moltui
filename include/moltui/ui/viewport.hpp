#pragma once

#include "moltui/core/structure.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
#include "moltui/render/rasterizer.hpp"

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/screen/box.hpp>
#include <functional>
#include <vector>

namespace moltui::ui {

class MolViewport : public ftxui::ComponentBase {
public:
    using AtomPickCallback = std::function<void(int atom_id)>;

    MolViewport(
        core::Structure& structure,
        render::Camera& camera,
        render::Rasterizer& rasterizer,
        AtomPickCallback on_pick = nullptr
    );

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;

    void trigger_rerender();

private:
    core::Structure& structure_;
    render::Camera& camera_;
    render::Rasterizer& rasterizer_;
    AtomPickCallback on_pick_;

    render::Framebuffer fb_;
    ftxui::Box box_;
    int last_mouse_x_{0};
    int last_mouse_y_{0};
    bool is_dragging_{false};
    bool is_panning_{false};
    int render_width_{80};
    int render_height_{40};
};

} // namespace moltui::ui
