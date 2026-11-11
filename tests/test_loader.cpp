#include "moltui/io/loader.hpp"
#include <cassert>
#include <cmath>
#include <glm/geometric.hpp>
#include <iostream>

using namespace moltui::io;

int main() {
    std::cout << "[Test] Loading caffeine.xyz... ";
    auto caffeine = StructureLoader::load_file("tests/data/caffeine.xyz");
    assert(caffeine.atoms.size() == 24);
    assert(caffeine.bonds.size() == 25);
    std::cout << "PASSED (" << caffeine.atoms.size() << " atoms, " << caffeine.bonds.size() << " bonds)\n";

    std::cout << "[Test] Loading benzene.xyz... ";
    auto benzene = StructureLoader::load_file("tests/data/benzene.xyz");
    assert(benzene.atoms.size() == 12);
    assert(benzene.bonds.size() == 12);
    std::cout << "PASSED (" << benzene.atoms.size() << " atoms, " << benzene.bonds.size() << " bonds)\n";

    std::cout << "[Test] Loading nacl.cif (network crystal)... ";
    auto nacl = StructureLoader::load_file("tests/data/nacl.cif");
    assert(nacl.unit_cell.has_value());
    assert(!nacl.is_molecular_crystal);
    assert(nacl.atoms.size() == 27);
    assert(nacl.bonds.size() == 54);
    for (const auto& b : nacl.bonds) {
        assert(b.cell_offset == glm::ivec3(0));
        float len = glm::length(nacl.atoms[b.atom2].pos - nacl.atoms[b.atom1].pos);
        assert(std::abs(len - 2.8201f) < 0.02f);
    }
    std::cout << "PASSED (" << nacl.atoms.size() << " atoms, " << nacl.bonds.size() << " bonds)\n";

    std::cout << "[Test] Loading structures.cif (multi-block acetic acid)... ";
    auto acetic_list = StructureLoader::load_structures("tests/data/structures.cif");
    assert(acetic_list.size() == 6);
    for (const auto& st : acetic_list) {
        assert(st.unit_cell.has_value());
        assert(st.is_molecular_crystal);
        // Z=4 C2H4O2 -> 32 atoms, 7 covalent bonds per molecule = 28 bonds
        assert(st.atoms.size() == 32);
        assert(st.bonds.size() == 28);
        for (const auto& b : st.bonds) {
            assert(b.cell_offset == glm::ivec3(0));
            float len = glm::length(st.atoms[b.atom2].pos - st.atoms[b.atom1].pos);
            assert(len > 0.8f && len < 1.8f);
        }
    }
    std::cout << "PASSED (" << acetic_list.size() << " blocks, 32 atoms / 28 bonds each)\n";

    std::cout << "All file loader tests PASSED!\n";
    return 0;
}
