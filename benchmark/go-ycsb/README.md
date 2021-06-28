<!--
 * @Author: your name
 * @Date: 2021-06-27 16:20:10
 * @LastEditTime: 2021-06-28 09:56:09
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/go-ycsb/README.md
-->
# Go YCSB

## What is Go YCSB?
[go-ycsb](https://github.com/pingcap/go-ycsb) is a Go port of YCSB. It fully supports all YCSB generators and the Core workload so we can do the basic CRUD benchmarks with Go.

## How to run?
Use generate.sh to generate data, and then use the main.cc test program to read data from the data file as workloads.

```c
./tester --num_thread=8 --pmem_file_path=/home/pmem0/PIE --pmem_file_size=10 --index=CCEH
```

command line parameter are as follows:
| Parameters                 | Usage                                              | Default Value   |
| ---------------------------| -------------------------------------------------- |-----------------|
| ``thread_num``             | number of created threads for insertion and search | 1               |
| ``pmem_file_path``         | persistent memory file path                        |                 |
| ``pmem_file_size``         | persistent memory file size (GB)                   | 10              |
| ``index``                  | index type (CCEH/FASTFAIR/RHTREE)                  | CCEH            |