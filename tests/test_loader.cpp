#include "moltui/io/loader.hpp"
#include <cassert>
#include <iostream>

using namespace moltui::io;

int main() {
    std::cout << "[Test] Loading caffeine.xyz... ";
    auto caffeine = StructureLoader::load_file("tests/data/caffeine.xyz");
    assert(caffeine.atoms.size() == 24);
    assert(!caffeine.bonds.empty());
    std::cout << "PASSED (" << caffeine.atoms.size() << " atoms, " << caffeine.bonds.size() << " bonds)\n";

    std::cout << "[Test] Loading benzene.xyz... ";
    auto benzene = StructureLoader::load_file("tests/data/benzene.xyz");
    assert(benzene.atoms.size() == 12);
    assert(!benzene.bonds.empty());
    std::cout << "PASSED (" << benzene.atoms.size() << " atoms, " << benzene.bonds.size() << " bonds)\n";

    std::cout << "[Test] Loading nacl.cif... ";
    auto nacl = StructureLoader::load_file("tests/data/nacl.cif");
    assert(nacl.unit_cell.has_value());
    assert(!nacl.atoms.empty());
    std::cout << "PASSED (Space group " << nacl.unit_cell->space_group << ", " << nacl.atoms.size() << " atoms in cell)\n";

    std::cout << "All file loader tests PASSED!\n";
    return 0;
}
