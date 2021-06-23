#include "rhtreenode.hpp"

#include "persist.h"

namespace PIE {
namespace RHTREE {

// For any slot stored in current node, use its byte at
// height position to replace current slot's cache
// Only replace those slots whoes cache is the same as input
// parameter old_ptr_start.
void LeafNode::ReplaceCache(uint8_t old_ptr_start, uint8_t height) {
  auto i = decltype(kBucketNumPerLeaf){0};
  auto j = decltype(kSlotNumPerBucket){0};

  for (i = 0; i < kBucketNumPerLeaf; ++i) {
    HashBucket *bucketp = buckets_ + i;
    for (j = 0; j < kSlotNumPerBucket; ++j) {
      auto slot = bucketp->slots[j];
      if (FETCH_SIG(slot) == 0) {
        continue;
      }
      if (FETCH_CACHE(slot) != old_ptr_start) {
        // For those invalid slot, we clear its slot to be
        // zero to prevent it affect next level node insertion
        bucketp->slots[j] = 0;
      }

      uint8_t new_cache = static_cast<RHTREE_Key_t>(FETCH_OFFSET(slot))[height];

      SET_CACHE(slot, new_cache);
      bucketp->slots[j] = slot;
    }
    asm_clwb((char *)bucketp);
  }
}

status_code_t LeafNode::leaf_insert(const RHTREE_Key_t &key,
                                    const RHTREE_Value_t &value,
                                    Allocator *allocator) {
  // Get some basic information about this leaf
  auto height = FETCH_HEIGHT(meta);
  auto [lbound, rbound] = GetPtrRange();

  auto hashval = Hash1(key);
  uint8_t fp = Signature1(hashval), bucketidx = hashval % kBucketNumPerLeaf;
  uint8_t cache = key[height];

  // Use input key as key-value pair pointer
  uint64_t writedata = 0;
  SET_OFFSET(writedata, key.Raw());
  SET_SIG(writedata, fp);
  SET_CACHE(writedata, cache);

  HashBucket *bucketp = buckets_ + bucketidx;
  lock->BucketLock(bucketidx);
  // To solve write-wirte conflict, we use spinlock to prevent any
  // two threads concurrently insert into one bucket
  // Besides, bucket lock can prevent two threads insert same key
  auto [existslot, emptyslot] =
      bucketp->FindEmptyAndExist(key, fp, cache, lbound, rbound);

  // unique-key check
  if (existslot >= 0) {
    lock->BucketUnLock(bucketidx);
    return kInsertKeyExist;
  }

  // Free slot check
  if (emptyslot == -1) {
    lock->BucketUnLock(bucketidx);
    return kNeedSplit;
  }

  bucketp->slots[emptyslot] = writedata;
  lock->BucketUnLock(bucketidx);

  asm_clwb(bucketp);
  return kOk;
}

status_code_t LeafNode::leaf_search(const RHTREE_Key_t &key,
                                    RHTREE_Value_t &value) {
  // Get some basic information about this leaf
  auto height = FETCH_HEIGHT(meta);

  auto hashval = Hash1(key);
  uint8_t fp = Signature1(hashval), bucketidx = hashval % kBucketNumPerLeaf;
  uint8_t cache = key[height];

#ifdef SNAPSHOT

  volatile HashBucket *bucketp = buckets_ + bucketidx;  // use volatile to
                                                        // makesure accessing
                                                        // memory
  for (auto i = decltype(kSlotNumPerBucket){0}; i < kSlotNumPerBucket; ++i) {
    uint64_t snapshot = bucketp->slots[i];  // atomically snapshot
    if ((FETCH_SIG(snapshot) == fp) && (FETCH_CACHE(snapshot) == cache) &&
        (static_cast<RHTREE_Key_t>(FETCH_OFFSET(snapshot)) == key)) {
      // Snapshot does not match
      if (snapshot != bucketp->slots[i]) {
        continue;
      }

      RHTREE_Key_t targetkey = FETCH_OFFSET(bucketp->slots[i]);
      // Jump over the key content by adding sizeof(uint32_t) and key's
      // length and padding;
      // calculating padding information.
      uint32_t mod = (key.Length() + sizeof(uint32_t)) % 8;
      uint32_t padding = (mod == 0 ? 0 : 8 - mod);
      value = *reinterpret_cast<RHTREE_Value_t *>(
          targetkey.Raw() + sizeof(uint32_t) + key.Length() + padding);
      return kOk;
    }
  }
  return kNotFound;
#else
  HashBucket *bucketp = buckets_ + bucketidx;
  lock->BucketLock(bucketidx);
  // To solve write-wirte conflict, we use spinlock to prevent any
  // two threads concurrently insert into one bucket
  // Besides, bucket lock can prevent two threads insert same key

  auto existslot = bucketp->FindExist(key, fp, cache);

  if (existslot == -1) {
    // Target key does not exist
    lock->BucketUnLock(bucketidx);
    return kNotFound;
  }

  // fetch target key's value
  RHTREE_Key_t targetkey = FETCH_OFFSET(bucketp->slots[existslot]);
  // Jump over the key content by adding sizeof(uint32_t) and key's
  // length and padding;
  // calculating padding information.
  uint32_t mod = (key.Length() + sizeof(uint32_t)) % 8;
  uint32_t padding = (mod == 0 ? 0 : 8 - mod);
  value = *reinterpret_cast<RHTREE_Value_t *>(
      targetkey.Raw() + sizeof(uint32_t) + key.Length() + padding);

  lock->BucketUnLock(bucketidx);
  return kOk;
#endif
}

status_code_t LeafNode::leaf_update(const RHTREE_Key_t &key,
                                    const RHTREE_Value_t &value,
                                    Allocator *allocator) {
  // update follows almost the same execution path as leaf search
  // does. In this simple implementation, we perform in-place update
  // which means we directly replace existed value with new value
  // Get some basic information about this leaf
  auto height = FETCH_HEIGHT(meta);

  auto hashval = Hash1(key);
  uint8_t fp = Signature1(hashval), bucketidx = hashval % kBucketNumPerLeaf;
  uint8_t cache = key[height];

  HashBucket *bucketp = buckets_ + bucketidx;
  lock->BucketLock(bucketidx);
  // To solve write-wirte conflict, we use spinlock to prevent any
  // two threads concurrently insert into one bucket
  // Besides, bucket lock can prevent two threads insert same key

  auto existslot = bucketp->FindExist(key, fp, cache);

  if (existslot == -1) {
    // Target key does not exist
    lock->BucketUnLock(bucketidx);
    return kNotFound;
  }

  // fetch target key's value
  RHTREE_Key_t targetkey = FETCH_OFFSET(bucketp->slots[existslot]);
  // Jump over the key content by adding sizeof(uint32_t) and key's
  // length and padding bytes
  // calculating padding information.
  uint32_t mod = (key.Length() + sizeof(uint32_t)) % 8;
  uint32_t padding = (mod == 0 ? 0 : 8 - mod);
  RHTREE_Value_t *valueptr = reinterpret_cast<RHTREE_Value_t *>(
      targetkey.Raw() + sizeof(uint32_t) + key.Length() + padding);
  // Do in-place update
  *valueptr = value;
  asm_clwb(valueptr);

  lock->BucketUnLock(bucketidx);
  return kOk;
}
};  // namespace RHTREE
};  // namespace PIE