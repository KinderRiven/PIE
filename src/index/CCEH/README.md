# Cache-Concious Extendible Hashing(CCEH)
## Introduction
This directory contains implementations of **Cache-Concious Extendible Hashing(CCEH)**, a variant of 
extendible hashing with elaborate design for NVM. We implement additonal string-key version of CCEH by replaing 
original 8B integer key with pointer to variable-size key, which is a common approach to support varibale-size key
in index structure. 

For more details about CCEH, please refer to original github repository [CCEH](https://github.com/DICL/CCEH) 
and USENIX FAST2019 paper "[Write-Optimized Dynamic Hashing for Persistent Memory](https://www.usenix.org/conference/fast19/presentation/nam)"

## Compilation
This directory contains simple correctness test: Insert a bunch of randomly generated keys first and search related value
to check these keys are correctly inserted.

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

Apparently we use the string version by default. To enable integer key test, please comment out ``-DKEYTYPE=STRINGKEY`` in Makefile.

## Features

| Features        |   Support   |
|-----------------|-------------|
| Integer key     |   √         |
| String  key     |   √         |
| Multi-Thread    |   √         |
| Unique-key check|   ×         |
