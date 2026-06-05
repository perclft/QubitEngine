#pragma once

#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <sw/redis++/redis++.h>
#include "qubit_engine_export.h"

namespace qubit_engine {
namespace workers {

class QUBIT_ENGINE_EXPORT WorkerPool {
public:
    WorkerPool(const std::string& redis_url, int num_workers);
    ~WorkerPool();

    void start();
    void stop();
    void runWorkerLoop(int worker_index);

private:
    std::string buildWorkerId(int index);
    static int64_t unixSecondsNow();

    std::string redis_url_;
    int num_workers_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_;
};

} // namespace workers
} // namespace qubit_engine
