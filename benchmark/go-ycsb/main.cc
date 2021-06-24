/*
 * @Date: 2021-04-17 11:58:39
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2021-06-24 20:23:13
 * @FilePath: /SplitKV/benchmark/go-ycsb/rocksdb_main.cc
 */

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

using namespace PIE;

const static uint32_t kNumThread = 8;
const static uint32_t kKeySize = 20;

#define OPT_TYPE_INSERT (1)
#define OPT_TYPE_UPDATE (2)
#define OPT_TYPE_READ (3)
#define OPT_TYPE_SCAN (4)

const char* g_ycsb_workload[] = { "workload/workloada.load", "workload/workloada.run" };

struct ycsb_operator_t {
public:
    uint8_t type_;
    std::string skew_;
    uint32_t other_; // for scan range record

public:
    ycsb_operator_t(uint8_t type, std::string& skew, uint32_t other)
        : type_(type)
        , skew_(skew)
        , other_(other)
    {
    }
};

struct thread_context_t {
public:
    uint32_t thread_id;
    Scheme* scheme;
    std::vector<ycsb_operator_t*>* vec_opt;
};

static std::vector<ycsb_operator_t*> g_vec_opt[kNumThread];

// READ usertable user6302928200575776280 [ field0 ]
static inline bool handle_read(std::ifstream& _fin, std::string& key)
{
    std::string _key;
    std::string _blank;

    _fin >> _blank; // usertable
    if (_blank != "usertable") {
        return false;
    }

    _fin >> _key; // key
    key = std::string(_key.begin() + 4, _key.end()); // skip user

    _fin >> _blank; // [
    _fin >> _blank; // field0
    _fin >> _blank; // ]
    return true;
}

// UPDATE usertable user6319145997088671705 [ field0=LbMRolQiuiSYinvnstaHBVUfUyHoeMIU ]
static inline bool handle_update(std::ifstream& _fin, std::string& key, std::string& value)
{
    std::string _key;
    std::string _value;
    std::string _blank;

    _fin >> _blank; // usertable
    if (_blank != "usertable") {
        return false;
    }

    _fin >> _key; // key
    key = std::string(_key.begin() + 4, _key.end()); // skip user

    _fin >> _blank; // [
    _fin >> _value; // value
    value = std::string(_value.begin() + 7, _value.end()); // skip field0=
    _fin >> _blank; // ]
    return true;
}

// INSERT usertable user4787722205795845578 [ field0=YgHLweyJStlEiEkwiKGvYfwCCtWGjVYX ]
static inline bool handle_insert(std::ifstream& _fin, std::string& key, std::string& value)
{
    std::string _key;
    std::string _value;
    std::string _blank;

    _fin >> _blank; // usertable
    if (_blank != "usertable") {
        return false;
    }

    _fin >> _key; // key
    key = std::string(_key.begin() + 4, _key.end()); // skip user

    _fin >> _blank; // [
    _fin >> _value; // value
    value = std::string(_value.begin() + 7, _value.end()); // skip field0=
    _fin >> _blank; // ]
    return true;
}

// SCAN usertable user6297265715691624980 337 [ field0 ]
static inline bool handle_scan(std::ifstream& _fin, std::string& key, uint32_t& range)
{
    std::string _key;
    std::string _blank;

    _fin >> _blank; // usertable
    if (_blank != "usertable") {
        return false;
    }

    _fin >> _key; // key
    key = std::string(_key.begin() + 4, _key.end()); // skip user

    _fin >> range; // range
    _fin >> _blank; // [
    _fin >> _blank; // field0
    _fin >> _blank; // ]
    return true;
}

void read_ycsb_file(const char* name)
{
    bool _res;
    uint32_t _num_opt = 0;
    std::ifstream _fin(name);

    uint8_t _type;
    std::string _line;
    std::string _key;
    std::string _value;
    uint32_t _range;

    if (_fin.is_open()) {
        while (_fin) {
            _fin >> _line;
            if (_line == "READ") {
                _res = handle_read(_fin, _key);
                if (_res) {
                    g_vec_opt[_num_opt % kNumThread].push_back(new ycsb_operator_t(OPT_TYPE_READ, _key, 0));
                }
                // std::cout << "READ " << _key << std::endl;
            } else if (_line == "UPDATE") {
                _res = handle_update(_fin, _key, _value);
                if (_res) {
                    g_vec_opt[_num_opt % kNumThread].push_back(new ycsb_operator_t(OPT_TYPE_UPDATE, _key, 0));
                }
                // std::cout << "UPDATE " << _key << " " << _value << std::endl;
            } else if (_line == "INSERT") {
                _res = handle_insert(_fin, _key, _value);
                if (_res) {
                    g_vec_opt[_num_opt % kNumThread].push_back(new ycsb_operator_t(OPT_TYPE_INSERT, _key, 0));
                }
                // std::cout << "INSERT " << _key << " " << _value << std::endl;
            } else if (_line == "SCAN") {
                _res = handle_scan(_fin, _key, _range);
                if (_res) {
                    g_vec_opt[_num_opt % kNumThread].push_back(new ycsb_operator_t(OPT_TYPE_SCAN, _key, _range));
                }
                // std::cout << "SCAN " << _key << " " << _range << std::endl;
            }
            _num_opt++;
        }
        _fin.close();
    }

    for (int i = 0; i < kNumThread; i++) {
        printf("[%d][SIZE:%zu]\n", i, g_vec_opt[i].size());
    }
}

static void run_thread(thread_context_t* context)
{
    uint64_t _insert_cnt = 0;
    uint64_t _update_cnt = 0;
    uint64_t _search_cnt = 0;
    uint64_t _insert_ok_cnt = 0;
    uint64_t _update_ok_cnt = 0;
    uint64_t _search_ok_cnt = 0;

    Scheme* _scheme = context->scheme;
    std::vector<ycsb_operator_t*>* _vec_opt = context->vec_opt;

    Timer _timer;
    printf("[thread_id:%d][num_opt:%zu]\n", context->thread_id, _vec_opt->size());
    _timer.Start();

    for (auto __iter = _vec_opt->begin(); __iter != _vec_opt->end(); __iter++) {
        ycsb_operator_t* __operator = *(__iter);
        void* __value = nullptr;
        if (__operator->type_ == OPT_TYPE_INSERT) {
            Slice __skey(__operator->skew_);
            __value = (void*)(*((uint64_t*)__skey.data()));
            Status __status = _scheme->Insert(__skey, __value);
            _insert_cnt++;
            if (__status.ok()) {
                _insert_ok_cnt++;
            }
        } else if (__operator->type_ == OPT_TYPE_UPDATE) {
            Slice __skey(__operator->skew_);
            __value = (void*)(*((uint64_t*)__skey.data()));
            Status __status = _scheme->Update(__skey, __value);
            _update_cnt++;
            if (__status.ok()) {
                _update_ok_cnt++;
            }
        } else if (__operator->type_ == OPT_TYPE_READ) {
            Slice __skey(__operator->skew_);
            __value = (void*)(*((uint64_t*)__skey.data()));
            Status __status = _scheme->Search(__skey, &__value);
            _search_cnt++;
            if (__status.ok()) {
                _search_ok_cnt++;
            }
        }
    }
    _timer.Stop();
    printf("[cost:%.2fseconds][iops:%.2f][insert:%llu/%llu][update:%llu/%llu][search:%llu/%llu]\n",
        _timer.GetSeconds(), 1.0 * _vec_opt->size() / _timer.GetSeconds(),
        _insert_cnt, _insert_ok_cnt, _update_cnt, _update_ok_cnt, _search_cnt, _search_ok_cnt);
}

void run_workload(const char* ycsb, Scheme* scheme)
{
    read_ycsb_file(ycsb);
    std::thread _thread[kNumThread];
    for (uint32_t i = 0; i < kNumThread; i++) {
        thread_context_t* __context = new thread_context_t();
        __context->thread_id = i;
        __context->scheme = scheme;
        __context->vec_opt = &g_vec_opt[i];
        _thread[i] = std::thread(run_thread, __context);
    }
    for (uint32_t i = 0; i < kNumThread; i++) {
        _thread[i].join();
    }
}

int main(int argc, char** argv)
{
    Options _options;
    _options.index_type = kCCEH;
    _options.scheme_type = kSingleScheme;
    _options.pmem_file_path = "/home/pmem0/PIE";
    _options.pmem_file_size = 100UL * 1024 * 1024 * 1024;

    Scheme* _scheme;
    Scheme::Create(_options, &_scheme);
    run_workload(g_ycsb_workload[0], _scheme);
    run_workload(g_ycsb_workload[1], _scheme);
    delete _scheme;
    return 0;
}