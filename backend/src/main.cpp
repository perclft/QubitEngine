#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "ServiceImpl.hpp" // FIX: Include header, not cpp

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    QubitEngineServiceImpl service; // Now correctly linked

    grpc::ServerBuilder builder;
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
