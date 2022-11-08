# Website Fingerprinting

Simply run `make`, the `find_core.py` will be executed to find the core handling network interrupts. *On the arm, please run `make arm`*.

`main-monitorx` is for AMD machines, and `main-umonitor` is for the newest Intel processors that support `umwait` instruction. Please make sure that you execute the right one by setting in the `collect.sh`.

To collect the interrupt traces for different websites (where we list in `list_15.txt`, `list.txt`), simply run `./run.sh`. The `count` specifies how many pieces of data to collect for each website.
