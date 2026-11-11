#include "moltui/core/pbc_unwrapper.hpp"
#include "moltui/core/elements.hpp"
#include <queue>
#include <cmath>
#include <glm/geometric.hpp>

namespace moltui::core {

namespace {

struct NeighborEdge {
    size_t target_atom;
    glm::vec3 pbc_shift; // Shift needed from u to v in fractional space
};

} // namespace

void PbcUnwrapper::unwrap(Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return;
    }

    const auto& cell = *structure.unit_cell;
    const size_t num_atoms = structure.atoms.size();

    // 1. Build adjacency list based on minimum image bonded contacts
    std::vector<std::vector<NeighborEdge>> adj(num_atoms);
    const float tolerance = 0.50f;

    for (size_t i = 0; i < num_atoms; ++i) {
        float r1 = ElementTable::get_covalent_radius(structure.atoms[i].element);
        for (size_t j = i + 1; j < num_atoms; ++j) {
            float r2 = ElementTable::get_covalent_radius(structure.atoms[j].element);
            float max_dist = r1 + r2 + tolerance;

            glm::vec3 frac_diff = structure.atoms[j].frac_pos - structure.atoms[i].frac_pos;
            glm::vec3 shift = glm::round(frac_diff);
            glm::vec3 min_img_frac = frac_diff - shift;
            glm::vec3 min_img_cart = cell.fractional_to_cartesian(min_img_frac);
            float dist = glm::length(min_img_cart);

            if (dist >= 0.4f && dist <= max_dist) {
                // atom i connects to atom j with shift
                adj[i].push_back(NeighborEdge{j, shift});
                // atom j connects to atom i with -shift
                adj[j].push_back(NeighborEdge{i, -shift});
            }
        }
    }

    // 2. BFS traversal to unwrap connected components
    std::vector<bool> visited(num_atoms, false);
    std::queue<size_t> q;

    for (size_t start = 0; start < num_atoms; ++start) {
        if (visited[start]) continue;

        visited[start] = true;
        q.push(start);

        while (!q.empty()) {
            size_t u = q.front();
            q.pop();

            for (const auto& edge : adj[u]) {
                size_t v = edge.target_atom;
                if (!visited[v]) {
                    // Fractional difference between current positions
                    glm::vec3 cur_diff = structure.atoms[v].frac_pos - structure.atoms[u].frac_pos;
                    glm::vec3 shift = glm::round(cur_diff);
                    
                    // Shift atom v so it is adjacent to atom u without periodic jump
                    structure.atoms[v].frac_pos -= shift;
                    structure.atoms[v].pos = cell.fractional_to_cartesian(structure.atoms[v].frac_pos);

                    visited[v] = true;
                    q.push(v);
                }
            }
        }
    }

    // 3. Recompute standard bonds on unwrapped Cartesian positions
    structure.compute_bonds();
}

void PbcUnwrapper::wrap_to_unit_cell(Structure& structure) {
    if (!structure.unit_cell.has_value() || structure.atoms.empty()) {
        return;
    }

    const auto& cell = *structure.unit_cell;
    for (auto& a : structure.atoms) {
        // Wrap fractional coords to [0, 1)
        a.frac_pos.x = a.frac_pos.x - std::floor(a.frac_pos.x);
        a.frac_pos.y = a.frac_pos.y - std::floor(a.frac_pos.y);
        a.frac_pos.z = a.frac_pos.z - std::floor(a.frac_pos.z);
        a.pos = cell.fractional_to_cartesian(a.frac_pos);
    }

    structure.compute_bonds();
}

} // namespace moltui::core
