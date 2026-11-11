#pragma once

#include "moltui/core/structure.hpp"
#include "moltui/render/camera.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/render/rasterizer.hpp"
#include "moltui/ui/viewport.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <string>
#include <vector>

namespace moltui::ui {

class App {
public:
    explicit App(
        core::Structure structure,
        render::TerminalGraphicsMode graphics_mode = render::ProtocolRenderer::detect_best_protocol()
    );

    explicit App(
        std::vector<core::Structure> structures,
        render::TerminalGraphicsMode graphics_mode = render::ProtocolRenderer::detect_best_protocol()
    );

    void run();

    render::Rasterizer& rasterizer() { return rasterizer_; }
    void set_adaptive_quality(bool enabled) { adaptive_quality_ = enabled; }
    bool adaptive_quality() const { return adaptive_quality_; }

private:
    std::vector<core::Structure> ensemble_;
    size_t ensemble_index_{0};
    core::Structure structure_;
    core::Structure original_structure_; // Preserved for cell resets/unwrapping
    render::Camera camera_;
    render::Rasterizer rasterizer_;
    render::TerminalGraphicsMode graphics_mode_;

    std::vector<size_t> selected_atom_ids_;
    bool is_unwrapped_{false};
    bool show_help_{false};
    bool command_mode_{false};
    bool quit_requested_{false};
    bool refit_requested_{false};
    bool adaptive_quality_{true};
    std::string command_buffer_{""};
    std::string status_message_{"Ready. Type : for command mode, ? for help."};

    // Trajectory animation
    bool is_playing_{false};

    void setup_camera();
    void on_atom_picked(int atom_id);
    void execute_command(const std::string& cmd);
    void toggle_unwrap();
    void cycle_style();
    void toggle_shading();
    void step_ensemble(int delta);
    
    // Measurement calculation helpers
    std::string format_measurements() const;
    std::string format_structure_info() const;
};

} // namespace moltui::ui
