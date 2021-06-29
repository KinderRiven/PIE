#include "clht-lb.hpp"

#include <cstdlib>

#include "clhtutils.h"
#include "persist.h"

__thread size_t check_ht_status_steps = CLHT_STATUS_INVOK_IN;

#if defined(CLHTDEBUG)
#define DEBUG_PRINT(fmt, args...)                                     \
  fprintf(stderr, "CLHT_DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, \
          __func__, ##args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

namespace PIE {
namespace CLHT {

static inline void movnt64(uint64_t *dest, uint64_t const src, bool front,
                           bool back) {
  assert(((uint64_t)dest & 7) == 0);
  if (front) asm_sfence();
  _mm_stream_si64((long long int *)dest, (long long int)src);
  if (back) asm_sfence();
}

clht_t *CLHTLBIndex::clht_create(uint64_t num_buckets) {
  clht_t *w =
      (clht_t *)nvmallocator_->AllocateAlign(sizeof(clht_t), CACHE_LINE_SIZE);

  if (w == nullptr) {
    printf("** malloc @ hatshtalbe\n");
    return nullptr;
  }

  w->ht = clht_hashtable_create(num_buckets);
  if (w->ht == nullptr) {
    nvmallocator_->Free(w);
    return nullptr;
  }

  w->resize_lock = LOCK_FREE;
  w->gc_lock = LOCK_FREE;
  w->status_lock = LOCK_FREE;
  w->version_list = nullptr;
  w->version_min = 0;
  w->ht_oldest = w->ht;

  // Persist all data
  persist_data((char *)w->ht->table, num_buckets * sizeof(bucket_t));
  persist_data((char *)w->ht, sizeof(clht_hashtable_t));
  persist_data((char *)w, sizeof(clht_t));

  return w;
}

clht_hashtable_t *CLHTLBIndex::clht_hashtable_create(uint64_t num_buckets) {
  clht_hashtable_t *hashtable = nullptr;

  if (num_buckets == 0) {
    return NULL;
  }

  /* Allocate the table itself. */
  // hashtable =
  //     (clht_hashtable_t*)memalign(CACHE_LINE_SIZE, sizeof(clht_hashtable_t));

  hashtable = (clht_hashtable_t *)nvmallocator_->AllocateAlign(
      sizeof(clht_hashtable_t), CACHE_LINE_SIZE);

  if (hashtable == NULL) {
    printf("** malloc @ hatshtalbe\n");
    return NULL;
  }

  /* hashtable->table = calloc(num_buckets, (sizeof(bucket_t))); */
  // hashtable->table =
  //     (bucket_t *)memalign(CACHE_LINE_SIZE, num_buckets *
  //     (sizeof(bucket_t)));

  // Allocate buckets
  hashtable->table = (bucket_t *)nvmallocator_->AllocateAlign(
      num_buckets * sizeof(bucket_t), CACHE_LINE_SIZE);

  if (hashtable->table == NULL) {
    printf("** alloc: hashtable->table\n");
    fflush(stdout);
    free(hashtable);
    return nullptr;
  }

  memset(hashtable->table, 0, num_buckets * (sizeof(bucket_t)));

  uint64_t i;
  for (i = 0; i < num_buckets; i++) {
    hashtable->table[i].lock = LOCK_FREE;
    uint32_t j;
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      hashtable->table[i].key[j] = 0;
    }
  }

  hashtable->num_buckets = num_buckets;
  hashtable->hash = num_buckets - 1;
  hashtable->version = 0;
  hashtable->table_tmp = nullptr;
  hashtable->table_new = nullptr;
  hashtable->table_prev = nullptr;
  hashtable->num_expands = 0;
  hashtable->num_expands_threshold = (CLHT_PERC_EXPANSIONS * num_buckets);
  if (hashtable->num_expands_threshold == 0) {
    hashtable->num_expands_threshold = 1;
  }
  hashtable->is_helper = 1;
  hashtable->helper_done = 0;

  return hashtable;
}

int CLHTLBIndex::clht_put(clht_addr_t key, clht_val_t val) {
  // Specify target bucket position
  clht_hashtable_t *hashtable = clht_->ht;
  size_t bin = clht_hash(hashtable, key);
  volatile bucket_t *bucket = hashtable->table + bin;

#if CLHT_READ_ONLY_FAIL == 1
  if (bucket_exists(bucket, key)) {
    return false;
  }
#endif

  clht_lock_t *lock = &bucket->lock;
  // spin to make sure current hash table is not
  // doint resize
  while (!LOCK_ACQ(lock, hashtable)) {
    hashtable = clht_->ht;
    size_t bin = clht_hash(hashtable, key);

    bucket = hashtable->table + bin;
    lock = &bucket->lock;
  }

  CLHT_GC_HT_VERSION_USED(hashtable);
  CLHT_CHECK_STATUS(h);
  clht_addr_t *empty = NULL;
  clht_val_t *empty_v = NULL;

  uint32_t j;
  do {
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      if (bucket->key[j] == key) {
        LOCK_RLS(lock);
        return false;
      } else if (empty == nullptr && bucket->key[j] == 0) {
        empty = (clht_addr_t *)&bucket->key[j];
        empty_v = &bucket->val[j];
      }
    }

    int resize = 0;
    if (likely(bucket->next == NULL)) {
      if (unlikely(empty == NULL)) {
        DPP(put_num_failed_expand);

        bucket_t *b = clht_bucket_create_stats(hashtable, &resize);
        b->val[0] = val;
#ifdef __tile__
        /* keep the writes in order */
        _mm_sfence();
#endif
        b->key[0] = key;
#ifdef __tile__
        /* make sure they are visible */
        _mm_sfence();
#endif
        // clflush((char *)b, sizeof(bucket_t), false, true);
        persist_data((char *)b, sizeof(bucket_t));
        movnt64((uint64_t *)&bucket->next, (uint64_t)b, false, true);

      } else {
        *empty_v = val;
#ifdef __tile__
        /* keep the writes in order */
        _mm_sfence();
#endif
        // clflush((char *)empty_v, sizeof(clht_val_t), false, true);
        persist_data((char *)empty_v, sizeof(clht_val_t));
        movnt64((uint64_t *)empty, (uint64_t)key, false, true);
      }

      LOCK_RLS(lock);
      if (unlikely(resize)) {
        /* ht_resize_pes(h, 1); */
        int ret = ht_status(clht_, 1, 0);

        // if crash, return true, because the insert anyway succeeded
        if (ret == 0) return true;
      }
      return true;
    }
    bucket = bucket->next;
  } while (true);
}

clht_val_t CLHTLBIndex::clht_get(clht_addr_t key) {
  clht_hashtable_t *hashtable = clht_->ht;
  size_t bin = clht_hash(hashtable, key);
  CLHT_GC_HT_VERSION_USED(hashtable);
  volatile bucket_t *bucket = hashtable->table + bin;

  uint32_t j;
  do {
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      clht_val_t val = bucket->val[j];
#ifdef __tile__
      _mm_lfence();
#endif
      if (bucket->key[j] == key) {
        if (likely(bucket->val[j] == val)) {
          return val;
        } else {
          return 0;
        }
      }
    }

    bucket = bucket->next;
  } while (unlikely(bucket != NULL));
  return 0;
}

size_t CLHTLBIndex::ht_status(clht_t *h, int resize_increase, int just_print) {
  if (TRYLOCK_ACQ(&h->status_lock) && !resize_increase) {
    return 0;
  }

  clht_hashtable_t *hashtable = h->ht;
  uint64_t num_buckets = hashtable->num_buckets;
  volatile bucket_t *bucket = nullptr;
  size_t size = 0;
  int expands = 0;
  int expands_max = 0;

  uint64_t bin;
  for (bin = 0; bin < num_buckets; bin++) {
    bucket = hashtable->table + bin;

    int expands_cont = -1;
    expands--;
    uint32_t j;
    do {
      expands_cont++;
      expands++;
      for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
        if (bucket->key[j] > 0) {
          size++;
        }
      }

      bucket = bucket->next;
    } while (bucket != NULL);

    if (expands_cont > expands_max) {
      expands_max = expands_cont;
    }
  }

  double full_ratio =
      100.0 * size / ((hashtable->num_buckets) * ENTRIES_PER_BUCKET);

  if (just_print) {
    printf(
        "[STATUS-%02d] #bu: %7zu / #elems: %7zu / full%%: %8.4f%% / expands: "
        "%4d / max expands: %2d\n",
        99, hashtable->num_buckets, size, full_ratio, expands, expands_max);
  } else {
    if (full_ratio > 0 && full_ratio < CLHT_PERC_FULL_HALVE) {
      ht_resize_pes(h, 0, 33);
    } else if ((full_ratio > 0 && full_ratio > CLHT_PERC_FULL_DOUBLE) ||
               expands_max > CLHT_MAX_EXPANSIONS || resize_increase) {
      int inc_by = (full_ratio / CLHT_OCCUP_AFTER_RES);
      int inc_by_pow2 = pow2roundup(inc_by);

      if (inc_by_pow2 == 1) {
        inc_by_pow2 = 2;
      }

      DEBUG_PRINT("Callig ht_resize_pes\n");

      int ret = ht_resize_pes(h, 1, inc_by_pow2);

      // return if crashed
      if (ret == -1) return 0;
    }
  }

  if (!just_print) {
    clht_gc_collect(h);
  }

  TRYLOCK_RLS(h->status_lock);
  return size;
}

// return -1 if crash is simulated.
int CLHTLBIndex::ht_resize_pes(clht_t *h, int is_increase, int by) {
  //    ticks s = getticks();

  check_ht_status_steps = CLHT_STATUS_INVOK;

  clht_hashtable_t *ht_old = h->ht;

  if (TRYLOCK_ACQ(&h->resize_lock)) {
    return 0;
  }

  size_t num_buckets_new;
  if (is_increase == true) {
    /* num_buckets_new = CLHT_RATIO_DOUBLE * ht_old->num_buckets; */
    num_buckets_new = by * ht_old->num_buckets;
  } else {
#if CLHT_HELP_RESIZE == 1
    ht_old->is_helper = 0;
#endif
    num_buckets_new = ht_old->num_buckets / CLHT_RATIO_HALVE;
  }

  /* printf("// resizing: from %8zu to %8zu buckets\n", ht_old->num_buckets,
   * num_buckets_new); */

  clht_hashtable_t *ht_new = clht_hashtable_create(num_buckets_new);
  ht_new->version = ht_old->version + 1;

#if CLHT_HELP_RESIZE == 1
  ht_old->table_tmp = ht_new;

  size_t b;
  for (b = 0; b < ht_old->num_buckets; b++) {
    bucket_t *bu_cur = ht_old->table + b;
    int ret = bucket_cpy(
        h, bu_cur, ht_new); /* reached a point where the helper is handling */
    if (ret == -1) return -1;

    if (!ret) break;
  }

  if (is_increase && ht_old->is_helper != 1) /* there exist a helper */
  {
    while (ht_old->helper_done != 1) {
      _mm_pause();
    }
  }

#else

  size_t b;
  for (b = 0; b < ht_old->num_buckets; b++) {
    bucket_t *bu_cur = ht_old->table + b;
    int ret = bucket_cpy(h, bu_cur, ht_new);
    if (ret == -1) return -1;
  }
#endif

#if defined(DEBUG)
  /* if (clht_size(ht_old) != clht_size(ht_new)) */
  /*   { */
  /*     printf("**clht_size(ht_old) = %zu != clht_size(ht_new) = %zu\n",
   * clht_size(ht_old), clht_size(ht_new)); */
  /*   } */
#endif

  ht_new->table_prev = ht_old;

  int ht_resize_again = 0;
  if (ht_new->num_expands >= ht_new->num_expands_threshold) {
    /* printf("--problem: have already %u expands\n", ht_new->num_expands); */
    ht_resize_again = 1;
    /* ht_new->num_expands_threshold = ht_new->num_expands + 1; */
  }

  // mfence();
  // clflush((char*)ht_new, sizeof(clht_hashtable_t), false, false);
  // clflush_next_check((char*)ht_new->table, num_buckets_new *
  // sizeof(bucket_t),
  //                    false);
  // mfence();
  asm_sfence();

  persist_data((char *)ht_new, sizeof(clht_hashtable_t));
  persist_data((char *)ht_new->table, num_buckets_new * sizeof(bucket_t));
  asm_sfence();

#if defined(CRASH_BEFORE_SWAP_CLHT)
  pid_t pid = fork();

  if (pid == 0) {
    // Crash state soon after pointer swap.
    // This state will verify that all structural changes
    // have been performed before the final pointer swap
    clht_lock_initialization(h);
    DEBUG_PRINT("Child process returned before root swap\n");
    DEBUG_PRINT("-------------ht old------------\n");
    clht_print(ht_old);
    DEBUG_PRINT("-------------ht new------------\n");
    clht_print(ht_new);
    DEBUG_PRINT("-------------ht current------------\n");
    clht_print(h->ht);
    DEBUG_PRINT("-------------------------\n");
    return -1;
  }

  else if (pid > 0) {
    int returnStatus;
    waitpid(pid, &returnStatus, 0);
    DEBUG_PRINT("Continuing in parent process to finish resizing during ins\n");
  } else {
    DEBUG_PRINT("Fork failed");
    return 0;
  }
#endif

  // atomically swap the root pointer
  // SWAP_U64((uint64_t*) h, (uint64_t) ht_new);
  // clflush((char *)h, sizeof(uint64_t), false, true);
  movnt64((uint64_t*)&h->ht, (uint64_t)ht_new, false, true);

#if defined(CRASH_AFTER_SWAP_CLHT)
  pid_t pid1 = fork();

  if (pid1 == 0) {
    // Crash state soon after pointer swap.
    // This state will verify that all structural changes
    // have been performed before the final pointer swap
    clht_lock_initialization(h);
    DEBUG_PRINT("Child process returned soon after root swap\n");
    DEBUG_PRINT("-------------ht old------------\n");
    clht_print(ht_old);
    DEBUG_PRINT("-------------ht new------------\n");
    clht_print(ht_new);
    DEBUG_PRINT("-------------ht current------------\n");
    clht_print(h->ht);
    DEBUG_PRINT("-------------------------\n");
    return -1;
  }

  else if (pid1 > 0) {
    int returnStatus;
    waitpid(pid1, &returnStatus, 0);
    DEBUG_PRINT("Continuing in parent process to finish resizing during ins\n");
  } else {
    DEBUG_PRINT("Fork failed");
    return 0;
  }
#endif
  DEBUG_PRINT("Parent reached correctly\n");
  ht_old->table_new = ht_new;
  TRYLOCK_RLS(h->resize_lock);

  //    ticks e = getticks() - s;
  //    double mba = (ht_new->num_buckets * 64) / (1024.0 * 1024);
  //    printf("[RESIZE-%02d] to #bu %7zu = MB: %7.2f    | took: %13llu ti =
  //    %8.6f s\n",
  //            clht_gc_get_id(), ht_new->num_buckets, mba, (unsigned long long)
  //            e, e / 2.1e9);

#if defined(CLHTDEBUG)
  DEBUG_PRINT("-------------ht old------------\n");
  clht_print(ht_old);
  DEBUG_PRINT("-------------ht new------------\n");
  clht_print(ht_new);
  DEBUG_PRINT("-------------ht current------------\n");
  clht_print(h->ht);
  DEBUG_PRINT("-------------------------\n");
#endif

#if CLHT_DO_GC == 1
  clht_gc_collect(h);
#else
  clht_gc_release(ht_old);
#endif

  if (ht_resize_again) {
    ht_status(h, 1, 0);
  }
  return 1;
}

int CLHTLBIndex::clht_gc_collect(clht_t *hashtable) {
#if CLHT_DO_GC == 1
  CLHT_GC_HT_VERSION_USED(hashtable->ht);
  return clht_gc_collect_cond(hashtable, 1);
#else
  return 0;
#endif
}

int CLHTLBIndex::clht_gc_release(clht_hashtable_t *hashtable) {
  /* the CLHT_LINKED version does not allocate any extra buckets! */
#if !defined(CLHT_LINKED) && !defined(LOCKFREE_RES)
  uint64_t num_buckets = hashtable->num_buckets;
  volatile bucket_t *bucket = NULL;

  uint64_t bin;
  for (bin = 0; bin < num_buckets; bin++) {
    bucket = hashtable->table + bin;
    bucket = bucket->next;
    while (bucket != NULL) {
      volatile bucket_t *cur = bucket;
      bucket = bucket->next;
      // ssmem_release(clht_alloc, (void *)cur);
      nvmallocator_->Free((void *)cur);
    }
  }
#endif

  // ssmem_release(clht_alloc, hashtable->table);
  // ssmem_release(clht_alloc, hashtable);
  nvmallocator_->Free((void *)(hashtable->table));
  nvmallocator_->Free((void *)hashtable);
  return 1;
}

uint32_t CLHTLBIndex::clht_put_seq(clht_hashtable_t* hashtable, clht_addr_t key,
                             clht_val_t val, uint64_t bin) {
  volatile bucket_t* bucket = hashtable->table + bin;
  uint32_t j;

  do {
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      if (bucket->key[j] == 0) {
        bucket->val[j] = val;
        bucket->key[j] = key;
        return true;
      }
    }

    if (bucket->next == NULL) {
      DPP(put_num_failed_expand);
      int null;
      bucket->next = clht_bucket_create_stats(hashtable, &null);
      bucket->next->val[0] = val;
      bucket->next->key[0] = key;
      return true;
    }

    bucket = bucket->next;
  } while (true);
}

int CLHTLBIndex::bucket_cpy(clht_t* h, volatile bucket_t* bucket,
                      clht_hashtable_t* ht_new) {
  if (!LOCK_ACQ_RES(&bucket->lock)) {
    return 0;
  }
  uint32_t j;
  do {
    for (j = 0; j < ENTRIES_PER_BUCKET; j++) {
      clht_addr_t key = bucket->key[j];
      if (key != 0) {
        uint64_t bin = clht_hash(ht_new, key);

#if defined(CRASH_DURING_NODE_CREATE)
        pid_t pid = fork();

        if (pid == 0) {
          // Crash state soon after pointer swap.
          // This state will verify that all structural changes
          // have been performed before the final pointer swap
          clht_lock_initialization(h);
          DEBUG_PRINT("Child process returned during new bucket creation\n");
          DEBUG_PRINT("-------------ht new------------\n");
          clht_print(ht_new);
          DEBUG_PRINT("-------------ht current------------\n");
          clht_print(h->ht);
          DEBUG_PRINT("-------------------------\n");
          return -1;
        }

        else if (pid > 0) {
          int returnStatus;
          waitpid(pid, &returnStatus, 0);
          DEBUG_PRINT("Continuing in parent process to finish bucket copy\n");
        } else {
          DEBUG_PRINT("Fork failed");
          return 0;
        }
#endif

        clht_put_seq(ht_new, key, bucket->val[j], bin);
      }
    }
    bucket = bucket->next;
  } while (bucket != NULL);

  return 1;
}

};  // namespace CLHT
};  // namespace PIE