#ifndef PIE_SRC_INDEX_WORT_WORT_HPP__
#define PIE_SRC_INDEX_WORT_WORT_HPP__

#include <byteswap.h>

#include <cstdint>

#include "allocator.hpp"
#include "index.hpp"
#include "internal_string.h"
#include "persist.h"
#include "status.hpp"

namespace PIE {
namespace WORT {

// each node of wort will recognize a token of 4bits
constexpr uint64_t kNodeBits = 4;
constexpr uint64_t kMaxDepth = 15;
constexpr uint64_t kNumNodeEntries = 0x1UL << kNodeBits;
constexpr uint64_t kLowBitMask = (0x1UL << kNodeBits) - 1;  // recognize token
constexpr uint64_t kMaxPrefixLen = 6;
constexpr uint64_t kMaxHeight = kMaxDepth + 1;

class WORTIndex : public Index {
 public:
  // Header of WORT node
  //  This header can be updated atomically to guarantee
  // Crash consistence
  struct art_node {
    uint8_t depth;
    uint8_t partial_len;
    uint8_t partial[kMaxPrefixLen];
  };

  // A WORT data store node with 16 child pointers
  struct art_node16 {
    art_node n;
    art_node *children[kNumNodeEntries];
  };

  struct art_leaf {
    void *value;
    uint32_t key_len;
    uint8_t key[];  // append key's content here
  };

 public:
  // The only constructor to create an empty tree
  WORTIndex(Allocator *nvmallocator)
      : nvmallocator_(nvmallocator), root_(nullptr), size_(0) {}

  // Default constructor will use Dram allcator and the whole
  // tree structure will be stored in DRAM
  WORTIndex()
      : nvmallocator_(new PIEDRAMAllocator()), root_(nullptr), size_(0) {}

  // Any copyable semantics is not allowed
  WORTIndex(const WORTIndex &) = delete;
  WORTIndex &operator=(const WORTIndex &) = delete;

 private:
  // Allocate an inner node
  art_node *AllocNode();

  // Allocate a leaf and store target key information. This function is expected
  // to return a persisted leaf node
  art_leaf *AllocLeaf(const char *key, size_t key_len, void *value);

  // Insert key into subtrie specified by input parameter art_node *n.
  // Insertion may cause subtrie be replaced, e.g prefix split. art_node **ref
  // is used to modify coresponding childpointer points to new subtrie
  void *recursive_insert(art_node *n, art_node **ref, const char *key,
                         size_t key_len, void *value, int depth, int *old,
                         bool replace);

 private:
  // Some helper functions

  // Check if search key matches key stored in input leaf. return zero if
  // matches, otherwise a non-zero value will be returned
  int leaf_matches(const art_leaf *leaf, const char *key, size_t key_len);

  // Calculate the index position of leaf1 and leaf2's longest commmon prefix
  // since "depth" index position
  int longest_common_prefix(art_leaf *leaf1, art_leaf *leaf2, int depth);

  // Return the index at which the node's prefix and key's prefix mismatches
  // the minimum return value is zero, which indicates the doesn't match at all
  int prefix_mismatch(const art_node *n, const char *key, size_t key_len,
                      int depth, art_leaf **leaf);

  // Set the c'th child points to input child
  void add_child(art_node16 *n, uint8_t c, void *child) {
    n->children[c] = (art_node *)child;
  }

  // Fetch the c'th child of n
  art_node **find_child(art_node *n, uint8_t c) {
    art_node16 *p = (art_node16 *)n;
    if (p->children[c]) {
      return &p->children[c];
    }
    return nullptr;
  }

  // Return the idx "token" of key. In WORT, each token is set to be 4bits
  uint8_t TokenAt(const char *key, int idx);

  // Set the idx "token" of key array. In WORT, each token is set to be 4bits
  void SetToken(char *key, int idx, uint8_t token);

  // This function is the read interface of original ART implementation
  void *art_search(const char *key, size_t key_len);

  // Return the number of prefix characters shared between the key and the node
  int check_prefix(const art_node *n, const char *key, size_t key_len,
                   int depth);

 public:
  status_code_t Insert(const char *key, size_t key_len, void *value) override {
    int old_val = 0;
    void *old = recursive_insert(root_, &root_, key, key_len, value, 0,
                                 &old_val, false);
    // return nullptr means every thing is ok
    if (old == nullptr) {
      if (!old_val) {  // value does not exist
        size_++;
        return kOk;
      } else {
        return kInsertKeyExist;
      }
    }
    return kNotDefined;
  }

  status_code_t Search(const char *key, size_t key_len, void **value) override {
    auto searchval = art_search(key, key_len);
    if (searchval == nullptr) {
      return kNotFound;
    }
    *value = searchval;
    return kOk;
  }

  status_code_t Update(const char *key, size_t key_len, void *value) override {
    // TODO
    return kOk;
  }

  status_code_t Upsert(const char *key, size_t key_len, void *value) override {
    int old_val = 0;
    void *old =
        recursive_insert(root_, &root_, key, key_len, value, 0, &old_val, true);
    if (old == nullptr) {
      return kOk;  // Upsert always succeed
    }
    return kNotDefined;
  }

  status_code_t ScanCount(const char *startkey, size_t key_len, size_t count,
                          void **vec) override {
    // TODO
    return kOk;
  }

  status_code_t Scan(const char *startkey, size_t startkey_len,
                     const char *endkey, size_t endkey_len,
                     void **vec) override {
    // TODO
    return kOk;
  }

  void Print() override { std::cout << "[WORT][Key Num: " << size_ << "]\n"; }

 private:
  Allocator *nvmallocator_;  // allocator for memory management
  art_node *root_;           // root of the radix tree
  uint64_t size_;            // record the number of different keys
};

inline WORTIndex::art_node *WORTIndex::AllocNode() {
  // Allocate cacheline aligned memory
  art_node *ret = reinterpret_cast<art_node *>(
      nvmallocator_->AllocateAlign(sizeof(art_node16), 64));
  // Init memory to zero to avoid invalid pointer error,
  // e.g segment fault
  memset((void *)ret, 0, sizeof(art_node16));
  return ret;
}

inline WORTIndex::art_leaf *WORTIndex::AllocLeaf(const char *key,
                                                 size_t key_len, void *value) {
  // Allocate space for both key content and leaf struct
  size_t alloc_size = sizeof(art_leaf) + key_len;
  art_leaf *leaf = reinterpret_cast<art_leaf *>(
      nvmallocator_->AllocateAlign(alloc_size, 64));

  leaf->value = value;
  leaf->key_len = key_len;
  memcpy(leaf->key, key, key_len);

  persist_data((char *)leaf, alloc_size);  // persist
  return leaf;
}

inline int WORTIndex::leaf_matches(const art_leaf *leaf, const char *key,
                                   size_t key_len) {
  if (leaf->key_len != key_len) {
    return 1;
  }
  return memcmp(key, leaf->key, key_len);
}

inline uint8_t WORTIndex::TokenAt(const char *key, int idx) {
  // calculate the byte position which the "idx" token belongs to
  uint8_t byte = key[idx / 2];
  // If idx is even, e.g: 0, fetch the high 4 bits, otherwise
  // the lower 4bits
  return (idx & 1) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
}

inline void WORTIndex::SetToken(char *key, int idx, uint8_t token) {
  uint8_t byte = key[idx / 2];  // modify on target byte position
  if (idx & 1) {
    // Set the lower 4bits of byte
    (byte &= 0xF0) |= token;  // first set target position to zero
  } else {
    (byte &= 0x0F) |= (token << 4);  // Note token needs to shift as well
  }
  key[idx / 2] = byte;
}

inline int WORTIndex::longest_common_prefix(art_leaf *leaf1, art_leaf *leaf2,
                                            int depth) {
  int idx, max_cmp = kMaxHeight - depth;
  for (idx = 0; idx < max_cmp; ++idx) {
    if (TokenAt((const char *)leaf1->key, depth + idx) !=
        TokenAt((const char *)leaf2->key, depth + idx)) {
      return idx;
    }
  }
  return idx;
}

inline int WORTIndex::prefix_mismatch(const art_node *n, const char *key,
                                      size_t key_len, int depth,
                                      art_leaf **leaf) {
  int max_cmp = std::min(std::min(kMaxPrefixLen, (uint64_t)n->partial_len),
                         kMaxHeight - (uint64_t)depth);
  int idx = 0;
  for (idx = 0; idx < max_cmp; idx++) {
    if (TokenAt((const char *)(n->partial), idx) != TokenAt(key, depth + idx)) {
      return idx;
    }
  }
  return idx;
}

inline int WORTIndex::check_prefix(const art_node *n, const char *key,
                                   size_t key_len, int depth) {
  int max_cmp = std::min(std::min((uint64_t)n->partial_len, kMaxPrefixLen),
                         kMaxHeight - (uint64_t)depth);
  int idx;
  for (idx = 0; idx < max_cmp; idx++) {
    if (TokenAt((const char *)n->partial, idx) != TokenAt(key, depth + idx)) {
      return idx;
    }
  }
  return idx;
}

};  // namespace WORT
};  // namespace PIE

#endif  // PIE_SRC_INDEX_WORT_WORT_HPP__