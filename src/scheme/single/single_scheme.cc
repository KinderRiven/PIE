/*
 * @Author: your name
 * @Date: 2021-06-22 16:07:03
 * @LastEditTime: 2021-06-24 13:19:27
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/scheme/single/single_scheme.cpp
 */

#include "single_scheme.hpp"
#include "index/CCEH/CCEH_MSB.hpp"
#include "index/FastFair/btree.hpp"
#include "index/RHTREE/rhtree.hpp"
#include "index/example/example_index.hpp"

using namespace PIE;

SingleScheme::SingleScheme(const Options& options)
{
    if (options.index_type == kExampleIndex) {
        std::cout << "[SingleScheme::SingleScheme - example::ExampleIndex]" << std::endl;
        index_ = new example::ExampleIndex();
    } else if (options.index_type == kCCEH) {
        std::cout << "[SingleScheme::SingleScheme - CCEH::CCEHIndex]" << std::endl;
        nvm_allocator_ = new PIENVMAllocator(options.pmem_file_path.c_str(), options.pmem_file_size);
        index_ = new CCEH::CCEHIndex(nvm_allocator_, 16);
    } else if (options.index_type == kRHTREE) {
        std::cout << "[SingleScheme::SingleScheme - RHTREE::RHTreeIndex]" << std::endl;
        dram_allocator_ = new PIEDRAMAllocator();
        nvm_allocator_ = new PIENVMAllocator(options.pmem_file_path.c_str(), options.pmem_file_size);
        index_ = new RHTREE::RHTreeIndex(dram_allocator_, nvm_allocator_);
    } else if (options.index_type == kFASTFAIR) {
        std::cout << "[SingleScheme::SingleScheme - FASTFAIR::FASTFAIRTree]" << std::endl;
        nvm_allocator_ = new PIENVMAllocator(options.pmem_file_path.c_str(), options.pmem_file_size);
        index_ = new FASTFAIR::FASTFAIRTree(nvm_allocator_);
    } else {
        std::cout << "[SingleScheme::SingleScheme - Unknow Index Type]" << std::endl;
    }
}

SingleScheme::~SingleScheme()
{
    printf("SingleScheme::~SingleScheme\n");
    delete index_;
}

Status SingleScheme::Insert(const Slice& key, void* value)
{
    status_code_t code = index_->Insert(key.data(), key.size(), value);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Insert Failed.");
    }
}

Status SingleScheme::Search(const Slice& key, void** value)
{
    status_code_t code = index_->Search(key.data(), key.size(), value);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Search Failed.");
    }
}

Status SingleScheme::Update(const Slice& key, void* value)
{
    status_code_t code = index_->Update(key.data(), key.size(), value);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Update Failed.");
    }
}

Status SingleScheme::Upsert(const Slice& key, void* value)
{
    status_code_t code = index_->Upsert(key.data(), key.size(), value);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Update Failed.");
    }
}

Status SingleScheme::ScanCount(const Slice& startkey, size_t count, void** vec)
{
    status_code_t code = index_->ScanCount(startkey.data(), startkey.size(), count, vec);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Upsert Failed.");
    }
}

Status SingleScheme::Scan(const Slice& startkey, const Slice& endkey, void** vec)
{
    status_code_t code = index_->Scan(startkey.data(), startkey.size(), endkey.data(), endkey.size(), vec);
    if (code == kOk) {
        return Status::OK();
    } else {
        return Status::IOError("Scan Failed.");
    }
}

void SingleScheme::Print()
{
    index_->Print();
}