#include "WorkerPool.hpp"
#include "JobExecutor.hpp"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace qubit_engine {
namespace workers {

WorkerPool::WorkerPool(const std::string &redis_url, int num_workers)
    : redis_url_(redis_url), num_workers_(num_workers), running_(false) {}

WorkerPool::~WorkerPool() {
  stop();
}

void WorkerPool::start() {
  if (running_) return;

  running_ = true;
  for (int i = 0; i < num_workers_; ++i) {
    workers_.emplace_back(&WorkerPool::runWorkerLoop, this, i);
  }
  spdlog::info("WorkerPool started with {} workers pointing to {}", num_workers_, redis_url_);
}

void WorkerPool::stop() {
  if (!running_) return;

  running_ = false;
  for (auto &t : workers_) {
    if (t.joinable()) {
      t.join();
    }
  }
  workers_.clear();
  spdlog::info("WorkerPool stopped.");
}

void WorkerPool::runWorkerLoop(int worker_index) {
  std::string worker_id = buildWorkerId(worker_index);
  
  try {
    sw::redis::Redis redis(redis_url_);
    JobExecutor executor(redis, worker_id);
    
    spdlog::info("{} started listening for jobs...", worker_id);

    while (running_) {
      // Blocking Pop from Queue with 1 second timeout and fail-safe retry logic
      decltype(redis.bzpopmax("queue:jobs", 1)) job;
      int retry_delay_ms = 100;
      const int max_delay_ms = 5000;

      while (running_) {
        try {
          job = redis.bzpopmax("queue:jobs", 1);
          break; // Success, exit retry loop
        } catch (const std::exception &ex) {
          spdlog::warn("{} failed to read from Redis: {}. Retrying in {}ms...", 
                       worker_id, ex.what(), retry_delay_ms);
          std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
          retry_delay_ms = std::min(retry_delay_ms * 2, max_delay_ms);
        }
      }

      if (!running_) break;
      if (!job) continue;

      std::string job_id = std::get<1>(*job);
      spdlog::info("{} pulled job ID: {}", worker_id, job_id);

      // Ack: Move to processing
      auto now = std::chrono::system_clock::now().time_since_epoch().count();
      redis.hset("jobs:processing", job_id, std::to_string(now));

      try {
        executor.executeJob(job_id);
        redis.hdel("jobs:processing", job_id);
        spdlog::info("{} successfully processed job: {}", worker_id, job_id);
      } catch (const std::exception &ex) {
        redis.hdel("jobs:processing", job_id);
        redis.lpush("queue:deadletter", job_id);
        redis.set("job:state:" + job_id, "4"); // FAILED
        redis.set("job:completed_at:" + job_id, std::to_string(unixSecondsNow()));
        redis.set("job:error:" + job_id, ex.what());
        spdlog::error("{} failed job {}: {}", worker_id, job_id, ex.what());
      }
    }
  } catch (const std::exception &ex) {
    spdlog::error("Fatal error in worker thread {}: {}", worker_id, ex.what());
  }
}

std::string WorkerPool::buildWorkerId(int index) {
  char hostname[256];
  std::string suffix = "worker-" + std::to_string(index);
#ifdef _WIN32
  DWORD host_size = sizeof(hostname);
  if (GetComputerNameA(hostname, &host_size)) {
    return std::string(hostname) + ":" + suffix;
  }
#else
  if (gethostname(hostname, sizeof(hostname)) == 0) {
    return std::string(hostname) + ":" + suffix;
  }
#endif
  return suffix;
}

int64_t WorkerPool::unixSecondsNow() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace workers
} // namespace qubit_engine
