#ifndef PIE_SRC_INCLUDE_RHTREE_RHTREENODE_HPP__
#define PIE_SRC_INCLUDE_RHTREE_RHTREENODE_HPP__

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <tuple>

#include "allocator.hpp"
#include "index.hpp"
#include "internal_string.h"
#include "persist.h"
#include "rhtreecity.hpp"

// Atomic 8 bytes for Leaf Node Meta Data
// Bit Map for Atomic Leaf Node Meta Data
// |-------_next--------|---ptr_start---|---ptr_num---|---height---|
// 63                 16 15            8 7           5 4           0

// An atomic kv pair structure
// Bit Map for atomic kv meta Data
// |------_offset--------|--_signature--|--_cache--|
// 63                  16 15           8 7         0

#define CACHE_BIT 0
#define SIG_BIT 8
#define OFFSET_BIT 16

#define NEXT_BIT 16
#define PTR_START_BIT 8
#define PTR_NUM_BIT 5
#define HEIGHT_BIT 0

#define CACHE_ZERO_MASK 0xFFFFFFFFFFFFFF00
#define SIG_ZERO_MASK 0xFFFFFFFFFFFF00FF
#define OFFSET_ZERO_MASK 0x000000000000FFFF

#define NEXT_ZERO_MASK 0x000000000000FFFF
#define PTR_START_ZERO_MASK 0xFFFFFFFFFFFF00FF
#define PTR_NUM_ZERO_MASK 0xFFFFFFFFFFFFFF1F
#define HEIGHT_ZERO_MASK 0xFFFFFFFFFFFFFFE0

#define SEED1 (4396)
#define SEED2 (1995)

namespace PIE {
namespace RHTREE {

using RHTREE_Key_t = InternalString;
using RHTREE_Value_t = void *;

// A bunch of constant value for Tree configurations
constexpr size_t kCacheLineSize = 64;
constexpr size_t kBucketSize = 64;
constexpr size_t kChildNumber = 256;
constexpr size_t kInitLeafNum = 128;
constexpr size_t kSlotNumPerBucket = kBucketSize / sizeof(uint64_t);
constexpr size_t kBucketNumPerLeaf = 32;  // which means a leaf is 2KB

// RHTree use non-zero signature to validate a hash slot
// We need two fixed hash value when one key has zero
// signature
constexpr uint8_t kDefaultHash1 = 17;
constexpr uint8_t kDefaultHash2 = 251;

struct RHTreeLock;  // A lightweight lock that provides
                    // both leaf lock in tree and hash bucket
                    // lock within a hash table

// Inline functions for fetching or manipulating slots' data
inline uint8_t FETCH_CACHE(const uint64_t slot) {
  return slot >> CACHE_BIT & 0xFF;
}

inline uint8_t FETCH_SIG(const uint64_t slot) { return slot >> SIG_BIT & 0xFF; }

inline uint64_t FETCH_OFFSET(const uint64_t slot) {
  return slot >> OFFSET_BIT & 0xFFFFFFFFFFFF;
}

inline void SET_CACHE(uint64_t &slot, uint8_t cache) {
  (slot &= CACHE_ZERO_MASK) |= (cache << CACHE_BIT);
}

inline void SET_SIG(uint64_t &slot, uint8_t sig) {
  (slot &= SIG_ZERO_MASK) |= (sig << SIG_BIT);
}

inline void SET_OFFSET(uint64_t &slot, uint64_t off) {
  (slot &= OFFSET_ZERO_MASK) |= (off << OFFSET_BIT);
}

// Some simple inline functions for manipulating a leaf node's metadata
// The corresponding bit layout is shown above
inline uint64_t FETCH_NEXT(const uint64_t slot) {
  return slot >> NEXT_BIT & 0xFFFFFFFFFFFF;
}

inline uint8_t FETCH_PTR_START(const uint64_t slot) {
  return slot >> PTR_START_BIT & 0xFF;
}

inline uint8_t FETCH_PTR_NUM(const uint64_t slot) {
  return slot >> PTR_NUM_BIT & 0x07;
}

inline uint8_t FETCH_HEIGHT(const uint64_t slot) {
  return slot >> HEIGHT_BIT & 0x1F;
}

inline void SET_NEXT(uint64_t &slot, uint64_t next) {
  (slot &= NEXT_ZERO_MASK) |= (next << NEXT_BIT);
}

inline void SET_PTR_START(uint64_t &slot, uint8_t ptr_start) {
  (slot &= PTR_START_ZERO_MASK) |= (ptr_start << PTR_START_BIT);
}

inline void SET_PTR_NUM(uint64_t &slot, uint8_t ptr_num) {
  (slot &= PTR_NUM_ZERO_MASK) |= (ptr_num << PTR_NUM_BIT);
}
inline void SET_HEIGHT(uint64_t &slot, uint8_t height) {
  (slot &= HEIGHT_ZERO_MASK) |= (height << HEIGHT_BIT);
}

inline bool ValidCache(uint8_t cache, uint8_t lbound, uint8_t rbound) {
  return (lbound <= cache && cache <= rbound);
}

// Other helpful functions
inline uint64_t Hash1(const RHTREE_Key_t &key) {
  return CityHash64WithSeed((const char *)(key.Data()), key.Length(), SEED1);
}

inline uint64_t Hash2(const RHTREE_Key_t &key) {
  return CityHash64WithSeed((const char *)(key.Data()), key.Length(), SEED2);
}

inline uint8_t Signature1(uint64_t hash) {
  uint8_t ret = (hash >> 32) & 0xFF;
  return (ret == 0) ? 17 : ret;
}

inline uint8_t Signature2(uint64_t hash) {
  uint8_t ret = (hash >> 32) & 0xFF;
  return (ret == 0) ? 251 : ret;
}

struct RHTreeLock {
 public:
  // Init all fields to be zero by default
  RHTreeLock() { memset(this, 0, sizeof(RHTreeLock)); }

 public:
  int try_rlock() {
    if (std::atomic_load(&writer_cnt) != 0) {
      return 1;
    }
    reader_cnt.fetch_add(1, std::memory_order_acquire);
    if (std::atomic_load(&writer_cnt) != 0) {
      reader_cnt.fetch_sub(1, std::memory_order_acquire);
      return 1;
    }
    return 0;
  }

  int try_wlock() {
    if (std::atomic_load(&reader_cnt) != 0) {
      return 1;
    }
    // try lock writer
    uint16_t expect = 0;
    if (writer_cnt.compare_exchange_weak(expect, 1,
                                         std::memory_order_acquire) == false) {
      return 1;
    }
    while (std::atomic_load(&reader_cnt) != 0)
      ;
    return 0;
  }

  void wr_unlock() { writer_cnt.store((uint16_t)0, std::memory_order_release); }

  void rd_unlock() { reader_cnt.fetch_sub(1, std::memory_order_release); }

  void BucketLock(int idx) {
    bool expect = false;
    while (!bucketlocks[idx].compare_exchange_strong(
        expect, true, std::memory_order_acquire)) {
      expect = false;
    }  // simple spin lock
  }

  void BucketUnLock(int idx) {
    bucketlocks[idx].store(false, std::memory_order_release);
  }  // Release spin lock

  std::atomic_uint16_t reader_cnt;
  std::atomic_uint16_t writer_cnt;
  std::atomic_bool bucketlocks[kBucketNumPerLeaf];
};

// A base class to indicate a variety kinds of node
// This Node only provides Node type check: Leaf or
// internal node;
struct Node {
  bool is_leaf;
  bool IsLeaf() const { return is_leaf; }
};

// An internal node class that have 256 children(1B) to  indicate
// next level node
struct InternalNode : public Node {
 public:
  void SetChild(uint32_t lbound, uint32_t rbound, Node *child) {
    for (decltype(lbound) i = lbound; i <= rbound; ++i) {
      children[i] = child;
    }
  }
  // Next level node pointers
  Node *children[kChildNumber];
};

// A leaf node class
// Leaf contains multiple hash buckets, which is similar to
// segment in CCEH or Dash. LeafNode can be replaced if you
// have specific optimizations on hash table, but remember
// do not make leaf too large to support scan range.
struct LeafNode : public Node {
 public:
  struct HashBucket {
   public:
    // search for a target item within a bucket:
    // match fingerprint(fp)->cache->complete key comparasion
    // if key does not exist, return -1
    int FindExist(const RHTREE_Key_t &key, uint8_t fp, uint8_t cache) const;

    // Search for an empty slot and return its idx. The number of
    // free slot is returned as well
    // If there is no free slot, return -1; Otherwise return the
    // free slot with minimal index
    int FindEmpty(uint8_t lbound, uint8_t rbound, int *free_slot) const;

    // Traverse the bucket, return the index of a slot that matches current
    // key and return a free slot's index.
    // We use auto return type to return a pair which contains:
    //  first the exist key's index and second the free slot index
    auto FindEmptyAndExist(const RHTREE_Key_t &key, uint8_t fp, uint8_t cache,
                           uint8_t lbound, uint8_t rbound) const;

   public:
    // each bucket occupies exactly multiple cache lines
    uint64_t slots[kSlotNumPerBucket];
  };

 public:
  // Init current leaf, including allocate lock space, init all memory
  // to be zero
  void Init() {
    memset(this, 0, sizeof(LeafNode));
    // Allocate Lock in DRAM
    lock = new RHTreeLock();
    is_leaf = true;
  }

  // A bunch of leaf node operation interface for manipulating hash items in
  // one leaf node, this enables decouple of tree operation and hash table
  // operation
  status_code_t leaf_insert(const RHTREE_Key_t &key,
                            const RHTREE_Value_t &value, Allocator *allocator);

  status_code_t leaf_search(const RHTREE_Key_t &key, RHTREE_Value_t &value);

  status_code_t leaf_update(const RHTREE_Key_t &key,
                            const RHTREE_Value_t &value, Allocator *allocator);

 public:
  // Lock mechanism for concurrency control. These methods only work for leaf
  // node's control in tree: for example, prohibiting two threads splitting
  // the same leaf node, allowing normal operation(insert, search) threads to
  // access the same node
  int TryRdLock() { return lock->try_rlock(); }
  int TryWrLock() { return lock->try_wlock(); }
  void UnRdLock() { lock->rd_unlock(); }
  void UnWrLock() { lock->wr_unlock(); }

  // For any VALID items in current leaf, replace its byte of height
  // position to current slot.By VALID we mean its signature is non-zero
  // and cache field equals to old_ptr_start.
  void ReplaceCache(uint8_t old_ptr_start, uint8_t height);

  // Other information fetching method
  auto GetPtrRange() const {
    uint8_t lbound = FETCH_PTR_START(meta),
            rbound = lbound - 1 + (1 << FETCH_PTR_NUM(meta));
    return std::tuple(lbound, rbound);
  }

  // Check if target key matches with the leaf's prefix
  bool PrefixMatch(const RHTREE_Key_t &key, int *height,
                   InternalNode **root) const;

  uint64_t SlotNum() const { return kBucketNumPerLeaf * kSlotNumPerBucket; }

  // Return Hash Table position and whole hash table size
  // We need these two methods when copying data for split
  char *HashDataAddr() { return reinterpret_cast<char *>(&buckets_[0]); }
  uint64_t HashSize() const { return sizeof(buckets_); }

 public:
  uint64_t meta;       // A meta that contains this leaf's meta infomation
  uint8_t prefix[16];  // an prefix array, 16B is basically enough
  std::atomic_bool split_flag;  // prevent two threads splitting one leaf
  RHTreeLock *lock;             // For concurrency control
  InternalNode *parent;         // points to its parent
  uint8_t padding[kCacheLineSize - 48];

  // Multiple Hash Buckets to form a hash table
  HashBucket buckets_[kBucketNumPerLeaf];
};

inline bool LeafNode::PrefixMatch(const RHTREE_Key_t &key, int *height,
                                  InternalNode **root) const {
  int leaf_height = FETCH_HEIGHT(meta);
  // Leaf's prefix is not consistent with target key
  if (leaf_height != 0 && memcmp(prefix, key.Data(), leaf_height) != 0) {
    return false;
  }

  // Check if pointer range matches
  // A mismatch may occur in multi-threads condition when
  // one thread access a splitting leaf
  auto [lbound, rbound] = GetPtrRange();
  uint8_t byte = key[leaf_height];
  if (!ValidCache(byte, lbound, rbound)) {
    *height = leaf_height;
    *root = parent;
    return false;
  }

  return true;
}

inline int LeafNode::HashBucket::FindExist(const RHTREE_Key_t &key, uint8_t fp,
                                           uint8_t cache) const {
  auto i = decltype(kSlotNumPerBucket){0};
  for (i = 0; i < kSlotNumPerBucket; ++i) {
    if ((FETCH_SIG(slots[i]) == fp) && (FETCH_CACHE(slots[i]) == cache) &&
        (static_cast<RHTREE_Key_t>(FETCH_OFFSET(slots[i])) == key)) {
      // If both Fingerprint, Cache is matched, we compare the
      // complete key
      return i;
    }
  }
  // no match, return -1 to indicate this key does not
  // exist in current bucket
  return -1;
}

inline int LeafNode::HashBucket::FindEmpty(uint8_t lbound, uint8_t rbound,
                                           int *freeslot_cnt) const {
  int free_cnt = 0, emptyslot = -1;
  auto i = decltype(kSlotNumPerBucket){0};
  for (i = 0; i < kSlotNumPerBucket; ++i) {
    if ((FETCH_SIG(slots[i]) == 0) ||
        !ValidCache(FETCH_CACHE(slots[i]), lbound, rbound)) {
      // Signature = 0 means this slot has been deleted
      // or not being occupied
      // !ValidCache means this slot doesn't match this leaf
      // node's pattern due to normal split
      ++free_cnt;
      if (emptyslot == -1) {
        emptyslot = i;
      }  // replace return value
    }
  }
  *freeslot_cnt = free_cnt;
  return emptyslot;
}

inline auto LeafNode::HashBucket::FindEmptyAndExist(const RHTREE_Key_t &key,
                                                    uint8_t fp, uint8_t cache,
                                                    uint8_t lbound,
                                                    uint8_t rbound) const {
  int existslot = -1, emptyslot = -1;
  auto i = decltype(kSlotNumPerBucket){0};
  for (i = 0; i < kSlotNumPerBucket; ++i) {
    if ((FETCH_SIG(slots[i]) == fp) && (FETCH_CACHE(slots[i] == cache)) &&
        (static_cast<RHTREE_Key_t>(FETCH_OFFSET(slots[i])) == key)) {
      existslot = i;
    }

    if ((FETCH_SIG(slots[i]) == 0) ||
        !ValidCache(FETCH_CACHE(slots[i]), lbound, rbound)) {
      if (emptyslot == -1) {
        emptyslot = i;  // Return the first met empty slot
      }
    }
  }
  return std::tuple(existslot, emptyslot);
}

};  // namespace RHTREE
};  // namespace PIE

#endif  // PIE_SRC_INCLUDE_RHTREE_RHTREENODE_HPP__