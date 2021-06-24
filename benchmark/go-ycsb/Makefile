all:
	g++ -std=c++17 main.cc -o tester -I../../include -L../../build -lPIE -L../../third-party/pmdk -lpmem -lpthread

export:
	export LD_LIBRARY_PATH=../../build:../../third-party/pmdk