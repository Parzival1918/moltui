#pragma once

#include "moltui/core/atom.hpp"
#include <vector>
#include <string>
#include <optional>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>

namespace moltui::core {

struct UnitCell {
    double a{1.0};
    double b{1.0};
    double c{1.0};
    double alpha{90.0};
    double beta{90.0};
    double gamma{90.0};
    
    std::string space_group{"P 1"};
    int space_group_number{1};
    
    glm::mat3 orth_matrix{1.0f};  // Fractional to Cartesian
    glm::mat3 frac_matrix{1.0f};  // Cartesian to Fractional
    
    void update_matrices();
    glm::vec3 fractional_to_cartesian(const glm::vec3& frac) const;
    glm::vec3 cartesian_to_fractional(const glm::vec3& cart) const;
};

struct Bounds {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    glm::vec3 center{0.0f};
    float radius{1.0f};
};

class Structure {
public:
    std::string title{"Untitled"};
    std::vector<Atom> atoms;
    std::vector<Bond> bonds;
    std::optional<UnitCell> unit_cell;
    
    // Multi-frame trajectory support
    std::vector<std::vector<glm::vec3>> trajectory_frames;
    size_t current_frame{0};

    // Cached bounds
    mutable Bounds bounds;
    mutable bool bounds_dirty{true};

    void clear();
    void compute_bounds() const;
    const Bounds& get_bounds() const;

    // Bonding heuristics
    void compute_bonds(float tolerance = 0.45f);

    // Periodic Boundary Operations
    bool is_periodic() const { return unit_cell.has_value(); }
    void unwrap_pbc();
    void wrap_pbc();

    // Supercell generation (e.g. 2x2x2)
    Structure generate_supercell(int nx, int ny, int nz) const;

    // Trajectory navigation
    void set_frame(size_t frame_index);
    size_t num_frames() const { return trajectory_frames.empty() ? 1 : trajectory_frames.size(); }
};

} // namespace moltui::core
