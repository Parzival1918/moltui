#include "moltui/render/camera.hpp"
#include "moltui/render/framebuffer.hpp"
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

int main() {
    test_framebuffer();
    test_rasterizer_sphere();
    std::cout << "All rasterizer tests PASSED!\n";
    return 0;
}
