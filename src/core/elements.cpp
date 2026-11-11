#include "moltui/core/elements.hpp"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace moltui::core {

namespace {

// Standard CPK colors and radii
const std::array<ElementInfo, 119> kElements = {{
    {0, "X", "Unknown", 0.70f, 1.50f, 0.0f, {0.6f, 0.6f, 0.6f}},
    {1, "H", "Hydrogen", 0.31f, 1.10f, 1.008f, {1.0f, 1.0f, 1.0f}},
    {2, "He", "Helium", 0.28f, 1.40f, 4.003f, {0.85f, 1.0f, 1.0f}},
    {3, "Li", "Lithium", 1.28f, 1.82f, 6.941f, {0.80f, 0.50f, 1.0f}},
    {4, "Be", "Beryllium", 0.96f, 1.53f, 9.012f, {0.76f, 1.0f, 0.0f}},
    {5, "B", "Boron", 0.84f, 1.92f, 10.81f, {1.0f, 0.71f, 0.71f}},
    {6, "C", "Carbon", 0.76f, 1.70f, 12.011f, {0.35f, 0.35f, 0.35f}},
    {7, "N", "Nitrogen", 0.71f, 1.55f, 14.007f, {0.19f, 0.31f, 0.97f}},
    {8, "O", "Oxygen", 0.66f, 1.52f, 15.999f, {0.90f, 0.15f, 0.15f}},
    {9, "F", "Fluorine", 0.57f, 1.47f, 18.998f, {0.56f, 0.88f, 0.31f}},
    {10, "Ne", "Neon", 0.58f, 1.54f, 20.180f, {0.70f, 0.89f, 0.96f}},
    {11, "Na", "Sodium", 1.66f, 2.27f, 22.990f, {0.67f, 0.36f, 0.95f}},
    {12, "Mg", "Magnesium", 1.41f, 1.73f, 24.305f, {0.54f, 1.0f, 0.0f}},
    {13, "Al", "Aluminum", 1.21f, 1.84f, 26.982f, {0.75f, 0.65f, 0.65f}},
    {14, "Si", "Silicon", 1.11f, 2.10f, 28.086f, {0.94f, 0.78f, 0.63f}},
    {15, "P", "Phosphorus", 1.07f, 1.80f, 30.974f, {1.0f, 0.50f, 0.0f}},
    {16, "S", "Sulfur", 1.05f, 1.80f, 32.065f, {0.90f, 0.90f, 0.10f}},
    {17, "Cl", "Chlorine", 1.02f, 1.75f, 35.453f, {0.12f, 0.94f, 0.12f}},
    {18, "Ar", "Argon", 1.06f, 1.88f, 39.948f, {0.50f, 0.82f, 0.89f}},
    {19, "K", "Potassium", 2.03f, 2.75f, 39.098f, {0.56f, 0.25f, 0.83f}},
    {20, "Ca", "Calcium", 1.76f, 2.31f, 40.078f, {0.24f, 1.0f, 0.0f}},
    {21, "Sc", "Scandium", 1.70f, 2.11f, 44.956f, {0.90f, 0.90f, 0.90f}},
    {22, "Ti", "Titanium", 1.60f, 2.00f, 47.867f, {0.75f, 0.76f, 0.78f}},
    {23, "V", "Vanadium", 1.53f, 2.00f, 50.942f, {0.65f, 0.65f, 0.67f}},
    {24, "Cr", "Chromium", 1.39f, 2.00f, 51.996f, {0.54f, 0.60f, 0.78f}},
    {25, "Mn", "Manganese", 1.39f, 2.00f, 54.938f, {0.61f, 0.48f, 0.78f}},
    {26, "Fe", "Iron", 1.32f, 2.00f, 55.845f, {0.88f, 0.40f, 0.20f}},
    {27, "Co", "Cobalt", 1.26f, 2.00f, 58.933f, {0.94f, 0.56f, 0.63f}},
    {28, "Ni", "Nickel", 1.24f, 1.63f, 58.693f, {0.31f, 0.82f, 0.31f}},
    {29, "Cu", "Copper", 1.32f, 1.40f, 63.546f, {0.78f, 0.50f, 0.20f}},
    {30, "Zn", "Zinc", 1.22f, 1.39f, 65.380f, {0.49f, 0.50f, 0.69f}},
    {31, "Ga", "Gallium", 1.22f, 1.87f, 69.723f, {0.76f, 0.56f, 0.56f}},
    {32, "Ge", "Germanium", 1.20f, 2.11f, 72.640f, {0.40f, 0.56f, 0.56f}},
    {33, "As", "Arsenic", 1.19f, 1.85f, 74.922f, {0.74f, 0.50f, 0.89f}},
    {34, "Se", "Selenium", 1.20f, 1.90f, 78.960f, {1.0f, 0.63f, 0.0f}},
    {35, "Br", "Bromine", 1.20f, 1.85f, 79.904f, {0.65f, 0.16f, 0.16f}},
    {36, "Kr", "Krypton", 1.16f, 2.02f, 83.798f, {0.36f, 0.72f, 0.82f}},
    {37, "Rb", "Rubidium", 2.20f, 3.03f, 85.468f, {0.44f, 0.18f, 0.69f}},
    {38, "Sr", "Strontium", 1.95f, 2.49f, 87.620f, {0.0f, 1.0f, 0.0f}},
    {39, "Y", "Yttrium", 1.90f, 2.00f, 88.906f, {0.58f, 1.0f, 1.0f}},
    {40, "Zr", "Zirconium", 1.75f, 2.00f, 91.224f, {0.0f, 1.0f, 1.0f}},
    {41, "Nb", "Niobium", 1.64f, 2.00f, 92.906f, {0.45f, 0.76f, 0.79f}},
    {42, "Mo", "Molybdenum", 1.54f, 2.00f, 95.960f, {0.33f, 0.71f, 0.71f}},
    {43, "Tc", "Technetium", 1.47f, 2.00f, 98.000f, {0.23f, 0.62f, 0.62f}},
    {44, "Ru", "Ruthenium", 1.46f, 2.00f, 101.07f, {0.14f, 0.56f, 0.56f}},
    {45, "Rh", "Rhodium", 1.42f, 2.00f, 102.91f, {0.04f, 0.49f, 0.55f}},
    {46, "Pd", "Palladium", 1.39f, 1.63f, 106.42f, {0.0f, 0.41f, 0.52f}},
    {47, "Ag", "Silver", 1.45f, 1.72f, 107.87f, {0.75f, 0.75f, 0.80f}},
    {48, "Cd", "Cadmium", 1.44f, 1.58f, 112.41f, {1.0f, 0.85f, 0.56f}},
    {49, "In", "Indium", 1.42f, 1.93f, 114.82f, {0.65f, 0.46f, 0.45f}},
    {50, "Sn", "Tin", 1.39f, 2.17f, 118.71f, {0.40f, 0.50f, 0.50f}},
    {51, "Sb", "Antimony", 1.39f, 2.06f, 121.76f, {0.62f, 0.39f, 0.71f}},
    {52, "Te", "Tellurium", 1.38f, 2.06f, 127.60f, {0.83f, 0.48f, 0.0f}},
    {53, "I", "Iodine", 1.39f, 1.98f, 126.90f, {0.58f, 0.0f, 0.58f}},
    {54, "Xe", "Xenon", 1.40f, 2.16f, 131.29f, {0.26f, 0.62f, 0.69f}},
    {55, "Cs", "Cesium", 2.44f, 3.43f, 132.91f, {0.34f, 0.09f, 0.56f}},
    {56, "Ba", "Barium", 2.15f, 2.68f, 137.33f, {0.0f, 0.79f, 0.0f}},
    {57, "La", "Lanthanum", 2.07f, 2.50f, 138.91f, {0.44f, 0.83f, 1.0f}},
    {74, "W", "Tungsten", 1.62f, 2.10f, 183.84f, {0.13f, 0.58f, 0.84f}},
    {77, "Ir", "Iridium", 1.41f, 2.00f, 192.22f, {0.09f, 0.33f, 0.53f}},
    {78, "Pt", "Platinum", 1.36f, 1.75f, 195.08f, {0.82f, 0.82f, 0.88f}},
    {79, "Au", "Gold", 1.36f, 1.66f, 196.97f, {1.0f, 0.82f, 0.14f}},
    {80, "Hg", "Mercury", 1.32f, 1.55f, 200.59f, {0.72f, 0.72f, 0.82f}},
    {82, "Pb", "Lead", 1.46f, 2.02f, 207.20f, {0.34f, 0.35f, 0.38f}},
    {83, "Bi", "Bismuth", 1.48f, 2.07f, 208.98f, {0.62f, 0.31f, 0.71f}},
    {92, "U", "Uranium", 1.96f, 1.86f, 238.03f, {0.0f, 0.56f, 0.0f}}
}};

std::string normalize_symbol(std::string_view sym) {
    std::string s;
    for (char c : sym) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            s.push_back(c);
        }
    }
    if (s.empty()) return "X";
    s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    for (size_t i = 1; i < s.size(); ++i) {
        s[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(s[i])));
    }
    return s;
}

} // namespace

const ElementInfo& ElementTable::get(int atomic_number) {
    for (const auto& elem : kElements) {
        if (elem.atomic_number == atomic_number) {
            return elem;
        }
    }
    return kElements[0]; // Unknown "X"
}

const ElementInfo& ElementTable::get(std::string_view symbol) {
    std::string norm = normalize_symbol(symbol);
    for (const auto& elem : kElements) {
        if (elem.symbol == norm) {
            return elem;
        }
    }
    return kElements[0];
}

int ElementTable::symbol_to_atomic_number(std::string_view symbol) {
    return get(symbol).atomic_number;
}

glm::vec3 ElementTable::get_color(std::string_view symbol) {
    return get(symbol).cpk_color;
}

float ElementTable::get_covalent_radius(std::string_view symbol) {
    return get(symbol).covalent_radius;
}

float ElementTable::get_ball_stick_radius(std::string_view symbol) {
    return std::clamp(get(symbol).covalent_radius * 0.50f, 0.22f, 0.58f);
}

float ElementTable::get_vdw_radius(std::string_view symbol) {
    return get(symbol).vdw_radius;
}

bool ElementTable::is_nonmetal(std::string_view symbol) {
    int z = symbol_to_atomic_number(symbol);
    switch (z) {
        case 1: case 2:
        case 5: case 6: case 7: case 8: case 9: case 10:
        case 14: case 15: case 16: case 17: case 18:
        case 33: case 34: case 35: case 36:
        case 52: case 53: case 54:
            return true;
        default:
            return false;
    }
}

} // namespace moltui::core
