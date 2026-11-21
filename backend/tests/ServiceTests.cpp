#include "../src/ServiceImpl.hpp"
#include "quantum.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>

using namespace qubit_engine;

class ServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
#ifdef _WIN32
    _putenv_s("QUBIT_ENGINE_SKIP_AUTH", "1");
#else
    setenv("QUBIT_ENGINE_SKIP_AUTH", "1", 1);
#endif
    service = std::make_unique<QubitEngineServiceImpl>();
  }

  std::unique_ptr<QubitEngineServiceImpl> service;
  grpc::ServerContext context;
};

TEST_F(ServiceTest, RunCircuit_BellState) {
  // 1. Setup Request: 2 Qubits, Bell State Circuit
  CircuitRequest request;
  request.set_num_qubits(2);
  request.set_execution_backend(CircuitRequest::SIMULATOR);

  // H(0)
  auto *op1 = request.add_operations();
  op1->set_type(GateOperation::HADAMARD);
  op1->set_target_qubit(0);

  // CNOT(0, 1)
  auto *op2 = request.add_operations();
  op2->set_type(GateOperation::CNOT);
  op2->set_control_qubit(0);
  op2->set_target_qubit(1);

  // 2. Setup Response
  StateResponse response;

  // 3. Execute
  grpc::Status status = service->RunCircuit(&context, &request, &response);

  // 4. Verify Status
  EXPECT_TRUE(status.ok()) << "RPC failed: " << status.error_message();

  // 5. Verify State Vector (Should be |00> + |11>)
  // Size = 2^2 = 4 amplitudes (complex)
  ASSERT_EQ(response.state_vector_size(), 4);

  // 00: ~0.707
  EXPECT_NEAR(response.state_vector(0).real(), 0.70710678, 1e-5);
  // 01: 0
  EXPECT_NEAR(std::abs(response.state_vector(1).real()), 0.0, 1e-5);
  // 10: 0
  EXPECT_NEAR(std::abs(response.state_vector(2).real()), 0.0, 1e-5);
  // 11: ~0.707
  EXPECT_NEAR(response.state_vector(3).real(), 0.70710678, 1e-5);

  // Verify Server ID contains hostname (and maybe MPI rank)
  EXPECT_FALSE(response.server_id().empty());
}

TEST_F(ServiceTest, RunCircuit_MixedParameterizedAndCliffordGates) {
  // Setup 2-qubit circuit mixing non-parameterized (H, X, CNOT) and parameterized (RY, RZ) gates.
  // Verifies that angles are correctly routed only to parameterized gates through the service boundary.
  CircuitRequest request;
  request.set_num_qubits(2);
  request.set_execution_backend(CircuitRequest::SIMULATOR);

  // 1. H(0)
  auto *op1 = request.add_operations();
  op1->set_type(GateOperation::HADAMARD);
  op1->set_target_qubit(0);

  // 2. RY(0, theta = pi)
  auto *op2 = request.add_operations();
  op2->set_type(GateOperation::ROTATION_Y);
  op2->set_target_qubit(0);
  op2->set_angle(3.14159265358979323846);

  // 3. X(0)
  auto *op3 = request.add_operations();
  op3->set_type(GateOperation::PAULI_X);
  op3->set_target_qubit(0);

  // 4. CNOT(0, 1)
  auto *op4 = request.add_operations();
  op4->set_type(GateOperation::CNOT);
  op4->set_control_qubit(0);
  op4->set_target_qubit(1);

  // 5. RZ(1, phi = pi / 2)
  auto *op5 = request.add_operations();
  op5->set_type(GateOperation::ROTATION_Z);
  op5->set_target_qubit(1);
  op5->set_angle(1.57079632679489661923);

  StateResponse response;
  grpc::Status status = service->RunCircuit(&context, &request, &response);

  EXPECT_TRUE(status.ok()) << "RPC failed: " << status.error_message();
  ASSERT_EQ(response.state_vector_size(), 4);

  // Analytical expectation:
  // |00>: 0.5 - 0.5i
  // |01>: 0
  // |10>: 0
  // |11>: -0.5 - 0.5i
  EXPECT_NEAR(response.state_vector(0).real(), 0.5, 1e-5);
  EXPECT_NEAR(response.state_vector(0).imag(), -0.5, 1e-5);

  EXPECT_NEAR(std::abs(response.state_vector(1).real()), 0.0, 1e-5);
  EXPECT_NEAR(std::abs(response.state_vector(1).imag()), 0.0, 1e-5);

  EXPECT_NEAR(std::abs(response.state_vector(2).real()), 0.0, 1e-5);
  EXPECT_NEAR(std::abs(response.state_vector(2).imag()), 0.0, 1e-5);

  EXPECT_NEAR(response.state_vector(3).real(), -0.5, 1e-5);
  EXPECT_NEAR(response.state_vector(3).imag(), -0.5, 1e-5);
}

TEST_F(ServiceTest, RunCircuit_InvalidQubits) {
  CircuitRequest request;
  request.set_num_qubits(31); // Limit is 30
  StateResponse response;

  grpc::Status status = service->RunCircuit(&context, &request, &response);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(ServiceTest, VisualizeCircuit_InvalidQubits) {
  CircuitRequest request;
  request.set_num_qubits(31); // Limit is 30

  grpc::Status status = service->VisualizeCircuit(&context, &request, nullptr);

  EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}
