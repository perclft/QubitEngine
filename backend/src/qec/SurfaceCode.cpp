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
    
    for (int y = 0; y <= d_; ++y) {
        for (int x = 0; x <= d_; ++x) {
            std::vector<int> data_qubits;
            auto add_if_valid = [&](int qx, int qy) {
                if (qx >= 0 && qx < d_ && qy >= 0 && qy < d_) {
                    data_qubits.push_back(qy * d_ + qx);
                }
            };
            add_if_valid(x - 1, y - 1);
            add_if_valid(x, y - 1);
            add_if_valid(x - 1, y);
            add_if_valid(x, y);
            
            if (data_qubits.size() == 2 || data_qubits.size() == 4) {
                bool is_x = ((x + y) % 2 == 0);
                
                if (data_qubits.size() == 2) {
                    bool on_top_or_bottom = (y == 0 || y == d_);
                    bool on_left_or_right = (x == 0 || x == d_);
                    
                    if (is_x && on_left_or_right) continue; // Drop X on Left/Right
                    if (!is_x && on_top_or_bottom) continue; // Drop Z on Top/Bottom
                }
                
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
            defect.id = i; // The index of the measurement qubit in the full stabilizer list
            defect.type = (i < x_stabilizers_.size()) ? 0 : 1;
            defect.x = stab->x; 
            defect.y = stab->y;
            defect.time = 0; // Time is assigned by the caller
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
    for (const auto& match : matches) {
        int d1 = match.first;
        int d2 = match.second;

        bool is_x = false;
        const Stabilizer* s1 = nullptr;
        if (d1 < x_stabilizers_.size()) {
            is_x = true;
            s1 = &x_stabilizers_[d1];
        } else {
            s1 = &z_stabilizers_[d1 - x_stabilizers_.size()];
        }
        
        // Find path from s1 to s2 (or boundary if d2 == -1)
        int curr_x = s1->x;
        int curr_y = s1->y;
        
        int target_x = curr_x;
        int target_y = curr_y;
        
        if (d2 == -1) {
            if (is_x) {
                // Match to X boundary (x=0 or x=d_)
                target_x = (curr_x < d_ - curr_x) ? 0 : d_;
                while (curr_x != target_x) {
                    int dx = (target_x > curr_x) ? 1 : -1;
                    int dy = 1; // arbitrary
                    int qx = curr_x + (dx > 0 ? 0 : -1);
                    int qy = curr_y + (dy > 0 ? 0 : -1);
                    if (qx >= 0 && qx < d_ && qy >= 0 && qy < d_) backend_->applyZ(qubitIndex(qx, qy));
                    curr_x += dx;
                    curr_y += dy;
                }
            } else {
                // Match to Z boundary (y=0 or y=d_)
                target_y = (curr_y < d_ - curr_y) ? 0 : d_;
                while (curr_y != target_y) {
                    int dx = 1; // arbitrary
                    int dy = (target_y > curr_y) ? 1 : -1;
                    int qx = curr_x + (dx > 0 ? 0 : -1);
                    int qy = curr_y + (dy > 0 ? 0 : -1);
                    if (qx >= 0 && qx < d_ && qy >= 0 && qy < d_) backend_->applyX(qubitIndex(qx, qy));
                    curr_x += dx;
                    curr_y += dy;
                }
            }
        } else {
            const Stabilizer* s2 = nullptr;
            if (is_x) s2 = &x_stabilizers_[d2];
            else s2 = &z_stabilizers_[d2 - x_stabilizers_.size()];
            target_x = s2->x;
            target_y = s2->y;
            
            while (curr_x != target_x || curr_y != target_y) {
                int dx = (target_x > curr_x) ? 1 : ((target_x < curr_x) ? -1 : 0);
                int dy = (target_y > curr_y) ? 1 : ((target_y < curr_y) ? -1 : 0);
                if (dx == 0) dx = 1; 
                if (dy == 0) dy = 1;
                
                int qx = curr_x + (dx > 0 ? 0 : -1);
                int qy = curr_y + (dy > 0 ? 0 : -1);
                
                if (qx >= 0 && qx < d_ && qy >= 0 && qy < d_) {
                    if (is_x) backend_->applyZ(qubitIndex(qx, qy));
                    else backend_->applyX(qubitIndex(qx, qy));
                }
                
                curr_x += dx;
                curr_y += dy;
            }
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
        
        // Apply Corrections
        applyCorrections(matches);
    }
    
    // Logical Measurement (Z logical)
    int logical_z = 0;
    // Z_L is a horizontal string of Z operators at y = 0 connecting the Z boundaries (Left/Right)
    for (int x = 0; x < d_; ++x) {
        logical_z ^= backend_->measure(x);
    }
    
    // If the logical Z measurement is 0, the state was preserved (we started in |0>)
    return (logical_z == 0);
}

} // namespace qubit_engine
