<!--
 * @Author: your name
 * @Date: 2021-06-27 16:20:10
 * @LastEditTime: 2021-06-29 14:47:29
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /PIE/benchmark/go-ycsb/README.md
-->
# db_bench

## What is db_bench?
[db_bench](https://github.com/google/leveldb) is the benchmark test program that comes with leveldb.

## How to run?

```c
./tester --key_length=8 --num_thread=8 --num_warmup=5000000 --num_test=1000000 --pmem_file_size=100 --pmem_file_path=/home/pmem0/PIE --index=CCEH
```

command line parameter are as follows:
| Parameters                 | Usage                                              | Default Value   |
| ---------------------------| -------------------------------------------------- |-----------------|
| ``thread_num``             | number of created threads for insertion and search | 1               |
| ``pmem_file_path``         | persistent memory file path                        |                 |
| ``pmem_file_size``         | persistent memory file size (GB)                   | 10              |
| ``index``                  | index type, specific supported indexes, please check the readme in the main directory             | CCEH                   |
| ``num_warmup``             | the amount of KV pair data to be inserted          | 5M              |
| ``num_test``               | the amount of KV pair data to be update/search     | 1M              |