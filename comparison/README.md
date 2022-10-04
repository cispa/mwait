# Comparison 

Code of comparing Transient-Writes-Monitor (TWM) with other conventional side channels for detecting victim events.

The victim event is either a transient access or a transient write to a targeted cache line pinned to a different physical core than the attacker.
 
Each asynchronous event is triggered after the process relinquishes the CPU `sched_yield()` and waits for a delay loop `wait() in cacheutils.h`. 
The iteration number is randomly selected below a decreasing threshold `(event_internal)`.