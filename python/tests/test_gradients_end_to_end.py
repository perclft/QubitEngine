"""
End-to-end integration tests for Python -> pybind11 -> C++ VQE gradient computation.
Tests both parameter-shift and adjoint differentiation against finite differences,
and validates Phase S/T gate inversion (regression test for NEW-01).
"""

import math
import pytest
import qubit_engine
from qubit_engine import (
    QuantumRegister,
    calculate_gradients,
    calculate_gradients_adjoint,
)


def compute_finite_difference(num_qubits, params, ansatz_func, hamiltonian, eps=1e-5):
    """Computes gradients via central finite difference for verification."""
    grads = []
    for i in range(len(params)):
        params_plus = list(params)
        params_minus = list(params)
        params_plus[i] += eps
        params_minus[i] -= eps

        # E(theta + eps)
        q_plus = QuantumRegister(num_qubits, True)
        ansatz_func(params_plus, q_plus)
        e_plus = sum(coeff * q_plus.expectationValue(pauli) for coeff, pauli in hamiltonian)

        # E(theta - eps)
        q_minus = QuantumRegister(num_qubits, True)
        ansatz_func(params_minus, q_minus)
        e_minus = sum(coeff * q_minus.expectationValue(pauli) for coeff, pauli in hamiltonian)

        grads.append((e_plus - e_minus) / (2.0 * eps))
    return grads


def test_single_qubit_gradients_vs_finite_difference():
    """Verify parameter-shift and adjoint methods against analytical and finite-difference gradients."""
    num_qubits = 1
    theta = math.pi / 3.0  # 60 degrees
    params = [theta]

    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])

    hamiltonian = [(1.0, "Z")]

    expected_analytical = -math.sin(theta)
    fd_grads = compute_finite_difference(num_qubits, params, ansatz, hamiltonian)
    grad_param_shift = calculate_gradients(num_qubits, params, ansatz, hamiltonian)
    grad_adjoint = calculate_gradients_adjoint(num_qubits, params, ansatz, hamiltonian)

    assert abs(grad_param_shift[0] - expected_analytical) < 1e-5, f"Param-shift vs analytical mismatch: {grad_param_shift[0]} vs {expected_analytical}"
    assert abs(grad_adjoint[0] - expected_analytical) < 1e-5, f"Adjoint vs analytical mismatch: {grad_adjoint[0]} vs {expected_analytical}"
    assert abs(grad_param_shift[0] - fd_grads[0]) < 1e-4, f"Param-shift vs FD mismatch: {grad_param_shift[0]} vs {fd_grads[0]}"
    assert abs(grad_adjoint[0] - fd_grads[0]) < 1e-4, f"Adjoint vs FD mismatch: {grad_adjoint[0]} vs {fd_grads[0]}"


def test_multi_qubit_multi_param_gradients():
    """Verify 2-qubit parameterized circuit with entangling gates."""
    num_qubits = 2
    params = [0.45, 1.23]

    def ansatz(p, qreg):
        qreg.applyRotationY(0, p[0])
        qreg.applyCNOT(0, 1)
        qreg.applyRotationZ(1, p[1])

    hamiltonian = [(0.5, "ZZ"), (0.5, "XX")]

    fd_grads = compute_finite_difference(num_qubits, params, ansatz, hamiltonian)
    ps_grads = calculate_gradients(num_qubits, params, ansatz, hamiltonian)
    adj_grads = calculate_gradients_adjoint(num_qubits, params, ansatz, hamiltonian)

    for i in range(len(params)):
        assert abs(ps_grads[i] - fd_grads[i]) < 1e-4, f"Param-shift mismatch at {i}: {ps_grads[i]} vs {fd_grads[i]}"
        assert abs(adj_grads[i] - fd_grads[i]) < 1e-4, f"Adjoint mismatch at {i}: {adj_grads[i]} vs {fd_grads[i]}"
        assert abs(ps_grads[i] - adj_grads[i]) < 1e-5, f"Method divergence at {i}: {ps_grads[i]} vs {adj_grads[i]}"


def test_phase_s_and_t_gate_inversion_regression_new01():
    """
    Regression test for Finding NEW-01:
    Adjoint backward pass must invert S and T gates properly.
    Prior to NEW-01, S and T were not inverted during backward pass, causing silent gradient corruption.
    """
    num_qubits = 1
    theta = 0.785398  # pi / 4
    params = [theta]

    # Circuit 1: Ry(theta) -> S with observable X
    def ansatz_s(p, qreg):
        qreg.applyRotationY(0, p[0])
        qreg.applyPhaseS(0)

    ham_x = [(1.0, "X")]
    fd_s = compute_finite_difference(num_qubits, params, ansatz_s, ham_x)
    ps_s = calculate_gradients(num_qubits, params, ansatz_s, ham_x)
    adj_s = calculate_gradients_adjoint(num_qubits, params, ansatz_s, ham_x)

    assert abs(adj_s[0] - fd_s[0]) < 1e-4, f"Adjoint S-gate failed inversion: adj={adj_s[0]} fd={fd_s[0]}"
    assert abs(adj_s[0] - ps_s[0]) < 1e-4, f"Adjoint S-gate diverges from param shift: adj={adj_s[0]} ps={ps_s[0]}"

    # Circuit 2: Ry(theta) -> T with observable X
    def ansatz_t(p, qreg):
        qreg.applyRotationY(0, p[0])
        qreg.applyPhaseT(0)

    fd_t = compute_finite_difference(num_qubits, params, ansatz_t, ham_x)
    ps_t = calculate_gradients(num_qubits, params, ansatz_t, ham_x)
    adj_t = calculate_gradients_adjoint(num_qubits, params, ansatz_t, ham_x)

    assert abs(adj_t[0] - fd_t[0]) < 1e-4, f"Adjoint T-gate failed inversion: adj={adj_t[0]} fd={fd_t[0]}"
    assert abs(adj_t[0] - ps_t[0]) < 1e-4, f"Adjoint T-gate diverges from param shift: adj={adj_t[0]} ps={ps_t[0]}"

    # Circuit 3: Ry(theta) -> S -> T with observable Z
    def ansatz_st(p, qreg):
        qreg.applyRotationY(0, p[0])
        qreg.applyPhaseS(0)
        qreg.applyPhaseT(0)

    ham_z = [(1.0, "Z")]
    fd_st = compute_finite_difference(num_qubits, params, ansatz_st, ham_z)
    ps_st = calculate_gradients(num_qubits, params, ansatz_st, ham_z)
    adj_st = calculate_gradients_adjoint(num_qubits, params, ansatz_st, ham_z)

    assert abs(adj_st[0] - fd_st[0]) < 1e-4, f"Adjoint S->T gates failed inversion: adj={adj_st[0]} fd={fd_st[0]}"
    assert abs(adj_st[0] - ps_st[0]) < 1e-4, f"Adjoint S->T diverges from param shift: adj={adj_st[0]} ps={ps_st[0]}"
