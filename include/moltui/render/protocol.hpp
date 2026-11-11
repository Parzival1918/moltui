#pragma once

#include "moltui/render/framebuffer.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace moltui::render {

enum class TerminalGraphicsMode {
    HalfBlockAnsi,
    KittyGraphics,
    Ascii,
    Sixel
};

class ProtocolRenderer {
public:
    static TerminalGraphicsMode detect_best_protocol();
    static std::optional<TerminalGraphicsMode> parse_mode(std::string_view name);
    static const char* mode_name(TerminalGraphicsMode mode);

    // Renders the framebuffer into ANSI TrueColor half-blocks (▀)
    static std::string render_half_blocks(const Framebuffer& fb);

    // Renders the framebuffer into plain ASCII art
    static std::string render_ascii(const Framebuffer& fb);

    // Maps a 1x2 pixel pair (x, y_top) and (x, y_top+1) to an ASCII luminance character
    static char sample_ascii_char(const Framebuffer& fb, int x, int y_top);

    // Encodes framebuffer into Sixel escape sequence
    static std::string render_sixel(const Framebuffer& fb);

    // Encodes framebuffer into Kitty graphics protocol escape sequence.
    // When image_id > 0, emits an in-place placement (C=1, q=2) scaled to cols x rows cells.
    static std::string render_kitty(
        const Framebuffer& fb,
        uint32_t image_id = 0,
        int cols = 0,
        int rows = 0
    );

    // Emits a lightweight Kitty command to re-place an already-transmitted image (a=p).
    static std::string render_kitty_place(uint32_t image_id, int cols = 0, int rows = 0);

    // Emits a Kitty graphics command to delete a transmitted image and its placements.
    static std::string delete_kitty_image(uint32_t image_id);
};

} // namespace moltui::render
