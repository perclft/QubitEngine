#include "SharedMemory.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <mutex>
#include <unordered_map>
#include <windows.h>

#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

namespace qubit_engine {
namespace ipc {

#ifdef _WIN32
static std::unordered_map<void*, HANDLE> active_handles;
static std::mutex handle_mutex;
#endif

void *SharedMemory::createSegment(const std::string &descriptor,
                                  size_t sizeBytes) {
#ifdef _WIN32
  HANDLE hMapFile = CreateFileMappingA(
      INVALID_HANDLE_VALUE,            // use paging file
      NULL,                            // default security
      PAGE_READWRITE,                  // read/write access
      (DWORD)(sizeBytes >> 32),        // maximum object size (high-order DWORD)
      (DWORD)(sizeBytes & 0xFFFFFFFF), // maximum object size (low-order DWORD)
      descriptor.c_str());             // name of mapping object

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

  // Store the handle to keep it alive and prevent leaks
  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    active_handles[pBuf] = hMapFile;
  }
  return pBuf;
#else
  int fd = shm_open(descriptor.c_str(), O_CREAT | O_RDWR, 0666);
  if (fd == -1) {
    throw std::runtime_error("shm_open failed");
  }

  if (ftruncate(fd, sizeBytes) == -1) {
    close(fd);
    throw std::runtime_error("ftruncate failed");
  }

  void *ptr = mmap(0, sizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd); // descriptor kept alive by mmap, and name exists in /dev/shm

  if (ptr == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
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
    active_handles[pBuf] = hMapFile;
  }

  return pBuf;
#else
  int fd = shm_open(descriptor.c_str(), O_RDWR, 0666);
  if (fd == -1) {
    throw std::runtime_error("shm_open failed");
  }

  void *ptr = mmap(0, sizeBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  if (ptr == MAP_FAILED) {
    throw std::runtime_error("mmap failed");
  }

  return ptr;
#endif
}

void SharedMemory::closeSegment(const std::string &descriptor, void *ptr,
                                size_t sizeBytes) {
#ifdef _WIN32
  UnmapViewOfFile(ptr);
  {
    std::lock_guard<std::mutex> lock(handle_mutex);
    auto it = active_handles.find(ptr);
    if (it != active_handles.end()) {
      CloseHandle(it->second);
      active_handles.erase(it);
    }
  }
#else
  munmap(ptr, sizeBytes);
#endif
}

void SharedMemory::unlinkSegment(const std::string &descriptor) {
#ifdef _WIN32
  // Windows auto-cleans up named shared memory when all handles and views are
  // closed.
#else
  shm_unlink(descriptor.c_str());
#endif
}

void SharedMemory::scheduleCleanup(const std::string &descriptor, void *ptr, size_t sizeBytes, int timeoutMs) {
  std::thread(&SharedMemory::performCleanup, descriptor, ptr, sizeBytes, timeoutMs).detach();
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
