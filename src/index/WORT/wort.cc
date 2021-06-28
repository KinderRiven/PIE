#include "wort.hpp"

namespace PIE {
namespace WORT {

#define WORT_ISLEAF(x) (((uintptr_t)x & 1))
#define WORT_SETLEAF(x) ((void *)((uintptr_t)x | 1))
#define WORT_LEAFRAW(x) ((art_leaf *)((void *)((uintptr_t)x & ~1)))

void *WORTIndex::recursive_insert(art_node *n, art_node **ref, const char *key,
                                  size_t key_len, void *value, int depth,
                                  int *old, bool replace) {
  // Create a new leaf if facing an empty subtrie
  if (!n) {
    *ref = (art_node *)WORT_SETLEAF(AllocLeaf(key, key_len, value));
    asm_clwb(ref);  // persist reference position
    return nullptr;
  }

  // facing a leaf, may need to check if we need to update this leaf (if key
  // completely matches) or split this leaf with adding an inner node
  if (WORT_ISLEAF(n)) {
    art_leaf *leaf = WORT_LEAFRAW(n);
    // CONDITION1: Updating current leaf Check if updating an existing value
    if (!leaf_matches(leaf, key, key_len)) {
      // if no need to replace this leaf with new value:
      *old = 1;       // indicate this key already exist
      if (replace) {  // upsert
        leaf->value = value;
        persist_data((char *)leaf, sizeof(art_leaf));  // persist
      }
      return nullptr;
    }

    // CONDITION2: Split leaf with adding an inner node. However we need to
    // find out the longest common prefix between new key and old key and "push"
    // them onto the newly created inner node
    art_node16 *new_node = reinterpret_cast<art_node16 *>(AllocNode());
    new_node->n.depth = depth;

    // Create a new leaf to store newly inserted key
    art_leaf *newleaf = AllocLeaf(key, key_len, value);

    // Determine longest prefix
    int longest_prefix = longest_common_prefix(leaf, newleaf, depth);
    new_node->n.partial_len = longest_prefix;

    // Set the longest common prefix of new inner node
    for (uint64_t i = 0; i < std::min(kMaxPrefixLen, (uint64_t)longest_prefix); i++) {
      SetToken((char *)new_node->n.partial, i, TokenAt(key, depth + i));
    }

    // Add child for new node: both old leaf and new leaf
    add_child(new_node,
              TokenAt((const char *)leaf->key, depth + longest_prefix),
              WORT_SETLEAF(leaf));
    add_child(new_node,
              TokenAt((const char *)newleaf->key, depth + longest_prefix),
              WORT_SETLEAF(newleaf));

    persist_data((char *)new_node, sizeof(art_node16));
    persist_data((char *)newleaf, sizeof(art_leaf));

    // Atomically write to make all above change crash consistent
    *ref = (art_node *)new_node;
    asm_clwb(ref);
    asm_sfence();

    return nullptr;
  }

  // Facing an inner node: it denotes a subtrie, we need to check if it needs
  // to do split or else
  if (n->depth != depth) {
    // Recover prefix
  }  // However, in our test, this would never happen

  if (n->partial_len) {
    // If this inner node has some prefix
    // Check current key's lookup token matches this node's prefix or not
    art_leaf *leaf = nullptr;
    int prefix_diff = prefix_mismatch(n, key, key_len, depth, &leaf);
    // CONDITION1. matches
    // Skipped the prefix
    if ((uint32_t)prefix_diff >= n->partial_len) {
      depth += n->partial_len;
      goto RECURSIVE_SEARCH;
    }

    // Condition2. Paritially match
    // Create a new node to split common prefix
    // STEP1. Allocate new inner node and set its first 8B header and pointer
    art_node16 *new_node = (art_node16 *)AllocNode();
    new_node->n.depth = depth;
    new_node->n.partial_len = prefix_diff;
    // Copy prefix_diff number token from n to newnode
    auto copytoken_cnt = std::min(kMaxPrefixLen, (uint64_t)prefix_diff);
    for (auto i = decltype(copytoken_cnt){0}; i < copytoken_cnt; ++i) {
      SetToken((char *)new_node->n.partial, i,
               TokenAt((const char *)n->partial, i));
    }

    // Generate old node update data
    art_node tmp_path;
    if (n->partial_len <= kMaxPrefixLen) {
      add_child(new_node, TokenAt((const char *)n->partial, prefix_diff), n);
      // Set tmp_path related information
      tmp_path.partial_len = n->partial_len - (prefix_diff + 1);
      tmp_path.depth = (depth + prefix_diff + 1);
      // memcpy(tmp_path.partial, n->partial + prefix_diff + 1,
      // std::min(kMaxPrefixLen, (uint64_t)tmp_path.partial_len)); tmp path's
      // prefix needs to be modified
      auto copytoken_cnt =
          std::min(kMaxPrefixLen, (uint64_t)tmp_path.partial_len);

      for (auto i = decltype(copytoken_cnt){0}; i < copytoken_cnt; ++i) {
        SetToken((char *)tmp_path.partial, i,
                 TokenAt((const char *)n->partial, prefix_diff + 1 + i));
      }
    }

    leaf = AllocLeaf(key, key_len, value);
    // Add newly created leaf to new create inner node
    add_child(new_node, TokenAt(key, depth + prefix_diff), WORT_SETLEAF(leaf));
    persist_data((char *)new_node, sizeof(art_node16));
    persist_data((char *)leaf, sizeof(art_leaf));

    // STEP2. Atomically update header of old leaf
    *((uint64_t *)n) = *((uint64_t *)&tmp_path);
    asm_clwb((char *)n);
    asm_mfence();

    // STEP3. Atomically update parent node's pointer to newly created inner
    // node
    *ref = (art_node *)new_node;

    asm_clwb(ref);
    asm_mfence();

    return nullptr;
  }

RECURSIVE_SEARCH:
  // Recursively search next level node only when:
  //  1. Successfully jump over last node's prefix
  //  2. last node has no prefix
  art_node **child = find_child(n, TokenAt(key, depth));
  if (child) {
    return recursive_insert(*child, child, key, key_len, value, depth + 1, old,
                            replace);
  }
  // Current node has no next level, then make a leaf for it
  art_leaf *leaf = AllocLeaf(key, key_len, value);

  // Atomically write
  add_child((art_node16 *)n, TokenAt(key, depth), WORT_SETLEAF(leaf));
  asm_clwb((char *)&(((art_node16 *)n)->children[TokenAt(key, depth)]));
  asm_sfence();

  return nullptr;
}

void *WORTIndex::art_search(const char *key, size_t key_len) {
  art_node **child;
  art_node *n = root_;
  int depth = 0;
  uint64_t prefix_len;

  while (n) {
    if (WORT_ISLEAF(n)) {
      n = reinterpret_cast<art_node *>(WORT_LEAFRAW(n));
      // check if current key matches the key stored in leaf
      if (!leaf_matches((art_leaf *)n, key, key_len)) {
        return ((art_leaf *)n)->value;
      }
      return nullptr;
    }

    if (n->depth == depth) {
      // fail if prefix does not match
      if (n->partial_len) {
        prefix_len = check_prefix(n, key, key_len, depth);
        if (prefix_len != std::min(kMaxPrefixLen, (uint64_t)n->partial_len)) {
          return nullptr;
        }
        depth += n->partial_len;
      }
    }

    child = find_child(n, TokenAt(key, depth));
    n = (child) ? *child : nullptr;
    depth++;
  }
  return nullptr;
}

};  // namespace WORT
};  // namespace PIE