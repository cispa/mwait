# Attacks on AES T-table

## Setup
The targeted version of openssl is 1.0.1e.

`export LD_LIBRARY_PATH=<openssl address>:$LD_LIBRARY_PATH`

Find T-table addresses by `nm <libcrypto.so address> | grep Te`

## Overview

### f+r 
Cache attack using `Flush+Reload`. 

The threshold `MIN_CACHE_MISS_CYCLES` needs to be reset.

### p+p
Cache attack using `Prime+Probe`. 
The code finding the eviction set is a strongly modified version of [evsets](https://github.com/cgvwzq/evsets).

The Find Prime Access Patterns is found by the tool [PrimeTime](https://github.com/KULeuven-COSIC/PRIME-SCOPE/tree/main/primetime).

The threshold `EVICTION_THRESHOLD` and `PRIME_THRESHOLD` need to be reset.

### calibration
`pad.c`: code of finding an appropriate padding for TLT to distinguish cache hits from cache misses.
`Ttable.c`: code of finding the T-table addresses.

### umwait
Cache attack using `Timing-less Timer(TLT)`.

The padding's length `T_INIT` needs to be reset.