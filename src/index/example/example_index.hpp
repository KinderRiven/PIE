/*
 * @Author: your name
 * @Date: 2021-06-22 18:50:26
 * @LastEditTime: 2021-06-22 19:02:02
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/index/example/example_index.hpp
 */
#ifndef PIE_SRC_INDEX_EXAMPLE_EXAMPLE_INDEX_HPP__
#define PIE_SRC_INDEX_EXAMPLE_EXAMPLE_INDEX_HPP__

#include "allocator.hpp"
#include "ccehhash.hpp"
#include "index.hpp"
#include "internal_string.h"

#include "persist.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <pthread.h>
#include <vector>

namespace PIE {

namespace example {
    class ExampleIndex : public Index {
    public:
        ExampleIndex();

        ~ExampleIndex();

    public:
        status_code_t Insert(const char* key, size_t key_len, void* value);

        status_code_t Search(const char* key, size_t key_len, void** value);

        status_code_t Update(const char* key, size_t key_len, void* value);

        status_code_t Upsert(const char* key, size_t key_len, void* value);

        status_code_t ScanCount(const char* startkey, size_t key_len, size_t count, void** vec);

        status_code_t Scan(const char* startkey, size_t startkey_len, const char* endkey, size_t endkey_len, void** vec);

        void Print();
    };
};
};

#endif