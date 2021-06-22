/*
 * @Author: your name
 * @Date: 2021-06-22 16:07:03
 * @LastEditTime: 2021-06-22 20:00:52
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/scheme/single/single_scheme.cpp
 */

#include "single_scheme.hpp"
#include "index/example/example_index.hpp"

using namespace PIE;

SingleScheme::SingleScheme(const Options& options)
{
    index_ = new example::ExampleIndex();
}

SingleScheme::~SingleScheme()
{
    printf("SingleScheme::~SingleScheme\n");
    delete index_;
}

Status SingleScheme::Insert(const Slice& key, void* value)
{
    status_code_t code = index_->Insert(key.data(), key.size(), value);
}

Status SingleScheme::Search(const Slice& key, void** value)
{
    status_code_t code = index_->Search(key.data(), key.size(), value);
}

Status SingleScheme::Update(const Slice& key, void* value)
{
    status_code_t code = index_->Update(key.data(), key.size(), value);
}

Status SingleScheme::Upsert(const Slice& key, void* value)
{
    status_code_t code = index_->Upsert(key.data(), key.size(), value);
}

Status SingleScheme::ScanCount(const Slice& startkey, size_t count, void** vec)
{
    status_code_t code = index_->ScanCount(startkey.data(), startkey.size(), count, vec);
}

Status SingleScheme::Scan(const Slice& startkey, const Slice& endkey, void** vec)
{
    status_code_t code = index_->Scan(startkey.data(), startkey.size(), endkey.data(), endkey.size(), vec);
}

void SingleScheme::Print()
{
    index_->Print();
}