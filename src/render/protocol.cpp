#include "moltui/render/protocol.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace moltui::render {

namespace {

constexpr std::string_view kBase64Alphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::string_view kAsciiRamp = ".:-=+*#%@";

std::string base64_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t octet_a = data[i];
        uint32_t octet_b = (i + 1 < len) ? data[i + 1] : 0;
        uint32_t octet_c = (i + 2 < len) ? data[i + 2] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out.push_back(kBase64Alphabet[(triple >> 18) & 0x3F]);
        out.push_back(kBase64Alphabet[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kBase64Alphabet[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kBase64Alphabet[triple & 0x3F] : '=');
    }

    return out;
}

std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

} // namespace

TerminalGraphicsMode ProtocolRenderer::detect_best_protocol() {
    const char* kitty_window = std::getenv("KITTY_WINDOW_ID");
    const char* term = std::getenv("TERM");
    const char* term_prog = std::getenv("TERM_PROGRAM");

    if (kitty_window != nullptr && kitty_window[0] != '\0') {
        return TerminalGraphicsMode::KittyGraphics;
    }
    if (term_prog != nullptr) {
        std::string prog = to_lower_ascii(term_prog);
        if (prog == "ghostty" || prog == "wezterm" || prog == "kitty") {
            return TerminalGraphicsMode::KittyGraphics;
        }
    }
    if (term != nullptr) {
        std::string t = to_lower_ascii(term);
        if (t.find("kitty") != std::string::npos || t.find("ghostty") != std::string::npos) {
            return TerminalGraphicsMode::KittyGraphics;
        }
    }

    // Default universal robust TrueColor ANSI half-blocks
    return TerminalGraphicsMode::HalfBlockAnsi;
}

std::optional<TerminalGraphicsMode> ProtocolRenderer::parse_mode(std::string_view name) {
    std::string key = to_lower_ascii(name);
    if (key == "auto") {
        return detect_best_protocol();
    }
    if (key == "kitty") {
        return TerminalGraphicsMode::KittyGraphics;
    }
    if (key == "half-blocks" || key == "half-block" || key == "halfblocks" ||
        key == "halfblock" || key == "half" || key == "color" || key == "ansi") {
        return TerminalGraphicsMode::HalfBlockAnsi;
    }
    if (key == "ascii") {
        return TerminalGraphicsMode::Ascii;
    }
    return std::nullopt;
}

const char* ProtocolRenderer::mode_name(TerminalGraphicsMode mode) {
    switch (mode) {
        case TerminalGraphicsMode::KittyGraphics: return "KITTY";
        case TerminalGraphicsMode::Ascii:         return "ASCII";
        case TerminalGraphicsMode::Sixel:         return "SIXEL";
        case TerminalGraphicsMode::HalfBlockAnsi:
        default:                                  return "HALF-BLOCK";
    }
}

char ProtocolRenderer::sample_ascii_char(const Framebuffer& fb, int x, int y_top) {
    if (x < 0 || x >= fb.width() || y_top < 0 || y_top >= fb.height()) {
        return ' ';
    }

    bool has_bot = (y_top + 1 < fb.height());
    bool top_hit = fb.get_depth(x, y_top) < 1e8f;
    bool bot_hit = has_bot && (fb.get_depth(x, y_top + 1) < 1e8f);

    if (!top_hit && !bot_hit) {
        return ' ';
    }

    auto luma = [](const glm::u8vec3& c) -> float {
        return (0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b) / 255.0f;
    };

    float avg_luma = 0.0f;
    if (top_hit && bot_hit) {
        avg_luma = 0.5f * (luma(fb.get_color(x, y_top)) + luma(fb.get_color(x, y_top + 1)));
    } else if (top_hit) {
        avg_luma = luma(fb.get_color(x, y_top));
    } else {
        avg_luma = luma(fb.get_color(x, y_top + 1));
    }

    avg_luma = std::clamp(avg_luma, 0.0f, 1.0f);
    size_t idx = static_cast<size_t>(std::round(avg_luma * (kAsciiRamp.size() - 1)));
    idx = std::min(idx, kAsciiRamp.size() - 1);
    return kAsciiRamp[idx];
}

std::string ProtocolRenderer::render_ascii(const Framebuffer& fb) {
    int w = fb.width();
    int h = fb.height();
    std::string out;
    out.reserve(static_cast<size_t>((w + 1) * ((h + 1) / 2)));

    for (int y = 0; y < h; y += 2) {
        for (int x = 0; x < w; ++x) {
            out.push_back(sample_ascii_char(fb, x, y));
        }
        out.push_back('\n');
    }

    return out;
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

std::string ProtocolRenderer::render_kitty(
    const Framebuffer& fb,
    uint32_t image_id,
    int cols,
    int rows
) {
    int w = fb.width();
    int h = fb.height();
    if (w <= 0 || h <= 0) {
        return "";
    }

    static_assert(sizeof(glm::u8vec3) == 3, "glm::u8vec3 must be tightly packed RGB");
    const auto& colors = fb.colors();
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(colors.data());
    std::string b64 = base64_encode(raw, colors.size() * 3);

    constexpr size_t kMaxChunkSize = 4096;
    std::string out;
    out.reserve(b64.size() + (b64.size() / kMaxChunkSize + 1) * 64 + 16);

    for (size_t offset = 0; offset < b64.size(); offset += kMaxChunkSize) {
        size_t chunk_len = std::min(kMaxChunkSize, b64.size() - offset);
        bool is_first = (offset == 0);
        bool has_more = (offset + chunk_len) < b64.size();

        out += "\x1b_G";
        if (is_first) {
            out += "a=T,f=24,s=" + std::to_string(w) + ",v=" + std::to_string(h) + ",q=2";
            if (image_id > 0) {
                out += ",i=" + std::to_string(image_id) + ",p=1,C=1";
            }
            if (cols > 0 && rows > 0) {
                out += ",c=" + std::to_string(cols) + ",r=" + std::to_string(rows);
            }
            out += has_more ? ",m=1;" : ",m=0;";
        } else {
            out += has_more ? "m=1;" : "m=0;";
        }
        out.append(b64, offset, chunk_len);
        out += "\x1b\\";
    }

    if (image_id == 0) {
        out.push_back('\n');
    }

    return out;
}

std::string ProtocolRenderer::render_kitty_place(uint32_t image_id, int cols, int rows) {
    std::string out = "\x1b_Ga=p,i=" + std::to_string(image_id) + ",p=1,C=1,q=2";
    if (cols > 0 && rows > 0) {
        out += ",c=" + std::to_string(cols) + ",r=" + std::to_string(rows);
    }
    out += "\x1b\\";
    return out;
}

std::string ProtocolRenderer::delete_kitty_image(uint32_t image_id) {
    return "\x1b_Ga=d,d=I,i=" + std::to_string(image_id) + ",q=2\x1b\\";
}

} // namespace moltui::render
