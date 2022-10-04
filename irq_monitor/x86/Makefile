all: umonitor monitorx monitor

umonitor:
	gcc main.c -Os -lpthread -o main -pthread -lrt -DUMONITOR
monitorx:
	gcc main.c -Os -lpthread -o main -pthread -lrt -DMONITORX
monitor:
	gcc main.c -Os -lpthread -o main -pthread -lrt -DMONITOR

clean:
	rm -f main

