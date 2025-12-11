#include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include <thread>
#include <atomic>
#include <grpcpp/grpcpp.h>
// HEADER FIX: Use the standard generated reflection header
#include <grpc/grpc_reflection_v1alpha/reflection.grpc.pb.h>
// HEADER FIX: Use the official gRPC extension header for the plugin
#include <grpcpp/ext/proto_server_reflection_plugin.h> 
#include "ServiceImpl.hpp"

std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
    std::cout << "\nShutdown signal received (" << signal << ")..." << std::endl;
    shutdown_requested = true;
}

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    QubitEngineServiceImpl service;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    // CRITICAL: Initialize the reflection plugin
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "QubitEngine (C++) listening on " << server_address << std::endl;

    // Register signals
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Wait loop
    while (!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "Stopping gRPC server..." << std::endl;
    server->Shutdown();
}

int main() {
    RunServer();
    return 0;
}
