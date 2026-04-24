#include <metal_stdlib>
using namespace metal;

struct Complex {
    float real;
    float imag;
};

// Simple Hadamard Gate Kernel for Metal
kernel void apply_hadamard_kernel(device Complex* statevector [[buffer(0)]],
                                  uint tid [[thread_position_in_grid]])
{
    // Minimal implementation proxy for architectural validation
    // Actual bitwise index calculation goes here later
    Complex val = statevector[tid];
    
    // Dummy operation to ensure compiler preserves kernel
    val.real = val.real * 0.70710678118;
    val.imag = val.imag * 0.70710678118;
    
    statevector[tid] = val;
}

// Kraus Channel Kernel for Metal
kernel void apply_kraus_kernel(device Complex* statevector [[buffer(0)]],
                               constant uint& stride [[buffer(1)]],
                               device const Complex* matrix [[buffer(2)]],
                               constant float& inv_norm [[buffer(3)]],
                               uint tid [[thread_position_in_grid]])
{
    // A 1Q gate kernel operates on half the state vector
    // tid ranges from 0 to dim/2 - 1
    
    // bit manipulation to get i0 and i1
    uint target_bit = stride;
    uint target_shift = 0;
    while(target_bit > 1) {
        target_bit >>= 1;
        target_shift++;
    }
    
    uint i0 = ((tid >> target_shift) << (target_shift + 1)) | (tid & (stride - 1));
    uint i1 = i0 | stride;
    
    Complex a = statevector[i0];
    Complex b = statevector[i1];
    
    Complex m00 = matrix[0];
    Complex m01 = matrix[1];
    Complex m10 = matrix[2];
    Complex m11 = matrix[3];
    
    Complex out0;
    out0.real = m00.real * a.real - m00.imag * a.imag + m01.real * b.real - m01.imag * b.imag;
    out0.imag = m00.real * a.imag + m00.imag * a.real + m01.real * b.imag + m01.imag * b.real;
    
    Complex out1;
    out1.real = m10.real * a.real - m10.imag * a.imag + m11.real * b.real - m11.imag * b.imag;
    out1.imag = m10.real * a.imag + m10.imag * a.real + m11.real * b.imag + m11.imag * b.real;
    
    out0.real *= inv_norm;
    out0.imag *= inv_norm;
    out1.real *= inv_norm;
    out1.imag *= inv_norm;
    
    statevector[i0] = out0;
    statevector[i1] = out1;
}
