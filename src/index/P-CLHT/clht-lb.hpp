#ifndef PIE_SRC_INDEX_PCLHT_CLHT_LB_HPP__
#define PIE_SRC_INDEX_PCLHT_CLHT_LB_HPP__

#include "allocator.hpp"
#include "atomic_ops.h"
#include "index.hpp"
#include "internal_string.h"
#include "status.hpp"

namespace PIE {
namespace CLHT {

/* #define DEBUG */

#define CLHT_READ_ONLY_FAIL 1
#define CLHT_HELP_RESIZE 1
#define CLHT_PERC_EXPANSIONS 1
#define CLHT_MAX_EXPANSIONS 24
#define CLHT_PERC_FULL_DOUBLE 50 /* % */
#define CLHT_RATIO_DOUBLE 2
#define CLHT_OCCUP_AFTER_RES 40
#define CLHT_PERC_FULL_HALVE 5 /* % */
#define CLHT_RATIO_HALVE 8
#define CLHT_MIN_CLHT_SIZE 8
#define CLHT_DO_CHECK_STATUS 0
#define CLHT_DO_GC 0
#define CLHT_STATUS_INVOK 500000
#define CLHT_STATUS_INVOK_IN 500000
#define LOAD_FACTOR 2

#if defined(RTM) /* only for processors that have RTM */
#define CLHT_USE_RTM 1
#else
#define CLHT_USE_RTM 0
#endif

#if CLHT_DO_CHECK_STATUS == 1
#define CLHT_CHECK_STATUS(h)                      \
  if (unlikely((--check_ht_status_steps) == 0)) { \
    ht_status(h, 0, 0);                           \
    check_ht_status_steps = CLHT_STATUS_INVOK;    \
  }

#else
#define CLHT_CHECK_STATUS(h)
#endif

#if CLHT_DO_GC == 1
#define CLHT_GC_HT_VERSION_USED(ht) \
  clht_gc_thread_version((clht_hashtable_t *)ht)
#else
#define CLHT_GC_HT_VERSION_USED(ht)
#endif

/* CLHT LINKED version specific parameters */
#define CLHT_LINKED_PERC_FULL_DOUBLE 75
#define CLHT_LINKED_MAX_AVG_EXPANSION 1
#define CLHT_LINKED_MAX_EXPANSIONS 7
#define CLHT_LINKED_MAX_EXPANSIONS_HARD 16
#define CLHT_LINKED_EMERGENCY_RESIZE \
  4 /* how many times to increase the size on emergency */
/* *************************************** */

#if defined(DEBUG)
#define DPP(x) x++
#else
#define DPP(x)
#endif

#define CACHE_LINE_SIZE 64
#define ENTRIES_PER_BUCKET 3

#ifndef ALIGNED
#if __GNUC__ && !SCC
#define ALIGNED(N) __attribute__((aligned(N)))
#else
#define ALIGNED(N)
#endif
#endif

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#if defined(__sparc__)
#define PREFETCHW(x)
#define PREFETCH(x)
#define PREFETCHNTA(x)
#define PREFETCHT0(x)
#define PREFETCHT1(x)
#define PREFETCHT2(x)

#define PAUSE asm volatile("rd    %%ccr, %%g0\n\t" ::: "memory")
#define _mm_pause() PAUSE
#define _mm_mfence()    \
  __asm__ __volatile__( \
      "membar #LoadLoad | #LoadStore | #StoreLoad | #StoreStore");
#define _mm_lfence() __asm__ __volatile__("membar #LoadLoad | #LoadStore");
#define _mm_sfence() __asm__ __volatile__("membar #StoreLoad | #StoreStore");

#elif defined(__tile__)
#define _mm_lfence() arch_atomic_read_barrier()
#define _mm_sfence() arch_atomic_write_barrier()
#define _mm_mfence() arch_atomic_full_barrier()
#define _mm_pause() cycle_relax()
#endif

#define CAS_U64_BOOL(a, b, c) (CAS_U64(a, b, c) == b)

typedef uintptr_t clht_addr_t;
typedef volatile uintptr_t clht_val_t;

#if defined(__tile__)
typedef volatile uint32_t clht_lock_t;
#else
typedef volatile uint8_t clht_lock_t;
#endif

constexpr size_t kEntriesPerBucket = 3;
constexpr size_t kCacheLineSize = 64;

struct bucket_t {
  clht_lock_t lock;
  volatile uint32_t hops;

  // An array of key and coresponding value
  clht_addr_t key[kEntriesPerBucket];
  clht_val_t val[kEntriesPerBucket];

  volatile bucket_t *next;
};

struct clht_t {
  clht_hashtable_t *ht;
  // For padding
  uint8_t next_cache_line[CACHE_LINE_SIZE - sizeof(void *)];
  clht_hashtable_t *ht_oldest;
  ht_ts_t *version_list;
  size_t version_min;
  volatile clht_lock_t resize_lock;
  volatile clht_lock_t gc_lock;
  volatile clht_lock_t status_lock;
} __attribute__((aligned(CACHE_LINE_SIZE)));

// Structure for recording basic hash table metadata
struct clht_hashtable_t {
  size_t num_buckets;
  bucket_t *table;  // The first bucket location
  size_t hash;
  size_t version;
  uint8_t next_cache_line[CACHE_LINE_SIZE - (3 * sizeof(size_t)) -
                          (sizeof(void *))];

  clht_hashtable_t *table_tmp;
  clht_hashtable_t *table_prev;
  clht_hashtable_t *table_new;
  volatile uint32_t num_expands;
  union {
    volatile uint32_t num_expands_threshold;
    uint32_t num_buckets_prev;
  };
  volatile int32_t is_helper;
  volatile int32_t helper_done;
  size_t version_min;
} __attribute__((aligned(CACHE_LINE_SIZE)));
// TODO: Do we have to use union? union is not recommended in C++

struct ht_ts_t {
  size_t version;
  clht_hashtable_t *versionp;
  int id;
  volatile struct ht_ts *next;
} __attribute__((aligned(CACHE_LINE_SIZE)));

static inline void _mm_pause_rep(uint64_t w) {
  while (w--) {
    _mm_pause();
  }
}

#if defined(XEON) | defined(COREi7)
#define TAS_RLS_MFENCE() _mm_sfence();
#elif defined(__tile__)
#define TAS_RLS_MFENCE() _mm_mfence();
#else
#define TAS_RLS_MFENCE()
#endif

#define LOCK_FREE 0
#define LOCK_UPDATE 1
#define LOCK_RESIZE 2

#if CLHT_USE_RTM == 1 /* USE RTM */
#define LOCK_ACQ(lock, ht) lock_acq_rtm_chk_resize(lock, ht)
#define LOCK_RLS(lock)                \
  if (likely(*(lock) == LOCK_FREE)) { \
    _xend();                          \
    DPP(put_num_failed_on_new);       \
  } else {                            \
    TAS_RLS_MFENCE();                 \
    *lock = LOCK_FREE;                \
    DPP(put_num_failed_expand);       \
  }
#else /* NO RTM */
#define LOCK_ACQ(lock, ht) lock_acq_chk_resize(lock, ht)

#define LOCK_RLS(lock) \
  TAS_RLS_MFENCE();    \
  *lock = 0;

#endif /* RTM */

#define LOCK_ACQ_RES(lock) lock_acq_resize(lock)

#define TRYLOCK_ACQ(lock) TAS_U8(lock)

#define TRYLOCK_RLS(lock) lock = LOCK_FREE

void ht_resize_help(clht_hashtable_t *h);

#if defined(DEBUG)
extern __thread uint32_t put_num_restarts;
#endif

static inline int lock_acq_chk_resize(clht_lock_t *lock, clht_hashtable_t *h) {
  char once = 1;
  clht_lock_t l;
  while ((l = CAS_U8(lock, LOCK_FREE, LOCK_UPDATE)) == LOCK_UPDATE) {
    if (once) {
      DPP(put_num_restarts);
      once = 0;
    }
    _mm_pause();
  }

  if (l == LOCK_RESIZE) {
    /* helping with the resize */
#if CLHT_HELP_RESIZE == 1
    ht_resize_help(h);
#endif

    while (h->table_new == NULL) {
      _mm_pause();
      _mm_mfence();
    }

    return 0;
  }

  return 1;
}

static inline int lock_acq_resize(clht_lock_t *lock) {
  clht_lock_t l;
  while ((l = CAS_U8(lock, LOCK_FREE, LOCK_RESIZE)) == LOCK_UPDATE) {
    _mm_pause();
  }

  if (l == LOCK_RESIZE) {
    return 0;
  }

  return 1;
}

/* ********************************************************************************
 */
#if CLHT_USE_RTM == 1 /* use RTM */
/* ********************************************************************************
 */

#include <immintrin.h> /*  */

static inline int lock_acq_rtm_chk_resize(clht_lock_t *lock,
                                          clht_hashtable_t *h) {
  int rtm_retries = 1;
  do {
    /* while (unlikely(*lock == LOCK_UPDATE)) */
    /* 	{ */
    /* 	  _mm_pause(); */
    /* 	} */

    if (likely(_xbegin() == _XBEGIN_STARTED)) {
      clht_lock_t lv = *lock;
      if (likely(lv == LOCK_FREE)) {
        return 1;
      } else if (lv == LOCK_RESIZE) {
        _xend();
#if CLHT_HELP_RESIZE == 1
        ht_resize_help(h);
#endif

        while (h->table_new == NULL) {
          _mm_mfence();
        }

        return 0;
      }

      DPP(put_num_restarts);
      _xabort(0xff);
    }
  } while (rtm_retries-- > 0);

  return lock_acq_chk_resize(lock, h);
}
#endif /* RTM */

/* ********************************************************************************
 */
/* intefance */
/* ********************************************************************************
 */

class CLHTLBIndex : public Index {
 public:
  CLHTLBIndex(Allocator *nvmallocator, uint64_t num_buckets)
      : nvmallocator_(nvmallocator) {
    clht_ = clht_create(num_buckets);
  }

 private:
  // Directly use original CLHT code to create hash table
  // Create a CLHT hash table and initialized with "num_buckets" buckets
  // num_buckets must be power of 2
  clht_t *clht_create(uint64_t num_buckets);
  clht_hashtable_t *clht_hashtable_create(uint64_t num_buckets);

  // hash a key into position
  uint64_t clht_hash(clht_hashtable_t *hashtable, clht_addr_t key) {
    return key & (hashtable->hash);
  }

 private:
  // CLHT original operation interface
  // return 0(false) to indicate inserted key is already in the index
  // return 1(true) to indicate insertion succeed
  int clht_put(clht_addr_t key, clht_val_t val);

  // Return target key's releted value if key is found. Otherwise return
  // 0 to indicate key not exist
  clht_val_t clht_get(clht_addr_t key);

  // helper functions
  // Check if target key exists in target bucket
  bool bucket_exists(volatile bucket_t *bucket, clht_addr_t key);
  bucket_t *clht_bucket_create();
  bucket_t *clht_bucket_create_stats(clht_hashtable_t *h, int *resize);

  size_t ht_status(clht_t *h, int resize_increate, int just_print);
  int ht_resize_pes(clht_t *h, int is_increase, int by);
  int clht_gc_collect(clht_t *h);
  int clht_gc_release(clht_hashtable_t *ht);

  uint32_t clht_put_seq(clht_hashtable_t *hashtable, clht_addr_t key,
                        clht_val_t val, uint64_t bin);

  int bucket_cpy(clht_t *h, volatile bucket_t *bucket,
                 clht_hashtable_t *ht_new);

 public:
  status_code_t Insert(const char *key, size_t key_len, void *value) override {
    int ret = clht_put((uint64_t)key, (clht_val_t)value);
    if (ret == 0) {
      return kInsertKeyExist;
    }
    return kOk;
  }

  status_code_t Search(const char *key, size_t key_len, void **value) override {
    auto val = clht_get((uint64_t)key);
    if (val == 0) {
      return kNotFound;
    }
    *value = (void *)val;
    return kOk;
  }

  status_code_t Update(const char *key, size_t key_len, void *value) override {
    return kOk;
  }

  status_code_t Upsert(const char *key, size_t key_len, void *value) override {
    return kOk;
  }

  status_code_t ScanCount(const char *startkey, size_t key_len, size_t count,
                          void **vec) override {
    return kOk;
  }

  status_code_t Scan(const char *startkey, size_t startkey_len,
                     const char *endkey, size_t endkey_len,
                     void **vec) override {
    return kOk;
  }

  void Print() override { return; }

 private:
  Allocator *nvmallocator_;
  clht_t *clht_;
};

inline bool CLHTLBIndex::bucket_exists(volatile bucket_t *bucket,
                                       clht_addr_t key) {
  uint32_t j;
  do {
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      if (bucket->key[j] == key) {
        return true;
      }
    }
    bucket = bucket->next;
  } while (unlikely(bucket != NULL));
  return false;
}

/* Create a new bucket. */
inline bucket_t *CLHTLBIndex::clht_bucket_create() {
  bucket_t *bucket = nullptr;
  // bucket = (bucket_t*)memalign(CACHE_LINE_SIZE, sizeof(bucket_t));
  bucket = (bucket_t *)nvmallocator_->AllocateAlign(sizeof(bucket_t),
                                                    CACHE_LINE_SIZE);
  if (bucket == NULL) {
    return NULL;
  }

  bucket->lock = 0;

  uint32_t j;
  for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
    bucket->key[j] = 0;
  }
  bucket->next = NULL;

  return bucket;
}

inline bucket_t *CLHTLBIndex::clht_bucket_create_stats(clht_hashtable_t *h,
                                                       int *resize) {
  bucket_t *b = clht_bucket_create();
  // if (IAF_U32(&h->num_expands) == h->num_expands_threshold)
  if (IAF_U32(&h->num_expands) >= h->num_expands_threshold) {
    /* printf("      -- hit threshold (%u ~ %u)\n", h->num_expands,
     * h->num_expands_threshold); */
    *resize = 1;
  }
  return b;
}

};  // namespace CLHT
};  // namespace PIE

#endif