#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include "qubit_engine_export.h"


namespace qubit_engine {
namespace ipc {

class QUBIT_ENGINE_EXPORT SharedMemory {
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
  static bool closeSegment(const std::string &descriptor, void *ptr,
                           size_t sizeBytes);
  static void unlinkSegment(const std::string &descriptor);
  static void scheduleCleanup(const std::string &descriptor, void *ptr, size_t sizeBytes, int timeoutMs);

private:
  std::string descriptor_;
  size_t sizeBytes_;
  void* ptr_;
  static void performCleanup(std::string descriptor, void *ptr, size_t sizeBytes, int timeoutMs);
};

struct QUBIT_ENGINE_EXPORT ShmRegistration {
  std::string ack_token;
  std::string segment_name;
  void* ptr = nullptr;
  size_t size_bytes = 0;
  bool cleaned_up = false;
};

class QUBIT_ENGINE_EXPORT ActiveShmRegistry {
public:
  static ActiveShmRegistry& instance();

  std::string registerSegment(const std::string& segment_name, void* ptr, size_t size_bytes);
  bool acknowledgeAndUnlink(const std::string& ack_token);
  size_t activeCount();
  void clearAll();

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ShmRegistration> registry_;
};

} // namespace ipc
} // namespace qubit_engine
