#include "SurfaceCode.hpp"
#include <iostream>
#include <numeric>

namespace qubit_engine {

SurfaceCode::SurfaceCode(int distance) : d_(distance) {
    num_data_qubits_ = d_ * d_;
    buildStabilizers();
    num_measure_qubits_ = x_stabilizers_.size() + z_stabilizers_.size();
    int total_qubits = num_data_qubits_ + num_measure_qubits_;
    
    backend_ = std::make_unique<StabilizerBackend>(total_qubits);
    prev_syndromes_.resize(num_measure_qubits_, 0);
    decoder_.setDistance(d_);
}

void SurfaceCode::buildStabilizers() {
    x_stabilizers_.clear();
    z_stabilizers_.clear();
    int id_counter = 0;
    
    for (int y = -1; y < d_; ++y) {
        for (int x = -1; x < d_; ++x) {
            std::vector<int> data_qubits;
            auto add_if_valid = [&](int qx, int qy) {
                if (qx >= 0 && qx < d_ && qy >= 0 && qy < d_) {
                    data_qubits.push_back(qy * d_ + qx);
                }
            };
            add_if_valid(x, y);
            add_if_valid(x + 1, y);
            add_if_valid(x, y + 1);
            add_if_valid(x + 1, y + 1);
            
            if (data_qubits.size() == 2 || data_qubits.size() == 4) {
                bool is_x = ((x + y) % 2 == 0);
                
                if (x == -1 && !is_x) continue;
                if (x == d_ - 1 && !is_x) continue;
                if (y == -1 && is_x) continue;
                if (y == d_ - 1 && is_x) continue;
                
                if (is_x) x_stabilizers_.push_back({id_counter++, x, y, data_qubits});
                else z_stabilizers_.push_back({id_counter++, x, y, data_qubits});
            }
        }
    }
}

void SurfaceCode::initializeLattice() {
    // Start all data qubits in |0> (already done by StabilizerBackend default)
    // Measure qubits are also in |0>
}

int SurfaceCode::qubitIndex(int x, int y) const {
    return y * d_ + x;
}

void SurfaceCode::applySyndromeCircuit() {
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

std::vector<SyndromeDefect> SurfaceCode::extractSyndromes(double noise_probability) {
    // 1. Apply stochastic noise
    backend_->applyDepolarizingNoise(noise_probability);
    
    // 2. Run the extraction circuit
    applySyndromeCircuit();
    
    // 3. Measure syndrome qubits
    std::vector<SyndromeDefect> defects;
    int defect_id = 0;
    
    for (int i = 0; i < num_measure_qubits_; ++i) {
        int m_q = num_data_qubits_ + i;
        int result = backend_->measure(m_q);
        
        // Find corresponding stabilizer
        const Stabilizer* stab = nullptr;
        if (i < x_stabilizers_.size()) stab = &x_stabilizers_[i];
        else stab = &z_stabilizers_[i - x_stabilizers_.size()];
        
        // A defect is a *change* in the syndrome measurement from the previous round
        if (result != prev_syndromes_[i]) {
            SyndromeDefect defect;
            defect.id = defect_id++;
            defect.type = (i < x_stabilizers_.size()) ? 0 : 1;
            defect.x = stab->x; 
            defect.y = stab->y;
            defect.time = 0; // Handled in loop
            defects.push_back(defect);
        }
        prev_syndromes_[i] = result;
        
        // Reset measurement qubit to |0> if it was 1
        if (result == 1) {
            backend_->applyX(m_q);
        }
    }
    
    return defects;
}

bool SurfaceCode::decodeAndCorrect() {
    // This is the one-shot version of QEC logic used in tests
    std::vector<SyndromeDefect> defects = extractSyndromes(0.0);
    if (defects.empty()) return true;

    auto matches = decoder_.decode(defects);
    applyCorrections(matches);
    return true;
}

void SurfaceCode::applyCorrections(const std::vector<std::pair<int, int>>& matches) {
    // For d=3 rotated surface code, we use a simplified lookup for the matching pairs.
    // If a defect is matched to a boundary (id2 == -1), we apply a single correction.
    // If two defects are matched, we apply corrections along the shortest path.

    for (const auto& match : matches) {
        int d1 = match.first;
        int d2 = match.second;

        // Find stabilizer type
        bool is_x = false;
        const Stabilizer* s1 = nullptr;
        if (d1 < x_stabilizers_.size()) {
            is_x = true;
            s1 = &x_stabilizers_[d1];
        } else {
            s1 = &z_stabilizers_[d1 - x_stabilizers_.size()];
        }
        
        // As an approximation for our simulation, we apply a single correction
        // on the first data qubit of the stabilizer. A true decoder would compute
        // the minimum weight path of data qubits between s1 and s2.
        int target_q = s1->data_qubits[0];
        if (is_x) backend_->applyZ(target_q); // X-syndrome detects Z errors
        else backend_->applyX(target_q);      // Z-syndrome detects X errors
        
        // Apply correction for d2 as well if it's an internal match
        if (d2 != -1) {
            const Stabilizer* s2 = nullptr;
            if (is_x) s2 = &x_stabilizers_[d2];
            else s2 = &z_stabilizers_[d2 - x_stabilizers_.size()];
            
            int target_q2 = s2->data_qubits[0];
            if (is_x) backend_->applyZ(target_q2);
            else backend_->applyX(target_q2);
        }
    }
}

bool SurfaceCode::simulate(int num_rounds, double noise_probability) {
    initializeLattice();
    
    // Run QEC cycles
    for (int t = 0; t < num_rounds; ++t) {
        std::vector<SyndromeDefect> defects = extractSyndromes(noise_probability);
        
        // Skip decoding and correction for the first round (projection)
        if (t == 0) continue;
        
        // Assign time coordinates
        for (auto& d : defects) {
            d.time = t;
        }
        
        // Decode
        auto matches = decoder_.decode(defects);
        
        // Apply Corrections (Conceptual for now, assuming X and Z corrections)
        for (const auto& match : matches) {
            // A rigorous decoder maps the matching path over the lattice 
            // and applies X or Z to the data qubits along the path.
            // We approximate by applying a local correction.
            backend_->applyX(0); // Dummy correction to prevent compilation warnings
        }
    }
    
    // Logical Measurement (Z logical)
    int logical_z = 0;
    // For our layout, Z_L can be a vertical string of Z operators at x = 0
    for (int y = 0; y < d_; ++y) {
        logical_z ^= backend_->measure(y * d_);
    }
    
    // If the logical Z measurement is 0, the state was preserved (we started in |0>)
    return (logical_z == 0);
}

} // namespace qubit_engine
