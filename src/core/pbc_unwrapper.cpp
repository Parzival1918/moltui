#include "moltui/core/pbc_unwrapper.hpp"
#include "moltui/core/bonding.hpp"
#include "moltui/core/elements.hpp"
#include <queue>
#include <cmath>
#include <glm/geometric.hpp>

namespace moltui::core {

namespace {

struct NeighborEdge {
    size_t target_atom;
    glm::ivec3 cell_offset; // Integer shift of target relative to source
};

std::vector<std::vector<NeighborEdge>> build_pbc_adjacency(const Structure& structure) {
    const size_t n = structure.atoms.size();
    std::vector<std::vector<NeighborEdge>> adj(n);
    auto bonds = BondPerceiver::compute_bonds(structure.atoms, structure.unit_cell, 0.28f, true);
    for (const auto& b : bonds) {
        adj[b.atom1].push_back({b.atom2, b.cell_offset});
        adj[b.atom2].push_back({b.atom1, -b.cell_offset});
    }
    return adj;
}

glm::vec3 wrap_unit_frac(const glm::vec3& f) {
    auto wrap_axis = [](float v) {
        float w = v - std::floor(v);
        if (w >= 1.0f - 1e-4f || w < 1e-4f) {
            return 0.0f;
        }
        return w;
    };
    return glm::vec3(wrap_axis(f.x), wrap_axis(f.y), wrap_axis(f.z));
}

} // namespace

bool PbcUnwrapper::bonding_percolates(const Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return false;
    }

    const size_t num_atoms = structure.atoms.size();
    auto adj = build_pbc_adjacency(structure);

    std::vector<bool> visited(num_atoms, false);
    std::vector<glm::ivec3> shifts(num_atoms, glm::ivec3(0));
    std::queue<size_t> q;

    for (size_t start = 0; start < num_atoms; ++start) {
        if (visited[start]) continue;

        visited[start] = true;
        shifts[start] = glm::ivec3(0);
        q.push(start);

        while (!q.empty()) {
            size_t u = q.front();
            q.pop();

            for (const auto& edge : adj[u]) {
                size_t v = edge.target_atom;
                glm::ivec3 expected_shift = shifts[u] + edge.cell_offset;
                if (!visited[v]) {
                    visited[v] = true;
                    shifts[v] = expected_shift;
                    q.push(v);
                } else if (shifts[v] != expected_shift) {
                    return true;
                }
            }
        }
    }

    return false;
}

void PbcUnwrapper::unwrap(Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return;
    }

    const auto& cell = *structure.unit_cell;
    const size_t num_atoms = structure.atoms.size();
    auto adj = build_pbc_adjacency(structure);

    std::vector<bool> visited(num_atoms, false);
    std::vector<glm::ivec3> shifts(num_atoms, glm::ivec3(0));
    std::queue<size_t> q;

    for (size_t start = 0; start < num_atoms; ++start) {
        if (visited[start]) continue;

        std::vector<size_t> component;
        visited[start] = true;
        shifts[start] = glm::ivec3(0);
        q.push(start);

        while (!q.empty()) {
            size_t u = q.front();
            q.pop();
            component.push_back(u);

            for (const auto& edge : adj[u]) {
                size_t v = edge.target_atom;
                if (!visited[v]) {
                    visited[v] = true;
                    shifts[v] = shifts[u] + edge.cell_offset;
                    q.push(v);
                }
            }
        }

        // Apply accumulated lattice shifts so the molecule is continuous
        glm::vec3 centroid_frac(0.0f);
        for (size_t idx : component) {
            structure.atoms[idx].frac_pos += glm::vec3(shifts[idx]);
            centroid_frac += structure.atoms[idx].frac_pos;
        }
        centroid_frac /= static_cast<float>(component.size());

        // Center whole molecule cleanly within the primary [0, 1) unit cell
        glm::vec3 recenter = glm::floor(centroid_frac);
        for (size_t idx : component) {
            structure.atoms[idx].frac_pos -= recenter;
            structure.atoms[idx].pos = cell.fractional_to_cartesian(structure.atoms[idx].frac_pos);
        }
    }

    // Recompute direct Cartesian bonds on unwrapped molecules (no wraparound lines)
    structure.compute_bonds(0.28f, false);
}

void PbcUnwrapper::wrap_to_unit_cell(Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return;
    }

    const auto& cell = *structure.unit_cell;
    std::vector<Atom> unique_atoms;
    unique_atoms.reserve(structure.atoms.size());

    for (auto a : structure.atoms) {
        a.frac_pos = wrap_unit_frac(a.frac_pos);
        bool dup = false;
        for (const auto& existing : unique_atoms) {
            glm::vec3 d = a.frac_pos - existing.frac_pos;
            if (glm::dot(d, d) < 1e-6f && a.element == existing.element) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            a.id = unique_atoms.size();
            a.pos = cell.fractional_to_cartesian(a.frac_pos);
            unique_atoms.push_back(std::move(a));
        }
    }

    structure.atoms = std::move(unique_atoms);
    structure.compute_bonds(0.28f, false);
}

void PbcUnwrapper::complete_network_boundaries(Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return;
    }

    wrap_to_unit_cell(structure);
    const auto& cell = *structure.unit_cell;
    std::vector<Atom> slab_atoms = structure.atoms;

    auto add_if_unique = [&](const Atom& base, const glm::vec3& f) {
        for (const auto& existing : slab_atoms) {
            glm::vec3 d = f - existing.frac_pos;
            if (glm::dot(d, d) < 1e-6f && base.element == existing.element) {
                return;
            }
        }
        Atom replica = base;
        replica.id = slab_atoms.size();
        replica.frac_pos = f;
        replica.pos = cell.fractional_to_cartesian(f);
        slab_atoms.push_back(std::move(replica));
    };

    const size_t base_count = slab_atoms.size();
    for (size_t i = 0; i < base_count; ++i) {
        const Atom base = slab_atoms[i];
        bool rx = base.frac_pos.x < 1e-3f;
        bool ry = base.frac_pos.y < 1e-3f;
        bool rz = base.frac_pos.z < 1e-3f;
        if (!rx && !ry && !rz) continue;

        for (int dx = 0; dx <= (rx ? 1 : 0); ++dx) {
            for (int dy = 0; dy <= (ry ? 1 : 0); ++dy) {
                for (int dz = 0; dz <= (rz ? 1 : 0); ++dz) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    glm::vec3 f = base.frac_pos + glm::vec3(dx, dy, dz);
                    add_if_unique(base, f);
                }
            }
        }
    }

    structure.atoms = std::move(slab_atoms);
    structure.compute_bonds(0.28f, false);
}

} // namespace moltui::core
