#include "JobExecutor.hpp"
#include "QuantumMetrics.hpp"
#include "GateDispatch.hpp"
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <chrono>

namespace qubit_engine {
namespace workers {

JobExecutor::JobExecutor(sw::redis::Redis &redis, const std::string &worker_id)
    : redis_(redis), worker_id_(worker_id) {}

void JobExecutor::executeJob(const std::string &job_id) {
  auto start_time = std::chrono::steady_clock::now();
  // Fetch canonical execution payload (protobuf-encoded CircuitRequest).
  const std::string circuit_key = "job:circuitpb:" + job_id;
  auto circuit_bytes = redis_.get(circuit_key);
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
  if (auto s = redis_.get(shots_key)) {
    try {
      shots = std::max<int32_t>(1, std::stoi(*s));
    } catch (...) {
      shots = 1;
    }
  }

  // Worker-visible status keys (Go scheduler overlays these in GetJobStatus).
  redis_.set("job:state:" + job_id, "2"); // RUNNING
  redis_.set("job:started_at:" + job_id, std::to_string(unixSecondsNow()));
  redis_.set("job:worker_id:" + job_id, worker_id_);
  redis_.del("job:error:" + job_id);

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
        buildJobResultJson(job_id, shot, state, measurements);

    // Push to Redis stream for scheduler to forward via gRPC.
    std::map<std::string, std::string> fields = {{"data", payload}};
    redis_.xadd(stream_key, "*", fields.begin(), fields.end());
  }

  // EOF marker signals end of stream to clients.
  std::map<std::string, std::string> eof_fields = {{"data", "EOF"}};
  redis_.xadd(stream_key, "*", eof_fields.begin(), eof_fields.end());

  redis_.set("job:state:" + job_id, "3"); // COMPLETED
  redis_.set("job:completed_at:" + job_id, std::to_string(unixSecondsNow()));

  auto end_time = std::chrono::steady_clock::now();
  std::chrono::duration<double> diff = end_time - start_time;
  QuantumMetrics::Instance().RecordJobDuration(diff.count());
  QuantumMetrics::Instance().IncrementJobCounter("success");
}

std::string JobExecutor::buildJobResultJson(const std::string &job_id, int32_t shot,
                                         const std::vector<Complex> &state_vec,
                                         const std::unordered_map<int32_t, bool> &measurements) {
  nlohmann::json state_array = nlohmann::json::array();
  for (const auto &c : state_vec) {
    state_array.push_back({{"real", c.real()}, {"imag", c.imag()}});
  }

  nlohmann::json classical;
  nlohmann::json meas;
  for (const auto &kv : measurements) {
    classical[std::to_string(kv.first)] = kv.second;
    meas[std::to_string(kv.first)] = kv.second;
  }

  nlohmann::json j = {{"job_id", job_id},
                      {"shot_number", shot},
                      {"state",
                       {{"state_vector", state_array},
                        {"classical_results", classical},
                        {"server_id", worker_id_}}},
                      {"measurements", meas}};

  return j.dump();
}

void JobExecutor::applyGateForJob(qubit_engine::QuantumRegister &qreg,
                                 const qubit_engine::GateOperation &op,
                                 std::unordered_map<int32_t, bool> &measurements) {
  qubit_engine::dispatchGate(qreg, op, &measurements, nullptr);
}

int64_t JobExecutor::unixSecondsNow() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace workers
} // namespace qubit_engine
