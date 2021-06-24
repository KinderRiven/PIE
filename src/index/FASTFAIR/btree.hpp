/*
  Copyright (c) 2018, UNIST. All rights reserved.  The license is a free
  non-exclusive, non-transferable license to reproduce, use, modify and display
  the source code version of the Software, with or without modifications solely
  for non-commercial research, educational or evaluation purposes. The license
  does not entitle Licensee to technical support, telephone assistance,
  enhancements or updates to the Software. All rights, title to and ownership
  interest in the Software, including all intellectual property rights therein
  shall remain in UNIST.

  Please use at your own risk.
*/

#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <future>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <vector>

#include "../../../util/internal_string.h"
#include "../../include/allocator.hpp"
#include "../../include/index.hpp"

// to silence warnings
#define UNUSED(x) ((void)(x))

#define PAGESIZE 512

#define CPU_FREQ_MHZ (1994)
#define DELAY_IN_NS (1000)
#define CACHE_LINE_SIZE 64
#define QUERY_NUM 25

#define IS_FORWARD(c) (c % 2 == 0)

#define STRINGKEY

#ifdef STRINGKEY
using entry_key_t = PIE::InternalString;
#else
using entry_key_t = int64_t;
#endif

namespace PIE {
namespace FASTFAIR {

pthread_mutex_t print_mtx;

/*
  static inline void cpu_pause() { __asm__ volatile("pause" ::: "memory"); }
  static inline unsigned long read_tsc(void) {
  unsigned long var;
  unsigned int hi, lo;

  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  var = ((unsigned long long int)hi << 32) | lo;

  return var;
  }
*/

unsigned long write_latency_in_ns = 0;
unsigned long long search_time_in_insert = 0;
unsigned int gettime_cnt = 0;
unsigned long long clflush_time_in_insert = 0;
unsigned long long update_time_in_insert = 0;
int clflush_cnt = 0;
int node_cnt = 0;

using namespace std;

inline void mfence() { asm volatile("mfence" ::: "memory"); }

inline void clflush(char *data, int len) {
    volatile char *ptr = (char *)((unsigned long)data & ~(CACHE_LINE_SIZE - 1));
    mfence();
    for (; ptr < data + len; ptr += CACHE_LINE_SIZE) {
        asm volatile("clflush %0" : "+m"(*(volatile char *)ptr));
    }
    mfence();
}

class page;

class btree {
  private:
    int height;
    char *root;
    PIE::Allocator *allocator;

  public:
    btree(PIE::Allocator *);
    void setNewRoot(char *);
    void getNumberOfNodes();
    status_code_t btree_insert(const entry_key_t &, char *);
    void btree_insert_internal(char *, const entry_key_t &, char *, uint32_t);
    status_code_t btree_delete(const entry_key_t &);
    void btree_delete_internal(const entry_key_t &, char *, uint32_t,
                               entry_key_t *, bool *, page **);
    char *btree_search(const entry_key_t &);
    status_code_t btree_search_range(const entry_key_t &, const entry_key_t &,
                                     void **);
    void printAll();

    friend class page;
};

class header {
  private:
    page *leftmost_ptr;     // 8 bytes
    page *sibling_ptr;      // 8 bytes
    uint32_t level;         // 4 bytes
    uint8_t switch_counter; // 1 bytes
    uint8_t is_deleted;     // 1 bytes
    int16_t last_index;     // 2 bytes
    std::mutex *mtx;        // 8 bytes

    friend class page;
    friend class btree;

  public:
    header() {
        mtx = new std::mutex();

        leftmost_ptr = nullptr;
        sibling_ptr = nullptr;
        switch_counter = 0;
        last_index = -1;
        is_deleted = false;
    }

    ~header() { delete mtx; }
};

class entry {
  private:
    entry_key_t key; // 8 bytes
    char *ptr;       // 8 bytes

  public:
    entry() : ptr() { key = 0ULL; }

    friend class page;
    friend class btree;
};

const int cardinality = (PAGESIZE - sizeof(header)) / sizeof(entry);
// const int count_in_line = CACHE_LINE_SIZE / sizeof(entry);

class page {
  private:
    header hdr;                 // header in persistent memory, 16 bytes
    entry records[cardinality]; // slots in persistent memory, 16 bytes * n

    page *new_page(PIE::Allocator *allocator, uint32_t level = 0) {
        page *p = (page *)allocator->Allocate(sizeof(page));
        p->init(level);
        return p;
    }

    page *new_page(PIE::Allocator *allocator, page *left, entry_key_t key,
                   page *right, uint32_t level = 0) {
        page *p = (page *)allocator->Allocate(sizeof(page));
        p->init(left, key, right, level);
        return p;
    }

  public:
    friend class btree;

    void init(uint32_t level = 0) {
        hdr.level = level;
        records[0].ptr = nullptr;
    }

    // this is called when tree grows
    void init(page *left, entry_key_t key, page *right, uint32_t level = 0) {
        hdr.leftmost_ptr = left;
        hdr.level = level;
        records[0].key = key;
        records[0].ptr = (char *)right;
        records[1].ptr = nullptr;

        hdr.last_index = 0;

        clflush((char *)this, sizeof(page));
    }

    // void *operator new(size_t size) {
    //     void *ret;
    //     posix_memalign(&ret, 64, size);
    //     return ret;
    // }

    inline int count() {
        uint8_t previous_switch_counter;
        int count = 0;
        do {
            previous_switch_counter = hdr.switch_counter;
            count = hdr.last_index + 1;

            while (count >= 0 && records[count].ptr != nullptr) {
                if (IS_FORWARD(previous_switch_counter))
                    ++count;
                else
                    --count;
            }

            if (count < 0) {
                count = 0;
                while (records[count].ptr != nullptr) {
                    ++count;
                }
            }

        } while (previous_switch_counter != hdr.switch_counter);

        return count;
    }

    inline bool remove_key(const entry_key_t &key) {
        // Set the switch_counter
        if (IS_FORWARD(hdr.switch_counter))
            ++hdr.switch_counter;

        bool shift = false;
        int i;
        for (i = 0; records[i].ptr != nullptr; ++i) {
            if (!shift && records[i].key == key) {
                records[i].ptr =
                    (i == 0) ? (char *)hdr.leftmost_ptr : records[i - 1].ptr;
                shift = true;
            }

            if (shift) {
#ifdef STRINGKEY
                records[i].key.BorrowFrom(records[i + 1].key);
#else
                records[i].key = records[i + 1].key;
#endif

                records[i].ptr = records[i + 1].ptr;

                // flush
                uint64_t records_ptr = (uint64_t)(&records[i]);
                int remainder = records_ptr % CACHE_LINE_SIZE;
                bool do_flush =
                    (remainder == 0) ||
                    ((((int)(remainder + sizeof(entry)) / CACHE_LINE_SIZE) ==
                      1) &&
                     ((remainder + sizeof(entry)) % CACHE_LINE_SIZE) != 0);
                if (do_flush) {
                    clflush((char *)records_ptr, CACHE_LINE_SIZE);
                }
            }
        }

        if (shift) {
            --hdr.last_index;
        }
        return shift;
    }

    bool remove(btree *bt, const entry_key_t &key, bool only_rebalance = false,
                bool with_lock = true) {
        hdr.mtx->lock();
        UNUSED(bt);
        UNUSED(only_rebalance);
        UNUSED(with_lock);
        bool ret = remove_key(key);

        hdr.mtx->unlock();

        return ret;
    }

    /*
     * Although we implemented the rebalancing of B+-Tree, it is currently
     * blocked for the performance. Please refer to the follow. Chi, P., Lee, W.
     * C., & Xie, Y. (2014, August). Making B+-tree efficient in PCM-based main
     * memory. In Proceedings of the 2014 international symposium on Low power
     * electronics and design (pp. 69-74). ACM.
     *
     * PIE: since this function is never called, we remove it for clear code
     */
    /*
      bool remove_rebalancing(btree *bt, entry_key_t key, bool only_rebalance =
      false, bool with_lock = true) {
      }
    */

    inline void insert_key(const entry_key_t &key, char *ptr, int *num_entries,
                           bool flush = true, bool update_last_index = true) {
        // update switch_counter
        if (!IS_FORWARD(hdr.switch_counter))
            ++hdr.switch_counter;

        // FAST
        if (*num_entries == 0) { // this page is empty
            entry *new_entry = (entry *)&records[0];
            entry *array_end = (entry *)&records[1];
#ifdef STRINGKEY
            new_entry->key.BorrowFrom(key);
#else
            new_entry->key = key;
#endif
            new_entry->ptr = (char *)ptr;

            array_end->ptr = (char *)nullptr;

            if (flush) {
                clflush((char *)this, CACHE_LINE_SIZE);
            }
        } else {
            int i = *num_entries - 1, inserted = 0, to_flush_cnt = 0;
            records[*num_entries + 1].ptr = records[*num_entries].ptr;
            if (flush) {
                if ((uint64_t) &
                    (records[*num_entries + 1].ptr) % CACHE_LINE_SIZE == 0)
                    clflush((char *)&(records[*num_entries + 1].ptr),
                            sizeof(char *));
            }

            // FAST
            for (i = *num_entries - 1; i >= 0; i--) {
                if (key < records[i].key) {
                    records[i + 1].ptr = records[i].ptr;
#ifdef STRINGKEY
                    records[i + 1].key.BorrowFrom(records[i].key);
#else
                    records[i + 1].key = records[i].key;
#endif

                    if (flush) {
                        uint64_t records_ptr = (uint64_t)(&records[i + 1]);

                        int remainder = records_ptr % CACHE_LINE_SIZE;
                        bool do_flush = (remainder == 0) ||
                                        ((((int)(remainder + sizeof(entry)) /
                                           CACHE_LINE_SIZE) == 1) &&
                                         ((remainder + sizeof(entry)) %
                                          CACHE_LINE_SIZE) != 0);
                        if (do_flush) {
                            clflush((char *)records_ptr, CACHE_LINE_SIZE);
                            to_flush_cnt = 0;
                        } else
                            ++to_flush_cnt;
                    }
                } else {
                    records[i + 1].ptr = records[i].ptr;
#ifdef STRINGKEY
                    records[i + 1].key.BorrowFrom(key);
#else
                    records[i + 1].key = key;
#endif
                    records[i + 1].ptr = ptr;

                    if (flush)
                        clflush((char *)&records[i + 1], sizeof(entry));
                    inserted = 1;
                    break;
                }
            }
            if (inserted == 0) {
                records[0].ptr = (char *)hdr.leftmost_ptr;
#ifdef STRINGKEY
                records[0].key.BorrowFrom(key);
#else
                records[0].key = key;
#endif
                records[0].ptr = ptr;
                if (flush)
                    clflush((char *)&records[0], sizeof(entry));
            }
        }

        if (update_last_index) {
            hdr.last_index = *num_entries;
        }
        ++(*num_entries);
    }

    // Insert a new key - FAST and FAIR
    page *store(PIE::Allocator *allocator, btree *bt, char *left,
                const entry_key_t &key, char *right, bool flush, bool with_lock,
                page *invalid_sibling = nullptr) {
        UNUSED(left);
        if (with_lock) {
            hdr.mtx->lock(); // Lock the write lock
        }
        if (hdr.is_deleted) {
            if (with_lock) {
                hdr.mtx->unlock();
            }

            return nullptr;
        }

        // If this node has a sibling node,
        if (hdr.sibling_ptr && (hdr.sibling_ptr != invalid_sibling)) {
            // Compare this key with the first key of the sibling
            if (key > hdr.sibling_ptr->records[0].key) {
                if (with_lock) {
                    hdr.mtx->unlock(); // Unlock the write lock
                }
                return hdr.sibling_ptr->store(allocator, bt, nullptr, key,
                                              right, true, with_lock,
                                              invalid_sibling);
            }
        }

        int num_entries = count();

        // FAST
        if (num_entries < cardinality - 1) {
            insert_key(key, right, &num_entries, flush);

            if (with_lock) {
                hdr.mtx->unlock(); // Unlock the write lock
            }

            return this;
        } else { // FAIR
            // overflow
            // create a new node
            page *sibling = new_page(allocator, hdr.level);
            int m = (int)ceil(num_entries / 2);
            entry_key_t split_key;
#ifdef STRINGKEY
            split_key.BorrowFrom(records[m].key);
#else
            split_key = records[m].key;
#endif

            // migrate half of keys into the sibling
            int sibling_cnt = 0;
            if (hdr.leftmost_ptr == nullptr) { // leaf node
                for (int i = m; i < num_entries; ++i) {
                    sibling->insert_key(records[i].key, records[i].ptr,
                                        &sibling_cnt, false);
                }
            } else { // internal node
                for (int i = m + 1; i < num_entries; ++i) {
                    sibling->insert_key(records[i].key, records[i].ptr,
                                        &sibling_cnt, false);
                }
                sibling->hdr.leftmost_ptr = (page *)records[m].ptr;
            }

            sibling->hdr.sibling_ptr = hdr.sibling_ptr;
            clflush((char *)sibling, sizeof(page));

            hdr.sibling_ptr = sibling;
            clflush((char *)&hdr, sizeof(hdr));

            // set to nullptr
            if (IS_FORWARD(hdr.switch_counter))
                hdr.switch_counter += 2;
            else
                ++hdr.switch_counter;
            records[m].ptr = nullptr;
            clflush((char *)&records[m], sizeof(entry));

            hdr.last_index = m - 1;
            clflush((char *)&(hdr.last_index), sizeof(int16_t));

            num_entries = hdr.last_index + 1;

            page *ret;

            // insert the key
            if (key < split_key) {
                insert_key(key, right, &num_entries);
                ret = this;
            } else {
                sibling->insert_key(key, right, &sibling_cnt);
                ret = sibling;
            }

            // Set a new root or insert the split key to the parent
            if (bt->root ==
                (char *)this) { // only one node can update the root ptr
                page *new_root = new_page(allocator, (page *)this, split_key,
                                          sibling, hdr.level + 1);
                bt->setNewRoot((char *)new_root);

                if (with_lock) {
                    hdr.mtx->unlock(); // Unlock the write lock
                }
            } else {
                if (with_lock) {
                    hdr.mtx->unlock(); // Unlock the write lock
                }
                bt->btree_insert_internal(nullptr, split_key, (char *)sibling,
                                          hdr.level + 1);
            }

            return ret;
        }
    }

    // Search keys with linear search
    void linear_search_range(const entry_key_t &min, const entry_key_t &max,
                             void **buf) {
        int i, off = 0;
        uint8_t previous_switch_counter;
        page *current = this;

        while (current) {
            int old_off = off;
            do {
                previous_switch_counter = current->hdr.switch_counter;
                off = old_off;

                entry_key_t tmp_key;
                char *tmp_ptr;

                if (IS_FORWARD(previous_switch_counter)) {
#ifdef STRINGKEY
                    if ((tmp_key.BorrowFrom(current->records[0].key)) > min) {
#else
                    if ((tmp_key = current->records[0].key) > min) {
#endif
                        if (tmp_key < max) {
                            if ((tmp_ptr = current->records[0].ptr) !=
                                nullptr) {
                                if (tmp_key == current->records[0].key) {
                                    if (tmp_ptr) {
                                        buf[off++] = (void *)tmp_ptr;
                                    }
                                }
                            }
                        } else
                            return;
                    }

                    for (i = 1; current->records[i].ptr != nullptr; ++i) {
#ifdef STRINGKEY
                        if ((tmp_key.BorrowFrom(current->records[i].key)) >
                            min) {
#else
                        if ((tmp_key = current->records[i].key) > min) {
#endif
                            if (tmp_key < max) {
                                if ((tmp_ptr = current->records[i].ptr) !=
                                    current->records[i - 1].ptr) {
                                    if (tmp_key == current->records[i].key) {
                                        if (tmp_ptr)
                                            buf[off++] = (void *)tmp_ptr;
                                    }
                                }
                            } else
                                return;
                        }
                    }
                } else {
                    for (i = count() - 1; i > 0; --i) {
#ifdef STRINGKEY
                        if ((tmp_key.BorrowFrom(current->records[i].key)) >
                            min) {
#else
                        if ((tmp_key = current->records[i].key) > min) {
#endif
                            if (tmp_key < max) {

                                if ((tmp_ptr = current->records[i].ptr) !=
                                    current->records[i - 1].ptr) {
                                    if (tmp_key == current->records[i].key) {
                                        if (tmp_ptr)
                                            buf[off++] = (void *)tmp_ptr;
                                    }
                                }
                            } else
                                return;
                        }
                    }

#ifdef STRINGKEY
                    if ((tmp_key.BorrowFrom(current->records[0].key)) > min) {
#else
                    if ((tmp_key = current->records[0].key) > min) {
#endif
                        if (tmp_key < max) {
                            if ((tmp_ptr = current->records[0].ptr) !=
                                nullptr) {
                                if (tmp_key == current->records[0].key) {
                                    if (tmp_ptr) {
                                        buf[off++] = (void *)tmp_ptr;
                                    }
                                }
                            }
                        } else
                            return;
                    }
                }
            } while (previous_switch_counter != current->hdr.switch_counter);

            current = current->hdr.sibling_ptr;
        }
    }

    char *linear_search(const entry_key_t &key) {
        int i = 1;
        uint8_t previous_switch_counter;
        char *ret = nullptr;
        char *t;
        entry_key_t tmp_key;

        if (hdr.leftmost_ptr == nullptr) { // Search a leaf node
            do {
                previous_switch_counter = hdr.switch_counter;
                ret = nullptr;

                // search from left ro right
                if (IS_FORWARD(previous_switch_counter)) {
#ifdef STRINGKEY
                    if ((tmp_key.BorrowFrom(records[0].key)) == key) {
#else
                    if ((tmp_key = records[0].key) == key) {
#endif
                        if ((t = records[0].ptr) != nullptr) {
                            if (tmp_key == records[0].key) {
                                ret = t;
                                continue;
                            }
                        }
                    }

                    for (i = 1; records[i].ptr != nullptr; ++i) {
#ifdef STRINGKEY
                        if ((tmp_key.BorrowFrom(records[i].key)) == key) {
#else
                        if ((tmp_key = records[i].key) == key) {
#endif
                            if (records[i - 1].ptr != (t = records[i].ptr)) {
                                if (tmp_key == records[i].key) {
                                    ret = t;
                                    break;
                                }
                            }
                        }
                    }
                } else { // search from right to left
                    for (i = count() - 1; i > 0; --i) {
#ifdef STRINGKEY
                        if ((tmp_key.BorrowFrom(records[i].key)) == key) {
#else
                        if ((tmp_key = records[i].key) == key) {
#endif
                            if (records[i - 1].ptr != (t = records[i].ptr) &&
                                t) {
                                if (tmp_key == records[i].key) {
                                    ret = t;
                                    break;
                                }
                            }
                        }
                    }

                    if (!ret) {
#ifdef STRINGKEY
                        if ((tmp_key.BorrowFrom(records[0].key)) == key) {
#else
                        if ((tmp_key = records[0].key) == key) {
#endif
                            if (nullptr != (t = records[0].ptr) && t) {
                                if (tmp_key == records[0].key) {
                                    ret = t;
                                    continue;
                                }
                            }
                        }
                    }
                }
            } while (hdr.switch_counter != previous_switch_counter);

            if (ret) {
                return ret;
            }

            if ((t = (char *)hdr.sibling_ptr) &&
                key >= ((page *)t)->records[0].key)
                return t;

            return nullptr;
        } else { // internal node
            do {
                previous_switch_counter = hdr.switch_counter;
                ret = nullptr;

                if (IS_FORWARD(previous_switch_counter)) {
#ifdef STRINGKEY
                    if (key < (tmp_key.BorrowFrom(records[0].key))) {
#else
                    if (key < (tmp_key = records[0].key)) {
#endif
                        if ((t = (char *)hdr.leftmost_ptr) != records[0].ptr) {
                            ret = t;
                            continue;
                        }
                    }

                    for (i = 1; records[i].ptr != nullptr; ++i) {
#ifdef STRINGKEY
                        if (key < (tmp_key.BorrowFrom(records[i].key))) {
#else
                        if (key < (tmp_key = records[i].key)) {
#endif
                            if ((t = records[i - 1].ptr) != records[i].ptr) {
                                ret = t;
                                break;
                            }
                        }
                    }

                    if (!ret) {
                        ret = records[i - 1].ptr;
                        continue;
                    }
                } else { // search from right to left
                    for (i = count() - 1; i >= 0; --i) {
#ifdef STRINGKEY
                        if (key >= (tmp_key.BorrowFrom(records[i].key))) {
#else
                        if (key >= (tmp_key = records[i].key)) {
#endif

                            if (i == 0) {
                                if ((char *)hdr.leftmost_ptr !=
                                    (t = records[i].ptr)) {
                                    ret = t;
                                    break;
                                }
                            } else {
                                if (records[i - 1].ptr !=
                                    (t = records[i].ptr)) {
                                    ret = t;
                                    break;
                                }
                            }
                        }
                    }
                }
            } while (hdr.switch_counter != previous_switch_counter);

            if ((t = (char *)hdr.sibling_ptr) != nullptr) {
                if (key >= ((page *)t)->records[0].key)
                    return t;
            }

            if (ret) {
                return ret;
            } else
                return (char *)hdr.leftmost_ptr;
        }

        return nullptr;
    }

    // print a node
    void print() {
        if (hdr.leftmost_ptr == nullptr)
            printf("[%d] leaf %p \n", this->hdr.level, this);
        else
            printf("[%d] internal %p \n", this->hdr.level, this);
        printf("last_index: %d\n", hdr.last_index);
        printf("switch_counter: %d\n", hdr.switch_counter);
        printf("search direction: ");
        if (IS_FORWARD(hdr.switch_counter))
            printf("->\n");
        else
            printf("<-\n");

        if (hdr.leftmost_ptr != nullptr)
            printf("%p ", hdr.leftmost_ptr);

        for (int i = 0; records[i].ptr != nullptr; ++i)
#ifdef STRINGKEY
            printf("%s,%p", records[i].key.Data(), records[i].ptr);
#else
            printf("%ld,%p ", records[i].key, records[i].ptr);
#endif

        printf("%p ", hdr.sibling_ptr);

        printf("\n");
    }

    void printAll() {
        if (hdr.leftmost_ptr == nullptr) {
            printf("printing leaf node: ");
            print();
        } else {
            printf("printing internal node: ");
            print();
            ((page *)hdr.leftmost_ptr)->printAll();
            for (int i = 0; records[i].ptr != nullptr; ++i) {
                ((page *)records[i].ptr)->printAll();
            }
        }
    }
};

/*
 * class btree
 */
btree::btree(PIE::Allocator *all) {
    page *p = (page *)all->Allocate(sizeof(page));
    p->init();
    root = (char *)p;
    height = 1;
    allocator = all;
}

void btree::setNewRoot(char *new_root) {
    this->root = (char *)new_root;
    clflush((char *)&(this->root), sizeof(char *));
    ++height;
}

char *btree::btree_search(const entry_key_t &key) {
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != nullptr) {
        p = (page *)p->linear_search(key);
    }

    page *t;
    while ((t = (page *)p->linear_search(key)) == p->hdr.sibling_ptr) {
        p = t;
        if (!p) {
            break;
        }
    }

    if (!t) {
#ifdef STRINGKEY
        printf("NOT FOUND %s, t = %p\n", key.Data(), t);
#else
        printf("NOT FOUND %lu, t = %p\n", key, t);
#endif
        return nullptr;
    }

    return (char *)t;
}

// insert the key in the leaf node
status_code_t btree::btree_insert(const entry_key_t &key,
                                  char *right) { // need to be string
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != nullptr) {
        p = (page *)p->linear_search(key);
    }

    if (!p->store(allocator, this, nullptr, key, right, true, true)) { // store
        btree_insert(key, right);
        return kOk;
    }
    return kFailed;
}

// store the key into the node at the given level
void btree::btree_insert_internal(char *left, const entry_key_t &key,
                                  char *right, uint32_t level) {
    if (level > ((page *)root)->hdr.level)
        return;

    page *p = (page *)this->root;

    while (p->hdr.level > level)
        p = (page *)p->linear_search(key);

    if (!p->store(allocator, this, nullptr, key, right, true, true)) {
        btree_insert_internal(left, key, right, level);
    }
}

status_code_t btree::btree_delete(const entry_key_t &key) {
    page *p = (page *)root;

    while (p->hdr.leftmost_ptr != nullptr) {
        p = (page *)p->linear_search(key);
    }

    page *t;
    while ((t = (page *)p->linear_search(key)) == p->hdr.sibling_ptr) {
        p = t;
        if (!p)
            break;
    }

    if (p) {
        if (!p->remove(this, key)) {
            btree_delete(key);
        }
        return kOk;
    } else {
#ifdef STRINGKEY
        printf("not found the key to delete %s\n", key.Data());
#else
        printf("not found the key to delete %lu\n", key);
#endif
    }
    return kFailed;
}

void btree::btree_delete_internal(const entry_key_t &key, char *ptr,
                                  uint32_t level, entry_key_t *deleted_key,
                                  bool *is_leftmost_node, page **left_sibling) {
    if (level > ((page *)this->root)->hdr.level)
        return;

    page *p = (page *)this->root;

    while (p->hdr.level > level) {
        p = (page *)p->linear_search(key);
    }

    p->hdr.mtx->lock();

    if ((char *)p->hdr.leftmost_ptr == ptr) {
        *is_leftmost_node = true;
        p->hdr.mtx->unlock();
        return;
    }

    *is_leftmost_node = false;

    for (int i = 0; p->records[i].ptr != nullptr; ++i) {
        if (p->records[i].ptr == ptr) {
            if (i == 0) {
                if ((char *)p->hdr.leftmost_ptr != p->records[i].ptr) {
                    *deleted_key = p->records[i].key;
                    *left_sibling = p->hdr.leftmost_ptr;
                    p->remove(this, *deleted_key, false, false);
                    break;
                }
            } else {
                if (p->records[i - 1].ptr != p->records[i].ptr) {
                    *deleted_key = p->records[i].key;
                    *left_sibling = (page *)p->records[i - 1].ptr;
                    p->remove(this, *deleted_key, false, false);
                    break;
                }
            }
        }
    }

    p->hdr.mtx->unlock();
}

// Function to search keys from "min" to "max"
status_code_t btree::btree_search_range(const entry_key_t &min,
                                        const entry_key_t &max, void **buf) {
    page *p = (page *)root;

    while (p) {
        if (p->hdr.leftmost_ptr != nullptr) {
            // The current page is internal
            p = (page *)p->linear_search(min);
        } else {
            // Found a leaf
            p->linear_search_range(min, max, buf);
            break;
        }
    }
    return kOk;
}

void btree::printAll() {
    pthread_mutex_lock(&print_mtx);
    int total_keys = 0;
    page *leftmost = (page *)root;
    printf("root: %p\n", root);
    do {
        page *sibling = leftmost;
        while (sibling) {
            if (sibling->hdr.level == 0) {
                total_keys += sibling->hdr.last_index + 1;
            }
            sibling->print();
            sibling = sibling->hdr.sibling_ptr;
        }
        printf("-----------------------------------------\n");
        leftmost = leftmost->hdr.leftmost_ptr;
    } while (leftmost);

    printf("total number of keys: %d\n", total_keys);
    pthread_mutex_unlock(&print_mtx);
}

class FASTFAIRTree : public Index {
  public:
    FASTFAIRTree(Allocator *allocator_)
        : tree(allocator_), allocator(allocator_){};

    status_code_t Insert(const char *key, size_t key_len,
                         void *value) override {
        auto des = allocator->Allocate(key_len);
        InternalString str(key, key_len, (uint8_t *)des);
        return tree.btree_insert(str, (char *)value);
    }

    status_code_t Search(const char *key, size_t key_len,
                         void **value) override {
#ifdef STRINGKEY
        char buf[512];
        auto k = InternalString(key, key_len, (uint8_t *)buf);
#else
        auto k = (uint64_t)key;
#endif
        if ((*value = tree.btree_search(k))) {
            return kOk;
        }
        return kNotFound;
    }

    status_code_t Update(const char *key, size_t key_len,
                         void *value) override {
        UNUSED(key);
        UNUSED(key_len);
        UNUSED(value);
        return kNotDefined;
    }

    status_code_t Upsert(const char *key, size_t key_len,
                         void *value) override {
        UNUSED(key);
        UNUSED(key_len);
        UNUSED(value);
        return kNotDefined;
    }

    status_code_t ScanCount(const char *startkey, size_t key_len, size_t count,
                            void **vec) override {
        UNUSED(startkey);
        UNUSED(key_len);
        UNUSED(count);
        UNUSED(vec);
        return kNotDefined;
    }

    status_code_t Scan(const char *startkey, size_t startkey_len,
                       const char *endkey, size_t endkey_len,
                       void **vec) override {
#ifdef STRINGKEY
        char bufs[512];
        char bufe[512];
        auto s = InternalString(startkey, startkey_len, (uint8_t *)bufs);
        auto e = InternalString(endkey, endkey_len, (uint8_t *)bufe);
#else
        auto s = (uint64_t)startkey;
        auto e = (uint64_t)endkey;
#endif
        return tree.btree_search_range(s, e, vec);
    }

    void Print() override { tree.printAll(); }

  private:
    btree tree;
    Allocator *allocator;
};
} // namespace FASTFAIR
} // namespace PIE
