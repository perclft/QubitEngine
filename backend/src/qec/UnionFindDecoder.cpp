#include "UnionFindDecoder.hpp"
#include <cmath>
#include <algorithm>
#include <set>
#include <queue>

namespace qubit_engine {

std::vector<std::pair<int, int>> UnionFindDecoder::decode(const std::vector<SyndromeDefect>& defects) {
    std::vector<std::pair<int, int>> matches;
    if (defects.empty()) return matches;

    // For a Rotated Surface Code, we build a local syndrome graph dynamically.
    // In a production UF decoder, the graph is pre-compiled. Here we compute edges on the fly.
    
    // 1. Separate defects by type (X and Z)
    std::vector<SyndromeDefect> x_defects, z_defects;
    for (const auto& d : defects) {
        if (d.type == 0) x_defects.push_back(d);
        else z_defects.push_back(d);
    }
    
    // We decode X and Z defects independently.
    auto decode_subgraph = [this](std::vector<SyndromeDefect>& sub_defects, bool is_x) {
        std::vector<std::pair<int, int>> sub_matches;
        if (sub_defects.empty()) return sub_matches;
        
        int n = sub_defects.size();
        DSU dsu(n);
        
        // Build edge list for greedy clustering
        std::vector<std::pair<double, std::pair<int, int>>> edges;
        
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                // Chebyshev distance
                double dist = std::max(std::abs(sub_defects[i].x - sub_defects[j].x),
                                       std::abs(sub_defects[i].y - sub_defects[j].y)) + 
                              std::abs(sub_defects[i].time - sub_defects[j].time);
                edges.push_back({dist, {i, j}});
            }
            // Boundary distance
            double b_dist = is_x ? std::min(sub_defects[i].x, d_ - sub_defects[i].x)
                                 : std::min(sub_defects[i].y, d_ - sub_defects[i].y);
            edges.push_back({b_dist, {i, -1}});
        }
        
        // Sort edges by distance (approximating uniform cluster growth)
        std::sort(edges.begin(), edges.end());
        
        // Initialize odd clusters
        for (int i = 0; i < n; ++i) {
            dsu.parity[i] = 1; // 1 defect = odd
        }
        
        // Grow clusters
        std::vector<std::pair<int, int>> valid_edges;
        for (const auto& edge : edges) {
            int u = edge.second.first;
            int v = edge.second.second;
            
            int root_u = dsu.find(u);
            
            // Skip if cluster is already even (and doesn't need to grow)
            // Wait: In true UF, an even cluster can still merge if an odd cluster bumps into it.
            // But if BOTH are even, or if an even cluster bumps into a boundary, we skip.
            if (v == -1) {
                if (dsu.parity[root_u] % 2 != 0 && !dsu.boundary[root_u]) {
                    dsu.boundary[root_u] = true;
                    dsu.parity[root_u] = 0; // Boundary makes it even (effectively resolved)
                    valid_edges.push_back({u, -1});
                }
            } else {
                int root_v = dsu.find(v);
                if (root_u != root_v) {
                    // Only merge if at least one is odd (or neither is connected to boundary if odd)
                    if (dsu.parity[root_u] % 2 != 0 || dsu.parity[root_v] % 2 != 0) {
                        dsu.unite(u, v);
                        valid_edges.push_back({u, v});
                        // If it becomes even, it's resolved.
                        int new_root = dsu.find(u);
                        if (dsu.boundary[new_root]) dsu.parity[new_root] = 0;
                    }
                }
            }
        }
        
        // Peeling algorithm (simplified on valid_edges spanning forest)
        // Count degrees
        std::vector<int> degree(n, 0);
        std::vector<std::vector<int>> adj(n);
        std::vector<int> boundary_connections(n, 0);
        
        for (const auto& edge : valid_edges) {
            if (edge.second == -1) {
                boundary_connections[edge.first]++;
            } else {
                adj[edge.first].push_back(edge.second);
                adj[edge.second].push_back(edge.first);
                degree[edge.first]++;
                degree[edge.second]++;
            }
        }
        
        std::queue<int> leaves;
        for (int i = 0; i < n; ++i) {
            if (degree[i] <= 1) leaves.push(i);
        }
        
        std::vector<bool> active(n, true);
        std::vector<int> current_parity(n, 1);
        
        while (!leaves.empty()) {
            int u = leaves.front();
            leaves.pop();
            if (!active[u]) continue;
            
            active[u] = false;
            
            // If it's connected to boundary and odd, pair it with boundary
            if (current_parity[u] == 1 && boundary_connections[u] > 0) {
                sub_matches.push_back({sub_defects[u].id, -1});
                current_parity[u] = 0;
            }
            
            for (int v : adj[u]) {
                if (active[v]) {
                    degree[v]--;
                    if (degree[v] <= 1) leaves.push(v);
                    
                    if (current_parity[u] == 1) {
                        // Match u and v
                        sub_matches.push_back({sub_defects[u].id, sub_defects[v].id});
                        current_parity[u] = 0;
                        current_parity[v] ^= 1; // Flip parity of parent
                    }
                }
            }
            
            // If it's still odd and has a boundary connection (maybe we used it late)
            if (current_parity[u] == 1 && boundary_connections[u] > 0) {
                sub_matches.push_back({sub_defects[u].id, -1});
                current_parity[u] = 0;
            }
        }
        
        return sub_matches;
    };
    
    auto x_matches = decode_subgraph(x_defects, true);
    auto z_matches = decode_subgraph(z_defects, false);
    
    matches.insert(matches.end(), x_matches.begin(), x_matches.end());
    matches.insert(matches.end(), z_matches.begin(), z_matches.end());
    
    return matches;
}

} // namespace qubit_engine
