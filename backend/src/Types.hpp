#pragma once
#include <complex>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace qubit_engine {

// --- Precision Settings ---
// Uncomment the desired precision
using Precision = double;
// using Precision = float;

using Complex = std::complex<Precision>;

// 32-byte alignment for AVX2 cache-line optimization
struct alignas(32) AlignedComplex {
  Precision real;
  Precision imag;

  constexpr AlignedComplex() : real(0), imag(0) {}
  constexpr AlignedComplex(Precision r) : real(r), imag(0) {}
  constexpr AlignedComplex(Precision r, Precision i) : real(r), imag(i) {}
  constexpr AlignedComplex(const Complex &c) : real(c.real()), imag(c.imag()) {}
  constexpr operator Complex() const { return Complex(real, imag); }

  AlignedComplex operator*(const AlignedComplex &o) const {
    return AlignedComplex(Complex(*this) * Complex(o));
  }
  AlignedComplex operator+(const AlignedComplex &o) const {
    return AlignedComplex(Complex(*this) + Complex(o));
  }
  AlignedComplex operator-(const AlignedComplex &o) const {
    return AlignedComplex(Complex(*this) - Complex(o));
  }
};

} // namespace qubit_engine
