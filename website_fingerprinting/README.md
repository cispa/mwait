# Website Fingerprinting

Uses `mwaitx` to detect network interrupts while opening a website. Saves the trace as `log.csv`

Set `CORE_OBSERVER` to the core that handles network interrupts (check `/proc/interrupts`). 
Run `./main-monitorx <website>`. Plot log(s) using `python3 plot.py <log file 1> [<log file 2> ...]`.
