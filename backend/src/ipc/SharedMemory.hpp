#pragma once
#include <cstdint>
#include <string>
#include <vector>


namespace qubit_engine {
namespace ipc {

class SharedMemory {
public:
  // RAII Constructor
  SharedMemory(const std::string& descriptor, size_t sizeBytes, bool create = false);
  
  // RAII Destructor
  ~SharedMemory();

  // Access the mapped data
  void* data() const { return ptr_; }
  
  // Static methods for manual control if needed
  static void *createSegment(const std::string &descriptor, size_t sizeBytes);
  static void *openSegment(const std::string &descriptor, size_t sizeBytes);
  static void closeSegment(const std::string &descriptor, void *ptr,
                           size_t sizeBytes);
  static void unlinkSegment(const std::string &descriptor);
  static void scheduleCleanup(const std::string &descriptor, void *ptr, size_t sizeBytes, int timeoutMs);

private:
  std::string descriptor_;
  size_t sizeBytes_;
  void* ptr_;
  static void performCleanup(std::string descriptor, void *ptr, size_t sizeBytes, int timeoutMs);
};

} // namespace ipc
} // namespace qubit_engine
