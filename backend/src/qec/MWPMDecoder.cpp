#include "MWPMDecoder.hpp"
#include <cmath>
#include <algorithm>
#include <set>
#include <limits>

namespace qubit_engine {

double MWPMDecoder::calculateDistance(const SyndromeDefect& a, const SyndromeDefect& b) const {
    // Chebyshev distance in 2D + time distance
    // For rotated surface codes, diagonal moves (x±1, y±1) correspond to single data qubit errors
    return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)) + std::abs(a.time - b.time);
}

std::vector<std::pair<int, int>> MWPMDecoder::decode(const std::vector<SyndromeDefect>& defects) {
    std::vector<std::pair<int, int>> matches;
    if (defects.empty()) return matches;

    // Generate all edges
    std::vector<MatchingEdge> edges;
    for (size_t i = 0; i < defects.size(); ++i) {
        for (size_t j = i + 1; j < defects.size(); ++j) {
            if (defects[i].type == defects[j].type) {
                edges.push_back({
                    static_cast<int>(i),
                    static_cast<int>(j),
                    calculateDistance(defects[i], defects[j])
                });
            }
        }
    }

    // Add boundary edges
    for (size_t i = 0; i < defects.size(); ++i) {
        double dist_to_boundary = 0;
        if (defects[i].type == 0) { // X-stabilizer
            // Boundary is Left/Right for X
            dist_to_boundary = std::min(defects[i].x, d_ - defects[i].x);
        } else { // Z-stabilizer
            // Boundary is Top/Bottom for Z
            dist_to_boundary = std::min(defects[i].y, d_ - defects[i].y);
        }
        edges.push_back({
            static_cast<int>(i),
            -1,
            dist_to_boundary
        });
    }

    // Sort edges by weight ascending (Greedy MWPM)
    std::sort(edges.begin(), edges.end(), [](const MatchingEdge& a, const MatchingEdge& b) {
        return a.weight < b.weight;
    });

    std::set<int> matched;
    for (const auto& edge : edges) {
        if (matched.find(edge.u) == matched.end() && 
            (edge.v == -1 || matched.find(edge.v) == matched.end())) {
            
            matches.push_back({defects[edge.u].id, edge.v == -1 ? -1 : defects[edge.v].id});
            matched.insert(edge.u);
            if (edge.v != -1) matched.insert(edge.v);
        }
    }

    return matches;
}

} // namespace qubit_engine
