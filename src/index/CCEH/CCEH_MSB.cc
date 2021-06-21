#include "CCEH_MSB.hpp"

#include <thread>

namespace PIE {
namespace CCEH {

  void CCEH::insert(const CCEH_Key_t &key, CCEH_Value_t value) {
    
    static thread_local uint8_t tmpkey[1024];

    auto f_hash = hash_funcs[0](Data(key), Size(key), f_seed);
    auto f_idx = (f_hash & kMask) * kNumPairPerCacheLine;

  RETRY:
    // calculate most significant bits
    // 11...11110000000...0000
    // |-depth-|
    auto x = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
    auto target = dir->_[x];

    if (!target) {
      std::this_thread::yield();
      goto RETRY;
    }

    /* acquire segment exclusive lock */
    if (!target->lock()){
      std::this_thread::yield();
      goto RETRY;
    }

    auto target_check = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
    if (target != dir->_[target_check]){
	    target->unlock();
      std::this_thread::yield();
      goto RETRY;
    }

    // For lazy deletion, insert will calculate each slot's hash value and 
    // check if this value satisfies segment's local depth pattern
    auto target_local_depth = target->local_depth;
    auto pattern = (f_hash >> (8 * sizeof(f_hash) - target->local_depth));

    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i) {
      auto loc = (f_idx + i) % Segment::kNumSlot;

      // for string key, _key stores the pointer's value, instead
      // of true string contents
      uint64_t _key = reinterpret_cast<uint64_t>(target->_[loc].key);
      size_t hashval = 0;
      const CCEH_Key_t &keycontent = target->_[loc].key;

      // calculate hash value
      if (_key == NONE || _key == INVALID) { goto INSERT1; }
      if (_key == SENTINEL) { continue; }

      // for string key, we need to copy key data to one buffer to 
      // calculate hash value
      hashval = hash_funcs[0](Data(keycontent), Size(keycontent), f_seed);

      // Check if this slot matches current segment pattern
      if ((hashval >> (8 * sizeof(f_hash) - target_local_depth)) != pattern) {

    INSERT1:
        if (CCEH_CAS(&target->_[loc].key, &_key, SENTINEL)) {
          // Successfully get this slot position
          // We need to set value first to guarantee crash 
          // consistence and concurrent consistence
          target->_[loc].value = value;
          asm_mfence();

          // Only do "value" copy, no content copy for string key
          target->_[loc].key = reinterpret_cast<uint64_t>(key);
          persist_data((char *)&target->_[loc], sizeof (CCEH_Pair));

          target->unlock();
          return;
        }
	    }
    }

    auto s_hash = hash_funcs[2](Data(key), Size(key), s_seed);
    auto s_idx  = (s_hash & kMask) * kNumPairPerCacheLine;

    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i){

	    auto loc = (s_idx + i) % Segment::kNumSlot;

      uint64_t _key = reinterpret_cast<uint64_t>(target->_[loc].key);
      size_t hashval = 0;
      const CCEH_Key_t &keycontent = target->_[loc].key;

      // calculate hash value
      if (_key == 0 || _key == INVALID) { goto INSERT2; }
      if (_key == SENTINEL) { continue; }

      // for string key, we need to copy key data to one buffer to 
      // calculate hash value
      hashval = hash_funcs[0](Data(keycontent), Size(keycontent), f_seed);

      if ((hashval >> (8 * sizeof(f_hash) - target_local_depth)) != pattern) {
        
      INSERT2:
        if (CCEH_CAS(&(target->_[loc].key), &_key, SENTINEL)) {
          // Successfully get this slot position
          target->_[loc].value = value;
          asm_mfence();

          // Only do "value" copy, no content copy for string key
          target->_[loc].key = reinterpret_cast<uint64_t>(key);
          persist_data((char *)&target->_[loc], sizeof (CCEH_Pair));

          // release exclusive lock
          target->unlock();
          return;
        }
      }
    }

    // COLLISION!!
    /* need to split segment but release the exclusive lock first to avoid deadlock */
    target->unlock();

    if (!target->suspend()) {
      std::this_thread::yield();
      goto RETRY;
    }

    /* need to check whether the target segment has been split */
#ifdef INPLACE
    if (target_local_depth != target->local_depth){
      target->sema = 0;
      std::this_thread::yield();
      goto RETRY;
    }
#else
    if (target_local_depth != dir->_[x]->local_depth){
      target->sema = 0;
      std::this_thread::yield();
      goto RETRY;
    }
#endif

    Segment** s = SegmentSplit(target);

DIR_RETRY:
    /* need to double the directory */
    if (target_local_depth == dir->depth){
    // Stop other thread enter when doing split
    if (!dir->suspend()){
      std::this_thread::yield;
      goto DIR_RETRY;
    }

    x = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
    auto dir_old = dir;
    auto d = dir->_;
    auto _dir = AllocDirectory(dir->depth+1);
    for (unsigned i = 0; i < dir->capacity; ++i) {
      if (i == x) {
        _dir->_[2*i]    = s[0];
        _dir->_[2*i+1]  = s[1];
      }
      else {
        _dir->_[2*i]    = d[i];
        _dir->_[2*i+1]  = d[i];
      }
    }
    persist_data((char*)&_dir->_[0], sizeof(Segment*)*_dir->capacity);
    persist_data((char*)&_dir, sizeof(Directory));
    dir = _dir;
    persist_data((char*)&dir, sizeof(void*));
#ifdef INPLACE
    s[0]->local_depth++;
    clflush((char*)&s[0]->local_depth, sizeof(size_t));
    /* release segment exclusive lock */
    s[0]->sema = 0;
#endif

	/* TBD */
	// delete dir_old;
    } else {
      // normal segment split
      while (!dir->lock()){
        asm("nop");
      }

      x = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
      if (dir->depth == target_local_depth + 1) {
        if (x % 2 == 0) {
          dir->_[x+1] = s[1];
    #ifdef INPLACE
          clflush((char*)&dir->_[x+1], 8);
    #else
          asm_mfence();
          dir->_[x] = s[0];
          persist_data((char*)&dir->_[x], 16);
    #endif
        } else {
          dir->_[x] = s[1];
    #ifdef INPLACE
          clflush((char*)&dir->_[x], 8);
    #else
          asm_mfence();
          dir->_[x-1] = s[0];
          persist_data((char*)&dir->_[x-1], 16);
    #endif
        }	    
        dir->unlock();
    #ifdef INPLACE
        s[0]->local_depth++;
        clflush((char*)&s[0]->local_depth, sizeof(size_t));
        /* release target segment exclusive lock */
        s[0]->sema = 0;
    #endif
      } else {
        int stride = pow(2, dir->depth - target_local_depth);
        auto loc = x - (x % stride);
        for (int i=0; i < stride / 2; ++i){
          dir->_[loc+stride/2+i] = s[1];
        }
#ifdef INPLACE
        clflush((char*)&dir->_[loc+stride/2], sizeof(void*)*stride/2);
#else 
        for (int i = 0; i < stride / 2; ++i){
          dir->_[loc+i] = s[0];
        }
        persist_data((char*)&dir->_[loc], sizeof(void*)*stride);
#endif
        dir->unlock();
#ifdef INPLACE
        s[0]->local_depth++;
        clflush((char*)&s[0]->local_depth, sizeof(size_t));
      /* release target segment exclusive lock */
        s[0]->sema = 0;
#endif
      }
    }
    std::this_thread::yield();
    goto RETRY;
  }

  // Search for target value and return
  // return nullptr if didn't find it
  CCEH_Value_t CCEH::get(const CCEH_Key_t &key) {

    auto f_hash = hash_funcs[0](Data(key), Size(key), f_seed);
    auto f_idx = (f_hash & kMask) * kNumPairPerCacheLine;

    RETRY:
      while(dir->sema < 0){
	      asm("nop");
      }

      auto x = (f_hash >> (8 * sizeof(f_hash) - dir->depth)); 
      auto target = dir->_[x];

      if (!target) {
        std::this_thread::yield();
        goto RETRY;
      }
      
      /* acquire segment shared lock */
      if (!target->lock()) {
        std::this_thread::yield();
        goto RETRY;
      }

      auto target_check = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
      if(target != dir->_[target_check]) {
        target->unlock();
        std::this_thread::yield();
        goto RETRY;
      }

    // Start do search operation within one segment
    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i) {
	    auto loc = (f_idx + i) % Segment::kNumSlot;
      // temporary store 8B value
      uint64_t _key = reinterpret_cast<uint64_t>(target->_[loc].key);

      if (_key != NONE && _key != INVALID && _key != SENTINEL) {
        // Do complete key compare
        if (target->_[loc].key == key) {
          auto v = target->_[loc].value;
          target->unlock();
          return v;
        }
      }
    }

    auto s_hash = hash_funcs[2](Data(key), Size(key), s_seed);
    auto s_idx = (s_hash & kMask) * kNumPairPerCacheLine;

    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i){
	    auto loc = (s_idx + i) % Segment::kNumSlot;
      // temporary store 8B value
      uint64_t _key = reinterpret_cast<uint64_t>(target->_[loc].key);

      if (_key != NONE && _key != INVALID && _key != SENTINEL) {
        // Do complete key compare
        if (target->_[loc].key == key) {
          auto v = target->_[loc].value;
          target->unlock();
          return v;
        }
      }
    }
    // key not found, release segment shared lock 
    target->unlock();
    // return nullptr to indicate Key is not found
    return nullptr;
  }

  Segment** CCEH::SegmentSplit(Segment *target) {
    Segment** split = new Segment*[2];

    split[0] = AllocSegment(target->local_depth+1);
    split[1] = AllocSegment(target->local_depth+1);

    auto pattern = ((size_t)1 << (sizeof(uint64_t) * 8 - target->local_depth - 1));
    
    // redistribute all data in current segment
    for (unsigned i = 0; i < Segment::kNumSlot; ++i) {
      auto _key = reinterpret_cast<uint64_t>(target->_[i].key);

      if (_key == 0 || _key == INVALID || _key == SENTINEL) { continue; }
      auto f_hash = hash_funcs[0](Data(target->_[i].key), Size(target->_[i].key), f_seed);
      auto s_hash = hash_funcs[2](Data(target->_[i].key), Size(target->_[i].key), s_seed);

      if (f_hash & pattern) {
        if (!split[1]->Insert4split(target->_[i].key, target->_[i].value, (f_hash & kMask) * kNumPairPerCacheLine)
          &&!split[1]->Insert4split(target->_[i].key, target->_[i].value, (s_hash & kMask) * kNumPairPerCacheLine)) {
          std::cerr << "[" << __func__ << "]: something wrong -- need to adjust probing distance" << std::endl;
        }
      } else {
        if (!split[0]->Insert4split(target->_[i].key, target->_[i].value, (f_hash & kMask) * kNumPairPerCacheLine)
          &&!split[0]->Insert4split(target->_[i].key, target->_[i].value, (s_hash & kMask) * kNumPairPerCacheLine)) {
            std::cerr << "[" << __func__ << "]: something wrong -- need to adjust probing distance" << std::endl;
        }
      }
    }

    persist_data((char *)split[0], sizeof (Segment));
    persist_data((char *)split[1], sizeof (Segment));

    return split;
  }

};  // namespace CCEH
};  // namespace PIE