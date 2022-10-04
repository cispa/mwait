all: pad Ttable

pad:
	gcc pad.c -Os -lpthread -o pad
Ttable:
	gcc main.c -Os -lpthread -o Ttable -g -I/home/rzhang/openssl/include/openssl/ -L/home/rzhang/openssl/ -lcrypto
clean:
	rm -f pad Ttable

