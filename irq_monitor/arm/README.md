# Interrupt detection on ARMv8

Detects hardware interrupts using `wfi`. 

Set `CORE_VICTIM` to the core that handles network interrupts (check `/proc/interrupts`). 
Run `./main`. In parallel, on any (other) core, run `bash -c 'while [ 1 ]; do curl localhost; done'`. 
You should see **increases in wakeups** while the network card handles the connection. 
