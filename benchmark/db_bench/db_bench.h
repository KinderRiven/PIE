/*
 * @Author: your name
 * @Date: 2020-11-02 13:42:48
 * @LastEditTime: 2021-06-29 14:34:43
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/db_bench/db_bench.h
 */
#ifndef INCLUDE_DB_BENCH_H_
#define INCLUDE_DB_BENCH_H_

#include "random.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DBBENCH_NUM_OPT_TYPE (3)
#define DBBENCH_PUT (0)
#define DBBENCH_GET (1)
#define DBBENCH_UPDATE (2)

namespace kv_benchmark {
// Only Support Single Thread
class DBBench {
public:
    DBBench(int type, uint32_t seed, size_t key_length)
        : type_(type)
        , key_length_(key_length)
    {
        printf(">>[YCSB] CREATE A NEW YCSB BENCHMARK!\n");
        random_ = new Random(seed);
        printf("  [TYPE:%d][SEED:%d][KV_LENGTH:%zuB]\n", type_, seed, key_length_);
    }

    ~DBBench()
    {
    }

public:
    bool initlizate()
    {
        return true;
    }

private:
    void generate_kv_pair(uint64_t uid, char* key)
    {
        *((uint64_t*)key) = uid;
    }

    int random_get_put()
    {
        return type_;
    }

public:
    int get_kv_pair(char* key, size_t& key_length)
    {
        generate_kv_pair((uint64_t)random_->Next(), key);
        key_length = key_length_;
        return random_get_put();
    }

public:
    int type_;

    Random* random_;

    size_t key_length_;
};
};

#endif