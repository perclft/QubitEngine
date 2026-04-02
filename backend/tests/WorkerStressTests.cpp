#include <gtest/gtest.h>
#include <sw/redis++/redis++.h>
#include "workers/WorkerPool.hpp"
#include "workers/JobExecutor.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include "quantum.pb.h"

using namespace qubit_engine::workers;

class WorkerStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://127.0.0.1:6379";
        try {
            redis = std::make_unique<sw::redis::Redis>(redis_url);
            redis->flushall();
        } catch (...) {
            GTEST_SKIP() << "Redis not available for stress tests";
        }
    }

    std::unique_ptr<sw::redis::Redis> redis;
};

TEST_F(WorkerStressTest, ConcurrentJobProcessing) {
    const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://127.0.0.1:6379";
    int num_workers = 4;
    WorkerPool pool(redis_url, num_workers);
    pool.start();

    int num_jobs = 20;
    
    // Create a simple circuit
    qubit_engine::CircuitRequest circuit;
    circuit.set_num_qubits(2);
    auto* op = circuit.add_operations();
    op->set_type(qubit_engine::GateOperation::HADAMARD);
    op->set_target_qubit(0);

    std::string circuit_bin;
    circuit.SerializeToString(&circuit_bin);

    // Push 20 jobs
    for (int i = 0; i < num_jobs; ++i) {
        std::string job_id = "stress-job-" + std::to_string(i);
        redis->set("job:circuitpb:" + job_id, circuit_bin);
        redis->set("job:shots:" + job_id, "10");
        redis->lpush("queue:jobs", job_id);
    }

    // Wait for completion
    int completed = 0;
    for (int retry = 0; retry < 50; ++retry) {
        completed = 0;
        for (int i = 0; i < num_jobs; ++i) {
            auto state = redis->get("job:state:stress-job-" + std::to_string(i));
            if (state && *state == "3") { // COMPLETED
                completed++;
            }
        }
        if (completed == num_jobs) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    pool.stop();
    EXPECT_EQ(completed, num_jobs);
}

TEST_F(WorkerStressTest, ErrorHandlingDeadLetter) {
    const char* redis_url = std::getenv("REDIS_ADDR") ? std::getenv("REDIS_ADDR") : "tcp://127.0.0.1:6379";
    WorkerPool pool(redis_url, 1);
    pool.start();

    // Push a job without circuit payload -> should fail
    std::string job_id = "invalid-job";
    redis->lpush("queue:jobs", job_id);

    // Wait for failure
    bool failed = false;
    for (int retry = 0; retry < 20; ++retry) {
        auto state = redis->get("job:state:" + job_id);
        if (state && *state == "4") { // FAILED
            failed = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Check deadletter queue
    auto dlq_item = redis->lpop("queue:deadletter");
    
    pool.stop();
    EXPECT_TRUE(failed);
    EXPECT_TRUE(dlq_item.has_value());
    EXPECT_EQ(*dlq_item, job_id);
}
