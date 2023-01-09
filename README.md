# UMWAIT
This repository contains the experiments of evaluation and case studies discussed in the paper 
* "(M)WAIT for It: Bridging the Gap between Microarchitectural and Architectural Side Channels" (USENIX Security'23). 

You can find the paper at the [USENIX website](https://publications.cispa.saarland/3769/1/mwait_sec23.pdf).


## Tested Setup
We reverse engineer the memory-monitoring functions on Intel and AMD. 
All systems are running Ubuntu 20.04 LTS (Linux kernel 5.4).

The result shows that the Intel-specific variant `UMONITOR/UMWAIT` instructions help convert microarchitectural into architectural states, which are only available on Intel latest core processors (Tremont and Alder Lake). 

In order to run the experiments and proof-of-concepts, the following prerequisites need to be fulfilled:

* Linux installation
  * Build tools (gcc, make)
  * [PTEditor](https://github.com/misc0110/PTEditor/)
  * Stress
  
Throughout our experiments, we successfully evaluated our implementations on the following CPUs. However, most of the implementation should work on CPUs with the same microarchitecture.

| CPU                          | Microcode    | Microarchitecture |
| ---------------------------- | -----------  | ----------------- |
| Intel Celeron N4500          | `0x24000014` | Jasper Lake       |
| Intel Core i9-12900K         | `0xf`        | Alder Lake        |
| Intel Core i7-8565U          | `0xec`       | Whiskey Lake      |
| Intel Core i7-10710U         | `0xe8`       | Comet Lake        |
| AMD Ryzen 5 2500U            | `0x810100b`  | Zen               |
| AMD Ryzen 5 3550H            | `0x8108102`  | Zen+              |
| AMD Ryzen 9 5900HX           | `0xa50000c`  | Zen3              |


## Materials
This repository contains the following materials:

* `Intel-umwait`: contains the code that test if `umonitor/umwait` work on the current processor.
* `trigger-tester`: contains the code that we used to analyse the wakeup-trigger of all `mwait-` variants (Table 1-2).
* `timed_mwait_feat`: contains the code that we reverse engineered the Intel's undocumented `timed-mwait` feature.
* `comparison`: contains the code that we constructed a standard benchmark for detecting fully asynchronous events with Transient-Writes-Monitor (TWM) and other conventional side-channel attacks for reference (Figure 1-2, Table 3).
* `covert_channel_eval`: contains the code that we created a timer-less covert channel with `umonitor/umwait` (Figure 4).
* `spectral`: contains the code that we used the timer-less covert channel for spectre attacks (Figure 5-6).
* `aes_example`: contains the code that we reproduced attacks on AES T-table implementation based on our Timer-less Timing Measurement (Figure 3, 7).
* `irq_monitor`: contains the code that we can monitor network interrupts via the `mwait-` instructions on x86 and `wfi` instruction on arm.
* `website_fingerprinting`: contains the code that we detected network interrupts while opening a website (Figure 8).
* `website_classify`: contains the classifier for website classification (Figure 9).

## Contact
If there are questions regarding these experiments, please send an email to `ruiyi.zhang (AT) cispa.de` or message `@Rayiizzz` on Twitter.

## Research Paper
The paper is available at the [USENIX website](https://www.usenix.org/conference/usenixsecurity23). 
You can cite our work with the following BibTeX entry:
```latex
@inproceedings{Zhang2023MWAIT,
  year={2023},
  title={{(M)WAIT for It: Bridging the Gap between Microarchitectural and Architectural Side Channels}},
  booktitle={USENIX Security},
  author={Ruiyi Zhang and Taehyun Kim and Daniel Weber and Michael Schwarz}
}
```

## Disclaimer
We are providing this code as-is. 
You are responsible for protecting yourself, your property and data, and others from any risks caused by this code. 