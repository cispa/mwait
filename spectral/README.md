# Spectre PoC

Uses `umwait` as the covert channel in a Spectre attack. 

Run using `./main`. Every second it outputs the leakage rate, error rate, and true capacity. 
An optional parameter can be provided to stop the experiment after N seconds: `./main N`.
If this parameter is provided, it outputs the last leakage rate, error rate, and true capacity as CSVs. 

Run `./test.sh` to evaluate the leakage rate for different `umwait` timeouts from 1000 to 200000. 
The result is stored in `log.csv`.
