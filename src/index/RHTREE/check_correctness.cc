// A simple correctness test for RHTree

#include <getopt.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <locale>
#include <random>
#include <thread>
#include <vector>

#include "rhtree.hpp"

const char *pmem_file = "/home/pmem0/pm";

// parse parameter
extern int optind, opterr, optopt;
extern char *optarg;

// Test parameter
size_t test_size;   // The number of insert operation count
size_t thread_num;  // The number of created threads
size_t key_len = 16;

struct ThreadResults {
  uint64_t throughput;
  uint64_t pass_time;  // use nanoseconds
  uint64_t fail_cnt;
};

static constexpr size_t max_thread_num =
    32;  // Set maximum running thread number

// Test framework data unit
using testpair = std::pair<const char *, size_t>;

// Insert & Search data for correctness test
std::vector<testpair> thread_data[max_thread_num];

// Evaluation index for insert & search operation
PIE::Allocator *nvm_allocator, *dram_allocator;
PIE::Index *test_index;

// To record each thread running performance
ThreadResults thread_results[max_thread_num];
std::thread threads[max_thread_num];

void InitTest();
void DoInsert();
void DoSearch();

static const char *generate_string();

int main(int argc, char *argv[]) {
  // Parse parameters
  struct option long_options[] = {{"size", required_argument, nullptr, 1},
                                  {"thread_num", required_argument, nullptr, 2},
                                  {"key_len", required_argument, nullptr, 3}};

  int opt_idx, c;
  while (EOF != (c = getopt_long(argc, argv, "s:t:", long_options, &opt_idx))) {
    switch (c) {
      case 1:
        test_size = atoll(optarg);
        break;
      case 2:
        thread_num = atoll(optarg);
        break;
      case 3:
        key_len = atoll(optarg);
        break;
      default:
        std::cerr << "Invalid Parameter: " << optarg << "\n";
    }
  }

  // Init index itself and prepare for
  // testing data
  InitTest();

  // Perform insert operation
  DoInsert();

  // Check if search can find right value of
  // previously inserted key
  DoSearch();

  return 0;
}

void InitTest() {
  assert(test_size > 0 && thread_num > 0);

  const size_t pmem_size = 100ULL * 1024 * 1024 * 1024;

  // Create new index for test
  nvm_allocator = new PIE::PIENVMAllocator(pmem_file, pmem_size);
  dram_allocator = new PIE::PIEDRAMAllocator();

  test_index = new PIE::RHTREE::RHTreeIndex(dram_allocator, nvm_allocator);

  // generate test data;
  // Apparently, each thread will have relatively average
  // size of test data
  decltype(test_size) cnt = 0;
  while (cnt < test_size) {
#ifdef STRINGKEY
    const char *key = generate_string();
#else
    uint64_t key = rand() % UINT64_MAX;
    key_len = 0;
#endif
    thread_data[cnt % thread_num].push_back({(const char *)key, key_len});
    ++cnt;
  }

  printf("[Finish Init Test]\n");
}

void DoInsert() {
  // A simple per-thread work
  auto execute = [](int thread_id) {
    int fail_time = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (const auto &item : thread_data[thread_id]) {
      // The inserted value is the same of key
      // in order to make value check simple
      PIE::status_code_t stat =
          test_index->Insert(item.first, item.second, (void *)item.first);
      if (stat != PIE::kOk) {
        fail_time++;
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dura =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Calculate iops for each second
    double iops = thread_data[thread_id].size() * 1e9 / (dura.count());

    thread_results[thread_id].fail_cnt = fail_time;
    thread_results[thread_id].pass_time = dura.count();
    thread_results[thread_id].throughput = iops;
  };

  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    threads[i] = std::thread(execute, i);
  }

  // Wait for all threads exiting
  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    threads[i].join();
  }

  // Calculate total throughput and other information to print
  uint64_t total_opts = 0, pass_time = 0, fail_cnt = 0;
  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    total_opts += thread_results[i].throughput;
    fail_cnt += thread_results[i].fail_cnt;
    pass_time = std::max(pass_time, thread_results[i].fail_cnt);
  }

  double succ_ratio = (double)(test_size - fail_cnt) / test_size;
  double mem_use = (double)(nvm_allocator->MemUsage()) / (1024 * 1024);
  double kops = (double)total_opts / 1000;
  std::cout << "[RHTree Finish Insertion]\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n";
  std::cout << "|    Index   | Thread Number | Throughput(kops/s) | Success "
               "Ratio | Memory Usage(MB) |\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n";
  // std::cout << "| |\n";
  std::cout << "| " << std::setw(strlen("   Index  ")) << "RHTREE"
            << " | " << std::setw(strlen("Thread Number")) << thread_num
            << " | " << std::setw(strlen("Throughput(kops/s)")) << kops << " | "
            << std::setw(strlen("Success Ratio")) << succ_ratio << " | "
            << std::setw(strlen("Memory Usage(MB)")) << mem_use << " |\n";

  // std::cout << "| |\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n\n";
}

void DoSearch() {
  // A simple per-thread work
  auto check = [](int thread_id) {
    int fail_time = 0;

    auto start = std::chrono::high_resolution_clock::now();

    void *value;

    for (const auto &item : thread_data[thread_id]) {
      // The inserted value is the same of key
      // in order to make value check simple
      PIE::status_code_t stat =
          test_index->Search(item.first, item.second, &value);
      if (stat != PIE::kOk || value != (void *)(item.first)) {
        fail_time++;
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto dura =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Calculate iops for each second
    double iops = thread_data[thread_id].size() * 1e9 / (dura.count());

    thread_results[thread_id].fail_cnt = fail_time;
    thread_results[thread_id].pass_time = dura.count();
    thread_results[thread_id].throughput = iops;
  };

  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    threads[i] = std::thread(check, i);
  }

  // Wait for all threads exiting
  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    threads[i].join();
  }

  // Calculate total throughput and other information to print
  uint64_t total_opts = 0, pass_time = 0, fail_cnt = 0;
  for (decltype(thread_num) i = {0}; i < thread_num; ++i) {
    total_opts += thread_results[i].throughput;
    fail_cnt += thread_results[i].fail_cnt;
    pass_time = std::max(pass_time, thread_results[i].fail_cnt);
  }

  double succ_ratio = (double)(test_size - fail_cnt) / test_size;
  double mem_use = (double)(nvm_allocator->MemUsage()) / (1024 * 1024);
  double kops = (double)total_opts / 1000;

  std::cout << "[RHTree Finish Check]\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n";
  std::cout << "|    Index   | Thread Number | Throughput(kops/s) | Success "
               "Ratio | Memory Usage(MB) |\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n";
  // std::cout << "| |\n";
  std::cout << "| " << std::setw(strlen("   Index  ")) << "RHTREE"
            << " | " << std::setw(strlen("Thread Number")) << thread_num
            << " | " << std::setw(strlen("Throughput(kops/s)")) << kops << " | "
            << std::setw(strlen("Success Ratio")) << succ_ratio << " | "
            << std::setw(strlen("Memory Usage(MB)")) << mem_use << " |\n";

  // std::cout << "| |\n";
  std::cout << "---------------------------------------------------------------"
               "-----------------------\n\n";
}

// randomly generate a string key
static const char *generate_string() {
  char *ret = new char[key_len];
  decltype(key_len) cnt = 0;
  // randomly write data
  while (cnt++ < key_len) {
    auto idx = rand() % key_len;
    ret[idx] = rand() % 255;
  }
  return reinterpret_cast<const char *>(ret);
}