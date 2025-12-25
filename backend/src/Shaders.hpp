#pragma once

const char *METAL_SHADERS_SOURCE = R"(
#include <metal_stdlib>
using namespace metal;

struct Complex {
    float real;
    float imag;
};

constant float INV_SQRT_2 = 0.70710678;

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
    
    res_a.real = (a.real + b.real) * INV_SQRT_2;
    res_a.imag = (a.imag + b.imag) * INV_SQRT_2;
    
    res_b.real = (a.real - b.real) * INV_SQRT_2;
    res_b.imag = (a.imag - b.imag) * INV_SQRT_2;
    
    state[j] = res_a;
    state[k] = res_b;
}

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
)";
