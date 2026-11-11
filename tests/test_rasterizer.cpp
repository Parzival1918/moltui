#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/render/rasterizer.hpp"
#include "moltui/core/structure.hpp"

#include <cassert>
#include <iostream>

using namespace moltui::render;
using namespace moltui::core;

void test_framebuffer() {
    std::cout << "[Test] Framebuffer... ";
    Framebuffer fb(100, 50);
    assert(fb.width() == 100);
    assert(fb.height() == 50);

    fb.clear({10, 20, 30});
    assert(fb.get_color(0, 0) == glm::u8vec3(10, 20, 30));
    assert(fb.get_atom_id(0, 0) == -1);

    // Depth test
    bool pass1 = fb.test_and_set(10, 10, 5.0f, {255, 0, 0}, 42);
    assert(pass1);
    assert(fb.get_atom_id(10, 10) == 42);
    assert(fb.get_color(10, 10) == glm::u8vec3(255, 0, 0));

    // Behind existing depth: should reject
    bool pass2 = fb.test_and_set(10, 10, 10.0f, {0, 255, 0}, 99);
    assert(!pass2);
    assert(fb.get_atom_id(10, 10) == 42);

    // In front of existing depth: should accept
    bool pass3 = fb.test_and_set(10, 10, 2.0f, {0, 0, 255}, 100);
    assert(pass3);
    assert(fb.get_atom_id(10, 10) == 100);

    std::cout << "PASSED\n";
}

void test_rasterizer_sphere() {
    std::cout << "[Test] Rasterizer Sphere Impostor... ";
    Structure st;
    Atom a;
    a.id = 7;
    a.element = "C";
    a.pos = {0.0f, 0.0f, 0.0f};
    st.atoms.push_back(a);

    Camera cam;
    cam.set_viewport(80, 40);
    cam.reset({0.0f, 0.0f, 0.0f}, 1.0f);

    Rasterizer rast;
    rast.settings().show_unit_cell = false;
    rast.settings().show_bonds = false;

    Framebuffer fb(80, 40);
    rast.render(st, cam, fb);

    // Center pixel (40, 20) must be hit by the sphere and tagged with atom id 7
    int hit_atom = fb.get_atom_id(40, 20);
    assert(hit_atom == 7);

    // Center pixel color should be non-background
    glm::u8vec3 col = fb.get_color(40, 20);
    assert(col != rast.settings().background_color);

    std::cout << "PASSED (Atom hit ID: " << hit_atom << ")\n";
}

void test_protocol_renderer() {
    std::cout << "[Test] ProtocolRenderer (Kitty, Half-Blocks, ASCII)... ";

    // Mode parsing
    assert(ProtocolRenderer::parse_mode("kitty") == TerminalGraphicsMode::KittyGraphics);
    assert(ProtocolRenderer::parse_mode("KITTY") == TerminalGraphicsMode::KittyGraphics);
    assert(ProtocolRenderer::parse_mode("half-blocks") == TerminalGraphicsMode::HalfBlockAnsi);
    assert(ProtocolRenderer::parse_mode("color") == TerminalGraphicsMode::HalfBlockAnsi);
    assert(ProtocolRenderer::parse_mode("ascii") == TerminalGraphicsMode::Ascii);
    assert(ProtocolRenderer::parse_mode("auto").has_value());
    assert(!ProtocolRenderer::parse_mode("unknown-mode").has_value());

    // Single-chunk Kitty encoding: 1x1 white pixel (255,255,255) -> base64 "////"
    Framebuffer tiny(1, 1);
    tiny.clear({255, 255, 255});
    std::string kitty_snap = ProtocolRenderer::render_kitty(tiny);
    assert(kitty_snap == "\x1b_Ga=T,f=24,s=1,v=1,q=2,m=0;////\x1b\\\n");

    std::string kitty_tui = ProtocolRenderer::render_kitty(tiny, 1, 20, 10);
    assert(kitty_tui == "\x1b_Ga=T,f=24,s=1,v=1,q=2,i=1,p=1,C=1,c=20,r=10,m=0;////\x1b\\");
    assert(ProtocolRenderer::render_kitty_place(1, 20, 10) ==
           "\x1b_Ga=p,i=1,p=1,C=1,q=2,c=20,r=10\x1b\\");
    assert(ProtocolRenderer::delete_kitty_image(1) == "\x1b_Ga=d,d=I,i=1,q=2\x1b\\");

    // Multi-chunk Kitty encoding: 64x64 -> 64*64*3 = 12288 raw bytes -> 16384 base64 bytes -> 4 chunks
    Framebuffer medium(64, 64);
    medium.clear({10, 20, 30});
    std::string kitty_multi = ProtocolRenderer::render_kitty(medium, 1, 40, 20);
    assert(kitty_multi.find("m=1;") != std::string::npos);
    assert(kitty_multi.find("\x1b_Gm=1;") != std::string::npos);
    assert(kitty_multi.find("\x1b_Gm=0;") != std::string::npos);

    // ASCII rendering: sphere in center -> non-space chars and no ANSI escapes
    Structure st;
    Atom a;
    a.id = 0;
    a.element = "H";
    a.pos = {0.0f, 0.0f, 0.0f};
    st.atoms.push_back(a);

    Camera cam;
    cam.set_viewport(40, 20);
    cam.reset({0.0f, 0.0f, 0.0f}, 1.0f);

    Rasterizer rast;
    rast.settings().show_unit_cell = false;
    rast.settings().show_bonds = false;

    Framebuffer fb(40, 20);
    rast.render(st, cam, fb);
    std::string ascii = ProtocolRenderer::render_ascii(fb);
    assert(ascii.find('\x1b') == std::string::npos);
    assert(ascii.find(' ') != std::string::npos);
    bool has_ink = false;
    for (char c : ascii) {
        if (c != ' ' && c != '\n') {
            has_ink = true;
            break;
        }
    }
    assert(has_ink);

    std::cout << "PASSED\n";
}

int main() {
    test_framebuffer();
    test_rasterizer_sphere();
    test_protocol_renderer();
    std::cout << "All rasterizer tests PASSED!\n";
    return 0;
}
