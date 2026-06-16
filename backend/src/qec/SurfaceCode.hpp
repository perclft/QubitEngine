#ifndef SURFACE_CODE_HPP
#define SURFACE_CODE_HPP

#include <vector>
#include <memory>
#include "../backends/StabilizerBackend.hpp"
#include "MWPMDecoder.hpp"
#include "UnionFindDecoder.hpp"
#include "DecoderType.hpp"
#include "qubit_engine_export.h"

namespace qubit_engine {

struct QUBIT_ENGINE_EXPORT Stabilizer {
    int id;
    int x;
    int y;
    std::vector<int> data_qubits;
};

// Implements a distance-d Surface Code simulation
class QUBIT_ENGINE_EXPORT SurfaceCode {
public:
    SurfaceCode(int distance, DecoderType decoder_type = DecoderType::MWPM);
    ~SurfaceCode() = default;

    // Runs a single round of syndrome extraction and returns detected defects
    std::vector<SyndromeDefect> extractSyndromes(double noise_probability);

    // Runs a full threshold simulation over multiple rounds and returns true if logical state was preserved
    bool decodeAndCorrect();
    void applyCorrections(const std::vector<std::pair<int, int>>& matches);
    bool simulate(int num_rounds, double noise_probability);

private:
    int d_; // Code distance
    int num_data_qubits_;
    int num_measure_qubits_;
    
    std::unique_ptr<StabilizerBackend> backend_;
    MWPMDecoder decoder_;
    UnionFindDecoder uf_decoder_;
    DecoderType decoder_type_;

    // To track the previous syndrome outcomes for time-domain defects
    std::vector<int> prev_syndromes_;

    void initializeLattice();
    void buildStabilizers();
    void applySyndromeCircuit();
    int qubitIndex(int x, int y) const;

    std::vector<Stabilizer> x_stabilizers_;
    std::vector<Stabilizer> z_stabilizers_;
};

} // namespace qubit_engine

#endif // SURFACE_CODE_HPP
