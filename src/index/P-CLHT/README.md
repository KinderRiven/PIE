# Persistent Cache Line Hash Table (P-CLHT)
## Introduction
This directory contains implementation of **Persistent Cache Line Hash Table (P-CLHT)**, a PM-based hash table. P-CLHT is one of the persistent indexes converted from its DRAM counterparts in RECIPE.

In order to provide unifrom operation interface, we encapsulate original P-CLHT code in C++ with a little modification. 

For more details about P-CLHT and RECIPE, please refer to original github repository [RECIPE](https://github.com/utsaslab/RECIPE) and corresponding SOSP'19 paper ["RECIPE : Converting Concurrent DRAM Indexes to Persistent-Memory Indexes"](https://www.cs.utexas.edu/~vijay/papers/sosp19-recipe.pdf). 

For more details about CLHT, please refer to original github repository [CLHT](https://github.com/LPD-EPFL/CLHT) and corresponding ASPLOS'15 paper ["Asynchronized Concurrency: The Secret to Scaling Concurrent Search Data Structures"](https://dl.acm.org/doi/10.1145/2786763.2694359). The detailed implementation paper of CLHT can bt found [here](https://www.semanticscholar.org/paper/Designing-ASCY-compliant-Concurrent-Search-Data-David-Guerraoui/65ae262a20db5f638fdb0bc57a7227df05e496b0)

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

Apparently we use the string version by default. To enable 8B integer key test, please comment out ``-DKEYTYPE=CCEH_STRINGKEY`` in Makefile.

## Features

| Features        |    Description                                                                                     | Support |
|-----------------|----------------------------------------------------------------------------------------------------|---------| 
| Integer key     |    Key is identified by 8B integer                                                                 | √       |
| String  key     |    Key is identified with string start(``const char *``) and its length(``size_t``)                | √       |
| Multi-Thread    |    Data structure operation is thread-safe                                                         | √       |
| Unique-key check|    Return error if insert existed key                                                              | √       |
| PMDK            |    Use pmdk library                                                                                | ×       |