/*
 * @Author: your name
 * @Date: 2021-06-22 15:27:31
 * @LastEditTime: 2021-06-24 13:11:19
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/include/options.hpp
 */

#ifndef PIE_INCLUDE_OPTIONS_HPP__
#define PIE_INCLUDE_OPTIONS_HPP__

#include <string>

namespace PIE {
enum index_type_t {
    kExampleIndex = 0,
    kCCEH = 1,
    kRHTREE = 2,
    kFASTFAIR = 3,
};

enum scheme_type_t {
    kSingleScheme = 0,
    kHybridScheme = 1,
};

class Options {
public:
    Options() { }

    ~Options() = default;

public:
    size_t pmem_file_size;

    std::string pmem_file_path;

    index_type_t index_type;

    scheme_type_t scheme_type;
};
};

#endif // PIE_INCLUDE_OPTIONS_HPP__