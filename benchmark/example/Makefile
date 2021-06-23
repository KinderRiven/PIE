all:
	g++ -std=c++17 example.cc -o example -I../../include -L../../build -lPIE -L../../third-party/pmdk -lpmem

export:
	export LD_LIBRARY_PATH=../../build:../../third-party/pmdk