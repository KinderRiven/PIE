/*
 * @Author: your name
 * @Date: 2021-06-22 14:35:53
 * @LastEditTime: 2021-06-22 19:37:06
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/include/scheme.hpp
 */

#ifndef PIE_INCLUDE_SCHEME_HPP__
#define PIE_INCLUDE_SCHEME_HPP__

#include <cstdint>
#include <cstdlib>

#include "options.hpp"
#include "slice.hpp"
#include "status.hpp"

namespace PIE {

class Scheme {
public:
    // Create a scheme according to the options.
    // Stores a pointer to a heap-allocated database in *schemeptr and returns OK on success.
    // Stores NULL in *schemeptr and returns a non-OK status on error.
    // Caller should delete *schemeptr when it is no longer needed.
    static Status Create(const Options& options, Scheme** schemeptr);

    Scheme() { }

    virtual ~Scheme();

private: // No copying allowed
    Scheme(const Scheme&);

    void operator=(const Scheme&);

public:
    // Insert one key-value pair into index.
    // Return kOk to indicate this insert operation success, otherwise
    // any non-ok code would indicate an error.
    // Note: kInsertExistKey would be considered tolerant
    virtual Status Insert(const Slice& key, void* value) = 0;

    // Search and return related value of given key
    // If the key does not exist in current index, kNotFound would
    // be returned, otherwise kOk is returned
    virtual Status Search(const Slice& key, void** value) = 0;

    // Update specified key with given value.
    // If the key does not exist in current index, kNotFound would
    // be returned, otherwise return kOk if update success
    virtual Status Update(const Slice& key, void* value) = 0;

    // Update specified key with given value if target key exists
    // otherwise insert target key-value pair.
    // This interface always return kOk unless memory allocation error
    // occurs
    virtual Status Upsert(const Slice& key, void* value) = 0;

    // Scan from start key and return its "count" successors
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    virtual Status ScanCount(const Slice& startkey, size_t count, void** vec) = 0;

    // Scan to fetch keys within the range [startkey, endkey);
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    virtual Status Scan(const Slice& startkey, const Slice& endkey, void** vec) = 0;

    // Printout Any related index message:
    // such as the height of B+Tree or max height of radix tree
    // The bucket/slot number of hash table
    virtual void Print() = 0;
};

};

#endif // PIE_INCLUDE_SCHEME_HPP__