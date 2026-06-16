#ifndef COLOR_CODE_HPP
#define COLOR_CODE_HPP

#include <vector>
#include <memory>
#include "../backends/StabilizerBackend.hpp"
#include "MWPMDecoder.hpp"
#include "UnionFindDecoder.hpp"
#include "DecoderType.hpp"
#include "qubit_engine_export.h"

namespace qubit_engine {

// Implements a 6.6.6 triangular Color Code
class QUBIT_ENGINE_EXPORT ColorCode {
public:
    ColorCode(int distance, DecoderType decoder_type = DecoderType::MWPM);
    ~ColorCode() = default;

    std::vector<SyndromeDefect> extractSyndromes(double noise_probability);

    bool decodeAndCorrect();
    void applyCorrections(const std::vector<std::pair<int, int>>& matches);
    bool simulate(int num_rounds, double noise_probability);

private:
    int d_;
    int num_data_qubits_;
    int num_measure_qubits_;
    
    std::unique_ptr<StabilizerBackend> backend_;
    MWPMDecoder decoder_;
    UnionFindDecoder uf_decoder_;
    DecoderType decoder_type_;

    std::vector<int> prev_syndromes_;

    void initializeLattice();
    void buildStabilizers();
    void applySyndromeCircuit();

    struct ColorStabilizer {
        int id;
        int color; // 0: Red, 1: Green, 2: Blue
        int type;  // 0: X, 1: Z
        std::vector<int> data_qubits;
    };

    std::vector<ColorStabilizer> x_stabilizers_;
    std::vector<ColorStabilizer> z_stabilizers_;
};

} // namespace qubit_engine

#endif // COLOR_CODE_HPP
