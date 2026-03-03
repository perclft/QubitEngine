#pragma once

#include <cstddef>
#include <new>

// Platform specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace qubit_engine {

template <typename T> struct HugePageAllocator {
  using value_type = T;

  HugePageAllocator() = default;

  template <typename U>
  constexpr HugePageAllocator(const HugePageAllocator<U> &) noexcept {}

  [[nodiscard]] T *allocate(std::size_t n) {
    if (n > std::size_t(-1) / sizeof(T))
      throw std::bad_alloc();

    size_t bytes = n * sizeof(T);

#ifdef _WIN32
    void *p = nullptr;
    SIZE_T minLargePageSize = GetLargePageMinimum();
    if (minLargePageSize != 0 && bytes >= minLargePageSize) {
      p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,
                       PAGE_READWRITE);
    }
    if (!p) {
      p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    }
    if (!p)
      throw std::bad_alloc();
    return static_cast<T *>(p);
#else
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#ifdef MAP_HUGETLB
    if (bytes >= 2 * 1024 * 1024) {
      flags |= MAP_HUGETLB;
    }
#endif
    void *p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (p == MAP_FAILED) {
      p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (p == MAP_FAILED)
        throw std::bad_alloc();
    }
    return static_cast<T *>(p);
#endif
  }

  void deallocate(T *p, std::size_t n) noexcept {
    size_t bytes = n * sizeof(T);
#ifdef _WIN32
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, bytes);
#endif
  }
};

template <class T, class U>
bool operator==(const HugePageAllocator<T> &, const HugePageAllocator<U> &) {
  return true;
}
template <class T, class U>
bool operator!=(const HugePageAllocator<T> &, const HugePageAllocator<U> &) {
  return false;
}

} // namespace qubit_engine
