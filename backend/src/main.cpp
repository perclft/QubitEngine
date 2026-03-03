#include "QuantumMetrics.hpp"
#include "ServiceImpl.hpp"
#include <atomic>
#include <csignal>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>


std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
  spdlog::info("Shutdown signal received ({})...", signal);
  shutdown_requested = true;
}

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  QubitEngineServiceImpl service;

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  // CRITICAL: Initialize the reflection plugin
  // grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

  spdlog::info("QubitEngine (C++) listening on {}", server_address);
  spdlog::info("QubitEngine v2 (Debug) - VisualizeCircuit enabled");

  // Start Prometheus Metrics Exposer
  QuantumMetrics::Instance().Start();

  // Register signals
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Wait loop
  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  spdlog::info("Stopping gRPC server...");
  server->Shutdown();
}

#ifdef MPI_ENABLED
#include <mpi.h> // Phase 23: OpenMPI
#endif

// ... (existing code)

int main(int argc, char **argv) {
#ifdef MPI_ENABLED
  // Initialize MPI
  MPI_Init(&argc, &argv);

  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  if (world_rank == 0) {
    spdlog::info("MPI Initialized with size: {}", world_size);
    RunServer();
  } else {
    // Worker nodes wait for instructions (or just run loop if architected that
    // way) For now, let's just have rank 0 run the server and others wait or
    // exit. In a real distributed kernel, the server would dispatch commands to
    // workers. We'll keep them alive to receive MPI calls.
    spdlog::info("Worker Node {} started.", world_rank);

    // Simple keep-alive for workers until Shutdown
    // Real implementation would have a receive loop here
    while (!shutdown_requested) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  MPI_Finalize();
#else
  RunServer();
#endif
  return 0;
}
