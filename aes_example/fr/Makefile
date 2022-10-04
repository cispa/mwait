all: spy
clean:
	rm -f *.o spy
spy: spy.cpp ./cacheutils.h
	g++ -std=gnu++11 -O2 -o $@ $< -I/home/rzhang/openssl/include/openssl/ -L/home/rzhang/openssl/ -lcrypto 
