grpc::Status QubitEngineServiceImpl::VisualizeCircuit(
    grpc::ServerContext *context, const CircuitRequest *request,
    grpc::ServerWriter<StateResponse> *writer) {
  std::cout << "[VisualizeCircuit] Received request for "
            << request->num_qubits() << " qubits." << std::endl;

  // 1. Initialize Register
  QuantumRegister qreg(request->num_qubits());

  // 2. Iterate and Stream
  for (const auto &op : request->operations()) {
    StateResponse response;

    // A. Apply Gate
    applyGate(qreg, op, &response);

    // B. Populate State Vector
    std::vector<std::complex<double>> state = qreg.getState();
    for (const auto &amp : state) {
      auto *c = response.add_state_vector();
      c->set_real(amp.real());
      c->set_imag(amp.imag());
    }

    // C. Stream Update
    if (!writer->Write(response)) {
      return grpc::Status::CANCELLED;
    }
  }

  return grpc::Status::OK;
}
