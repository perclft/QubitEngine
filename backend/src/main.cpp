#include <atomic>
#include <csignal>
#include <cstdint>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <fstream>
#include <sstream>
#include <chrono>

#include "QuantumMetrics.hpp"
#include "ServiceImpl.hpp"
#include "workers/WorkerPool.hpp"

#ifdef MPI_ENABLED
#include "mpi/MpiCoordinator.hpp"
#include <mpi.h>
#endif

std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
  spdlog::info("Shutdown signal received ({})...", signal);
  shutdown_requested = true;
}

void RunServer() {
  const char* port_env = std::getenv("PORT") ? std::getenv("PORT") : std::getenv("QUBIT_ENGINE_PORT");
  std::string server_address = port_env ? ("0.0.0.0:" + std::string(port_env)) : "0.0.0.0:50051";
  QubitEngineServiceImpl service;

  grpc::ServerBuilder builder;
  std::shared_ptr<grpc::ServerCredentials> credentials;

  const char* cert_path = std::getenv("QUBIT_ENGINE_CERT_PATH");
  const char* key_path = std::getenv("QUBIT_ENGINE_KEY_PATH");

  if (cert_path && key_path) {
    try {
      std::ifstream cert_file(cert_path);
      std::ifstream key_file(key_path);
      if (!cert_file || !key_file) {
        throw std::runtime_error("Could not open certificate or key file");
      }
      std::stringstream cert_ss, key_ss;
      cert_ss << cert_file.rdbuf();
      key_ss << key_file.rdbuf();

      grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp;
      pkcp.private_key = key_ss.str();
      pkcp.cert_chain = cert_ss.str();

      grpc::SslServerCredentialsOptions ssl_opts;
      ssl_opts.pem_key_cert_pairs.push_back(pkcp);

      credentials = grpc::SslServerCredentials(ssl_opts);
      spdlog::info("TLS Enabled: Loaded certificates from {} and {}", cert_path, key_path);
    } catch (const std::exception& e) {
      spdlog::error("Failed to load TLS certificates: {}. Falling back to insecure.", e.what());
      credentials = grpc::InsecureServerCredentials();
    }
  } else {
    spdlog::warn("QUBIT_ENGINE_CERT_PATH or QUBIT_ENGINE_KEY_PATH not set. Using insecure credentials.");
    credentials = grpc::InsecureServerCredentials();
  }

  builder.AddListeningPort(server_address, credentials);
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

  spdlog::set_level(spdlog::level::debug);
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

  spdlog::info("QubitEngine (C++) listening on {}", server_address);

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  spdlog::info("Stopping gRPC server...");
  server->Shutdown();
}

int main(int argc, char **argv) {
  int world_rank = 0;
  int world_size = 1;

#ifdef MPI_ENABLED
  qubit_engine::mpi::MpiCoordinator::instance().init(&argc, &argv);
  world_rank = qubit_engine::mpi::MpiCoordinator::instance().getRank();
  world_size = qubit_engine::mpi::MpiCoordinator::instance().getWorldSize();
#endif

  if (world_rank == 0) {
    const char* metrics_port = std::getenv("METRICS_PORT") ? std::getenv("METRICS_PORT") : "9090";
    QuantumMetrics::Instance().Start(std::string("0.0.0.0:") + metrics_port);
  }

  const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://redis:6379";
  int num_workers = 4;
  if (const char* env_workers = std::getenv("WORKER_COUNT")) {
    num_workers = std::atoi(env_workers);
    if (num_workers < 0) num_workers = 0;
  }

#ifdef MPI_ENABLED
  if (world_size > 1) {
    if (world_rank == 0) {
      spdlog::info("Rank 0: Starting gRPC Ingress Server for MPI cluster (size={})...", world_size);
      RunServer();
      spdlog::info("Rank 0: Shutting down MPI cluster...");
      qubit_engine::mpi::MpiCoordinator::instance().broadcastShutdown();
    } else {
      spdlog::info("Rank {}: Starting MPI worker loop...", world_rank);
      qubit_engine::mpi::MpiCoordinator::instance().runWorkerLoop();
    }
    qubit_engine::mpi::MpiCoordinator::instance().finalize();
    return 0;
  }
#endif

  std::unique_ptr<qubit_engine::workers::WorkerPool> pool;
  if (num_workers > 0) {
    pool = std::make_unique<qubit_engine::workers::WorkerPool>(redis_url, num_workers);
    pool->start();
  }
  
  RunServer();
  
  if (pool) {
    pool->stop();
  }

#ifdef MPI_ENABLED
  qubit_engine::mpi::MpiCoordinator::instance().finalize();
#endif

  return 0;
}
