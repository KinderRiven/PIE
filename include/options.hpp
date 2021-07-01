/*
 * @Author: KinderRiven
 * @Date: 2021-06-22 15:27:31
 * @LastEditTime: 2021-07-01 10:51:25
 * @LastEditors: KinderRiven
 * @Description: In User Settings Edit
 * @FilePath: /PIE/include/options.hpp
 */

#ifndef PIE_INCLUDE_OPTIONS_HPP__
#define PIE_INCLUDE_OPTIONS_HPP__

#include <string>

namespace PIE {
enum index_type_t {
    kExampleIndex = 0, // example
    kCCEH = 1, // Hashing - CCEH
    kRHTREE = 2, // RH-Tree
    kFASTFAIR = 3, // B+Tree - FAST-FAIR
    kWORT = 4, // Trie - WORT
};

enum scheme_type_t {
    kSingleScheme = 0,
    kHybridScheme = 1,
};

class Options {
public:
    Options()
        : pmem_file_size(2UL * 1024 * 1024 * 1024)
        , index_type(kCCEH)
        , scheme_type(kSingleScheme)
    {
        pmem_file_path = "/home/pmem0/PIE";
    }

    ~Options() = default;

public:
    // persistent memory pool size
    // default : 2GB
    size_t pmem_file_size;

    // persistent memory poll path
    // defaul : /home/pmem0/PIE
    std::string pmem_file_path;

    // index type
    // default : CCEH
    index_type_t index_type;

    // scheme type
    // default : SingleScheme
    scheme_type_t scheme_type;
};
};

#endif // PIE_INCLUDE_OPTIONS_HPP__