#include "ColorCode.hpp"
#include <iostream>

namespace qubit_engine {

ColorCode::ColorCode(int distance) : d_(distance) {
    // 6.6.6 Triangular Color Code.
    // For d=3, this is exactly the 7-qubit Steane code.
    if (d_ == 3) {
        num_data_qubits_ = 7;
    } else if (d_ == 5) {
        num_data_qubits_ = 19;
    } else {
        // Fallback for unsupported distances in this implementation
        num_data_qubits_ = (3 * d_ * d_ + 1) / 4; 
    }
    
    buildStabilizers();
    num_measure_qubits_ = x_stabilizers_.size() + z_stabilizers_.size();
    
    int total_qubits = num_data_qubits_ + num_measure_qubits_;
    backend_ = std::make_unique<StabilizerBackend>(total_qubits);
    prev_syndromes_.resize(num_measure_qubits_, 0);
    decoder_.setDistance(d_);
}

void ColorCode::buildStabilizers() {
    x_stabilizers_.clear();
    z_stabilizers_.clear();
    
    int id_counter = 0;
    
    if (d_ == 3) {
        // Steane Code (d=3 Color Code)
        // 3 plaquettes, each has an X and Z stabilizer
        std::vector<std::vector<int>> plaquettes = {
            {0, 1, 2, 3}, // Red
            {1, 4, 3, 5}, // Green
            {2, 3, 5, 6}  // Blue
        };
        
        int c = 0;
        for (const auto& p : plaquettes) {
            x_stabilizers_.push_back({id_counter++, c, 0, p});
            z_stabilizers_.push_back({id_counter++, c, 1, p});
            c++;
        }
    } else {
        // Placeholder for general distance. Building a true 6.6.6 triangle requires 
        // a complex boundary-aware hexagonal lattice generator.
        // For simulation purposes, we rely on d=3.
    }
}

void ColorCode::initializeLattice() {
}

void ColorCode::applySyndromeCircuit() {
    int m_idx = num_data_qubits_;
    
    // X-stabilizers
    for (const auto& stab : x_stabilizers_) {
        backend_->applyHadamard(m_idx);
        for (int d_q : stab.data_qubits) backend_->applyCNOT(m_idx, d_q);
        backend_->applyHadamard(m_idx);
        m_idx++;
    }
    
    // Z-stabilizers
    for (const auto& stab : z_stabilizers_) {
        for (int d_q : stab.data_qubits) backend_->applyCNOT(d_q, m_idx);
        m_idx++;
    }
}

std::vector<SyndromeDefect> ColorCode::extractSyndromes(double noise_probability) {
    backend_->applyDepolarizingNoise(noise_probability);
    applySyndromeCircuit();
    
    std::vector<SyndromeDefect> defects;
    int defect_id = 0;
    
    for (int i = 0; i < num_measure_qubits_; ++i) {
        int m_q = num_data_qubits_ + i;
        int result = backend_->measure(m_q);
        
        if (result != prev_syndromes_[i]) {
            SyndromeDefect defect;
            defect.id = defect_id++;
            defect.type = (i < x_stabilizers_.size()) ? 0 : 1;
            // In a color code, coordinates are colors/plaquettes. 
            // For MWPM projection, we'd map this to a 3D matching graph.
            defect.x = i; 
            defect.y = 0;
            defect.time = 0; 
            defects.push_back(defect);
        }
        prev_syndromes_[i] = result;
        
        if (result == 1) {
            backend_->applyX(m_q);
        }
    }
    
    return defects;
}

void ColorCode::applyCorrections(const std::vector<std::pair<int, int>>& matches) {
    // Advanced decoding for color codes usually involves projecting to 3 surface codes
    // or using Restriction decoders.
    // For this demonstration, we apply dummy corrections on matched pairs.
    for (const auto& match : matches) {
        backend_->applyX(0); // Placeholder
    }
}

bool ColorCode::decodeAndCorrect() {
    std::vector<SyndromeDefect> defects = extractSyndromes(0.0);
    if (defects.empty()) return true;

    auto matches = decoder_.decode(defects);
    applyCorrections(matches);
    return true;
}

bool ColorCode::simulate(int num_rounds, double noise_probability) {
    initializeLattice();
    
    for (int t = 0; t < num_rounds; ++t) {
        std::vector<SyndromeDefect> defects = extractSyndromes(noise_probability);
        if (t == 0) continue;
        
        for (auto& d : defects) d.time = t;
        auto matches = decoder_.decode(defects);
        applyCorrections(matches);
    }
    
    // Logical Z for Steane code is Z on all qubits
    int logical_z = 0;
    for (int i = 0; i < num_data_qubits_; ++i) {
        logical_z ^= backend_->measure(i);
    }
    
    return (logical_z == 0);
}

} // namespace qubit_engine
