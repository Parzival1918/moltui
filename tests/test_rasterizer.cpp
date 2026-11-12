#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
#include "moltui/render/protocol.hpp"
#include "moltui/render/rasterizer.hpp"
#include "moltui/render/view_utils.hpp"
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

void test_rasterizer_cylinder_and_shading() {
    std::cout << "[Test] Rasterizer Cylinder & Flat/Lit Shading... ";
    Structure st;
    Atom a1, a2;
    a1.id = 0;
    a1.element = "C";
    a1.pos = {-1.0f, 0.0f, 0.0f};
    a2.id = 1;
    a2.element = "O";
    a2.pos = {1.0f, 0.0f, 0.0f};
    st.atoms = {a1, a2};
    st.bonds.push_back({0, 1, 1.0f, {0, 0, 0}});

    Camera cam;
    cam.set_viewport(80, 40);
    cam.reset({0.0f, 0.0f, 0.0f}, 1.5f);

    Rasterizer rast;
    rast.settings().show_unit_cell = false;
    rast.settings().style = RenderStyle::BallAndStick;

    Framebuffer fb_lit(80, 40);
    rast.render(st, cam, fb_lit);
    // Midpoint (40, 20) should be rendered by the 3D cylinder bond
    assert(fb_lit.get_depth(40, 20) < 1e8f);
    assert(fb_lit.get_color(40, 20) != rast.settings().background_color);

    // Left half of bond belongs to C (neutral grey), right half to O (reddish)
    glm::u8vec3 left_col = fb_lit.get_color(35, 20);
    glm::u8vec3 right_col = fb_lit.get_color(45, 20);
    assert(right_col.r > right_col.b + 30);
    assert(std::abs(static_cast<int>(left_col.r) - static_cast<int>(left_col.b)) < 15);

    // Test Flat shading + Perspective projection
    rast.settings().shading = RenderShading::Flat;
    cam.orthographic = false;
    Framebuffer fb_flat(80, 40);
    rast.render(st, cam, fb_flat);
    assert(fb_flat.get_depth(40, 20) < 1e8f);

    // Test COM-centered rotation after panning
    cam.orthographic = true;
    cam.set_viewport(100, 100);
    glm::vec3 com(2.0f, -1.0f, 0.5f);
    cam.reset(com, 3.0f);
    cam.pan(50.0f, -30.0f);
    glm::vec3 screen_before = cam.world_to_screen(com);
    cam.rotate(45.0f, 25.0f);
    glm::vec3 screen_after = cam.world_to_screen(com);
    assert(std::abs(screen_before.x - screen_after.x) < 1e-3f);
    assert(std::abs(screen_before.y - screen_after.y) < 1e-3f);

    // Test Camera::look_along puts requested view_dir pointing toward +z (viewer)
    cam.reset(glm::vec3(0.0f), 3.0f);
    cam.look_along(glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 view_x = glm::vec3(cam.view_matrix() * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f));
    assert(std::abs(view_x.x) < 1e-4f);
    assert(std::abs(view_x.y) < 1e-4f);
    assert(view_x.z > 0.999f);

    cam.look_along(glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    glm::vec3 view_neg_y = glm::vec3(cam.view_matrix() * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f));
    assert(std::abs(view_neg_y.x) < 1e-4f);
    assert(std::abs(view_neg_y.y) < 1e-4f);
    assert(view_neg_y.z > 0.999f);

    std::cout << "PASSED\n";
}

void test_supercell_unit_cell_rendering() {
    std::cout << "[Test] Supercell Subcell Edges & Origin Vectors... ";
    Structure base;
    UnitCell cell;
    cell.a = 5.0;
    cell.b = 5.0;
    cell.c = 5.0;
    cell.update_matrices();
    base.unit_cell = cell;
    base.atoms = {{0, "C", "C1", {2.5f, 2.5f, 2.5f}, {0.5f, 0.5f, 0.5f}, 1.0f, 0.0f, 0.0f, "", "", 0, false, false}};

    Structure super = base.generate_supercell(2, 2, 2);
    assert(super.unit_cell.has_value());
    assert(super.unit_cell->subcell_repeats == glm::ivec3(2, 2, 2));

    Camera cam;
    cam.orthographic = true;
    cam.set_viewport(200, 200);
    cam.reset({5.0f, 5.0f, 5.0f}, 10.0f);

    Rasterizer rast;
    rast.settings().show_bonds = false;
    rast.settings().show_unit_cell = true;
    for (auto& a : super.atoms) a.visible = false; // Isolate cell edges & origin vectors

    Framebuffer fb(200, 200);
    rast.render(super, cam, fb);

    // Midpoint of origin a-axis (2.5, 0, 0) should be reddish (red vector shaft)
    glm::vec3 s_red = cam.world_to_screen({2.5f, 0.0f, 0.0f});
    glm::u8vec3 col_red = fb.get_color(static_cast<int>(s_red.x), static_cast<int>(s_red.y));
    assert(col_red.r > col_red.b + 40);

    // Midpoint of second cell along a (7.5, 0, 0) should NOT be red arrow; should be neutral cell edge
    glm::vec3 s_edge = cam.world_to_screen({7.5f, 0.0f, 0.0f});
    glm::u8vec3 col_edge = fb.get_color(static_cast<int>(s_edge.x), static_cast<int>(s_edge.y));
    assert(col_edge != rast.settings().background_color);
    assert(col_edge.b >= col_edge.r);

    // Interior subcell divider at x=5, y=5 (5.0, 5.0, 10.0) should be drawn
    glm::vec3 s_mid = cam.world_to_screen({5.0f, 5.0f, 10.0f});
    assert(fb.get_color(static_cast<int>(s_mid.x), static_cast<int>(s_mid.y)) != rast.settings().background_color);

    std::cout << "PASSED\n";
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

void test_symmetry_color_and_asym() {
    std::cout << "[Test] Symmetry Color Modes & Asymmetric Unit Filter... ";
    assert(parse_color_mode("element") == ColorMode::Element);
    assert(parse_color_mode("cpk") == ColorMode::Element);
    assert(parse_color_mode("symop") == ColorMode::SymmetryOp);
    assert(parse_color_mode("asym") == ColorMode::AsymmetricSite);
    assert(parse_color_mode("molecule") == ColorMode::Molecule);
    assert(!parse_color_mode("unknown").has_value());

    Structure st;
    Atom a1, a2;
    a1.id = 0;
    a1.element = "C";
    a1.pos = {-1.5f, 0.0f, 0.0f};
    a1.symop_index = 0;
    a1.asym_site_index = 0;
    a1.molecule_id = 0;

    a2.id = 1;
    a2.element = "C";
    a2.pos = {1.5f, 0.0f, 0.0f};
    a2.symop_index = 1;
    a2.asym_site_index = 0;
    a2.molecule_id = 1;

    st.atoms = {a1, a2};

    Camera cam;
    cam.set_viewport(80, 40);
    cam.reset({0.0f, 0.0f, 0.0f}, 3.0f);

    Rasterizer rast;
    rast.settings().show_unit_cell = false;
    rast.settings().show_bonds = false;
    rast.settings().color_mode = ColorMode::SymmetryOp;

    assert(Rasterizer::atom_color(a1, ColorMode::SymmetryOp) !=
           Rasterizer::atom_color(a2, ColorMode::SymmetryOp));
    assert(Rasterizer::atom_color(a1, ColorMode::AsymmetricSite) ==
           Rasterizer::atom_color(a2, ColorMode::AsymmetricSite));
    assert(Rasterizer::atom_color(a1, ColorMode::Molecule) !=
           Rasterizer::atom_color(a2, ColorMode::Molecule));

    Framebuffer fb(80, 40);
    rast.settings().asymmetric_unit_only = true;
    assert(Rasterizer::is_atom_displayed(a1, rast.settings()));
    assert(!Rasterizer::is_atom_displayed(a2, rast.settings()));
    rast.render(st, cam, fb);

    bool found_a1 = false;
    bool found_a2 = false;
    for (int y = 0; y < fb.height(); ++y) {
        for (int x = 0; x < fb.width(); ++x) {
            if (fb.get_atom_id(x, y) == 0) found_a1 = true;
            if (fb.get_atom_id(x, y) == 1) found_a2 = true;
        }
    }
    assert(found_a1);
    assert(!found_a2);
    std::cout << "PASSED\n";
}

int main() {
    test_framebuffer();
    test_rasterizer_sphere();
    test_rasterizer_cylinder_and_shading();
    test_supercell_unit_cell_rendering();
    test_protocol_renderer();
    test_symmetry_color_and_asym();
    std::cout << "All rasterizer tests PASSED!\n";
    return 0;
}
