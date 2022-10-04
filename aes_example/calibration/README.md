# UMWAIT Calibration for AES T-table Attacks

Find an appropriate padding for distinguishing cache hits from cache misses by `./pad`

Then update the length of padding in `main.c T_INIT`

Find T-table addresses by `./Ttable`

Execute `export LD_LIBRARY_PATH=/home/rzhang/openssl:$LD_LIBRARY_PATH` before finding T-table Addresses
