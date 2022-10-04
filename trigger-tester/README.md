# Trigger Test

Tests various events (reads, writes, flushes, prefetches) to see what wakes up the different `mwait` instructions. 

Run using `sudo ./test-<monitor variant>`. 

If you are testing `mwait` instruction on Intel, make sure use `sudo ./enable-msr.sh` to enable timed `mwait` feature in advance.

On AMD, monitor and mwait can be made unprivileged by setting bit 10 of MSR `0xc0010015`.

`sudo bash -c 'modprobe msr; CUR=$(rdmsr 0xc0010015); ENABLED=$(printf "%x" $((0x$CUR | 1024))); wrmsr -a 0xc0010015 0x$ENABLED'`