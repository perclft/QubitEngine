#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "ServiceImpl.cpp" // Include the implementation class

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    QubitEngineServiceImpl service;

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
