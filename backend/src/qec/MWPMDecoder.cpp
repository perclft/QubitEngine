#include "MWPMDecoder.hpp"
#include <cmath>
#include <algorithm>
#include <set>
#include <limits>

namespace qubit_engine {

double MWPMDecoder::calculateDistance(const SyndromeDefect& a, const SyndromeDefect& b) const {
    // Manhattan distance in 3D (x, y, time)
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.time - b.time);
}

std::vector<std::pair<int, int>> MWPMDecoder::decode(const std::vector<SyndromeDefect>& defects) {
    std::vector<std::pair<int, int>> matches;
    if (defects.empty()) return matches;

    // Generate all edges
    std::vector<MatchingEdge> edges;
    for (size_t i = 0; i < defects.size(); ++i) {
        for (size_t j = i + 1; j < defects.size(); ++j) {
            edges.push_back({
                static_cast<int>(i),
                static_cast<int>(j),
                calculateDistance(defects[i], defects[j])
            });
        }
    }

    // Sort edges by weight ascending (Greedy MWPM)
    std::sort(edges.begin(), edges.end(), [](const MatchingEdge& a, const MatchingEdge& b) {
        return a.weight < b.weight;
    });

    std::set<int> matched;
    for (const auto& edge : edges) {
        if (matched.find(edge.u) == matched.end() && matched.find(edge.v) == matched.end()) {
            matches.push_back({defects[edge.u].id, defects[edge.v].id});
            matched.insert(edge.u);
            matched.insert(edge.v);
        }
    }

    // Any remaining unmatched defect might need to be matched to the boundary
    // For a rigorous surface code implementation, boundary nodes are treated as 
    // virtual defects at the edge of the lattice. For now, we return paired defects.
    
    return matches;
}

} // namespace qubit_engine
