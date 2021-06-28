SRC_DBBENCH := main.cc workload_generator.cc

PMDK_LINK_FLAGS := -L../../third-party/pmdk -lpmem
PIE_LINK_FLAGS := -L../../build -lPIE
TBB_LINK_FALGS := -L../../third-party/tbb -ltbb
LINK_FLAGS := -Wl,--no-whole-archive -lrt -lnuma -lm -lisal -luuid -ldl -lpthread

all: dbbench

detail:
	g++ -std=c++11 detail.cc -o detail

dbbench:
	g++ -std=c++11 -I../../include $(SRC_DBBENCH) -o db_bench $(PMDK_LINK_FLAGS) $(PIE_LINK_FLAGS) $(TBB_LINK_FALGS) $(LINK_FLAGS)
