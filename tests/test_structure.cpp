#include "moltui/core/elements.hpp"
#include "moltui/core/structure.hpp"
#include "moltui/core/bonding.hpp"
#include "moltui/core/pbc_unwrapper.hpp"

#include <cassert>
#include <iostream>
#include <cmath>

using namespace moltui::core;

void test_element_table() {
    std::cout << "[Test] ElementTable... ";
    const auto& c = ElementTable::get("C");
    assert(c.atomic_number == 6);
    assert(c.symbol == "C");
    assert(std::abs(c.covalent_radius - 0.76f) < 1e-4);

    const auto& h = ElementTable::get(1);
    assert(h.symbol == "H");

    const auto& fe = ElementTable::get("Fe");
    assert(fe.atomic_number == 26);
    std::cout << "PASSED\n";
}

void test_unit_cell_transforms() {
    std::cout << "[Test] UnitCell Transforms... ";
    UnitCell cell;
    cell.a = 10.0;
    cell.b = 10.0;
    cell.c = 10.0;
    cell.alpha = 90.0;
    cell.beta = 90.0;
    cell.gamma = 90.0;
    cell.update_matrices();

    glm::vec3 frac(0.5f, 0.25f, 0.75f);
    glm::vec3 cart = cell.fractional_to_cartesian(frac);
    assert(std::abs(cart.x - 5.0f) < 1e-4);
    assert(std::abs(cart.y - 2.5f) < 1e-4);
    assert(std::abs(cart.z - 7.5f) < 1e-4);

    glm::vec3 back_frac = cell.cartesian_to_fractional(cart);
    assert(std::abs(back_frac.x - frac.x) < 1e-4);
    assert(std::abs(back_frac.y - frac.y) < 1e-4);
    assert(std::abs(back_frac.z - frac.z) < 1e-4);
    std::cout << "PASSED\n";
}

void test_pbc_unwrapping() {
    std::cout << "[Test] PBC Molecular Unwrapping... ";
    Structure st;
    UnitCell cell;
    cell.a = 10.0;
    cell.b = 10.0;
    cell.c = 10.0;
    cell.alpha = 90.0;
    cell.beta = 90.0;
    cell.gamma = 90.0;
    cell.update_matrices();
    st.unit_cell = cell;

    // A diatomic molecule C-O split across boundary x=0 and x=1
    // Atom 0: C at frac x = 0.05 (cart x = 0.5)
    // Atom 1: O at frac x = 0.95 (cart x = 9.5)
    // Wrapped distance in box = 9.0 A
    // Periodic minimum image distance = 1.0 A (bonded!)
    Atom c;
    c.id = 0;
    c.element = "C";
    c.frac_pos = {0.05f, 0.5f, 0.5f};
    c.pos = cell.fractional_to_cartesian(c.frac_pos);

    Atom o;
    o.id = 1;
    o.element = "O";
    o.frac_pos = {0.95f, 0.5f, 0.5f};
    o.pos = cell.fractional_to_cartesian(o.frac_pos);

    st.atoms = {c, o};
    
    // In wrapped coordinates, Cartesian distance is 9.0 A
    float dist_wrapped = glm::length(st.atoms[1].pos - st.atoms[0].pos);
    assert(dist_wrapped > 8.0f);

    // Unwrap across PBC!
    st.unwrap_pbc();

    // In unwrapped coordinates, C and O must be adjacent (~1.0 A apart)
    float dist_unwrapped = glm::length(st.atoms[1].pos - st.atoms[0].pos);
    assert(std::abs(dist_unwrapped - 1.0f) < 1e-3);
    assert(st.bonds.size() == 1);

    std::cout << "PASSED (Bond preserved, distance " << dist_unwrapped << " A)\n";
}

int main() {
    test_element_table();
    test_unit_cell_transforms();
    test_pbc_unwrapping();
    std::cout << "All structure tests PASSED!\n";
    return 0;
}
