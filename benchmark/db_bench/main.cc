#include <algorithm>
#include <assert.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <pthread.h>
#include <queue>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "options.hpp"
#include "scheme.hpp"
#include "slice.hpp"
#include "timer.h"
#include "workload_generator.h"

using namespace PIE;

struct workload_options {
public:
    int type;
    char name[256];

public:
    int num_threads;
    size_t key_length;
    std::string result_path;
    uint64_t num_test;
    Scheme* scheme;
};

static void start_workload(struct workload_options* options)
{
    kv_benchmark::DBBench* _benchmarks[32];
    uint32_t _seed = 1000;

    for (int i = 0; i < options->num_threads; i++) {
        _benchmarks[i] = new kv_benchmark::DBBench(options->type, _seed + i, options->key_length);
    }

    kv_benchmark::generator_parameter _gparam;
    _gparam.key_length = options->key_length;
    _gparam.num_threads = options->num_threads;
    _gparam.num_test = options->num_test;
    _gparam.result_path = options->result_path;

    kv_benchmark::WorkloadGenerator* _run = new kv_benchmark::WorkloadGenerator(options->name, &_gparam, options->scheme, _benchmarks);
    _run->Run();
}

int main(int argc, char* argv[])
{
    Options _options;
    _options.index_type = kCCEH;
    _options.scheme_type = kSingleScheme;
    _options.pmem_file_path = "/home/pmem0/PIE";
    _options.pmem_file_size = 100UL * 1024 * 1024 * 1024;

    int _num_threads = 1;
    size_t _key_length = 8;
    size_t _num_test = 1000000;
    size_t _num_warmup = 5000000;

    char _index_type[128];
    char _pmem_path[128] = "/home/pmem0";

    // Workload Generator
    for (int i = 0; i < argc; i++) {
        char junk;
        uint64_t n;
        double f;
        if (sscanf(argv[i], "--key_length=%llu%c", &n, &junk) == 1) {
            _key_length = n;
        } else if (sscanf(argv[i], "--num_thread=%llu%c", &n, &junk) == 1) {
            _num_threads = n;
        } else if (sscanf(argv[i], "--num_warmup=%llu%c", &n, &junk) == 1) { // GB
            _num_warmup = n;
        } else if (sscanf(argv[i], "--num_test=%llu%c", &n, &junk) == 1) { // GB
            _num_test = n;
        } else if (sscanf(argv[i], "--pmem_file_size=%llu%c", &n, &junk) == 1) {
            _options.pmem_file_size = n * (1024UL * 1024 * 1024);
        } else if (strncmp(argv[i], "--pmem_file_path=", 17) == 0) {
            strcpy(_pmem_path, argv[i] + 17);
            _options.pmem_file_path.assign(argv[i] + 17);
        } else if (strncmp(argv[i], "--index=", 8) == 0) {
            strcpy(_index_type, argv[i] + 8);
            if (!strcmp(_index_type, "CCEH")) {
                _options.index_type = kCCEH;
            } else if (!strcmp(_index_type, "FASTFAIR")) {
                _options.index_type = kFASTFAIR;
            } else if (!strcmp(_index_type, "RHTREE")) {
                _options.index_type = kRHTREE;
            }
        } else if (i > 0) {
            std::cout << "ERROR PARAMETER [" << argv[i] << "]" << std::endl;
            exit(1);
        }
    }

    Scheme* _scheme;
    Scheme::Create(_options, &_scheme);

    // CREATE RESULT SAVE PATH
    time_t _t = time(NULL);
    struct tm* _lt = localtime(&_t);
    char _result_path[128];
    sprintf(_result_path, "%04d%02d%02d_%02d%02d%02d", 1900 + _lt->tm_year, _lt->tm_mon, _lt->tm_mday, _lt->tm_hour, _lt->tm_min, _lt->tm_sec);
    mkdir(_result_path, 0777);

    struct workload_options _wopt;
    _wopt.key_length = _key_length;
    _wopt.num_threads = _num_threads;
    _wopt.scheme = _scheme;
    _wopt.result_path.assign(_result_path);

    strcpy(_wopt.name, "WARMUP");
    _wopt.type = DBBENCH_PUT;
    _wopt.num_test = _num_test;
    start_workload(&_wopt);

    strcpy(_wopt.name, "SINGLE_PUT");
    _wopt.type = DBBENCH_PUT;
    _wopt.num_test = _num_test;
    start_workload(&_wopt);

    strcpy(_wopt.name, "SINGLE_GET");
    _wopt.type = DBBENCH_GET;
    _wopt.num_test = _num_test;
    start_workload(&_wopt);
    return 0;
}
