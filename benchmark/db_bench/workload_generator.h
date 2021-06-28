/*
 * @Author: your name
 * @Date: 2020-11-02 13:42:47
 * @LastEditTime: 2021-06-28 11:16:55
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/db_bench/workload_generator.h
 */
#ifndef INCLUDE_WORKLOAD_GENERATOR_H_
#define INCLUDE_WORKLOAD_GENERATOR_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include "db_bench.h"
#include "options.hpp"
#include "scheme.hpp"
#include "slice.hpp"
#include "timer.h"
#include "workload_generator.h"

namespace kv_benchmark {

struct generator_parameter {
public:
    int num_threads;

    size_t key_length;

    uint64_t num_test;

    std::string result_path;
};

class WorkloadGenerator {
public:
    WorkloadGenerator(const char* name, struct generator_parameter* param, PIE::Scheme* scheme, DBBench* benchmark[]);

public:
    void Run();

    void Print();

private:
    char name_[256];

    int num_threads_;

    size_t key_length_;

    uint64_t num_test_;

    std::string result_path_;

    PIE::Scheme* scheme_;

    kv_benchmark::DBBench* benchmarks_[32];
};
};

#endif