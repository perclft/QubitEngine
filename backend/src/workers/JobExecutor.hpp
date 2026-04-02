#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sw/redis++/redis++.h>
#include "Types.hpp"
#include "QuantumRegister.hpp"
#include "quantum.pb.h"

namespace qubit_engine {
namespace workers {

class JobExecutor {
public:
    JobExecutor(sw::redis::Redis& redis, const std::string& worker_id);
    
    void executeJob(const std::string& job_id);

private:
    std::string buildJobResultJson(const std::string& job_id, int32_t shot,
                                  const std::vector<Complex>& state_vec,
                                  const std::unordered_map<int32_t, bool>& measurements);

    void applyGateForJob(QuantumRegister& qreg,
                        const GateOperation& op,
                        std::unordered_map<int32_t, bool>& measurements);

    static int64_t unixSecondsNow();

    sw::redis::Redis& redis_;
    std::string worker_id_;
};

} // namespace workers
} // namespace qubit_engine
