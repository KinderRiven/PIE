/*
 * @Date: 2021-06-28 10:22:32
 * @LastEditors: KinderRiven
 * @LastEditTime: 2021-07-01 10:51:57
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