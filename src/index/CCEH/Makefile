CC				=	g++
SRC 			= CCEH_MSB.cc
TEST_SRC 	= check_correctness.cc
CLAGS 		= -Wall -O0 -g
TARGET		= test
ROOT			= ../../..
INCLUDE		= -I$(ROOT)/util	-I$(ROOT)/src/include -I$(ROOT)
CLIB			= -lpthread -lpmem
KEYTYPE   = -DSTRINGKEY


$(TARGET): clean
	$(CC)	$(CLAGS) $(KEYTYPE) $(SRC) $(TEST_SRC) $(INCLUDE) $(CLIB) -o $(TARGET)

clean:
	rm TARGET *.o -rf