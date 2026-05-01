#include <metal_stdlib>
using namespace metal;

struct Complex {
    float real;
    float imag;
};

// Constant 1/sqrt(2)
constant float INV_SQRT_2 = 0.70710678118654752440f;
constant float PI = 3.14159265358979323846f;

// Kernel for Hadamard Gate
kernel void hadamard_kernel(device Complex* state [[buffer(0)]],
                            constant uint& stride [[buffer(1)]],
                            uint id [[thread_position_in_grid]]) {
    
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    
    uint k = j + stride;
    
    Complex a = state[j];
    Complex b = state[k];
    
    Complex res_a, res_b;
    
    // res_a = (a + b) * INV_SQRT_2
    res_a.real = (a.real + b.real) * INV_SQRT_2;
    res_a.imag = (a.imag + b.imag) * INV_SQRT_2;
    
    // res_b = (a - b) * INV_SQRT_2
    res_b.real = (a.real - b.real) * INV_SQRT_2;
    res_b.imag = (a.imag - b.imag) * INV_SQRT_2;
    
    state[j] = res_a;
    state[k] = res_b;
}

// Pauli-X Kernel
kernel void paulix_kernel(device Complex* state [[buffer(0)]],
                          constant uint& stride [[buffer(1)]],
                          uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;
    
    Complex temp = state[j];
    state[j] = state[k];
    state[k] = temp;
}

// Pauli-Y Kernel
kernel void pauliy_kernel(device Complex* state [[buffer(0)]],
                          constant uint& stride [[buffer(1)]],
                          uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;
    
    Complex a = state[j];
    Complex b = state[k];
    
    Complex res_a, res_b;
    
    // Y = [0 -i; i 0]
    // a' = -i*b = b.imag - i*b.real
    res_a.real = b.imag;
    res_a.imag = -b.real;
    
    // b' = i*a = -a.imag + i*a.real
    res_b.real = -a.imag;
    res_b.imag = a.real;
    
    state[j] = res_a;
    state[k] = res_b;
}

// Pauli-Z Kernel
kernel void pauliz_kernel(device Complex* state [[buffer(0)]],
                          constant uint& stride [[buffer(1)]],
                          uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    // uint j = 2 * group * stride + offset;
    uint k = 2 * group * stride + offset + stride;
    
    // Z = [1 0; 0 -1]
    // b' = -b
    Complex b = state[k];
    state[k].real = -b.real;
    state[k].imag = -b.imag;
}

// RX, RY, RZ Kernels
kernel void rx_kernel(device Complex* state [[buffer(0)]],
                      constant uint& stride [[buffer(1)]],
                      constant float& theta [[buffer(2)]],
                      uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;

    float half_theta = theta * 0.5;
    float c = cos(half_theta);
    float s = sin(half_theta);
    
    Complex a = state[j];
    Complex b = state[k];
    
    // RX = [c -is; -is c]
    Complex res_a;
    res_a.real = c * a.real + s * b.imag;
    res_a.imag = c * a.imag - s * b.real;
    
    Complex res_b;
    res_b.real = s * a.imag + c * b.real;
    res_b.imag = -s * a.real + c * b.imag;
    
    state[j] = res_a;
    state[k] = res_b;
}

kernel void ry_kernel(device Complex* state [[buffer(0)]],
                      constant uint& stride [[buffer(1)]],
                      constant float& theta [[buffer(2)]],
                      uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;

    float half_theta = theta * 0.5;
    float c = cos(half_theta);
    float s = sin(half_theta);
    
    Complex a = state[j];
    Complex b = state[k];
    
    // RY = [c -s; s c]
    Complex res_a;
    res_a.real = c * a.real - s * b.real;
    res_a.imag = c * a.imag - s * b.imag;
    
    Complex res_b;
    res_b.real = s * a.real + c * b.real;
    res_b.imag = s * a.imag + c * b.imag;
    
    state[j] = res_a;
    state[k] = res_b;
}

kernel void rz_kernel(device Complex* state [[buffer(0)]],
                      constant uint& stride [[buffer(1)]],
                      constant float& theta [[buffer(2)]],
                      uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;
    
    float half_theta = theta * 0.5;
    float c = cos(half_theta);
    float s = sin(half_theta);
    
    // RZ = [e^-it/2 0; 0 e^it/2]
    // e^-it/2 = c - is
    // e^it/2  = c + is
    
    Complex a = state[j];
    Complex b = state[k];
    
    Complex res_a;
    res_a.real = c * a.real + s * a.imag;
    res_a.imag = c * a.imag - s * a.real;
    
    Complex res_b;
    res_b.real = c * b.real - s * b.imag;
    res_b.imag = c * b.imag + s * b.real;
    
    state[j] = res_a;
    state[k] = res_b;
}

// CNOT Kernel
kernel void cnot_kernel(device Complex* state [[buffer(0)]],
                        constant uint& target_stride [[buffer(1)]],
                        constant uint& control_stride [[buffer(2)]],
                        uint id [[thread_position_in_grid]]) {
    // We treat 'target' as the stride for pairing
    uint group = id / target_stride;
    uint offset = id % target_stride;
    uint j = 2 * group * target_stride + offset;
    uint k = j + target_stride;
    
    // Check if Control Bit is Set
    // Since j and k differ only by target_bit, and target != control,
    // they share the same control bit value.
    bool control_active = (j & control_stride) != 0;
    
    if (control_active) {
        // Swap amplitudes j and k (effectively X on target)
        Complex temp = state[j];
        state[j] = state[k];
        state[k] = temp;
    }
}

// Toffoli (CCNOT) Kernel
kernel void toffoli_kernel(device Complex* state [[buffer(0)]],
                           constant uint& target_stride [[buffer(1)]],
                           constant uint& ctrl1_stride [[buffer(2)]],
                           constant uint& ctrl2_stride [[buffer(3)]],
                           uint id [[thread_position_in_grid]]) {
    uint group = id / target_stride;
    uint offset = id % target_stride;
    uint j = 2 * group * target_stride + offset;
    uint k = j + target_stride;
    
    if ((j & ctrl1_stride) && (j & ctrl2_stride)) {
        Complex temp = state[j];
        state[j] = state[k];
        state[k] = temp;
    }
}
// Phase S Kernel (Z rotation by PI/2)
// S = [1 0; 0 i]
kernel void phases_kernel(device Complex* state [[buffer(0)]],
                          constant uint& stride [[buffer(1)]],
                          uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    // uint j = 2 * group * stride + offset;
    uint k = 2 * group * stride + offset + stride;
    
    // b' = i*b = -b.imag + i*b.real
    Complex b = state[k];
    Complex res_b;
    res_b.real = -b.imag;
    res_b.imag = b.real;
    
    state[k] = res_b;
}

kernel void phaset_kernel(device Complex* state [[buffer(0)]],
                          constant uint& stride [[buffer(1)]],
                          uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    // uint j = 2 * group * stride + offset;
    uint k = 2 * group * stride + offset + stride;
    
    Complex b = state[k];
    
    // b' = b * (1/sqrt2 + i/sqrt2)
    // real = b.real * inv_sqrt2 - b.imag * inv_sqrt2
    // imag = b.real * inv_sqrt2 + b.imag * inv_sqrt2
    
    Complex res_b;
    res_b.real = (b.real - b.imag) * INV_SQRT_2;
    res_b.imag = (b.real + b.imag) * INV_SQRT_2;
    
    state[k] = res_b;
}

// Partial Reduction Kernel for Probability of |0> on a specific qubit
kernel void measure_prob0_kernel(device Complex* state [[buffer(0)]],
                                 constant uint& stride [[buffer(1)]],
                                 device float* partial_sums [[buffer(2)]],
                                 uint id [[thread_position_in_grid]],
                                 uint tid [[thread_position_in_threadgroup]],
                                 uint gid [[threadgroup_position_in_grid]],
                                 uint threads_per_group [[threads_per_threadgroup]],
                                 uint total_threads [[threads_per_grid]]) {
    threadgroup float local_sum[1024];

    float prob = 0.0f;
    if (id < total_threads) {
        if ((id & stride) == 0) {
            prob = state[id].real * state[id].real + state[id].imag * state[id].imag;
        }
    }
    local_sum[tid] = prob;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    // Reduction
    for (uint s = threads_per_group / 2; s > 0; s >>= 1) {
        if (tid < s) {
            local_sum[tid] += local_sum[tid + s];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (tid == 0) {
        partial_sums[gid] = local_sum[0];
    }
}

// Normalization and Projection Kernel
kernel void project_state_kernel(device Complex* state [[buffer(0)]],
                                 constant uint& stride [[buffer(1)]],
                                 constant uint& outcome [[buffer(2)]],
                                 constant float& norm [[buffer(3)]],
                                 uint id [[thread_position_in_grid]],
                                 uint total_threads [[threads_per_grid]]) {
    if (id < total_threads) {
        bool is_one = (id & stride) != 0;
        if ((outcome == 0 && is_one) || (outcome == 1 && !is_one)) {
            state[id].real = 0.0f;
            state[id].imag = 0.0f;
        } else {
            state[id].real /= norm;
            state[id].imag /= norm;
        }
    }
}

// Extract probabilities for all basis states
kernel void compute_probabilities_kernel(device Complex* state [[buffer(0)]],
                                         device float* probabilities [[buffer(1)]],
                                         uint id [[thread_position_in_grid]]) {
    Complex c = state[id];
    probabilities[id] = c.real * c.real + c.imag * c.imag;
}

// 1-Qubit Dense Unitary Kernel
kernel void dense_unitary_1q_kernel(device Complex* state [[buffer(0)]],
                                    constant uint& stride [[buffer(1)]],
                                    constant Complex* matrix [[buffer(2)]],
                                    uint id [[thread_position_in_grid]]) {
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    uint k = j + stride;

    Complex a = state[j];
    Complex b = state[k];

    // res = matrix * [a; b]
    Complex res_a, res_b;
    res_a.real = matrix[0].real * a.real - matrix[0].imag * a.imag + matrix[1].real * b.real - matrix[1].imag * b.imag;
    res_a.imag = matrix[0].real * a.imag + matrix[0].imag * a.real + matrix[1].real * b.imag + matrix[1].imag * b.real;
    
    res_b.real = matrix[2].real * a.real - matrix[2].imag * a.imag + matrix[3].real * b.real - matrix[3].imag * b.imag;
    res_b.imag = matrix[2].real * a.imag + matrix[2].imag * a.real + matrix[3].real * b.imag + matrix[3].imag * b.real;

    state[j] = res_a;
    state[k] = res_b;
}

// 2-Qubit Dense Unitary Kernel
kernel void dense_unitary_2q_kernel(device Complex* state [[buffer(0)]],
                                    constant uint& stride_low [[buffer(1)]],
                                    constant uint& stride_high [[buffer(2)]],
                                    constant Complex* matrix [[buffer(3)]],
                                    uint id [[thread_position_in_grid]]) {
    // id iterates dim/4
    // Logic to find i00, i01, i10, i11 similar to CPU version
    // For simplicity in this PR, we focus on 1q fusion first but providing the skeleton
    uint low_mask = (1 << stride_low) - 1;
    uint mid_mask = (1 << (stride_high - 1)) - 1;
    
    uint i = ((id >> (stride_high - 1)) << stride_high) |
             (((id & mid_mask) >> stride_low) << (stride_low + 1)) |
             (id & low_mask);

    uint i00 = i;
    uint i01 = i | (1 << stride_low);
    uint i10 = i | (1 << stride_high);
    uint i11 = i | (1 << stride_low) | (1 << stride_high);

    Complex v[4] = { state[i00], state[i01], state[i10], state[i11] };
    Complex res[4];

    for (int r = 0; r < 4; ++r) {
        res[r].real = 0; res[r].imag = 0;
        for (int c = 0; c < 4; ++c) {
            Complex m = matrix[r * 4 + c];
            res[r].real += m.real * v[c].real - m.imag * v[c].imag;
            res[r].imag += m.real * v[c].imag + m.imag * v[c].real;
        }
    }

    state[i00] = res[0];
    state[i01] = res[1];
    state[i10] = res[2];
    state[i11] = res[3];
}


// Kernel for diagonal expectation values (products of Z and I)
kernel void diagonal_expectation_kernel(device Complex* state [[buffer(0)]],
                                        constant uint64_t& z_mask [[buffer(1)]],
                                        device float* partial_sums [[buffer(2)]],
                                        constant uint& total_threads [[buffer(3)]],
                                        uint id [[thread_position_in_grid]],
                                        uint tid [[thread_position_in_threadgroup]],
                                        uint gid [[threadgroup_position_in_grid]],
                                        uint threads_per_group [[threads_per_threadgroup]]) {
    threadgroup float local_sum[1024];
    float val = 0.0f;
    if (id < total_threads) {
        float prob = state[id].real * state[id].real + state[id].imag * state[id].imag;
        // Use __builtin_popcountll for uint64_t
        if (__builtin_popcountll(id & z_mask) % 2) val = -prob;
        else val = prob;
    }
    local_sum[tid] = val;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    for (uint s = threads_per_group / 2; s > 0; s >>= 1) {
        if (tid < s) {
            local_sum[tid] += local_sum[tid + s];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (tid == 0) {
        partial_sums[gid] = local_sum[0];
    }
}

// Expectation Z Kernel (simple legacy or targeted)
kernel void expectation_z_kernel(device Complex* state [[buffer(0)]],
                                 constant uint& stride [[buffer(1)]],
                                 device float* partial_sums [[buffer(2)]],
                                 uint id [[thread_position_in_grid]],
                                 uint tid [[thread_position_in_threadgroup]],
                                 uint gid [[threadgroup_position_in_grid]],
                                 uint threads_per_group [[threads_per_threadgroup]],
                                 uint total_threads [[threads_per_grid]]) {
    threadgroup float local_sum[1024];
    float val = 0.0f;
    if (id < total_threads) {
        float prob = state[id].real * state[id].real + state[id].imag * state[id].imag;
        if (id & stride) val = -prob;
        else val = prob;
    }
    local_sum[tid] = val;
    threadgroup_barrier(mem_flags::mem_threadgroup);

    for (uint s = threads_per_group / 2; s > 0; s >>= 1) {
        if (tid < s) {
            local_sum[tid] += local_sum[tid + s];
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
    }
    
    if (tid == 0) {
        partial_sums[gid] = local_sum[0];
    }
}

// Native SWAP Kernel — directly exchanges |01⟩ ↔ |10⟩ amplitudes
// Eliminates the 3× CNOT dispatch overhead of the decomposed version.
// Thread count: dim/4 (one thread per group of 4 basis states)
kernel void swap_kernel(device Complex* state [[buffer(0)]],
                        constant uint& stride_low [[buffer(1)]],
                        constant uint& stride_high [[buffer(2)]],
                        uint id [[thread_position_in_grid]]) {
    uint low_mask = (1 << stride_low) - 1;
    uint mid_mask = (1 << (stride_high - 1)) - 1;

    uint i = ((id >> (stride_high - 1)) << stride_high) |
             (((id & mid_mask) >> stride_low) << (stride_low + 1)) |
             (id & low_mask);

    // Only swap the |01⟩ and |10⟩ basis states; |00⟩ and |11⟩ stay unchanged
    uint i01 = i | (1 << stride_low);
    uint i10 = i | (1 << stride_high);

    Complex temp = state[i01];
    state[i01] = state[i10];
    state[i10] = temp;
}

// Native CZ Kernel — applies phase flip to |11⟩ component
// Eliminates the H-CNOT-H decomposition overhead.
// Thread count: dim/4 (one thread per group of 4 basis states)
kernel void cz_kernel(device Complex* state [[buffer(0)]],
                      constant uint& stride_low [[buffer(1)]],
                      constant uint& stride_high [[buffer(2)]],
                      uint id [[thread_position_in_grid]]) {
    uint low_mask = (1 << stride_low) - 1;
    uint mid_mask = (1 << (stride_high - 1)) - 1;

    uint i = ((id >> (stride_high - 1)) << stride_high) |
             (((id & mid_mask) >> stride_low) << (stride_low + 1)) |
             (id & low_mask);

    // CZ: only negate the |11⟩ amplitude
    uint i11 = i | (1 << stride_low) | (1 << stride_high);
    state[i11].real = -state[i11].real;
    state[i11].imag = -state[i11].imag;
}

// 2-Qubit Kraus Channel Kernel
// Applies a 4×4 matrix to the (q1,q2) subspace and renormalizes.
// Thread count: dim/4
kernel void kraus_2q_kernel(device Complex* state [[buffer(0)]],
                            constant uint& stride_low [[buffer(1)]],
                            constant uint& stride_high [[buffer(2)]],
                            constant Complex* matrix [[buffer(3)]],
                            constant float& inv_norm [[buffer(4)]],
                            uint id [[thread_position_in_grid]]) {
    uint low_mask = (1 << stride_low) - 1;
    uint mid_mask = (1 << (stride_high - 1)) - 1;

    uint i = ((id >> (stride_high - 1)) << stride_high) |
             (((id & mid_mask) >> stride_low) << (stride_low + 1)) |
             (id & low_mask);

    uint i00 = i;
    uint i01 = i | (1 << stride_low);
    uint i10 = i | (1 << stride_high);
    uint i11 = i | (1 << stride_low) | (1 << stride_high);

    Complex v[4] = { state[i00], state[i01], state[i10], state[i11] };
    Complex res[4];

    for (int r = 0; r < 4; ++r) {
        res[r].real = 0; res[r].imag = 0;
        for (int c = 0; c < 4; ++c) {
            Complex m = matrix[r * 4 + c];
            res[r].real += m.real * v[c].real - m.imag * v[c].imag;
            res[r].imag += m.real * v[c].imag + m.imag * v[c].real;
        }
        res[r].real *= inv_norm;
        res[r].imag *= inv_norm;
    }

    state[i00] = res[0];
    state[i01] = res[1];
    state[i10] = res[2];
    state[i11] = res[3];
}
