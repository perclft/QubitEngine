#ifndef MWPM_DECODER_HPP
#define MWPM_DECODER_HPP

#include <vector>
#include <utility>

namespace qubit_engine {

// A defect (syndrome) detected in the surface code
struct SyndromeDefect {
    int id;
    int x;
    int y;
    int time; // For 3D matching (phenomenological noise)
};

// Edge between two defects
struct MatchingEdge {
    int u;
    int v;
    double weight;
};

class MWPMDecoder {
public:
    MWPMDecoder() = default;

    // Takes a list of defects and returns a list of paired defect IDs.
    // This uses a Greedy Approximation to Minimum-Weight Perfect Matching.
    std::vector<std::pair<int, int>> decode(const std::vector<SyndromeDefect>& defects);

private:
    double calculateDistance(const SyndromeDefect& a, const SyndromeDefect& b) const;
};

} // namespace qubit_engine

#endif // MWPM_DECODER_HPP
