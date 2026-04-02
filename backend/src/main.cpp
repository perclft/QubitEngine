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
#include <mpi.h>
#endif

std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
  spdlog::info("Shutdown signal received ({})...", signal);
  shutdown_requested = true;
}

void RunServer() {
  std::string server_address("0.0.0.0:50051");
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
  
  QuantumMetrics::Instance().Start();

  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  while (!shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  spdlog::info("Stopping gRPC server...");
  server->Shutdown();
}

int main(int argc, char **argv) {
  QuantumMetrics::Instance().Start("0.0.0.0:9090");

  const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://redis:6379";
  int num_workers = 4;
  if (const char* env_workers = std::getenv("WORKER_COUNT")) {
    num_workers = std::max(1, std::atoi(env_workers));
  }

#ifdef MPI_ENABLED
  MPI_Init(&argc, &argv);
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  if (world_rank == 0) {
    RunServer();
  } else {
    qubit_engine::workers::WorkerPool pool(redis_url, 1);
    pool.start();
    while (!shutdown_requested) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    pool.stop();
  }
  MPI_Finalize();
#else
  qubit_engine::workers::WorkerPool pool(redis_url, num_workers);
  pool.start();
  
  RunServer();
  
  pool.stop();
#endif

  return 0;
}
