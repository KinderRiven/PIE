# Radix-Hash Tree (RHTree)
## Introduction
This directory contains implementations of **Radix Hash Tree (RHTree)**, a hybrid structure index composed of Radix Tree and Hash Table. RHTree originally supports string key as most radix tree or trie index does. 

RHTree is basically a NVM version of **[Burst-Trie](https://people.eng.unimelb.edu.au/jzobel/fulltext/acmtois02.pdf)** and **[B-Trie](http://seminar.at.ispras.ru/wp-content/uploads/2010/03/778_2008_Article_94.pdf)** (i.e. additional **Crash Consistency**).    RHTree is firstly used in a LSM-Tree key-value storage system  LightKV, to reduce compaction rang and tail latency caused by backend compaction in conventional LSMT-based key-value storage system.

For more details about LightKV and RHTree, please refer to MSST2020 paper ["LightKV: A Cross Media Key Value Store with Persistent Memory to Cut Long Tail Latency"](https://storageconference.us/2020/Papers/12.LightKV.pdf)

## Compilation
This directory contains simple correctness test: Insert a bunch of randomly generated keys first and search related value to check if these keys are correctly inserted.

To run this simple test, type:
```
make test
./test --thread_num=1 --size=100000000 --key_len=16
```

command line parameter are as follows:
| Parameters      | Usage                                              | Default Value   |
| ----------------| -------------------------------------------------- |-----------------|
| ``thread_num``  | number of created threads for insertion and search | 1(Single thread)|
| ``size``        | number of inserted keys                            | 100M            |
| ``key_len``     | size of generated key                              | 16              |

**Note**: RHTree only supports string key(both fixed size and variable size), a ``uint64_t`` key can be converted to a string key of fixed 8 bytes length to adjust RHTree interface.

## Features
| Features        |    Description                                                                                     | Support |
|-----------------|----------------------------------------------------------------------------------------------------|---------| 
| Integer key     |    Key is identified by 8B integer                                                                 | ×       |
| String  key     |    Key is identified with string start(``const char *``) and its length(``size_t``)                | √       |
| Multi-Thread    |    Data structure operation is thread-safe                                                         | √       |
| Unique-key check|    Return error if insert existed key                                                              | √       |
| PMDK            |    Use pmdk library                                                                                | ×       |
