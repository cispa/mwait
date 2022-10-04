all: umonitor monitorx

umonitor:
	gcc main.c -Os -lpthread -o main -pthread -lrt -DUMONITOR
monitorx:
	gcc main.c -Os -lpthread -o main -pthread -lrt -DMONITORX

clean:
	rm -f main

