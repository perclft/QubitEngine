#include "QuantumRegister.hpp"
#include "MetalContext.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>

#ifdef MPI_ENABLED
#include <mpi.h>
#endif
// #include <immintrin.h>

const double INV_SQRT_2 = 1.0 / std::sqrt(2.0);

// --- Lifecycle ---
QuantumRegister::QuantumRegister(size_t n) : num_qubits(n) {
  local_rank = 0;
  world_size = 1;

#ifdef MPI_ENABLED
  int initialized;
  MPI_Initialized(&initialized);
  if (!initialized) {
    MPI_Init(NULL, NULL);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &local_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
#endif

  size_t total_dim = 1ULL << n;
  size_t local_dim = total_dim / world_size;
  if (local_dim == 0)
    local_dim = 1;

  state.resize(local_dim, 0.0);
  if (local_rank == 0)
    state[0] = 1.0;
}

QuantumRegister::~QuantumRegister() {}

// --- Core Gates ---

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

// --- Core Gates ---

void QuantumRegister::applyHadamard(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(__aarch64__)
    // NEON Optimization for Apple Silicon
    const float64x2_t inv_sqrt_2_vec = vmovq_n_f64(INV_SQRT_2);

#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; j += 2) {
        // Check if we can process 2 complex numbers (4 doubles) at once
        // State is vector<Complex>, so memory is [r0, i0, r1, i1, ...]
        // j points to Complex index.
        // 2 Complex numbers = 4 doubles.

        // We process 2 complex numbers at a time if stride >= 2
        // If stride == 1, we process 1 complex number (2 doubles) but need
        // careful handling.

        // Case A: Stride >= 2 (Inner loop processes contiguous blocks)
        if (stride >= 2 && (j + 2 <= i + stride)) {
          // Load a[j], a[j+1] -> 4 doubles
          float64_t *ptr_a = reinterpret_cast<float64_t *>(&state[j]);
          float64_t *ptr_b = reinterpret_cast<float64_t *>(&state[j + stride]);

          // Load 4 doubles (2 vectors of 2)
          float64x2_t a0 = vld1q_f64(ptr_a);     // r0, i0
          float64x2_t a1 = vld1q_f64(ptr_a + 2); // r1, i1
          float64x2_t b0 = vld1q_f64(ptr_b);     // r0', i0'
          float64x2_t b1 = vld1q_f64(ptr_b + 2); // r1', i1'

          // H|0> = (a+b)/rt2, H|1> = (a-b)/rt2
          float64x2_t sum0 = vaddq_f64(a0, b0);
          float64x2_t diff0 = vsubq_f64(a0, b0);
          float64x2_t sum1 = vaddq_f64(a1, b1);
          float64x2_t diff1 = vsubq_f64(a1, b1);

          // Store back
          vst1q_f64(ptr_a, vmulq_f64(sum0, inv_sqrt_2_vec));
          vst1q_f64(ptr_b, vmulq_f64(diff0, inv_sqrt_2_vec));
          vst1q_f64(ptr_a + 2, vmulq_f64(sum1, inv_sqrt_2_vec));
          vst1q_f64(ptr_b + 2, vmulq_f64(diff1, inv_sqrt_2_vec));
        } else {
          // Scalar fallback for boundary or small stride
          Complex a = state[j];
          Complex b = state[j + stride];
          state[j] = (a + b) * INV_SQRT_2;
          state[j + stride] = (a - b) * INV_SQRT_2;

          if (j + 1 < i + stride) {
            Complex a2 = state[j + 1];
            Complex b2 = state[j + 1 + stride];
            state[j + 1] = (a2 + b2) * INV_SQRT_2;
            state[j + 1 + stride] = (a2 - b2) * INV_SQRT_2;
          }
        }
      }
    }
#else
    // Local AVX-style loop (Existing)
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = (a + b) * INV_SQRT_2;
        state[j + stride] = (a - b) * INV_SQRT_2;
      }
    }
#endif
  }
}

void QuantumRegister::applyX(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  if (stride < local_dim) {
#if defined(__aarch64__)
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; j += 2) {
        if (stride >= 2 && (j + 2 <= i + stride)) {
          float64_t *ptr_a = reinterpret_cast<float64_t *>(&state[j]);
          float64_t *ptr_b = reinterpret_cast<float64_t *>(&state[j + stride]);

          // Load
          float64x2_t a0 = vld1q_f64(ptr_a);
          float64x2_t a1 = vld1q_f64(ptr_a + 2);
          float64x2_t b0 = vld1q_f64(ptr_b);
          float64x2_t b1 = vld1q_f64(ptr_b + 2);

          // Swap by storing b to a and a to b
          vst1q_f64(ptr_a, b0);
          vst1q_f64(ptr_a + 2, b1);
          vst1q_f64(ptr_b, a0);
          vst1q_f64(ptr_b + 2, a1);
        } else {
          std::swap(state[j], state[j + stride]);
          if (j + 1 < i + stride) {
            std::swap(state[j + 1], state[j + 1 + stride]);
          }
        }
      }
    }
#else
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        std::swap(state[j], state[j + stride]);
      }
    }
#endif
  }
}

// --- GPU Memory Control ---
void QuantumRegister::toGPU() {
  auto &metal = MetalContext::getInstance();
  if (!metal.isAvailable())
    metal.initialize();

  if (metal.isAvailable()) {
    metal.uploadState(reinterpret_cast<double *>(state.data()), num_qubits);
    on_gpu = true;
  }
}

void QuantumRegister::toCPU() {
  if (!on_gpu)
    return;
  auto &metal = MetalContext::getInstance();
  if (metal.isAvailable()) {
    metal.downloadState(reinterpret_cast<double *>(state.data()), num_qubits);
    on_gpu = false;
  }
}

void QuantumRegister::applyHadamardMetal(size_t target) {
  auto &metal = MetalContext::getInstance();
  if (!metal.isAvailable()) {
    metal.initialize();
  }

  if (metal.isAvailable()) {
    if (on_gpu) {
      // Fast Path: Zero-Copy (Resident)
      metal.runHadamard(num_qubits, target);
    } else {
      // Slow Path: Copy (One-shot)
      metal.runHadamard(state.data(), num_qubits, target);
    }
  } else {
    std::cerr << "Metal not available, using CPU." << std::endl;
    applyHadamard(target);
  }
}

// --- Core Gates ---

// --- Y and Z Gates ---

void QuantumRegister::applyY(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  if (stride < local_dim) {
#if defined(__aarch64__)
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; j += 2) {
        if (stride >= 2 && (j + 2 <= i + stride)) {
          float64_t *ptr_a = reinterpret_cast<float64_t *>(&state[j]);
          float64_t *ptr_b = reinterpret_cast<float64_t *>(&state[j + stride]);

          // Load 2 complex numbers from A and B
          // A = [ra0, ia0, ra1, ia1]
          // B = [rb0, ib0, rb1, ib1]
          float64x2_t a0 = vld1q_f64(ptr_a);
          float64x2_t a1 = vld1q_f64(ptr_a + 2);
          float64x2_t b0 = vld1q_f64(ptr_b);
          float64x2_t b1 = vld1q_f64(ptr_b + 2);

          // Apply Y:
          // State[j]   = -i * State[j+stride]
          // State[j+s] =  i * State[j]

          // -i * (r + qi) = -i*r + q = q - ir
          //  i * (r + qi) =  i*r - q = -q + ir

          // Permute B for -i multiplication: rb -> ib, ib -> rb
          // We need to swap real/imag parts.
          // NEON doesn't have a direct complex swap instruction for f64x2 (it
          // does for f32). We can interpret as uint64 to use vqtbl or just
          // manual shuffle if needed, but standard vtrn/vswp might work.
          // Actually, vcombine usually cleaner.

          // Let's rely on standard logic:
          // b0 = [rb0, ib0]
          // swapped_b0 = [ib0, rb0]
          float64x2_t swapped_b0 = vextq_f64(b0, b0, 1);
          float64x2_t swapped_b1 = vextq_f64(b1, b1, 1);

          // swapped_b0 = [ib0, rb0]
          // We want [ib0, -rb0]
          static const float64x2_t neg_imag = {1.0, -1.0};
          static const float64x2_t neg_real = {-1.0, 1.0};

          // Result A = -i * B = (ib - i*rb) = [ib, -rb]
          float64x2_t res_a0 = vmulq_f64(swapped_b0, neg_imag);
          float64x2_t res_a1 = vmulq_f64(swapped_b1, neg_imag);

          // A to be moved to B position:
          // Result B = i * A = (i*ra - ia) = [-ia, ra]
          float64x2_t swapped_a0 = vextq_f64(a0, a0, 1);
          float64x2_t swapped_a1 = vextq_f64(a1, a1, 1);

          float64x2_t res_b0 = vmulq_f64(swapped_a0, neg_real);
          float64x2_t res_b1 = vmulq_f64(swapped_a1, neg_real);

          vst1q_f64(ptr_a, res_a0);
          vst1q_f64(ptr_a + 2, res_a1);
          vst1q_f64(ptr_b, res_b0);
          vst1q_f64(ptr_b + 2, res_b1);
        } else {
          Complex i_unit(0, 1);
          Complex a = state[j];
          Complex b = state[j + stride];
          state[j] = -i_unit * b;
          state[j + stride] = i_unit * a;
          if (j + 1 < i + stride) {
            Complex a2 = state[j + 1];
            Complex b2 = state[j + 1 + stride];
            state[j + 1] = -i_unit * b2;
            state[j + 1 + stride] = i_unit * a2;
          }
        }
      }
    }
#else
    Complex i_unit(0, 1);
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];
        state[j] = -i_unit * b;
        state[j + stride] = i_unit * a;
      }
    }
#endif
  }
}

void QuantumRegister::applyZ(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  if (stride < local_dim) {
#if defined(__aarch64__)
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; j += 2) {
        if (stride >= 2 && (j + 2 <= i + stride)) {
          float64_t *ptr_b = reinterpret_cast<float64_t *>(&state[j + stride]);

          float64x2_t b0 = vld1q_f64(ptr_b);
          float64x2_t b1 = vld1q_f64(ptr_b + 2);

          // Z|1> = -|1>
          b0 = vnegq_f64(b0);
          b1 = vnegq_f64(b1);

          vst1q_f64(ptr_b, b0);
          vst1q_f64(ptr_b + 2, b1);
        } else {
          state[j + stride] = -state[j + stride];
          if (j + 1 < i + stride)
            state[j + 1 + stride] = -state[j + 1 + stride];
        }
      }
    }
#else
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] = -state[j + stride];
      }
    }
#endif
  }
}

// ... CNOT (Skipping manual NEON for now as it's complex/sparse) ...

void QuantumRegister::applyCNOT(size_t control, size_t target) {
  size_t local_dim = state.size();

  // Determine # of local qubits
  // local_dim = 1 << n_local.  n_local = log2(local_dim).
  // Or just check if stride >= local_dim.

  if (control == target) {
    throw std::invalid_argument("Control and target must be distinct");
  }

  size_t c_stride = 1ULL << control;
  size_t t_stride = 1ULL << target;

  bool c_is_global = (c_stride >= local_dim);
  bool t_is_global = (t_stride >= local_dim);

  if (!c_is_global && !t_is_global) {
// --- Case 1: Purely Local ---
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; ++i) {
      if ((i & c_stride)) {
        size_t partner = i ^ t_stride;
        // Optimization: avoid double swap by iterating only lower index
        if (i < partner) {
          std::swap(state[i], state[partner]);
        }
      }
    }
    return;
  }

#ifdef MPI_ENABLED
  // --- Distributed Logic ---
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  // Global Qubit Index relative to "Rank Bits"
  // rank_bit = qubit_index - log2(local_dim)
  // Actually, stride/local_dim gives the bit position in rank?

  // Case 2: Control Is Global
  if (c_is_global) {
    // Does my rank have Control=1?
    // Bit in rank is (c_stride / local_dim).
    size_t rank_c_bit = c_stride / local_dim;
    bool control_set = (rank & rank_c_bit);

    if (control_set) {
      // Control is 1 for ALL local amplitudes.
      // We must apply X to target.
      if (t_is_global) {
        // Target is global -> Swap with partner rank
        size_t rank_t_bit = t_stride / local_dim;
        int partner = rank ^ rank_t_bit;

        std::vector<Complex> recv_buf(local_dim);
        MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                     recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        state = recv_buf; // X Gate on global qubit = Swap Ranks
      } else {
        // Target is local -> Apply local X
        applyX(target);
      }
    }
    // If control is 0, do nothing.
    return;
  }

  // Case 3: Target Is Global (and Control is Local)
  if (t_is_global) {
    // We need to exchange state with partner rank
    // and swap ONLY where local control is 1.
    size_t rank_t_bit = t_stride / local_dim;
    int partner = rank ^ rank_t_bit;

    std::vector<Complex> recv_buf(local_dim);

    // Exchange full states
    MPI_Sendrecv(state.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 recv_buf.data(), local_dim * 2, MPI_DOUBLE, partner, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

// Conditional Swap
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; ++i) {
      if (i & c_stride) {
        // Control is 1 -> Swap happened (take partner's data)
        state[i] = recv_buf[i];
      } else {
        // Control is 0 -> No swap happened (keep my data)
        // state[i] = state[i];
      }
    }
  }
#else
  if (c_is_global || t_is_global) {
    std::cerr << "Error: Global CNOT requested but MPI not enabled."
              << std::endl;
  }
#endif
}

// --- Advanced Gates ---

void QuantumRegister::applyToffoli(size_t c1, size_t c2, size_t t) {
  size_t local_dim = state.size();
  size_t c1_s = 1ULL << c1;
  size_t c2_s = 1ULL << c2;
  size_t t_s = 1ULL << t;

  if (t_s < local_dim) { // Assuming local for MVP
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; ++i) {
      if ((i & c1_s) && (i & c2_s) && !(i & t_s)) {
        std::swap(state[i], state[i + t_s]);
      }
    }
  }
}

// --- Phase & Rotation Gates ---

void QuantumRegister::applyPhaseS(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  Complex i_unit(0, 1);

  if (stride < local_dim) {
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= i_unit;
      }
    }
  }
}

void QuantumRegister::applyPhaseT(size_t target) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;
  Complex phase(1.0 / std::sqrt(2.0), 1.0 / std::sqrt(2.0)); // exp(i*pi/4)

  if (stride < local_dim) {
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j + stride] *= phase;
      }
    }
  }
}

void QuantumRegister::applyRotationY(size_t target, double angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  double c = std::cos(angle / 2.0);
  double s = std::sin(angle / 2.0);

  if (stride < local_dim) {
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        Complex a = state[j];
        Complex b = state[j + stride];

        // Ry = [[c, -s], [s, c]]
        state[j] = c * a - s * b;
        state[j + stride] = s * a + c * b;
      }
    }
  }
}

void QuantumRegister::applyRotationZ(size_t target, double angle) {
  size_t local_dim = state.size();
  size_t stride = 1ULL << target;

  // Rz = [[exp(-i*t/2), 0], [0, exp(i*t/2)]]
  Complex z0(std::cos(-angle / 2.0), std::sin(-angle / 2.0));
  Complex z1(std::cos(angle / 2.0), std::sin(angle / 2.0));

  if (stride < local_dim) {
#if defined(__aarch64__)
    double cr0 = z0.real();
    double ci0 = z0.imag();
    double cr1 = z1.real();
    double ci1 = z1.imag();

    // Create vectors of constants
    float64x2_t v_cr0 = vmovq_n_f64(cr0);
    float64x2_t v_ci0 = vmovq_n_f64(ci0);
    float64x2_t v_cr1 = vmovq_n_f64(cr1);
    float64x2_t v_ci1 = vmovq_n_f64(ci1);

    // Helpers for complex multiplication (a+bi)*(c+di) = (ac-bd) + (ad+bc)i
    // We need logic to handle the "sub" and "add" parts.

    // Strategy:
    // Load a = [r, i]
    // result_real = r*cr - i*ci
    // result_imag = r*ci + i*cr

    // Using NEON:
    // a = [r, i]
    // a_swap = [i, r] (using vextq_f64)

    // term1 = a * [cr, cr] = [r*cr, i*cr]
    // term2 = a_swap * [ci, ci] = [i*ci, r*ci]

    // result = [r*cr - i*ci, i*cr + r*ci]
    //        = term1 +/- term2
    // NEON has vsubq / vaddq. We need specific usage.
    // Real part: sub. Imag part: add.
    // Can we do this in one vector op?
    // Maybe vmulq then some shuffle?
    // No, standard is:
    // res_r = r*cr - i*ci
    // res_i = i*cr + r*ci

    // Let's implement this logic cleanly.

#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; j += 2) {
        if (stride >= 2 && (j + 2 <= i + stride)) {
          // --- Process |0> block (multiply by z0) ---
          float64_t *ptr_a = reinterpret_cast<float64_t *>(&state[j]);
          float64x2_t a0 = vld1q_f64(ptr_a);
          float64x2_t a1 = vld1q_f64(ptr_a + 2);

          // Calc a0 * z0
          float64x2_t a0_swap = vextq_f64(a0, a0, 1); // [i0, r0]
          float64x2_t t1 = vmulq_f64(a0, v_cr0);      // [r0*cr, i0*cr]
          float64x2_t t2 = vmulq_f64(a0_swap, v_ci0); // [i0*ci, r0*ci]

          // We want [r0*cr - i0*ci, i0*cr + r0*ci]
          // t1 = [A, B], t2 = [C, D]
          // Want [A-C, B+D]
          // Can use vsubq for real, vaddq for imag? No, element-wise.
          // We need a vector mask { -1, 1 } to multiply t2?
          // t2_masked = t2 * [1, -1] ? Wait.
          // A - C = r*cr - i*ci. Correct.
          // B + D = i*cr + r*ci. Correct.
          // So we want t1 + (t2 * [-1, 1])?
          // No, A - C means we subtract the first element.
          // B + D means we add the second.
          // Let's multiply t2 by {-1, 1} then add.
          // {-1*C, 1*D} = {-i*ci, r*ci}.
          // t1 + t2' = [r*cr - i*ci, i*cr + r*ci]. Matches!

          static const float64x2_t mask_neg_real = {-1.0, 1.0};
          t2 = vmulq_f64(t2, mask_neg_real);
          a0 = vaddq_f64(t1, t2);
          vst1q_f64(ptr_a, a0);

          // Calc a1 * z0
          float64x2_t a1_swap = vextq_f64(a1, a1, 1);
          t1 = vmulq_f64(a1, v_cr0);
          t2 = vmulq_f64(a1_swap, v_ci0);
          t2 = vmulq_f64(t2, mask_neg_real);
          a1 = vaddq_f64(t1, t2);
          vst1q_f64(ptr_a + 2, a1);

          // --- Process |1> block (multiply by z1) ---
          float64_t *ptr_b = reinterpret_cast<float64_t *>(&state[j + stride]);
          float64x2_t b0 = vld1q_f64(ptr_b);
          float64x2_t b1 = vld1q_f64(ptr_b + 2);

          float64x2_t b0_swap = vextq_f64(b0, b0, 1);
          t1 = vmulq_f64(b0, v_cr1);
          t2 = vmulq_f64(b0_swap, v_ci1);
          t2 = vmulq_f64(t2, mask_neg_real);
          b0 = vaddq_f64(t1, t2);
          vst1q_f64(ptr_b, b0);

          float64x2_t b1_swap = vextq_f64(b1, b1, 1);
          t1 = vmulq_f64(b1, v_cr1);
          t2 = vmulq_f64(b1_swap, v_ci1);
          t2 = vmulq_f64(t2, mask_neg_real);
          b1 = vaddq_f64(t1, t2);
          vst1q_f64(ptr_b + 2, b1);

        } else {
          // Scalar fallback
          state[j] *= z0;
          state[j + stride] *= z1;
          if (j + 1 < i + stride) {
            state[j + 1] *= z0;
            state[j + 1 + stride] *= z1;
          }
        }
      }
    }
#else
#pragma omp parallel for
    for (size_t i = 0; i < local_dim; i += 2 * stride) {
      for (size_t j = i; j < i + stride; ++j) {
        state[j] *= z0;
        state[j + stride] *= z1;
      }
    }
#endif
  }
}

// --- Fix: Noise Implementation ---
void QuantumRegister::applyDepolarizingNoise(double probability) {
  // Stochastic Noise Model
  // For each qubit, apply random Pauli error with probability p
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  for (size_t i = 0; i < num_qubits; ++i) {
    if (dis(gen) < probability) {
      // Error occurred!
      // Depolarizing channel: X, Y, or Z with equal prob (p/3)
      double type = dis(gen);
      if (type < 0.333)
        applyX(i);
      else if (type < 0.666)
        applyY(i);
      else
        applyZ(i);
    }
  }
}

// --- Measurement & VQE ---

int QuantumRegister::measure(size_t target) {
  double prob0 = 0.0;
  size_t stride = 1ULL << target;
  for (size_t i = 0; i < state.size(); ++i) {
    if (!(i & stride))
      prob0 += std::norm(state[i]);
  }

  // Thread-safe RNG for measurement
  thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_real_distribution<> dis(0.0, 1.0);
  int outcome = (dis(gen) > prob0) ? 1 : 0;

  // Collapse State (Projective)
  double norm = 0.0;
  if (outcome == 0) {
    for (size_t i = 0; i < state.size(); ++i) {
      if (i & stride)
        state[i] = 0.0;
      else
        norm += std::norm(state[i]);
    }
  } else {
    for (size_t i = 0; i < state.size(); ++i) {
      if (!(i & stride))
        state[i] = 0.0;
      else
        norm += std::norm(state[i]);
    }
  }
  // Normalize
  norm = std::sqrt(norm);
  if (norm > 1e-9) {
    for (auto &val : state)
      val /= norm;
  }

  return outcome;
}

double QuantumRegister::expectationValue(const std::string &pauli_string) {
  // Calculates <psi | P | psi>
  // Assumes pauli_string length == num_qubits (or pads with I)
  // Supports 'I', 'Z'. 'X'/'Y' require basis rotation (Todo for Phase 28).

  double expected_value = 0.0;

  // This loop is O(2^N), can be slow. Parallelize?
  // #pragma omp parallel for reduction(+:expected_value)
  for (size_t i = 0; i < state.size(); ++i) {
    double prob = std::norm(state[i]);
    if (prob < 1e-15)
      continue;

    // Determine eigenvalue for basis state |i>
    // Eigenvalue is product of eigenvalues of each qubit
    // Z|0> = +1, Z|1> = -1.
    // Parity of 1s at Z positions determine sign.

    int sign = 1;
    for (size_t q = 0; q < num_qubits && q < pauli_string.size(); ++q) {
      char op = pauli_string[q];
      // String index 0 usually qubit 0? Or N-1?
      // "Z0 Z1 Z2..." typically string index corresponds to qubit index.

      if (op == 'Z') {
        if ((i >> q) & 1)
          sign *= -1;
      } else if (op == 'X' || op == 'Y') {
        // Warn: Approximated as Z for Alpha (or throw?)
        // Let's treat as Z for stability of Alpha,
        // but ideally we should rotate basis previously.
      }
    }
    expected_value += prob * sign;
  }
  return expected_value;
}

std::vector<Complex> QuantumRegister::getStateVector() const { return state; }

std::vector<double> QuantumRegister::getProbabilities() { return {}; }
