#include <gtest/gtest.h>
#include "ipc/SharedMemory.hpp"
#include <thread>
#include <chrono>

using namespace qubit_engine::ipc;

TEST(SharedMemoryTest, DoubleCloseIsSafeNoOp) {
    std::string desc = "test_shm_double_close";
    size_t size = 1024;
    
    // Create segment
    void* ptr = SharedMemory::createSegment(desc, size);
    ASSERT_NE(ptr, nullptr);

    // First close returns true (unmapped)
    EXPECT_TRUE(SharedMemory::closeSegment(desc, ptr, size));

    // Second close on same pointer returns false (safe no-op, duplicate blocked)
    EXPECT_FALSE(SharedMemory::closeSegment(desc, ptr, size));
    
    SharedMemory::unlinkSegment(desc);
}

TEST(SharedMemoryTest, ScheduleCleanupDoesNotRaceOrCrash) {
    std::string desc = "test_shm_schedule_cleanup";
    size_t size = 1024;

    {
        SharedMemory shm(desc, size, true);
        ASSERT_NE(shm.data(), nullptr);
        // Schedule cleanup synchronously/short delay
        SharedMemory::scheduleCleanup(desc, shm.data(), size, 10);
        // Destructor runs here
    }

    // Wait past the cleanup timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Calling performCleanup / closeSegment directly must be a safe no-op now
    EXPECT_NO_THROW(SharedMemory::unlinkSegment(desc));
}
