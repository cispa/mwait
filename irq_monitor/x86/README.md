# Interrupt detection

Detects hardware interrupts using `mwait-` variants. 

Set `CORE_VICTIM` to the core that handles network interrupts (check `/proc/interrupts`). 
Run `./main`. In parallel, on any (other) core, run `bash -c 'while [ 1 ]; do curl localhost; done'`. 
You should see increases in wakeups while the network card handles the connection. 

`umonitor/umwait` and `monitorx/mwaitx` are unprivileged Intel-/AMD-specific instructions and such that our first choice. 

`monitor/mwait` is privileged. If you want to test with it, monitor and mwait can be made unprivileged by setting bit 10 of MSR `0xc0010015`.

`sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR | 1024))); wrmsr -a 0xc0010015 0x$ENABLED'`