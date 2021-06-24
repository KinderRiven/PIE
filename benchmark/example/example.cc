/*
 * @Author: your name
 * @Date: 2021-06-22 19:43:54
 * @LastEditTime: 2021-06-24 19:16:27
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/example/example.cc
 */

#include "options.hpp"
#include "scheme.hpp"
#include "slice.hpp"

using namespace PIE;

int main(int argc, char** argv)
{
    int _num_kv = 1000000;
    Options _options;
    _options.index_type = kFASTFAIR;
    _options.scheme_type = kSingleScheme;
    _options.pmem_file_path = "/home/pmem0/PIE";
    _options.pmem_file_size = 100UL * 1024 * 1024 * 1024;

    uint64_t _found_count = 0;
    Scheme* _scheme;
    Scheme::Create(_options, &_scheme);

    // TEST INSERT
    for (int i = 1; i <= _num_kv; i++) {
        uint64_t __key64 = i;
        Slice __key((const char*)&__key64, 8UL);
        void* __value = (void*)(__key64);
        _scheme->Insert(__key, __value);
    }
    // TEST SEARCH
    for (int i = 1; i <= _num_kv; i++) {
        uint64_t __key64 = i;
        Slice __key((const char*)&__key64, 8UL);
        void* __value;
        _scheme->Search(__key, &__value);
        if ((uint64_t)__value == i) {
            _found_count++;
        }
    }
    printf("%llu/%llu\n", _found_count, _num_kv);
    delete _scheme;
    return 0;
}