# PIE
Framework for Persistent Index Evaluation (PIE)

## Overview
PIE is a code repository based on persistent memory which includes the index, memory allocator and benchmark. It provides a unified interface that allows library users to easily incorporate their work into our architecture. In addition, the architecture includes and summarizes a variety of frontier research based on persistent memory.

## Including Index

### B+-Tree

* [FAST-FAIR](https://www.usenix.org/system/files/conference/fast18/fast18-hwang.pdf): Endurable Transient Inconsistency in Byte-Addressable Persistent B+-Tree.

### Hashing

* [CCEH](http://www.cs.fsu.edu/~awang/courses/cop5611_f2018/nvm-hashing.pdf): Write-Optimized Dynamic Hashing for Persistent Memory.

### Other
* RHTREE : Radix Hashing Tree.

* [WORT](https://www.usenix.org/system/files/conference/fast17/fast17-lee.pdf): WORT: Write Optimal Radix Tree for Persistent Memory Storage Systems.
