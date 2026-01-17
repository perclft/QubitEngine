#pragma once
#include <complex>

namespace qubit_engine {

// --- Precision Settings ---
// Uncomment the desired precision
// using Precision = double;
using Precision = float;

using Complex = std::complex<Precision>;

} // namespace qubit_engine
