<!--
 * @Author: your name
 * @Date: 2021-06-23 20:09:47
 * @LastEditTime: 2021-06-24 11:08:51
 * @LastEditors: your name
 * @Description: In User Settings Edit
 * @FilePath: /PIE/src/index/CCEH/README.md
-->
# FAST-FAIR Tree
## Introduction
This directory contains implementations of **FAST-FAIR Tree**, a variant of 
B+ Tree with elaborate design for NVM. We implement additonal string-key support for FASTFAIR by replaing 
original 8B integer key with pointer to variable-size key, which is a common approach to support variable-sized key
in index structures. 

For more details about FASTFAIR, please refer to original github repository [FASTFAIR](https://github.com/DICL/FAST_FAIR) 
and USENIX FAST2019 paper ["Write-Optimized Dynamic Hashing for Persistent Memory"](https://www.usenix.org/conference/fast18/presentation/hwang)

## Compilation
This is a header-only port, make sure that your compiler is C++17 compatible. For CentOS users, please use `scl` tool and install `devtoolset-7`.

## Features
| Features        |    Description                                                                                     | Support |
|-----------------|----------------------------------------------------------------------------------------------------|---------| 
| Integer key     |    Key is identified by 8B integer                                                                 | √       |
| String  key     |    Key is identified with string start(``const char *``) and its length(``size_t``)                | √       |
| Multi-Thread    |    Data structure operation is thread-safe                                                         | √       |
| Unique-key check|    Return error if insert existed key                                                              | ×       |
| PMDK            |    Use pmdk library                                                                                | ×       |
