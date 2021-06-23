#ifndef PIE_SRC_INCLUDE_ALLOCATOR_HPP__
#define PIE_SRC_INCLUDE_ALLOCATOR_HPP__

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>

#include "libpmem.h"

namespace PIE {

constexpr size_t cache_line_size = 64;

// A simple abstract base class that provides
// Allocate interface
class Allocator {
 public:
  // Return a pointer to a newly allocated memory blocks of "size" bytes
  virtual void *Allocate(size_t size) = 0;

  // deallocate memory of pre-allocated by this allocator, specified by
  // the start address "addr"
  virtual void Free(void *addr) = 0;

  // Allocate memory with the alignment of the second parameter
  // alignment needs to be the power of 2
  virtual void *AllocateAlign(size_t size, size_t alignment) = 0;

  // Print current memory usage
  virtual void Print() const = 0;

  // return more precise memory usage of Bytes
  virtual uint64_t MemUsage() const = 0;

  virtual ~Allocator() = default;
};

// Default Dram Allocator using standard C lib malloc
class PIEDRAMAllocator : public Allocator {
 public:
  PIEDRAMAllocator() { memory_usage_.store(0, std::memory_order_relaxed); }

  // Copyable and movable semantics are prohibited
  PIEDRAMAllocator(const PIEDRAMAllocator &) = delete;
  PIEDRAMAllocator &operator=(const PIEDRAMAllocator &) = delete;

  PIEDRAMAllocator(PIEDRAMAllocator &&) = delete;
  PIEDRAMAllocator &operator=(PIEDRAMAllocator &&) = delete;

  void *Allocate(size_t size) override {
    memory_usage_.fetch_add(size, std::memory_order_relaxed);
    return ::malloc(size);
  }

  void *AllocateAlign(size_t size, size_t alignement) override {
    void *ret;
    posix_memalign(&ret, alignement, size);
    memory_usage_.fetch_add(size, std::memory_order_relaxed);
    return ret;
  }

  void Free(void *addr) override { ::free(addr); }

  void Print() const override {
    // Get & Print current memory usage
    std::cout << "[DRAM Allocator]\n"
              << "[Usage: "
              << memory_usage_.load(std::memory_order_relaxed) /
                     (1.0 * (1 << 20))
              << "MB]\n";
  }

  uint64_t MemUsage() const override {
    return memory_usage_.load(std::memory_order_relaxed);
  }

 private:
  std::atomic<size_t> memory_usage_;
};

// Simple default thread-safe Nvm Allocator which use
// log-append method to manage memory pool of specified file
class PIENVMAllocator : public Allocator {
 public:
  // Default constructor, note that a default-constructed
  // nvm allocator is basically unusable
  PIENVMAllocator() = default;

  // Copyable and movable semantics are prohibited
  PIENVMAllocator(const PIENVMAllocator &) = delete;
  PIENVMAllocator &operator=(const PIENVMAllocator &) = delete;

  PIENVMAllocator(PIENVMAllocator &&) = delete;
  PIENVMAllocator &operator=(PIENVMAllocator &&) = delete;

  // Open specified pool file use pmdk library
  PIENVMAllocator(const char *poolfile, size_t size);

  ~PIENVMAllocator() { pmem_unmap(unalignedregion_base_, mapped_len); }

 public:
  // Allocate expeced memory size in aligned or unaligned region
  // If size >= CACHE_LINE_SIZE: allocate in aligned region
  void *Allocate(size_t size) override {
    if (size >= cache_line_size) {
      return AllocateInAligned(size, cache_line_size);
    } else {
      return AllocateInUnAligned(size);
    }
  };

  void *AllocateAlign(size_t size, size_t alignment) override {
    return AllocateInAligned(size, alignment);
  }

  // Append-only allocator is not able to deallocate memory
  void Free(void *addr) override { return; }

  void Print() const override {
    // Calculate usage proportion
    auto unaligned_proportion = static_cast<double>(unalignedregion_used_.load(
                                    std::memory_order_relaxed)) /
                                unalignedregion_size_;

    auto aligned_proportion = static_cast<double>(alignedregion_used_.load(
                                  std::memory_order_relaxed)) /
                              alignedregion_size_;

    // Print out basic information
    std::cout << "[NVMAllocator]\n";
    std::cout << "[Unaligned Region][Base:"
              << static_cast<void *>(unalignedregion_base_) << "]"
              << "[Usage: "
              << unalignedregion_used_.load(std::memory_order_relaxed) << "B"
              << "(" << std::setprecision(4) << unaligned_proportion * 100
              << "%)]\n";

    std::cout << "[Aligned   Region][Base:"
              << static_cast<void *>(alignedregion_base_) << "]"
              << "[Usage: "
              << alignedregion_used_.load(std::memory_order_relaxed) << "B"
              << "(" << std::setprecision(4) << aligned_proportion * 100
              << "%)]\n";
  }

  uint64_t MemUsage() const override {
    return unalignedregion_used_.load(std::memory_order_relaxed) +
           alignedregion_used_.load(std::memory_order_relaxed);
  }

 private:
  void *AllocateInAligned(size_t size, size_t alignment);
  void *AllocateInUnAligned(size_t size);

 public:
  // unaligned region is ahead of the whole memory pool
  // aligned region is behind the memory pool
  // unaligned region is small and aligned region is bigger
  uint8_t *unalignedregion_base_;
  uint8_t *alignedregion_base_;

  size_t unalignedregion_size_;
  size_t alignedregion_size_;

  // Use atomic variable to guarantee thread-safety
  std::atomic<size_t> unalignedregion_used_;
  std::atomic<size_t> alignedregion_used_;

  size_t mapped_len;
};

inline PIENVMAllocator::PIENVMAllocator(const char *poolfile, size_t filesize) {
  int is_pmem;

  // Check if pool file is opened correctly
  auto base = pmem_map_file(poolfile, 0, 0, 0666, &mapped_len, &is_pmem);
  if (is_pmem != 1) {
    std::cerr << "[PIENVMAllocator: Faild to open poolfile: " << poolfile
              << " ]\n";
    exit(1);
  } else {
    std::cout << "Use PMDK to mmap file! (" << poolfile << ")"
              << "(" << mapped_len / (1.0 * (1 << 20)) << "MB)"
              << "(is_pmem: " << is_pmem << ")\n";
  }

  // distribute aligned & unaligned region
  // unalignedregion size only has ten percent size of the
  // whole memory pool and aligned region has 90 percent
  unalignedregion_base_ = reinterpret_cast<uint8_t *>(base);
  unalignedregion_size_ = 0.3 * mapped_len;
  unalignedregion_used_.store(0);

  alignedregion_base_ = unalignedregion_base_ + unalignedregion_size_;
  alignedregion_size_ = 0.7 * mapped_len;
  alignedregion_used_.store(0);
}

inline void *PIENVMAllocator::AllocateInAligned(size_t size, size_t alignment) {
  // check if alignment is power of 2
  assert((alignment & (alignment - 1)) == 0);

  // Calculate the size "exceed" the alignment
  size_t mod = size & (alignment - 1);
  size_t slop = (mod == 0 ? 0 : alignment - mod);

  // padding expected size to be multiple alignment
  size_t alloc_size = size + slop;

  auto ret =
      alignedregion_used_.fetch_add(alloc_size, std::memory_order_relaxed);

  // Check if there is enough space
  if (ret + alloc_size <= alignedregion_size_) {
    return reinterpret_cast<void *>(alignedregion_base_ + ret);
  } else {
    std::cerr << "[PIENVMAllocator: Aligned region has no enough space]"
              << std::endl;
    exit(1);
  }
}

inline void *PIENVMAllocator::AllocateInUnAligned(size_t size) {
  auto ret = unalignedregion_used_.fetch_add(size, std::memory_order_relaxed);

  // check if there is enought space
  if (ret + size <= unalignedregion_size_) {
    return reinterpret_cast<void *>(unalignedregion_base_ + ret);
  } else {
    std::cerr << "[PIENVMAllocator: UnAligned region has no enough space]"
              << std::endl;
    exit(1);
  }
}

};  // namespace PIE

#endif