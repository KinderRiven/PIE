/*
 * @Author: your name
 * @Date: 2021-06-22 15:23:31
 * @LastEditTime: 2021-06-22 19:09:03
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/include/single_scheme.hpp
 */

#ifndef PIE_SRC_SCHEME_SINGLE_SINGLE_SCHEME_HPP__
#define PIE_SRC_SCHEME_SINGLE_SINGLE_SCHEME_HPP__

#include "index.hpp"
#include "scheme.hpp"

namespace PIE {
class SingleScheme : public Scheme {
public:
    SingleScheme(const Options& options);

    ~SingleScheme();

public:
    // Insert one key-value pair into index.
    // Return kOk to indicate this insert operation success, otherwise
    // any non-ok code would indicate an error.
    // Note: kInsertExistKey would be considered tolerant
    Status Insert(Slice& key, void* value);

    // Search and return related value of given key
    // If the key does not exist in current index, kNotFound would
    // be returned, otherwise kOk is returned
    Status Search(Slice& key, void** value);

    // Update specified key with given value.
    // If the key does not exist in current index, kNotFound would
    // be returned, otherwise return kOk if update success
    Status Update(Slice& key, void* value);

    // Update specified key with given value if target key exists
    // otherwise insert target key-value pair.
    // This interface always return kOk unless memory allocation error
    // occurs
    Status Upsert(Slice& key, void* value);

    // Scan from start key and return its "count" successors
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    Status ScanCount(Slice& startkey, size_t count, void** vec);

    // Scan to fetch keys within the range [startkey, endkey);
    // Coresponding values are placed in a void* array specified by "vec"
    // However, values are not guaranteed to be SORTED;
    Status Scan(Slice& startkey, Slice& endkey, void** vec);

    // Printout Any related index message:
    // such as the height of B+Tree or max height of radix tree
    // The bucket/slot number of hash table
    void Print();

private:
    Index* index_;
};
};

#endif // PIE_SRC_SCHEME_SINGLE_SINGLE_SCHEME_HPP__