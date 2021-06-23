/*
 * @Author: your name
 * @Date: 2021-06-22 15:27:31
 * @LastEditTime: 2021-06-23 14:04:06
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/include/options.hpp
 */

#ifndef PIE_INCLUDE_OPTIONS_HPP__
#define PIE_INCLUDE_OPTIONS_HPP__

namespace PIE {
enum index_type_t {
    kExampleIndex = 0,
    kCCEH = 1,
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
    index_type_t index_type;

    scheme_type_t scheme_type;

    std::string pmem_file_path;

    size_t pmem_file_size;
};
};

#endif // PIE_INCLUDE_OPTIONS_HPP__