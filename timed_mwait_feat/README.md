# Timed MWAIT

Quick test for the undocumented timed MWAIT feature via a simple Linux kernel module.

Run `sudo ./run.sh`. It first enable this feature by writing to MSR 0xe2. Then the bit 31 of ECX of mwait indicates this feature is actually used. The timeout value is stored in the implicit 64-bits EDX:EBX registers pair.

You can see the timed MWAIT wakes due to the timeout (Set by `offset`)
