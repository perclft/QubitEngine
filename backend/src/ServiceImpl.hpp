#pragma once
#include <api/proto/quantum.grpc.pb.h>
#include <grpcpp/grpcpp.h>

class QubitEngineServiceImpl final : public qubit_engine::QuantumCompute::Service {
public:
    grpc::Status RunCircuit(grpc::ServerContext* context, 
                          const qubit_engine::CircuitRequest* request,
                          qubit_engine::StateResponse* response) override;
};
