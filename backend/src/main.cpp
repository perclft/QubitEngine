#include "QuantumMetrics.hpp"
#include "ServiceImpl.hpp"
#include "QuantumRegister.hpp"
#include "GateDispatch.hpp"
#include <atomic>
#include <csignal>
#include <cstdint>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <sw/redis++/redis++.h>
#include <thread>
#include <unordered_map>

std::atomic<bool> shutdown_requested(false);

void signalHandler(int signal) {
  spdlog::info("Shutdown signal received ({})...", signal);
  shutdown_requested = true;
}

static inline int64_t unixSecondsNow() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

static std::string jsonEscape(const std::string &s) {
  std::string out;
  out.reserve(s.size() + 16);
  for (unsigned char c : s) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        // Minimal escaping for control chars
        char buf[7];
        std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
        out += buf;
      } else {
        out.push_back((char)c);
      }
    }
  }
  return out;
}

static void applyGateForJob(qubit_engine::QuantumRegister &qreg,
                            const qubit_engine::GateOperation &op,
                            std::unordered_map<int32_t, bool> &measurements) {
  qubit_engine::dispatchGate(qreg, op, &measurements, nullptr);
}

static std::string buildJobResultJson(const std::string &job_id, int32_t shot,
                                      const std::string &worker_id,
                                      const std::vector<qubit_engine::Complex>
                                          &state_vec,
                                      const std::unordered_map<int32_t, bool>
                                          &measurements) {
  std::string json;
  json.reserve(256 + state_vec.size() * 24);
  json += "{";
  json += "\"job_id\":\"" + jsonEscape(job_id) + "\",";
  json += "\"shot_number\":" + std::to_string(shot) + ",";

  json += "\"state\":{";
  json += "\"state_vector\":[";
  for (size_t i = 0; i < state_vec.size(); ++i) {
    const auto &c = state_vec[i];
    json += "{\"real\":" + std::to_string(c.real()) +
            ",\"imag\":" + std::to_string(c.imag()) + "}";
    if (i + 1 < state_vec.size())
      json += ",";
  }
  json += "],";
  json += "\"classical_results\":{";
  bool first = true;
  for (const auto &kv : measurements) {
    if (!first)
      json += ",";
    first = false;
    json += "\"" + std::to_string(kv.first) + "\":" + (kv.second ? "true" : "false");
  }
  json += "},";
  json += "\"server_id\":\"" + jsonEscape(worker_id) + "\"";
  json += "},";

  json += "\"measurements\":{";
  first = true;
  for (const auto &kv : measurements) {
    if (!first)
      json += ",";
    first = false;
    json += "\"" + std::to_string(kv.first) + "\":" + (kv.second ? "true" : "false");
  }
  json += "}";
  json += "}";
  return json;
}

static void executeJob(sw::redis::Redis &redis, const std::string &job_id,
                       const std::string &worker_id) {
  auto start_time = std::chrono::steady_clock::now();
  // Fetch canonical execution payload (protobuf-encoded CircuitRequest).
  const std::string circuit_key = "job:circuitpb:" + job_id;
  auto circuit_bytes = redis.get(circuit_key);
  if (!circuit_bytes) {
    throw std::runtime_error("missing circuit payload at " + circuit_key);
  }

  qubit_engine::CircuitRequest circuit;
  if (!circuit.ParseFromString(*circuit_bytes)) {
    throw std::runtime_error("failed to parse CircuitRequest protobuf for job " +
                             job_id);
  }

  int32_t shots = 1;
  const std::string shots_key = "job:shots:" + job_id;
  if (auto s = redis.get(shots_key)) {
    try {
      shots = std::max<int32_t>(1, std::stoi(*s));
    } catch (...) {
      shots = 1;
    }
  }

  // Worker-visible status keys (Go scheduler overlays these in GetJobStatus).
  redis.set("job:state:" + job_id, "2"); // RUNNING
  redis.set("job:started_at:" + job_id, std::to_string(unixSecondsNow()));
  redis.set("job:worker_id:" + job_id, worker_id);
  redis.del("job:error:" + job_id);

  const std::string stream_key = "stream:results:" + job_id;

  // Execute N shots. For now, we run the full circuit per-shot (simple and correct).
  for (int32_t shot = 1; shot <= shots; ++shot) {
    qubit_engine::QuantumRegister qreg((size_t)circuit.num_qubits());
    std::unordered_map<int32_t, bool> measurements;

    for (const auto &op : circuit.operations()) {
      applyGateForJob(qreg, op, measurements);
    }

    auto state = qreg.getStateVector();
    std::string payload =
        buildJobResultJson(job_id, shot, worker_id, state, measurements);

    // Push to Redis stream for scheduler to forward via gRPC.
    // Go expects msg.Values["data"] to be a JSON string.
    std::map<std::string, std::string> fields = {{"data", payload}};
    redis.xadd(stream_key, "*", fields.begin(), fields.end());
  }

  // EOF marker signals end of stream to clients.
  std::map<std::string, std::string> eof_fields = {{"data", "EOF"}};
  redis.xadd(stream_key, "*", eof_fields.begin(), eof_fields.end());

  redis.set("job:state:" + job_id, "3"); // COMPLETED
  redis.set("job:completed_at:" + job_id, std::to_string(unixSecondsNow()));

  auto end_time = std::chrono::steady_clock::now();
  std::chrono::duration<double> diff = end_time - start_time;
  QuantumMetrics::Instance().RecordJobDuration(diff.count());
  QuantumMetrics::Instance().IncrementJobCounter("success");
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

// Shared worker loop: polls Redis for jobs, executes them, handles errors
// and dead letter queue. Used by both MPI worker nodes and local thread pool.
static void runWorkerLoop(sw::redis::Redis &redis, const std::string &worker_id) {
  while (!shutdown_requested) {
#ifdef MPI_ENABLED
    // MPI Probe for distributed tensor/networking (non-blocking)
    int flag = 0;
    MPI_Status mpi_status;
    MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &mpi_status);
#endif

    // Blocking Pop from Queue with 1 second timeout
    auto job = redis.bzpopmax("queue:jobs", 1);
    if (!job) continue;

    std::string job_id = std::get<1>(*job);
    spdlog::info("{} pulled job ID: {}", worker_id, job_id);

    // Ack: Move to processing
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    redis.hset("jobs:processing", job_id, std::to_string(now));

    try {
      executeJob(redis, job_id, worker_id);
      redis.hdel("jobs:processing", job_id);
      spdlog::info("{} successfully processed and ACKed job: {}", worker_id, job_id);
    } catch (const std::exception &ex) {
      redis.hdel("jobs:processing", job_id);
      redis.lpush("queue:deadletter", job_id);
      redis.set("job:state:" + job_id, "4"); // FAILED
      redis.set("job:completed_at:" + job_id, std::to_string(unixSecondsNow()));
      redis.set("job:error:" + job_id, ex.what());
      spdlog::error("{} failed job {}: {}", worker_id, job_id, ex.what());
    } catch (...) {
      redis.hdel("jobs:processing", job_id);
      redis.lpush("queue:deadletter", job_id);
      redis.set("job:state:" + job_id, "4"); // FAILED
      redis.set("job:completed_at:" + job_id, std::to_string(unixSecondsNow()));
      redis.set("job:error:" + job_id, "unknown execution failure");
      spdlog::error("{} failed job {} (unknown exception)", worker_id, job_id);
    }
  }
}

// Build a worker ID string from hostname + suffix
static std::string buildWorkerId(const std::string &suffix) {
  char hostname[1024];
  if (gethostname(hostname, 1024) == 0) {
    return std::string(hostname) + ":" + suffix;
  }
  return suffix;
}

int main(int argc, char **argv) {
  // Suggestion 16: Start Prometheus metrics server
  QuantumMetrics::Instance().Start("0.0.0.0:9090");

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
    try {
      const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://redis:6379";
      sw::redis::Redis redis(redis_url);
      std::string wid = buildWorkerId("mpi-worker-" + std::to_string(world_rank));
      spdlog::info("{} connected to Redis.", wid);
      runWorkerLoop(redis, wid);
    } catch (const sw::redis::Error &err) {
      spdlog::error("Redis Error: {}", err.what());
    }
  }

  MPI_Finalize();
#else
  // Single-node execution: The server still runs gRPC for development testing
  // Spin a resilient local polling thread pool
  int num_workers = 4;
  std::vector<std::thread> workers;
  
  for (int i = 0; i < num_workers; ++i) {
    workers.emplace_back([i]() {
      try {
        const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://redis:6379";
        sw::redis::Redis redis(redis_url);
        std::string wid = buildWorkerId("worker-" + std::to_string(i));
        spdlog::info("{} connected to Redis.", wid);
        runWorkerLoop(redis, wid);
      } catch (const sw::redis::Error &err) {
        spdlog::error("Redis Error in {}: {}", "worker-" + std::to_string(i), err.what());
      }
    });
  }

  RunServer();
  
  for (auto& w : workers) {
    if (w.joinable()) w.join();
  }
#endif
  return 0;
}
