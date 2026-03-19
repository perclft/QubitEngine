#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace qubit_engine {
namespace ipc {

class SharedMemory {
public:
  // Creates or opens a shared memory segment of the given size.
  // descriptor should be a unique string (e.g., "/qe_shm_job123" on POSIX or
  // "Local\\qe_shm_job123" on Windows)
  static void *createSegment(const std::string &descriptor, size_t sizeBytes);

  // Opens an existing shared memory segment
  static void *openSegment(const std::string &descriptor, size_t sizeBytes);

  // Unmaps the memory and closes the handle/descriptor
  static void closeSegment(const std::string &descriptor, void *ptr,
                           size_t sizeBytes);

  // Marks the shared memory descriptor for deletion (POSIX only, no-op on
  // Windows)
  static void unlinkSegment(const std::string &descriptor);

  // Schedules an automatic cleanup of the memory mapped file after a given timeout
  static void scheduleCleanup(const std::string &descriptor, void *ptr, size_t sizeBytes, int timeoutMs);

private:
  static void performCleanup(std::string descriptor, void *ptr, size_t sizeBytes, int timeoutMs);
};

} // namespace ipc
} // namespace qubit_engine
