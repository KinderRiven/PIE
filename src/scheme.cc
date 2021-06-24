/*
 * @Author: your name
 * @Date: 2021-06-22 16:18:12
 * @LastEditTime: 2021-06-24 11:14:42
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/scheme.cc
 */

#include "scheme.hpp"
#include "scheme/single/single_scheme.hpp"

using namespace PIE;

Status Scheme::Create(const Options& options, Scheme** schemeptr)
{
    if (options.scheme_type == kSingleScheme) {
        std::cout << "[Scheme::Create - SingleScheme]" << std::endl;
        *schemeptr = new SingleScheme(options);
        return Status::OK();
    } else {
        return Status::NotSupported("Create Scheme Failed.");
    }
}

Scheme::~Scheme()
{
    printf("Scheme::~Scheme\n");
}