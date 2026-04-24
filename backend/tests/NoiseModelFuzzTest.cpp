#include <cstdint>
#include <cstddef>
#include <vector>
#include <complex>
#include "../src/NoiseModel.hpp"

using namespace qubit_engine;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size < 8) return 0;

    NoiseModel noise_model;

    // We need 4 complex numbers for a 2x2 Kraus matrix (8 doubles = 64 bytes)
    // To make it simple for the fuzzer, we'll construct basic noise channels
    // based on small chunks of data.
    
    size_t offset = 0;
    while (offset + 5 < Size) {
        int target_qubit = Data[offset++] % 10;
        uint8_t noise_type = Data[offset++] % 4;
        
        // consume 4 bytes for a float probability
        float prob = 0.0f;
        uint32_t raw_prob = (Data[offset] << 24) | (Data[offset+1] << 16) | (Data[offset+2] << 8) | Data[offset+3];
        offset += 4;
        
        // normalize prob to [0, 1]
        prob = (float)(raw_prob) / (float)0xFFFFFFFF;
        
        try {
            switch (noise_type) {
                case 0:
                    noise_model.addBitFlipError(target_qubit, prob);
                    break;
                case 1:
                    noise_model.addPhaseFlipError(target_qubit, prob);
                    break;
                case 2:
                    noise_model.addDepolarizingError(target_qubit, prob);
                    break;
                case 3:
                    // Custom Kraus channel
                    if (offset + 32 <= Size) {
                        std::vector<std::vector<Complex>> custom_kraus;
                        std::vector<Complex> m(4);
                        for (int i=0; i<4; i++) {
                            // Extract two floats for real and imag
                            float re = (float)Data[offset++] / 255.0f;
                            float im = (float)Data[offset++] / 255.0f;
                            m[i] = Complex(re, im);
                        }
                        custom_kraus.push_back(m);
                        // Make sure to add identity - K^dagger K to satisfy sum K_i^dagger K_i = I
                        // Or just let addCustomError handle/throw on validation
                        noise_model.addCustomError(target_qubit, custom_kraus);
                    }
                    break;
            }
        } catch (...) {
            // Ignore validation exceptions like non-TP channels
        }
    }

    return 0;
}
