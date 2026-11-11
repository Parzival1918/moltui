#include "moltui/core/structure.hpp"
#include "moltui/core/bonding.hpp"
#include "moltui/core/pbc_unwrapper.hpp"
#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/geometric.hpp>
#include <algorithm>

namespace moltui::core {

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
}

void UnitCell::update_matrices() {
    double cos_a = std::cos(alpha * kDegToRad);
    double cos_b = std::cos(beta * kDegToRad);
    double cos_g = std::cos(gamma * kDegToRad);
    double sin_g = std::sin(gamma * kDegToRad);
    if (sin_g == 0.0) sin_g = 1e-7;

    double v_term = 1.0 - cos_a * cos_a - cos_b * cos_b - cos_g * cos_g + 2.0 * cos_a * cos_b * cos_g;
    double vol_factor = std::sqrt(std::max(0.0, v_term));

    // IUPAC orthogonalization matrix (column vectors a, b, c)
    glm::mat3 m(0.0f);
    m[0][0] = static_cast<float>(a);
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;

    m[1][0] = static_cast<float>(b * cos_g);
    m[1][1] = static_cast<float>(b * sin_g);
    m[1][2] = 0.0f;

    m[2][0] = static_cast<float>(c * cos_b);
    m[2][1] = static_cast<float>(c * (cos_a - cos_b * cos_g) / sin_g);
    m[2][2] = static_cast<float>(c * vol_factor / sin_g);

    orth_matrix = m;
    frac_matrix = glm::inverse(m);
}

glm::vec3 UnitCell::fractional_to_cartesian(const glm::vec3& frac) const {
    return orth_matrix * frac;
}

glm::vec3 UnitCell::cartesian_to_fractional(const glm::vec3& cart) const {
    return frac_matrix * cart;
}

void Structure::clear() {
    atoms.clear();
    bonds.clear();
    unit_cell.reset();
    trajectory_frames.clear();
    current_frame = 0;
    bounds_dirty = true;
}

void Structure::compute_bounds() const {
    if (atoms.empty()) {
        bounds = {glm::vec3(-1.0f), glm::vec3(1.0f), glm::vec3(0.0f), 2.0f};
        bounds_dirty = false;
        return;
    }

    glm::vec3 min_p(1e9f);
    glm::vec3 max_p(-1e9f);

    for (const auto& a : atoms) {
        if (!a.visible) continue;
        min_p = glm::min(min_p, a.pos);
        max_p = glm::max(max_p, a.pos);
    }

    // Include unit cell corners if present
    if (unit_cell.has_value()) {
        for (int i = 0; i < 8; ++i) {
            glm::vec3 corner_frac(
                (i & 1) ? 1.0f : 0.0f,
                (i & 2) ? 1.0f : 0.0f,
                (i & 4) ? 1.0f : 0.0f
            );
            glm::vec3 corner_cart = unit_cell->fractional_to_cartesian(corner_frac);
            min_p = glm::min(min_p, corner_cart);
            max_p = glm::max(max_p, corner_cart);
        }
    }

    bounds.min = min_p;
    bounds.max = max_p;
    bounds.center = 0.5f * (min_p + max_p);
    bounds.radius = glm::length(max_p - bounds.center);
    if (bounds.radius < 0.5f) bounds.radius = 0.5f;

    bounds_dirty = false;
}

const Bounds& Structure::get_bounds() const {
    if (bounds_dirty) {
        compute_bounds();
    }
    return bounds;
}

void Structure::compute_bonds(float tolerance) {
    bonds = BondPerceiver::compute_bonds(atoms, unit_cell, tolerance);
}

void Structure::unwrap_pbc() {
    PbcUnwrapper::unwrap(*this);
    bounds_dirty = true;
}

void Structure::wrap_pbc() {
    PbcUnwrapper::wrap_to_unit_cell(*this);
    bounds_dirty = true;
}

Structure Structure::generate_supercell(int nx, int ny, int nz) const {
    if (!unit_cell.has_value() || (nx <= 1 && ny <= 1 && nz <= 1)) {
        return *this;
    }

    Structure super;
    super.title = title + " [supercell]";
    
    UnitCell new_cell = *unit_cell;
    new_cell.a *= nx;
    new_cell.b *= ny;
    new_cell.c *= nz;
    new_cell.update_matrices();
    super.unit_cell = new_cell;

    size_t id = 0;
    for (int ix = 0; ix < nx; ++ix) {
        for (int iy = 0; iy < ny; ++iy) {
            for (int iz = 0; iz < nz; ++iz) {
                glm::vec3 offset_frac(
                    static_cast<float>(ix),
                    static_cast<float>(iy),
                    static_cast<float>(iz)
                );
                for (const auto& a : atoms) {
                    Atom na = a;
                    na.id = id++;
                    // Supercell fractional coordinate
                    na.frac_pos = (a.frac_pos + offset_frac) / glm::vec3(nx, ny, nz);
                    na.pos = new_cell.fractional_to_cartesian(na.frac_pos);
                    super.atoms.push_back(na);
                }
            }
        }
    }

    super.compute_bonds();
    return super;
}

void Structure::set_frame(size_t frame_index) {
    if (trajectory_frames.empty()) return;
    current_frame = std::min(frame_index, trajectory_frames.size() - 1);
    const auto& frame_positions = trajectory_frames[current_frame];
    size_t n = std::min(atoms.size(), frame_positions.size());
    for (size_t i = 0; i < n; ++i) {
        atoms[i].pos = frame_positions[i];
        if (unit_cell.has_value()) {
            atoms[i].frac_pos = unit_cell->cartesian_to_fractional(atoms[i].pos);
        }
    }
    bounds_dirty = true;
}

} // namespace moltui::core
