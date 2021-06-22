/*
 * @Author: your name
 * @Date: 2021-06-22 16:18:12
 * @LastEditTime: 2021-06-22 16:40:56
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/scheme.cc
 */

#include "scheme.hpp"
#include "scheme/single/single_scheme.hpp"

using namespace PIE;

static Status Scheme::Create(const Options& options, Scheme** schemeptr)
{
    *schemeptr = new SingleScheme(options);
    return Status::OK();
}