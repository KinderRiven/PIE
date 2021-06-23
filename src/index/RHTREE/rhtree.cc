#include "rhtree.hpp"
#include "persist.h"

namespace PIE {
namespace RHTREE {

  // Building an empty RHTree
  void RHTreeIndex::Init() {
    // Allocate root
    root_ = AllocINode();

    // allocate multiple leaf nodes together
    size_t allocsize = sizeof(RHTreeLeaf) * kInitLeafNum;
    RHTreeLeaf *init_leaf = reinterpret_cast<RHTreeLeaf *>  
          (nvm_allocator_->AllocateAlign (allocsize, kCacheLineSize));
    
    memset((void *)init_leaf, 0, allocsize);
    
    uint8_t avg_ptr_num = kChildNumber / kInitLeafNum;
    // builtin_ffs can get the exponent part of target value
    uint8_t avg_eptr_num = __builtin_ffs(avg_ptr_num) - 1;

    for (auto i = decltype(kInitLeafNum){0}; i < kInitLeafNum; ++i) {

      RHTreeLeaf *leaf = init_leaf + i;

      // Init each leaf
      leaf->Init();

      // Set parents and set its child pointer to coresponding leaf
      leaf->parent = root_;
      root_->SetChild (i * avg_ptr_num, (i+1) * avg_ptr_num - 1, leaf);

      RHTreeLeaf *next = (i == kInitLeafNum - 1) ? nullptr : (leaf + 1);

      // Set meta data
      uint64_t meta = 0;
      SET_NEXT      (meta, (uint64_t)next);
      SET_PTR_START (meta, i * avg_ptr_num);
      SET_PTR_NUM   (meta, avg_eptr_num);
      SET_HEIGHT    (meta, 0);

      leaf->meta = meta;
    }
  }

  void RHTreeIndex::Recover() {
    // TODO
  }

  status_code_t RHTreeIndex::Insert(const char *key, size_t key_len, 
                                    void *value ) {
    
    // We place internal_key + value together. Padding additional bytes
    // after key to make value is 8B aligned
    size_t mod = (sizeof(uint32_t) + key_len) % 8;
    size_t padding = (mod == 0 ? 0 : 8 - mod);
    size_t alloc_size = sizeof(uint32_t) + key_len + padding 
                      + sizeof(void *);

    void *dataptr = nvm_allocator_->Allocate(alloc_size);
    RHTREE_Key_t internalkey(key, key_len, (uint8_t *)dataptr);

    // Set value behind key
    *reinterpret_cast<void **>((char *)dataptr + alloc_size - sizeof(void*)) = value;
    persist_data((char *)dataptr, alloc_size);

    for (;;) {
      // reaches coresponding node and do insert operation
      RHTreeLeaf *leaf = decend_to_leaf(internalkey);
      auto stat = leaf->leaf_insert(internalkey, value, nvm_allocator_);
      leaf->UnRdLock(); // release lock to avoid deadlock

      // If leaf insert fails due to hash table full, split current
      // leaf node to make space for incomming insertion. 
      // To prevent multiple threads perform split on the same node,
      // one split flag is needed, which ensures only one thread is 
      // splitting current leaf
      if (stat == kNeedSplit) {
        bool flag = false;
        if (leaf->split_flag.compare_exchange_strong (flag, true)) {
          // Splitting thread has to wait other threads exit current
          // leaf
          while (leaf->TryWrLock () != 0) ;
          leaf = split (leaf);
          leaf->UnWrLock ();
          leaf->split_flag.store (false);
        }
        // Top-Down traverse again
        leaf = nullptr;
      } else {
        if (stat == kInsertKeyExist) {
          nvm_allocator_->Free(dataptr);
        }
        return stat;
      }
    }// This loop ensures that this function always has 
     // a return value
  }

  status_code_t RHTreeIndex::Search(const char *key, size_t key_len, 
                                    void **value) {
    static thread_local uint8_t key_buff[1024];
    RHTREE_Key_t internalkey(key, key_len, key_buff);

    RHTreeLeaf *leaf = decend_to_leaf(internalkey);
    return leaf->leaf_search(internalkey, *value);
  }

  status_code_t RHTreeIndex::Update(const char *key, size_t key_len, 
                                    void *value ) {
    static thread_local uint8_t key_buff[1024];
    RHTREE_Key_t internalkey(key, key_len, key_buff);

    RHTreeLeaf *leaf = decend_to_leaf(internalkey);
    return leaf->leaf_update(internalkey, value, nvm_allocator_);
  }

  status_code_t RHTreeIndex::Upsert(const char *key, size_t key_len, 
                                    void *value ) { 
    // TODO
    return kOk;
  }

  status_code_t RHTreeIndex::ScanCount(const char *startkey, size_t key_len, 
                                       size_t count, void **vec) {
    // TODO
    return kOk;
  }

  status_code_t RHTreeIndex::Scan(const char *startkey, size_t startkey_len, 
                                  const char *endkey  , size_t endkey_len , 
                                  void **vec) {
    // TODO
    return kOk;
  }

  RHTreeLeaf *RHTreeIndex::split(RHTreeLeaf *leaf) {
    if (FETCH_PTR_NUM(leaf->meta) == 0) {
      return levelsplit(leaf);
    } else {
      return normalsplit(leaf);
    }
  }

  RHTreeLeaf *RHTreeIndex::normalsplit(RHTreeLeaf *leaf) {
    // Calculate basic information
    uint64_t meta = leaf->meta;
    uint8_t new_ptr_num = FETCH_PTR_NUM (meta) - 1;
    uint8_t new_ptr_start = FETCH_PTR_START (meta) + (1 << new_ptr_num);
    uint8_t old_height = FETCH_HEIGHT (meta);

    RHTreeLeaf *new_leaf = AllocLeaf ();

    // step1: set new node's attribute
    new_leaf->parent = leaf->parent;

    // Set new leaf's meta data
    uint64_t newMeta = 0;
    SET_NEXT      (newMeta, FETCH_NEXT (meta));
    SET_PTR_START (newMeta, new_ptr_start);
    SET_PTR_NUM   (newMeta, new_ptr_num);
    SET_HEIGHT    (newMeta, old_height);

    memcpy (new_leaf->prefix, leaf->prefix, old_height);
    new_leaf->meta = newMeta;
    
    // Copy hash table data to new node: No need to delete invalid slot in 
    // both old and new node to make space for incomming insertion due to 
    // lazy deletion
    memcpy (new_leaf->HashDataAddr (), leaf->HashDataAddr (), new_leaf->HashSize ());
    persist_data ((char *)new_leaf, sizeof (RHTreeLeaf));

    new_leaf->parent->SetChild (new_ptr_start, 
                                new_ptr_start-1+(1 << new_ptr_num), 
                                new_leaf);

    SET_NEXT (meta, (uint64_t)new_leaf);
    SET_PTR_NUM (meta, new_ptr_num);
    // Write next pointer of current leaf to make new created
    // leaf visible
    leaf->meta = meta;

    return leaf;
  }

  RHTreeLeaf *RHTreeIndex::levelsplit(RHTreeLeaf *leaf) {
    uint64_t meta = leaf->meta;

    InternalNode *new_inode = AllocINode(), *parent = leaf->parent;
    uint64_t old_idx = FETCH_PTR_START (meta);
    RHTreeLeaf *new_leaf = AllocLeaf();
    uint32_t old_ptr_start = old_idx;
    uint32_t old_height = FETCH_HEIGHT (meta);

    // step1: Set new leaf's attributes
    new_leaf->parent = new_inode;
    // set meta data
    uint64_t newMeta = 0;
    SET_NEXT      (newMeta, FETCH_NEXT (meta));
    SET_PTR_START (newMeta, kChildNumber / 2);
    SET_PTR_NUM   (newMeta, 7);
    SET_HEIGHT    (newMeta, old_height + 1);

    // set prefix
    memcpy (new_leaf->prefix, leaf->prefix, old_height);
    new_leaf->prefix[old_height] = old_idx;
    new_leaf->meta = newMeta;
    
    SET_NEXT      (meta, (uint64_t)new_leaf);
    SET_PTR_START (meta, 0);
    SET_PTR_NUM   (meta, 7);
    SET_HEIGHT    (meta, old_height + 1);

    // No need to persist data as Replace Cache already done it
    leaf->ReplaceCache (old_ptr_start, old_height+1);

    // However, new node needs to persist data due to lazy deletion
    memcpy (new_leaf->HashDataAddr (), leaf->HashDataAddr(), new_leaf->HashSize());
    persist_data ((char *)new_leaf, sizeof (RHTreeLeaf));

    leaf->parent = new_inode;
    leaf->prefix[old_height] = old_idx;
    asm_mfence ();  // one fence is enough as meta and prefix is in the 
                    // same cache line

    // Modify old leaf's meta data as last step 
    leaf->meta = meta;
    asm_clwb ((char *)(&(leaf->meta)));

    // persist data makes sure that change parent's child pointer
    // occurs at the last step
    // set parent to make it visible
    new_leaf->parent->SetChild (0, kChildNumber / 2 - 1, leaf);
    new_leaf->parent->SetChild (kChildNumber / 2, kChildNumber - 1, new_leaf);
    parent->SetChild (old_idx, old_idx, new_inode);

    return leaf;
  }

};  // namespace RHTREE
};  // namespace PIE