#include <metal_stdlib>
using namespace metal;

struct Complex {
    double real;
    double imag;
};

// Constant 1/sqrt(2)
constant double INV_SQRT_2 = 0.70710678118654752440;

// Kernel for Hadamard Gate
// Logic matches the CPU scalar logic but parallelized per pair (strided).
// Each thread handles a distinct index 'i' in the range [0, local_dim / 2).
// But mapping thread IDs to the strided access pattern is tricky.
// Global ID 'gid' can map to 'i' and 'j' in the loop.

// Simplest mapping: 
// 1 Thread = 1 Pair (state[j], state[j+stride])
// We need to map linear 'id' to the correct 'j' index.
// There are (N/2) pairs. 
// If stride is S. 
// Pairs are at indices:
// [0, S), [2S, 3S), [4S, 5S)...
// A generic way:
// group = id / S
// offset_in_group = id % S
// j = 2 * group * S + offset_in_group

kernel void hadamard_kernel(device Complex* state [[buffer(0)]],
                            constant uint& stride [[buffer(1)]],
                            uint id [[thread_position_in_grid]]) {
    
    // Calculate the 'j' index for this thread
    uint group = id / stride;
    uint offset = id % stride;
    uint j = 2 * group * stride + offset;
    
    // Check bounds (though grid size handles this usually)
    // state size is assumed to be known or implicitly valid based on dispatch
    
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
