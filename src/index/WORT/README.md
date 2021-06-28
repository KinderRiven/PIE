# Write Optimal Radix Tree (WORT)
## Introduction
This directory contains implementation of **Write Optimal Radix Tree (WORT)**, a PM-aware radix tree. As far as we know, this is the first research work to design and construct a PM-based radix tree.

We implement WORT in C++ based on the author's original code with a little modification. For example, we use ``CLWB`` and ``sfence`` to guarantee crash consistence. In addition, we implement split process as the orignal paper describes, instead of what original code shows, which we think a little bit confusing.

For more details about WORT, please refer to original github repository [WORT](https://github.com/SeKwonLee/WORT) and FAST'17 paper ["WORT: Write Optimal Radix Tree for Persistent Memory Storage Systems"](https://www.usenix.org/system/files/conference/fast17/fast17-lee.pdf)

## Compilation
This directory contains simple correctness test: Insert a bunch of randomly generated keys first and search related value to check if these keys are correctly inserted.

To run this simple test, type:
```
make test
./test --size=100000000
```
command line parameter are as follows:
| Parameters      | Usage                                              | Default Value   |
| ----------------| -------------------------------------------------- |-----------------|
| ``size``        | number of inserted keys                            | 100M            |

**Note:** According to the original code, we found that WORT only supports varible-size key of at most 8B. WORT is not a concurrent indexing neither. So we ommit related commandline parameters

## Features

| Features        |    Description                                                                                     | Support |
|-----------------|----------------------------------------------------------------------------------------------------|---------| 
| Integer key     |    Key is identified by 8B integer                                                                 | ×       |
| String  key     |    Key is identified with string start(``const char *``) and its length(``size_t``)                | √ (8B at most)    |
| Multi-Thread    |    Data structure operation is thread-safe                                                         | ×       |
| Unique-key check|    Return error if insert existed key                                                              | √       |
| PMDK            |    Use pmdk library                                                                                | ×       |