/*
 * @Author: your name
 * @Date: 2021-06-22 19:43:54
 * @LastEditTime: 2021-06-22 19:48:09
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/example/example.cc
 */

#include "scheme.hpp"
#include "slice.hpp"

int main(int argc, char** argv)
{
    Options _options;
    PIE::Scheme* _scheme;
    PIE::Scheme::Create(_options, &_scheme);

    PIE::Slice _key;
    void* _value = nullptr;
    _scheme->Insert(_key, _value);
    return 0;
}