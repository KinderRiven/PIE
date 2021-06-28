SRC_DBBENCH := main.cc workload_generator.cc

SPDK_LINK_FLAGS := -Wl,--whole-archive -L../../third-party/dpdk -lspdk_env_dpdk  -lspdk_env_dpdk_rpc \
    -L../../third-party/spdk -ldpdk -lspdk_json -lspdk_jsonrpc -lspdk_log_rpc  -lspdk_app_rpc  -lspdk_rpc \
    -lspdk_bdev_rpc -lspdk_bdev_null -lspdk_bdev_malloc -lspdk_bdev_nvme -lspdk_bdev_zone_block -lspdk_bdev \
    -lspdk_event_bdev -lspdk_event_copy -lspdk_event_net -lspdk_event_vmd -lspdk_event \
    -lspdk_thread -lspdk_sock_posix -lspdk_sock -lspdk_notify -lspdk_net -lspdk_nvme \
    -lspdk_log -lspdk_trace -lspdk_util -lspdk_copy -lspdk_conf -lspdk_vmd

PMDK_LINK_FLAGS := -L../../third-party/pmdk -lpmem

BEEKV_LINK_FLAGS := -L../../build -lbeekv

TBB_LINK_FALGS := -L../../third-party/tbb -ltbb

ROCKSDB_LINK_FLAGS := -L../../third-party/rocksdb -lrocksdb -ljemalloc -lsnappy -lgflags -lz -lbz2 -llz4 -lzstd

LINK_FLAGS := -Wl,--no-whole-archive -lrt -lnuma -lm -lisal -luuid -ldl -lpthread


all: splitkv

detail:
	g++ -std=c++11 detail.cc -o detail

splitkv:
	g++ -std=c++11 -I../../include $(SRC_DBBENCH) -o beekv_dbbench $(SPDK_LINK_FLAGS) $(PMDK_LINK_FLAGS) $(BEEKV_LINK_FLAGS) $(TBB_LINK_FALGS) $(LINK_FLAGS)
