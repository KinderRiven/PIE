#ifndef PIE_SRC_INDEX_RHTREE_RHTREE_HPP__
#define PIE_SRC_INDEX_RHTREE_RHTREE_HPP__

#include <iostream>

#include "rhtreenode.hpp"

namespace PIE {
namespace RHTREE {

using RHTreeLeaf = LeafNode;

class RHTreeIndex : public Index {
 public:
  // Create a RHTree index
  // The input parameter recover means rebulding or recover
  // from pmem pool specified by nvm_allocator
  RHTreeIndex(Allocator *dram_allocator, Allocator *nvm_allocator,
              bool recover = false)
      : dram_allocator_(dram_allocator), nvm_allocator_(nvm_allocator) {
    if (recover) {
      Recover();
    } else {
      Init();
    }
  }

  RHTreeIndex() = delete;
  RHTreeIndex(const RHTreeIndex &) = delete;
  RHTreeIndex &operator=(const RHTreeIndex &) = delete;

  ~RHTreeIndex();

 private:
  // Recover from given pmem pool
  void Recover();
  // Building RHTree from scratch
  void Init();

 public:
  status_code_t Insert(const char *key, size_t key_len, void *value) override;

  status_code_t Search(const char *key, size_t key_len, void **value) override;

  status_code_t Update(const char *key, size_t key_len, void *value) override;

  status_code_t Upsert(const char *key, size_t key_len, void *value) override;

  status_code_t ScanCount(const char *startkey, size_t key_len, size_t count,
                          void **vec) override;

  status_code_t Scan(const char *startkey, size_t startkey_len,
                     const char *endkey, size_t endkey_len,
                     void **vec) override;

  void Print() override {
    std::cout << "[INode Size: " << sizeof(InternalNode) << "]"
              << "[Leaf Size: " << sizeof(LeafNode) << "]\n"
              << "[Leaf is cacheline Aligned: "
              << (sizeof(LeafNode) % kCacheLineSize == 0) << "]\n";
  }

 private:
  // splitting a target leaf node and return the splitted
  // node. Newly created leaf node can be accessed with
  // next pointer in leaf.meta
  LeafNode *split(LeafNode *);

  // Do normal split for target leaf node. Return value is
  // the same as split function above;
  LeafNode *normalsplit(LeafNode *);

  // Do level split for target leaf node. Return value is
  // the same as split function above
  LeafNode *levelsplit(LeafNode *);

  // Allocate an inner node with dram allocator and
  // do initializations
  InternalNode *AllocINode() {
    auto ret = reinterpret_cast<InternalNode *>(
        dram_allocator_->Allocate(sizeof(InternalNode)));
    ret->is_leaf = false;
    return ret;
  }

  // Allocate a leaf node with nvm allocator and
  // do initializations
  RHTreeLeaf *AllocLeaf() {
    auto ret = reinterpret_cast<RHTreeLeaf *>(
        nvm_allocator_->AllocateAlign(sizeof(RHTreeLeaf), kCacheLineSize));
    ret->is_leaf = true;
    ret->Init();
    return ret;
  }

  // For a specific key, traverse from the root node to its coresponding
  // leaf node. This may occur multiple retry
  RHTreeLeaf *decend_to_leaf(const RHTREE_Key_t &key);

 private:
  InternalNode *root_;  // The root of the whole tree structure
  Allocator *dram_allocator_, *nvm_allocator_;
};

inline RHTreeLeaf *RHTreeIndex::decend_to_leaf(const RHTREE_Key_t &key) {
  InternalNode *root = root_;
  int height = 0;
  RHTreeLeaf *leaf = nullptr;

  // retry until prefix matches
  while (leaf == nullptr || !leaf->PrefixMatch(key, &height, &root)) {
    if (leaf != nullptr) {
      leaf->UnRdLock();
    }
    Node *curr = root;
    int curr_height = height;
    // Move down until reaches a leaf
    while (!curr->IsLeaf()) {
      curr =
          reinterpret_cast<InternalNode *>(curr)->children[key[curr_height++]];
    }
    leaf = reinterpret_cast<RHTreeLeaf *>(curr);
    // Spin until access this leaf
    while (leaf->TryRdLock() != 0)
      ;
  }
  return leaf;
}

};  // namespace RHTREE
};  // namespace PIE

#endif  // RHTREE_HPP__