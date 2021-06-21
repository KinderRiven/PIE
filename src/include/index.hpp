#ifndef PIE_SRC_INCLUDE_INDEX_HPP__
#define PIE_SRC_INCLUDE_INDEX_HPP__

#include "status.hpp"

#include <cstdint>
#include <cstdlib>

namespace PIE {

  // An abstrat base index that provides basic 
  // operation interfaces
  class Index {
   public:
    Index() = default;
    Index(const Index&) = delete;
    Index(Index&&)      = delete;
    Index& operator=(const Index&)  = delete;
    Index& operator=(Index&&)       = delete;

    ~Index()=default;

   public:  
    // Insert one key-value pair into index. 
    // Return kOk to indicate this insert operation success, otherwise 
    // any non-ok code would indicate an error.
    // Note: kInsertExistKey would be considered tolerant
    virtual status_code_t Insert(const char *key, size_t key_len, void *value ) = 0;
    
    // Search and return related value of given key 
    // If the key does not exist in current index, kNotFound would 
    // be returned, otherwise kOk is returned
    virtual status_code_t Search(const char *key, size_t key_len, void **value) = 0;

    // Update specified key with given value.
    // If the key does not exist in current index, kNotFound would 
    // be returned, otherwise return kOk if update success
    virtual status_code_t Update(const char *key, size_t key_len, void *value ) = 0;

    // Update specified key with given value if target key exists
    // otherwise insert target key-value pair. 
    // This interface always return kOk unless memory allocation error
    // occurs
    virtual status_code_t Upsert(const char *key, size_t key_len, void *value ) = 0;

    // Scan from start key and return its "count" successors
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    virtual status_code_t ScanCount(const char *startkey, size_t key_len, size_t count, 
                                    void **vec) = 0;

    // Scan to fetch keys within the range [startkey, endkey);
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    virtual status_code_t Scan(const char *startkey, size_t startkey_len, 
                               const char *endkey  , size_t endkey_len  , void **vec) = 0;

    // Printout Any related index message:
    // such as the height of B+Tree or max height of radix tree
    // The bucket/slot number of hash table
    virtual void Print() = 0;
  };
};


#endif