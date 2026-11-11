#include "moltui/render/protocol.hpp"
#include <cstdlib>
#include <sstream>
#include <vector>

namespace moltui::render {

TerminalGraphicsMode ProtocolRenderer::detect_best_protocol() {
    const char* kitty_window = std::getenv("KITTY_WINDOW_ID");
    const char* term = std::getenv("TERM");
    const char* term_prog = std::getenv("TERM_PROGRAM");

    if (kitty_window != nullptr) {
        return TerminalGraphicsMode::KittyGraphics;
    }
    if (term_prog != nullptr) {
        std::string prog(term_prog);
        if (prog == "ghostty" || prog == "WezTerm" || prog == "kitty") {
            return TerminalGraphicsMode::KittyGraphics;
        }
    }
    if (term != nullptr) {
        std::string t(term);
        if (t.find("kitty") != std::string::npos) {
            return TerminalGraphicsMode::KittyGraphics;
        }
    }

    // Default universal robust TrueColor ANSI half-blocks
    return TerminalGraphicsMode::HalfBlockAnsi;
}

std::string ProtocolRenderer::render_half_blocks(const Framebuffer& fb) {
    int w = fb.width();
    int h = fb.height();
    std::string out;
    out.reserve(w * (h / 2) * 20);

    glm::u8vec3 last_fg{255, 255, 255};
    glm::u8vec3 last_bg{255, 255, 255};
    bool first = true;

    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; ++x) {
            glm::u8vec3 top = fb.get_color(x, y);
            glm::u8vec3 bot = (y + 1 < h) ? fb.get_color(x, y + 1) : glm::u8vec3(0, 0, 0);

            if (first || top != last_fg) {
                out += "\x1b[38;2;" + std::to_string(top.r) + ";" +
                                     std::to_string(top.g) + ";" +
                                     std::to_string(top.b) + "m";
                last_fg = top;
            }

            if (first || bot != last_bg) {
                out += "\x1b[48;2;" + std::to_string(bot.r) + ";" +
                                     std::to_string(bot.g) + ";" +
                                     std::to_string(bot.b) + "m";
                last_bg = bot;
            }

            out += "\u2580"; // ▀ Upper half block
            first = false;
        }
        out += "\x1b[0m\n";
        first = true;
    }

    return out;
}

std::string ProtocolRenderer::render_sixel(const Framebuffer& fb) {
    // Fallback to half-block ANSI
    return render_half_blocks(fb);
}

std::string ProtocolRenderer::render_kitty(const Framebuffer& fb) {
    // Fallback to half-block ANSI
    return render_half_blocks(fb);
}

} // namespace moltui::render
