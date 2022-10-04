all: umonitor monitorx

arm: main.c core_observer.h
	gcc main.c -Os -o main-wfi -pthread -DWFI

umonitor: main.c core_observer.h
	gcc main.c -Os -lpthread -o main-umonitor -pthread -lrt -DUMONITOR
monitorx: main.c core_observer.h
	gcc main.c -Os -lpthread -o main-monitorx -pthread -lrt -DMONITORX

core_observer.h:
	python3 find_core.py

clean:
	rm -f main-umonitor main-monitorx main-wfi core_observer.h


