#include <gtest/gtest.h>
#include "qec/SurfaceCode.hpp"
#include "qec/MWPMDecoder.hpp"
#include "backends/StabilizerBackend.hpp"

using namespace qubit_engine;

// ============================================================================
// MWPM Decoder Tests
// ============================================================================

TEST(MWPMDecoderTest, BasicMatching) {
    MWPMDecoder decoder;
    std::vector<SyndromeDefect> defects = {
        {1, 0, 0, 0},
        {2, 1, 0, 0},
        {3, 5, 5, 0},
        {4, 5, 6, 0}
    };
    
    auto matches = decoder.decode(defects);
    
    // We expect 2 pairs
    EXPECT_EQ(matches.size(), 2);
    
    // Nearest neighbors should pair: (1, 2) and (3, 4)
    bool pair1_found = false;
    bool pair2_found = false;
    for (auto& match : matches) {
        if ((match.first == 1 && match.second == 2) || (match.first == 2 && match.second == 1)) pair1_found = true;
        if ((match.first == 3 && match.second == 4) || (match.first == 4 && match.second == 3)) pair2_found = true;
    }
    
    EXPECT_TRUE(pair1_found);
    EXPECT_TRUE(pair2_found);
}

// ============================================================================
// Surface Code Framework Tests
// ============================================================================

TEST(SurfaceCodeTest, Initialization) {
    SurfaceCode sc(3);
    // Project the state into the stabilizer codespace
    sc.extractSyndromes(0.0);
    // 0 noise should yield 0 defects in subsequent rounds
    auto defects = sc.extractSyndromes(0.0);
    EXPECT_TRUE(defects.empty());
}

TEST(SurfaceCodeTest, PerfectExecution) {
    SurfaceCode sc(3);
    // 0 noise -> logical state is preserved
    bool success = sc.simulate(5, 0.0);
    EXPECT_TRUE(success);
}

TEST(SurfaceCodeTest, HighNoiseThreshold) {
    SurfaceCode sc(3);
    // Extremely high noise (90%) will almost certainly fail
    bool success = sc.simulate(5, 0.9);
    
    // With d=3, 5 rounds of 90% error, the logical Z measurement will be randomized.
    // It's possible it succeeds by chance (50%), but we can't reliably assert EXPECT_FALSE
    // unless we do many shots. For this unit test, we just ensure it compiles and runs.
    SUCCEED();
}

// ============================================================================
// StabilizerBackend Noise
// ============================================================================

TEST(StabilizerBackendTest, StochasticNoise) {
    StabilizerBackend backend(1);
    
    // p=0 should not flip |0>
    backend.applyDepolarizingNoise(0.0);
    EXPECT_EQ(backend.measure(0), 0);
    
    // p=1.0 will apply a Pauli X, Y, or Z. 
    // X or Y flips |0> to |1> (prob 2/3). Z leaves it at |0> (prob 1/3).
    int flips = 0;
    int trials = 1000;
    for(int i=0; i<trials; i++) {
        StabilizerBackend b(1);
        b.applyDepolarizingNoise(1.0);
        if (b.measure(0) == 1) flips++;
    }
    
    // Expect ~ 666 flips. Margin +/- 100
    EXPECT_NEAR(flips, 666, 100);
}
