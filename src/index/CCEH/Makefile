CC				=	g++
SRC 			= CCEH_MSB.cc
TEST_SRC 	= check_correctness.cc
CLAGS 		= -Wall -O0 -g
TARGET		= test
ROOT			= ../../..
INCLUDE		= -I$(ROOT)/util	-I$(ROOT)/src/include -I$(ROOT)
CLIB			= -lpthread -lpmem


$(TARGET): clean
	$(CC)	$(CLAGS) $(SRC) $(TEST_SRC) $(INCLUDE) $(CLIB) -o $(TARGET)

clean:
	rm TARGET *.o -rf