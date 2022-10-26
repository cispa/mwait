all: umonitor monitorx monitor

umonitor:
	gcc main.c -Os -lpthread -o intel-umonitor -pthread -lrt -DUMONITOR
monitorx:
	gcc main.c -Os -lpthread -o amd-monitorx -pthread -lrt -DMONITORX
monitor:
	gcc main.c -Os -lpthread -o monitor -pthread -lrt -DMONITOR

clean:
	rm -f intel-umonitor amd-monitorx monitor

