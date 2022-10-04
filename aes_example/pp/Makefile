CXX      = g++
CXXFLAGS = -std=gnu++11 -I/home/rzhang/openssl/include/openssl/ -L/home/rzhang/openssl/ -lcrypto
CC       = gcc
CCFLAGS  = -lm
OBJS     = spy.o eviction.o

all: spy
clean:
	rm -f *.o spy
spy : $(OBJS)
	$(CXX) -o $@ $(OBJS) $(CXXFLAGS)

%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $<

%.o : %.c 
	$(CC) -c $(CCFLAGS) $<
