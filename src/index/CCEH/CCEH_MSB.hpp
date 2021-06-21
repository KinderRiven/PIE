#ifndef PIE_SRC_INDEX_CCEH_CCEH_MSB_HPP__
#define PIE_SRC_INDEX_CCEH_CCEH_MSB_HPP__

#include "index.hpp"
#include "allocator.hpp"
#include "internal_string.h"
#include "ccehhash.hpp"

#include "persist.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <pthread.h>
#include <iostream>

#define f_seed 0xc70697UL
#define s_seed 0xc70697UL

namespace PIE  {
namespace CCEH {
  
  // Key-Value data type definition
  // key can be either variable-sized string or 64-bits integer
  // Value is void *;
#ifdef STRING
  #define CCEH_Key_t InternalString
  #define ConvertToCCEHKey(key, len, dst)  InternalString((key), (len), (dst))
#else 
  #define CCEH_Key_t uint64_t
  #define ConvertToCCEHKey(key, len, dst) reinterpret_cast<uint64_t>((key))
#endif
  using CCEH_Value_t = void *;

  // Data() and Size() provides uniform interface for fetching size and 
  // start address of data for different key-value type. Here we treat
  // integer type uint64_t as 8B string
  const uint8_t* Data(const InternalString &key) { return key.Data(); }
  size_t Size(const InternalString &key) { return key.Length(); }

  const uint8_t* Data(uint64_t key) { return (const uint8_t*)(&key); }
  size_t Size(uint64_t key) { return sizeof(key); }


  // Note that #define is not bounded by namespace
  // Thus we use CCEH prefix to indicate this is only allowed
  // to be used within CCEH implementation
  #define CCEH_CAS(_p, _u, _v)  (__atomic_compare_exchange_n (_p, _u, _v, false, \
    __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))

  constexpr size_t kSegmentBits         = 8;
  constexpr size_t kMask                = (1 << kSegmentBits)-1;
  constexpr size_t kShift               = kSegmentBits;
  constexpr size_t kSegmentSize         = (1 << kSegmentBits) * 16 * 4;
  constexpr size_t kNumPairPerCacheLine = 4;
  constexpr size_t kNumCacheLine        = 8;

  // The Following const uint64_t has special usage. 
  // DO NOT use them as integer key when using CCEH.

  // SENTINEL is used to indicate this slot is already being 
  // used. When one insert thread happens to see such flag, it 
  // would treat this slot as non-empty one but see its integer
  // value as invalid one.
  constexpr uint64_t SENTINEL = -2;

  // INVALID is used to indicate data stored in one slot is 
  // invalid, which means: for integer key, this uint64_t 
  // specified value is may not exist in index now; For string
  // key, pointer specified by this uint64_t may point to 
  // invalid memory location. We use INVALID to first mark a 
  // deletion slot, deallocate its memory and finally erase it
  // by setting this slot to be NONE
  constexpr uint64_t INVALID  = -1;

  // NONE is a special flag which is used to indicate current 
  // position does not store any data. Therefore, a newly allocate
  // segment's pair array should be initialized as 0's
  constexpr uint64_t NONE     =  0;

  // The most foundamental key-value store unit which contains both 
  // key & value together. Unlike most tree-based structure, CCEH has
  // no fingerprint or temperory hashtag
  struct CCEH_Pair {
    CCEH_Key_t    key;
    CCEH_Value_t  value;
  };

  // According to CCEH paper: Segment is aggregation of multiple hash
  // slots(buckets) to reduce the size of directory in traditional 
  // Extendible Hash
  // A segment can store 1024 slots by default
  struct Segment {
    // Slot number within a single segment
    static constexpr size_t kNumSlot = kSegmentSize / sizeof(CCEH_Pair);

    // Constructors
    Segment(void) : local_depth(0) {}
    Segment(size_t depth) :local_depth(depth) {}
    ~Segment(void) {}

    // Wait until all threads exit
    bool suspend(void){
      int64_t val;
      do {
        val = sema;
        if (val < 0)
          return false;
      } while (!CCEH_CAS(&sema, &val, -1));

      int64_t wait = 0 - val - 1;
      while (val && sema != wait) {
        asm("nop");
      }
      return true;
    }

    // shared lock
    bool lock(void){
      int64_t val = sema;
      while (val > -1){
        if (CCEH_CAS(&sema, &val, val+1)) {
          return true;
        }
        val = sema;
      }
      return false;
    }

    void unlock(void){
      int64_t val = sema;
      while (!CCEH_CAS(&sema, &val, val-1)){
        val = sema;
      }
    }
    // Insert4split function insert specified key-value pair
    // into current segment
    bool Insert4split(const CCEH_Key_t&,  CCEH_Value_t, size_t);

    // Pair array to store multiple slots
    CCEH_Pair _[kNumSlot];
    // Sema for concurrency control
    int64_t sema = 0;
    // local depth is a necessary variable for 
    // extendible hash
    size_t local_depth;
  };

  struct Directory {

    static constexpr size_t kDefaultDepth = 10;

    // Directory has no constructors: 
    // Its initilization and allocation is managed by 
    // CCEH class
    Directory()=default;
    ~Directory()=default;


    bool suspend(void) {
      int64_t val;
      do {
        val = sema;
        if (val < 0) {
          return false;
        }
      } while (!CCEH_CAS(&sema, &val, -1));

      int64_t wait = 0 - val - 1;
      while(val && sema != wait){
        asm("nop");
      }
      return true;
    }

    bool lock(void) {
      int64_t val = sema;
      while(val > -1){
        if (CCEH_CAS(&sema, &val, val+1)) {
          return true;
        }
        val = sema;
      }
        return false;
    }

    void unlock(void){
      int64_t val = sema;
      while(!CCEH_CAS(&sema, &val, val-1)){
        val = sema;
      }
    }
    
    // A pointer array consists multiple pointers
    // points to different segments (when local depth==global)
    // or same segments (when local depth < global);
    Segment** _;

    // Some neccessary information and concurrency control bit
    int64_t sema = 0;
    size_t capacity;
    size_t depth;
  };

  // CCEH means Cache-Concious Extendible Hashing
  // The derivated class of Index specifies the 
  // implementations of index operations
  class CCEH : public Index {
   public:  
    // Note: Any constructor of CCEH need to have
    // exactly on memory allocator 
    CCEH::CCEH(Allocator* nvm_allocator, size_t initCap) 
        : nvm_allocator_(nvm_allocator) {
      dir = AllocDirectory(static_cast<size_t>(log2(initCap)));
      for (unsigned i = 0; i < dir->capacity; ++i) {
        dir->_[i] = AllocSegment(static_cast<size_t>(log2(initCap)));
      }
      printf("[CCEH is working!]\n");
    }

    CCEH::CCEH(Allocator* nvm_allocator)
        : nvm_allocator_(nvm_allocator) {
      dir = AllocDirectory(0);
      for (unsigned i = 0; i < dir->capacity; ++i) {
        dir->_[i] = AllocSegment(0);
      }
    }

    ~CCEH()=default;

    // CCEH inner write/read interface
    // Note: for insert operation, the the key parameter
    // specifies the data storage locatition
    void insert(const CCEH_Key_t&, CCEH_Value_t);
    CCEH_Value_t get(const CCEH_Key_t&);
   
   public:
    status_code_t Insert(const char *key, size_t len, void  *value) override;
    status_code_t Search(const char *key, size_t len, void **value) override;
    // Use Insert implementation as CCEH does not provide update
    // interface. 
    inline status_code_t Update(const char *key, size_t len, 
                                void  *value) override {
      return Insert(key, len, value);
    }

    // Use Insert implementation as CCEH does not provide upsert
    // interface. 
    inline status_code_t Upsert(const char *key, size_t len, 
                                void  *value) override {
      return Insert(key, len, value);
    }

    // Two Scan interface of CCEH, both of them need to Scan across all key 
    // value pair to find key-values reside in range [startkey,..) or [startkey, endkey)
    //  ScanCount sort all key-value pair and pick the first count elements, thus the 
    //  return array is sorted
    //  Scan does not sort them thus the return array is unsorted
    status_code_t ScanCount(const char *startkey, size_t key_len, 
                            size_t count, void **vec) override {
      // TODO
      return kOk;
    }

    status_code_t Scan(const char *startkey, size_t startkey_len, 
                       const char *endkey, size_t endkey_len, void **vec) override {
      // TODO
      return kOk;
    }


    // Print some basic information
    inline void Print() override {
      nvm_allocator_->Print();
      std::cout << "[CCEH]" << "[Global Depth]" << "[" << dir->depth << "]" << std::endl;
      return;
    }
  
   private:
    // Split a segment and return its  two  "child" segment via a segment array
    Segment**  SegmentSplit(Segment *);

    // Allocate a directory of specific depth its capacity would be pow(2, depth)
    Directory* AllocDirectory(size_t depth);

    // Allocate a segment and set its local  depth to be exactly input depth
    Segment*   AllocSegment(size_t depth);
   private:

    // root directory of CCEH, any access operation first search dir for target
    // segment, then dive into segment to perform read or write
    Directory* dir;

    // nvm_allocator_ is used to allocate any neccessary message
    Allocator* nvm_allocator_;
  };

  // Insert key-value pair into CCEH index with provided insert
  // interface. CCEH only provides meta data index. 
  // By meta data we mean an integer key or a pointer to persistent 
  // data for true variable-length string key
  inline status_code_t CCEH::Insert(const char *key, size_t len, 
                                    void  *value) {
    // First convert generalized key to be internalkey and persist it
    uint8_t *dataptr = reinterpret_cast<uint8_t*>
                    (nvm_allocator_->Allocate(sizeof(uint32_t)+len));
    const CCEH_Key_t& internalkey = ConvertToCCEHKey(key, len, dataptr);
    persist_data((char *)dataptr, sizeof(uint32_t)+len);

    insert (internalkey, value);

    // According to CCEH::insert implementation, insert always successed
    // except for memory allocation or system errors.
    return kOk;
  }

  // Search for coresponding value of key sepcified by "key+len" pair
  // Use CCEH internal "get" function implementation
  inline status_code_t CCEH::Search(const char *key, size_t len, 
                                    void **value) {
    // Use local buffer to avoid dynamic memory allocation
    // Can we use static buffer? Is it safe to use static 
    // under concurrency condition?
    static thread_local uint8_t key_buff[1024];
    CCEH_Key_t internalkey = ConvertToCCEHKey(key, len, key_buff);
    CCEH_Value_t val = get(internalkey);
    if (val == nullptr) {
      return kNotFound;
    }
    *value = val;
    return kOk;
  }

  // Allocate directory of which global depth is depth_. pow(2, depth_)
  // segment pointer are needed. But do not allocate segment inside this
  // helper functions
  inline Directory* CCEH::AllocDirectory(size_t depth_) {
    Directory* directory_ptr = reinterpret_cast<Directory*>
              (nvm_allocator_->Allocate(sizeof (Directory)));

    directory_ptr->capacity = pow(2, depth_);
    directory_ptr->depth = depth_;
    // Allocate pointer array
    directory_ptr->_ = reinterpret_cast<Segment**>
                (nvm_allocator_->Allocate(sizeof (Segment*) * directory_ptr->capacity));
    // Init concurrency control bit
    directory_ptr->sema = 0;
    return directory_ptr;
  }

  // Allocate a segment of which local depth is given parameter
  // depth. We need to init memory to be zero for string key condition
  inline Segment* CCEH::AllocSegment(size_t depth) {
    auto ret = reinterpret_cast<Segment*>
          (nvm_allocator_->AllocateAlign(sizeof(Segment), 64));
    // Init all memory to be zero
    memset (ret, 0, sizeof(Segment));
    ret->sema = 0;
    ret->local_depth = depth;
    return ret;
  }
  

  // Only used for segment splitting
  inline bool Segment::Insert4split(const CCEH_Key_t& keyptr, CCEH_Value_t value, size_t loc) {
    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i) {
      auto slot = (loc + i) % kNumSlot;
      if (_[slot].key == INVALID || _[slot].key == NONE) {
        _[slot].key = reinterpret_cast<uint64_t>(keyptr);
        _[slot].value = value;
        return true;
      }
    }
    return false;
  }

}; // namespace CCEH
}; // namespace PIE

#endif  // PIE_SRC_INDEX_CCEH_CCEH_MSB_HPP__