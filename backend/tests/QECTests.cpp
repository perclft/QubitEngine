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
    decoder.setDistance(10);
    std::vector<SyndromeDefect> defects = {
        {1, 0, 0, 0, 0}, // id, type, x, y, time
        {2, 0, 1, 0, 0},
        {3, 0, 5, 5, 0},
        {4, 0, 5, 6, 0}
    };
    
    auto matches = decoder.decode(defects);
    
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

TEST(MWPMDecoderTest, BoundaryMatching) {
    MWPMDecoder decoder;
    decoder.setDistance(5); // x goes from -1 to 4
    
    std::vector<SyndromeDefect> defects = {
        {1, 0, 0, 2, 0}, // distance to x=-1 boundary is 1
        {2, 0, 3, 2, 0}  // distance to x=5 boundary is 2, distance to 1 is 3
    };
    
    auto matches = decoder.decode(defects);
    
    // They should match to the boundary instead of each other
    // Defect 1 matches boundary (-1), Defect 2 matches boundary (-1)
    bool def1_boundary = false;
    bool def2_boundary = false;
    for (auto& match : matches) {
        if ((match.first == 1 && match.second == -1) || (match.first == -1 && match.second == 1)) def1_boundary = true;
        if ((match.first == 2 && match.second == -1) || (match.first == -1 && match.second == 2)) def2_boundary = true;
    }
    
    EXPECT_TRUE(def1_boundary);
    EXPECT_TRUE(def2_boundary);
}

// ============================================================================
// Surface Code Framework Tests
// ============================================================================

TEST(SurfaceCodeTest, Initialization_D3) {
    SurfaceCode sc(3);
    sc.extractSyndromes(0.0);
    auto defects = sc.extractSyndromes(0.0);
    EXPECT_TRUE(defects.empty());
}

TEST(SurfaceCodeTest, Initialization_D5) {
    SurfaceCode sc(5);
    sc.extractSyndromes(0.0);
    auto defects = sc.extractSyndromes(0.0);
    EXPECT_TRUE(defects.empty());
}

TEST(SurfaceCodeTest, PerfectExecution_D3) {
    SurfaceCode sc(3);
    bool success = sc.simulate(5, 0.0);
    EXPECT_TRUE(success);
}

TEST(SurfaceCodeTest, PerfectExecution_D7) {
    SurfaceCode sc(7);
    bool success = sc.simulate(3, 0.0);
    EXPECT_TRUE(success);
}

TEST(SurfaceCodeTest, HighNoiseThreshold) {
    SurfaceCode sc(3);
    bool success = sc.simulate(5, 0.9);
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
