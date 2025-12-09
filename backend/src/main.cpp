#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "ServiceImpl.hpp"

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    QubitEngineServiceImpl service;

    grpc::ServerBuilder builder;
    
    // SECURITY NOTE: InsecureServerCredentials is used here for 
    // internal Kubernetes simulation only. In a production FinTech environment,
    // this would be replaced with SslServerCredentials (mTLS) or 
    // handled by a Service Mesh (Linkerd/Istio) sidecar.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    builder.RegisterService(&service);
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "QubitEngine (C++) listening on " << server_address << std::endl;
    server->Wait();
}

int main() {
    RunServer();
    return 0;
}
