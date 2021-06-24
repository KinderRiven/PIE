CC := g++

LIB_SPLITKV_PATH := /home/hanshukai/SplitKV
LIB_DPDK_PATH := /home/hanshukai/import_libs/spdk/dpdk/build/lib
LIB_SPDK_PATH := /home/hanshukai/import_libs/spdk/build/lib
LIB_PMDK_PATH := /home/hanshukai/import_libs/pmdk/src/nondebug
LIB_ROCKSDB_PATH := $(LIB_SPLITKV_PATH)/third-party/rocksdb
LIB_PEBBLESDB_PATH := $(LIB_SPLITKV_PATH)/third-party/pebblesdb
LIB_MATRIXKV_PATH := $(LIB_SPLITKV_PATH)/third-party/matrixkv
LIB_TBB_PATH := $(LIB_SPLITKV_PATH)/third-party/tbb
LIB_BUILD_PATH := $(LIB_SPLITKV_PATH)/build

DB_LINK_FLAGS := -Wl,--no-whole-archive -L$(LIB_BUILD_PATH) -lsplitkv

DPDK_LINK_FLAGS := -Wl,--whole-archive -L$(LIB_DPDK_PATH) \
    -lrte_eal -lrte_mempool -lrte_ring -lrte_mbuf -lrte_mempool_ring -lrte_pci \
    -lrte_bus_pci -lrte_kvargs -lrte_vhost -lrte_net -lrte_hash -lrte_telemetry \
    -lrte_cryptodev -lrte_power -lrte_rcu

SPDK_LINK_FLAGS := -Wl,--whole-archive -L$(LIB_SPDK_PATH) \
    -lspdk_env_dpdk  -lspdk_env_dpdk_rpc -lspdk_json \
    -lspdk_jsonrpc -lspdk_rpc \
    -lspdk_event_net -lspdk_event_vmd -lspdk_event \
    -lspdk_thread -lspdk_sock_posix -lspdk_sock -lspdk_notify -lspdk_net -lspdk_nvme \
    -lspdk_log -lspdk_trace -lspdk_util -lspdk_conf -lspdk_vmd

PMDK_LINK_FLAGS := -L$(LIB_PMDK_PATH) -lpmem -lpmemobj -lpmempool

ROCKSDB_LINK_FLAGS := -L$(LIB_ROCKSDB_PATH) -lrocksdb -ljemalloc -lsnappy -lgflags -lz -lbz2 -llz4 -lzstd

PEBBLESDB_LINK_FLAGS := -L$(LIB_PEBBLESDB_PATH) -lpebblesdb -ljemalloc -lsnappy -lgflags -lz -lbz2 -llz4 -lzstd

MATRIXKV_LINK_FLAGS := -L$(LIB_MATRIXKV_PATH) -lrocksdb -ljemalloc -lsnappy -lgflags -lz -lbz2 -llz4 -lzstd

LINK_FLAGS := -L$(LIB_TBB_PATH) -lpthread -lrt -lnuma -ldl -luuid -lm -lisal -ltbb

SPLITKV_INC := -I$(LIB_SPLITKV_PATH)/include/splitkv

ROCKSDB_INC := -I$(LIB_SPLITKV_PATH)/include/rocksdb

PEBBLESDB_INC := -I$(LIB_SPLITKV_PATH)/include/pebblesdb

MATRIXKV_INC := -I./matrixkv

all: clean splitkv pebblesdb matrixkv rocksdb

splitkv:
	$(CC) --std=c++11 splitkv_main.cc -o splitkv_tester $(SPLITKV_INC) $(DPDK_LINK_FLAGS) $(SPDK_LINK_FLAGS) $(PMDK_LINK_FLAGS) $(LINK_FLAGS) $(DB_LINK_FLAGS) $(ROCKSDB_LINK_FLAGS)

rocksdb:
	$(CC) --std=c++11 rocksdb_main.cc -o rocksdb_tester $(ROCKSDB_INC) $(LINK_FLAGS) $(ROCKSDB_LINK_FLAGS)

pebblesdb:
	$(CC) --std=c++11 pebblesdb_main.cc -o pebblesdb_tester $(PEBBLESDB_INC) $(LINK_FLAGS) $(PEBBLESDB_LINK_FLAGS)

matrixkv:
	$(CC) --std=c++11 matrixkv_main.cc -o matrixkv_tester $(MATRIXKV_INC) $(LINK_FLAGS) $(MATRIXKV_LINK_FLAGS) $(PMDK_LINK_FLAGS)

clean:
	rm -rf splitkv_tester rocksdb_tester pebblesdb_tester matrixkv_tester

export:
	export LD_LIBRARY_PATH=/home/hanshukai/SplitKV/build:/home/hanshukai/import_libs/pmdk/src/nondebug:/home/hanshukai/import_libs/spdk/build/lib:/home/hanshukai/import_libs/spdk/dpdk/build/lib:/home/hanshukai/SplitKV/third-party/rocksdb:/home/hanshukai/SplitKV/third-party/tbb
