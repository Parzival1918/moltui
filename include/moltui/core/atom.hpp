#pragma once

#include <string>
#include <glm/vec3.hpp>

namespace moltui::core {

struct Atom {
    size_t id{0};
    std::string element{"C"};
    std::string label{"C"};
    glm::vec3 pos{0.0f};           // Cartesian coordinates in Angstroms
    glm::vec3 frac_pos{0.0f};      // Fractional coordinates in crystal cell [0, 1)
    
    float occupancy{1.0f};
    float b_factor{0.0f};
    float charge{0.0f};
    
    // Optional macromolecular metadata
    std::string chain_id{""};
    std::string residue_name{""};
    int residue_seq{0};
    
    // Display flags
    bool selected{false};
    bool visible{true};
};

struct Bond {
    size_t atom1{0};
    size_t atom2{0};
    float order{1.0f};
    glm::ivec3 cell_offset{0, 0, 0}; // Periodic translation offset if bond crosses boundary
};

} // namespace moltui::core
