all: umonitor monitor monitorx monitor-intel

umonitor:
	gcc hist.c -Os -lpthread -o test-umonitor -DUMONITOR
monitor:
	gcc hist.c -Os -lpthread -o test-monitor -DMONITOR
monitorx:
	gcc hist.c -Os -lpthread -o test-monitorx -DMONITORX
monitor-intel:
	gcc hist.c -Os -lpthread -o test-monitor-intel -DMONITOR_INTEL


clean:
	rm -f hist

