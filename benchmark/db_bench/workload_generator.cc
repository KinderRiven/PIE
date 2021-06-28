#include "workload_generator.h"
#include "timer.h"
#include <assert.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using namespace PIE;
using namespace kv_benchmark;

// #define RESULT_OUTPUT_TO_FILE
static char _g_oname[DBBENCH_NUM_OPT_TYPE][32] = { "PUT", "GET", "UPDATE" };
static int g_numa[] = { 0, 2, 4, 6, 8, 20, 22, 24, 26, 28, 10, 12, 14, 16, 18, 30, 32, 34, 36, 38 };

struct thread_param_t {
public:
    PIE::Scheme* scheme;
    kv_benchmark::DBBench* benchmark;

public:
    int thread_id;
    int count;

public: // result
    uint64_t sum_latency;
    uint64_t result_count[16];
    uint64_t result_latency[16];
    uint64_t result_success[16];

public:
    std::vector<uint64_t> vec_latency[16];

public:
    thread_param_t()
        : sum_latency(0)
    {
        memset(result_count, 0, sizeof(result_count));
        memset(result_latency, 0, sizeof(result_latency));
        memset(result_success, 0, sizeof(result_success));
    }
};

static void result_output(const char* name, std::vector<uint64_t>& data)
{
    std::ofstream fout(name);
    if (fout.is_open()) {
        for (int i = 0; i < data.size(); i++) {
            fout << data[i] << std::endl;
        }
        fout.close();
    }
}

static void thread_task(thread_param_t* param)
{
    int _thread_id = param->thread_id;

#if 1
    cpu_set_t _mask;
    CPU_ZERO(&_mask);
    CPU_SET(g_numa[_thread_id], &_mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(_mask), &_mask) < 0) {
        printf("threadpool, set thread affinity failed.\n");
    }
#endif

    PIE::Scheme* _scheme = param->scheme;
    kv_benchmark::DBBench* _benchmark = param->benchmark;

    assert((_benchmark != nullptr) && (_scheme != nullptr));
    _benchmark->initlizate();

    char _key[128];
    void* _value;
    size_t _key_length;
    int _count = param->count;

    Timer _t1, _t2;
    uint64_t _latency = 0;

    _t1.Start();
    for (int i = 0; i < _count; i++) {
        int __type = _benchmark->get_kv_pair(_key, _key_length);
        if (__type == DBBENCH_PUT) {
            Slice __skey(_key, _key_length);
            _t2.Start();
            Status __status = _scheme->Insert(__skey, _value);
            _t2.Stop();
            param->result_count[__type]++;
            if (__status.ok()) {
                param->result_success[__type]++;
            }
        } else if (__type == DBBENCH_GET) {
            Slice __skey(_key, _key_length);
            _t2.Start();
            Status __status = _scheme->Insert(__skey, _value);
            _t2.Stop();
            param->result_count[__type]++;
            if (__status.ok()) {
                param->result_success[__type]++;
            }
        } else if (__type == DBBENCH_UPDATE) {
            Slice __skey(_key, _key_length);
            _t2.Start();
            Status __status = _scheme->Update(__skey, _value);
            _t2.Stop();
            param->result_count[__type]++;
            if (__status.ok()) {
                param->result_success[__type]++;
            }
        }
        _latency = _t2.Get();
        param->result_latency[__type] += _latency;
        param->sum_latency += _latency;
        param->vec_latency[__type].push_back(_latency);
    }
    _t1.Stop();
    printf("*** THREAD%02d FINISHED [TIME:%.2f]\n", _thread_id, _t1.GetSeconds());
}

WorkloadGenerator::WorkloadGenerator(const char* name, struct generator_parameter* param, PIE::Scheme* scheme, DBBench* benchmarks[])
    : scheme_(scheme)
    , num_threads_(param->num_threads)
    , result_path_(param->result_path)
    , num_test_(param->num_test)
    , key_length_(param->key_length)
{
    strcpy(name_, name);
    for (int i = 0; i < num_threads_; i++) {
        benchmarks_[i] = benchmarks[i];
    }
}

void WorkloadGenerator::Run()
{
    std::thread _threads[32];
    thread_param_t _params[32];
    uint64_t _count = num_test_ / num_threads_;

    for (int i = 0; i < num_threads_; i++) {
        _params[i].thread_id = i;
        _params[i].benchmark = benchmarks_[i];
        _params[i].scheme = scheme_;
        _params[i].count = _count;
        _threads[i] = std::thread(thread_task, &_params[i]);
    }
    for (int i = 0; i < num_threads_; i++) {
        _threads[i].join();
    }

    char _result_file[128];
    sprintf(_result_file, "%s/%s.result", result_path_.c_str(), name_);
    std::ofstream _fout(_result_file);

    for (int i = 0; i < num_threads_; i++) {
        double __lat = 1.0 * _params[i].sum_latency / (1000UL * _params[i].count);
#ifdef RESULT_OUTPUT_TO_FILE
        // output into file
        _fout << ">>thread" << i << std::endl;
        _fout << "  [0] count:" << _params[i].count << "]" << std::endl;
        _fout << "  [1] lat:" << __lat << "us" << std::endl;
        _fout << "  [2] iops:" << 1000000.0 / __lat << std::endl;
#endif
        // output into screen
        std::cout << ">>thread" << i << std::endl;
        std::cout << "  [0] count:" << _params[i].count << "]" << std::endl;
        std::cout << "  [1] lat:" << __lat << "us" << std::endl;
        std::cout << "  [2] iops:" << 1000000.0 / __lat << std::endl;
        for (int j = 0; j < DBBENCH_NUM_OPT_TYPE; j++) {
            if (_params[i].vec_latency[j].size() > 0) {
                char __name[128];
                __lat = 1.0 * _params[i].result_latency[j] / (1000UL * _params[i].result_count[j]);
                std::string __str = _g_oname[j];
#ifdef RESULT_OUTPUT_TO_FILE
                // output into file
                sprintf(__name, "%s/%s_%s.lat", result_path_.c_str(), name_, _g_oname[j]);
                result_output(__name, _params[i].vec_latency[j]);
                _fout << "  [" << __str << "][lat:" << __lat << "][iops:" << 1000000.0 / __lat << "][count:" << _params[i].result_count[j] << "|" << 100.0 * _params[i].result_count[j] / _params[i].count << "%%][success:" << _params[i].result_success[j] << "|" << 100.0 * _params[i].result_success[j] / _params[i].result_count[j] << "%%]" << std::endl;
#endif
                std::cout << "  [" << __str << "][lat:" << __lat << "][iops:" << 1000000.0 / __lat << "][count:" << _params[i].result_count[j] << "|" << 100.0 * _params[i].result_count[j] / _params[i].count << "%%][success:" << _params[i].result_success[j] << "|" << 100.0 * _params[i].result_success[j] / _params[i].result_count[j] << "%%]" << std::endl;
            }
        }
    }
    _fout.close();
}
