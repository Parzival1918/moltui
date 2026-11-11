#pragma once

#include "moltui/render/framebuffer.hpp"
#include <string>

namespace moltui::render {

enum class TerminalGraphicsMode {
    HalfBlockAnsi,
    KittyGraphics,
    Sixel
};

class ProtocolRenderer {
public:
    static TerminalGraphicsMode detect_best_protocol();

    // Renders the framebuffer into ANSI TrueColor half-blocks (▀)
    static std::string render_half_blocks(const Framebuffer& fb);

    // Encodes framebuffer into Sixel escape sequence
    static std::string render_sixel(const Framebuffer& fb);

    // Encodes framebuffer into Kitty graphics protocol escape sequence
    static std::string render_kitty(const Framebuffer& fb);
};

} // namespace moltui::render
