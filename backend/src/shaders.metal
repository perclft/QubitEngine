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

// Expectation Z Kernel (for generic diagonal observables or simple Z reduction)
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
