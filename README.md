# PIE
Framework for Persistent Index Evaluation (PIE)

## Overview
PIE is a code repository based on persistent memory which includes the index, memory allocator and benchmark. It provides a unified interface that allows library users to easily incorporate their work into our architecture. In addition, the architecture includes and summarizes a variety of frontier research based on persistent memory.

## Directory Structure

| Directiry Path | Description |
| -------------- | ----------- |
| include        | Header file |
| src/allocator  | If you implement a memory allocator class, create a folder placement code file in the directory. |
| src/index      | If you implement an index class, create a folder placement code file in the directory. |
| src/store      | If you implement an store class, create a folder placement code file in the directory. |
| test_script    | Test scripts for pmemkv |
| benchmark      | Benchmark test class |
| lib            | Reference code file for third-party libraries |

## Including Index

### B+-Tree

* [FAST-FAIR](https://www.usenix.org/system/files/conference/fast18/fast18-hwang.pdf): Endurable Transient Inconsistency in Byte-Addressable Persistent B+-Tree.

### Hashing

* [CCEH](http://www.cs.fsu.edu/~awang/courses/cop5611_f2018/nvm-hashing.pdf): Write-Optimized Dynamic Hashing for Persistent Memory.

### Hashing
* RHTREE : Radix Hashing Tree.

### Other
* [WORT](https://www.usenix.org/system/files/conference/fast17/fast17-lee.pdf): WORT: Write Optimal Radix Tree for Persistent Memory Storage Systems.
