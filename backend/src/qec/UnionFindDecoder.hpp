#ifndef UNION_FIND_DECODER_HPP
#define UNION_FIND_DECODER_HPP

#include <vector>
#include <utility>
#include <unordered_map>
#include <memory>
#include "qubit_engine_export.h"
#include "MWPMDecoder.hpp" // For SyndromeDefect

namespace qubit_engine {

// Graph node for Union-Find decoding
struct UFNode {
    int id;
    int x;
    int y;
    int type; // 0 for X, 1 for Z
    bool is_syndrome;
    bool is_boundary;
    std::vector<int> neighbors; // IDs of neighboring nodes
};

class QUBIT_ENGINE_EXPORT UnionFindDecoder {
public:
    UnionFindDecoder() : d_(3) {}
    
    void setDistance(int d) { d_ = d; }

    // Takes a list of defects, constructs the local syndrome graph, and decodes
    // Returns a list of matched defect pairs, or abstract edges that indicate corrections
    std::vector<std::pair<int, int>> decode(const std::vector<SyndromeDefect>& defects);

private:
    int d_;

    // Disjoint Set Union data structure for cluster management
    struct DSU {
        std::vector<int> parent;
        std::vector<int> size;
        std::vector<int> parity;
        std::vector<bool> boundary;

        DSU(int n) {
            parent.resize(n);
            size.assign(n, 1);
            parity.assign(n, 0);
            boundary.assign(n, false);
            for (int i = 0; i < n; ++i) parent[i] = i;
        }

        int find(int i) {
            if (parent[i] == i) return i;
            return parent[i] = find(parent[i]);
        }

        void unite(int i, int j) {
            int root_i = find(i);
            int root_j = find(j);
            if (root_i != root_j) {
                if (size[root_i] < size[root_j]) std::swap(root_i, root_j);
                parent[root_j] = root_i;
                size[root_i] += size[root_j];
                parity[root_i] = (parity[root_i] + parity[root_j]) % 2;
                boundary[root_i] = boundary[root_i] || boundary[root_j];
            }
        }
    };
};

} // namespace qubit_engine

#endif // UNION_FIND_DECODER_HPP
