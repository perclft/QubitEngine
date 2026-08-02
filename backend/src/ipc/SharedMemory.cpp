#include "SharedMemory.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <unordered_map>
#include <windows.h>
#include <sddl.h>

#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

namespace qubit_engine {
namespace ipc {

static std::mutex handle_mutex;
static std::unordered_set<void*> active_mappings;

#ifdef _WIN32
struct SharedMemorySegmentRAII {
    HANDLE hMapFile;
    void* pBuf;

    SharedMemorySegmentRAII(HANDLE h, void* p) : hMapFile(h), pBuf(p) {}
    ~SharedMemorySegmentRAII() {
        if (pBuf) UnmapViewOfFile(pBuf);
        if (hMapFile) CloseHandle(hMapFile);
    }
    
    // Disable copy for RAII
    SharedMemorySegmentRAII(const SharedMemorySegmentRAII&) = delete;
    SharedMemorySegmentRAII& operator=(const SharedMemorySegmentRAII&) = delete;
};

static std::unordered_map<void*, std::shared_ptr<SharedMemorySegmentRAII>> active_handles;
#endif

SharedMemory::SharedMemory(const std::string& descriptor, size_t sizeBytes, bool create)
    : descriptor_(descriptor), sizeBytes_(sizeBytes), ptr_(nullptr) {
  if (create) {
    ptr_ = createSegment(descriptor, sizeBytes);
  } else {
    ptr_ = openSegment(descriptor, sizeBytes);
  }
}

SharedMemory::~SharedMemory() {
  if (ptr_) {
    closeSegment(descriptor_, ptr_, sizeBytes_);
    unlinkSegment(descriptor_);
    ptr_ = nullptr;
  }
}

void *SharedMemory::createSegment(const std::string &descriptor,
                                  size_t sizeBytes) {
#ifdef _WIN32
  SECURITY_ATTRIBUTES sa;
  sa.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa.bInheritHandle = FALSE;
  
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;;GA;;;OW)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
    throw std::runtime_error("Could not construct security descriptor");
  }

  HANDLE hMapFile = CreateFileMappingA(
      INVALID_HANDLE_VALUE,            // use paging file
      &sa,                             // restricted security
      PAGE_READWRITE,                  // read/write access
      (DWORD)(sizeBytes >> 32),        // maximum object size (high-order DWORD)
      (DWORD)(sizeBytes & 0xFFFFFFFF), // maximum object size (low-order DWORD)
      descriptor.c_str());             // name of mapping object
      
  LocalFree(sa.lpSecurityDescriptor);

  if (hMapFile == NULL) {
    throw std::runtime_error("Could not create file mapping object: " +
                             std::to_string(GetLastError()));
  }

  void *pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeBytes);
  if (pBuf == NULL) {
    CloseHandle(hMapFile);
    throw std::runtime_error("Could not map view of file: " +
                             std::to_string(GetLastError()));
  }

  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    active_handles[pBuf] = std::make_shared<SharedMemorySegmentRAII>(hMapFile, pBuf);
    active_mappings.insert(pBuf);
  }
  return pBuf;
#else
  int fd = shm_open(descriptor.c_str(), O_CREAT | O_RDWR, 0600);
  if (fd == -1) {
    throw std::runtime_error("shm_open failed");
  }

  if (ftruncate(fd, sizeBytes) == -1) {
    close(fd);
    throw std::runtime_error("ftruncate failed");
  }

  void *ptr = mmap(0, sizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (ptr == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }

  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    active_mappings.insert(ptr);
  }

  return ptr;
#endif
}

void *SharedMemory::openSegment(const std::string &descriptor,
                                size_t sizeBytes) {
#ifdef _WIN32
  HANDLE hMapFile =
      OpenFileMappingA(FILE_MAP_ALL_ACCESS, // read/write access
                       FALSE,               // do not inherit the name
                       descriptor.c_str()); // name of mapping object

  if (hMapFile == NULL) {
    throw std::runtime_error("Could not open file mapping object: " +
                             std::to_string(GetLastError()));
  }

  void *pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeBytes);
  if (pBuf == NULL) {
    CloseHandle(hMapFile);
    throw std::runtime_error("Could not map view of file: " +
                             std::to_string(GetLastError()));
  }

  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    active_handles[pBuf] = std::make_shared<SharedMemorySegmentRAII>(hMapFile, pBuf);
    active_mappings.insert(pBuf);
  }

  return pBuf;
#else
  int fd = shm_open(descriptor.c_str(), O_RDWR, 0600);
  if (fd == -1) {
    throw std::runtime_error("shm_open failed");
  }

  void *ptr = mmap(0, sizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (ptr == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }

  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    active_mappings.insert(ptr);
  }

  return ptr;
#endif
}

bool SharedMemory::closeSegment(const std::string &descriptor, void *ptr,
                                size_t sizeBytes) {
  if (!ptr) return false;

  std::lock_guard<std::mutex> lock(handle_mutex);
  auto it = active_mappings.find(ptr);
  if (it == active_mappings.end()) {
    // Pointer is already unmapped/closed. Safe no-op.
    return false;
  }
  active_mappings.erase(it);

#ifdef _WIN32
  auto handle_it = active_handles.find(ptr);
  if (handle_it != active_handles.end()) {
    active_handles.erase(handle_it);
  }
#else
  munmap(ptr, sizeBytes);
#endif
  return true;
}

void SharedMemory::unlinkSegment(const std::string &descriptor) {
#ifdef _WIN32
  // Windows auto-cleans up named shared memory when all handles are closed.
#else
  shm_unlink(descriptor.c_str());
#endif
}

void SharedMemory::scheduleCleanup(const std::string &descriptor, void *ptr, size_t sizeBytes, int timeoutMs) {
  std::thread t(&SharedMemory::performCleanup, descriptor, ptr, sizeBytes, timeoutMs);
  t.detach();
}

void SharedMemory::performCleanup(std::string descriptor, void *ptr, size_t sizeBytes, int timeoutMs) {
  std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
  try {
    closeSegment(descriptor, ptr, sizeBytes);
    unlinkSegment(descriptor);
  } catch (...) {
    // Ignore cleanup destruct errors
  }
}

} // namespace ipc
} // namespace qubit_engine
