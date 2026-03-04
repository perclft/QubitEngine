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
#include <sw/redis++/redis++.h>
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
    // Go Scheduler Decoupling: C++ Worker nodes now poll Redis directly instead
    // of waiting on gRPC This removes the heavy 1,000-thread gRPC bottleneck
    // from Go completely.
    try {
      sw::redis::Redis redis("tcp://localhost:6379");
      spdlog::info("Worker Node {} connected to Redis.", world_rank);

      while (!shutdown_requested) {
        // MPI Probe for distributed tensor/networking (non-blocking)
        int flag = 0;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);
        if (flag) {
        }

        // Blocking Pop from Queue with 1 second timeout
        auto job = redis.bzpopmax("queue:jobs", 1);
        if (job) {
          spdlog::info("Worker Node {} pulled job ID: {}", world_rank,
                       std::get<1>(*job));
          // executeJob(std::get<1>(*job)); // Hook into engine cleanly
        }
      }
    } catch (const sw::redis::Error &err) {
      spdlog::error("Redis Error: {}", err.what());
    }
  }

  MPI_Finalize();
#else
  // Single-node execution: The server still runs gRPC for development testing
  // But ideally, we should also spin a local polling thread here too.
  RunServer();
#endif
  return 0;
}
