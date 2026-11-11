#pragma once

#include <string>
#include <string_view>
#include <array>
#include <glm/vec3.hpp>

namespace moltui::core {

struct ElementInfo {
    int atomic_number;
    std::string_view symbol;
    std::string_view name;
    float covalent_radius;     // in Angstroms
    float vdw_radius;          // in Angstroms
    float atomic_mass;         // in Da
    glm::vec3 cpk_color;       // RGB [0.0, 1.0]
};

class ElementTable {
public:
    static const ElementInfo& get(int atomic_number);
    static const ElementInfo& get(std::string_view symbol);
    static int symbol_to_atomic_number(std::string_view symbol);
    static glm::vec3 get_color(std::string_view symbol);
    static float get_covalent_radius(std::string_view symbol);
    static float get_vdw_radius(std::string_view symbol);
};

} // namespace moltui::core
