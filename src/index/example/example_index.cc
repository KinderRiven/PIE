/*
 * @Author: your name
 * @Date: 2021-06-22 18:50:26
 * @LastEditTime: 2021-06-22 19:53:59
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/index/example/example_index.hpp
 */

#include "example_index.hpp"

using namespace PIE;
using namespace PIE::example;

ExampleIndex::ExampleIndex()
{
    printf("ExampleIndex::ExampleIndex\n");
}

ExampleIndex::~ExampleIndex()
{
    printf("ExampleIndex::~ExampleIndex\n");
}

status_code_t ExampleIndex::Insert(const char* key, size_t key_len, void* value)
{
    printf("ExampleIndex::Insert\n");
    return kOk;
}

status_code_t ExampleIndex::Search(const char* key, size_t key_len, void** value)
{
    printf("ExampleIndex::Search\n");
    return kOk;
}

status_code_t ExampleIndex::Update(const char* key, size_t key_len, void* value)
{
    printf("ExampleIndex::Update\n");
    return kOk;
}

status_code_t ExampleIndex::Upsert(const char* key, size_t key_len, void* value)
{
    printf("ExampleIndex::Upsert\n");
    return kOk;
}

status_code_t ExampleIndex::ScanCount(const char* startkey, size_t key_len, size_t count, void** vec)
{
    printf("ExampleIndex::ScanCount\n");
    return kOk;
}

status_code_t ExampleIndex::Scan(const char* startkey, size_t startkey_len, const char* endkey, size_t endkey_len, void** vec)
{
    printf("ExampleIndex::Scan\n");
    return kOk;
}

void ExampleIndex::Print()
{
    printf("ExampleIndex::Print.\n");
}