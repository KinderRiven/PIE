/*
 * @Author: your name
 * @Date: 2021-06-22 19:43:54
 * @LastEditTime: 2021-06-23 20:15:43
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
    Options _options;
    _options.index_type = kCCEH;
    _options.scheme_type = kSingleScheme;
    _options.pmem_file_path = "/home/pmem0/PIE";
    _options.pmem_file_size = 2UL * 1024 * 1024 * 1024;

    Scheme* _scheme;
    Scheme::Create(_options, &_scheme);

    Slice _key("HelloWorld");
    void* _value = nullptr;
    _scheme->Insert(_key, _value);
    delete _scheme;
    return 0;
}