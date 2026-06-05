#ifndef MWPM_DECODER_HPP
#define MWPM_DECODER_HPP

#include <vector>
#include <utility>
#include "qubit_engine_export.h"

namespace qubit_engine {

// A defect (syndrome) detected in the surface code
struct QUBIT_ENGINE_EXPORT SyndromeDefect {
    int id;
    int type; // 0 for X-stabilizer, 1 for Z-stabilizer
    int x;
    int y;
    int time; // For 3D matching (phenomenological noise)
};

// Edge between two defects
struct QUBIT_ENGINE_EXPORT MatchingEdge {
    int u;
    int v;
    double weight;
};

class QUBIT_ENGINE_EXPORT MWPMDecoder {
public:
    MWPMDecoder() : d_(3) {}
    
    void setDistance(int d) { d_ = d; }

    // Takes a list of defects and returns a list of paired defect IDs.
    // This uses a Greedy Approximation to Minimum-Weight Perfect Matching.
    std::vector<std::pair<int, int>> decode(const std::vector<SyndromeDefect>& defects);

private:
    int d_;
    double calculateDistance(const SyndromeDefect& a, const SyndromeDefect& b) const;
};

} // namespace qubit_engine

#endif // MWPM_DECODER_HPP
