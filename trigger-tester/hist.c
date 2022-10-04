#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include "cacheutils.h"
#include <pthread.h>
#include <sched.h>
#include <immintrin.h>
#include <signal.h>
#include "ptedit_header.h"
#include "r0e.h"

// define cores for experiment
#define CORE_VICTIM 0
#define CORE_ATTACKER 1

// number of measurements
#define AVG_NO_TRIGGER 5
#define SKIP_TRIGGER 5
#define AVG_TRIGGER 5

// time interval for observing number of wakeups (in microseconds)
#define TIMER 10*1000

// direct physical map offset
#define DPM 0xffff888000000000ull


#define OFFSET (64 * 21)

#ifdef UMONITOR
#define INSTR_MONITOR asm volatile("umonitor %%rax" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; umwait %%rcx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "UMONITOR"
#endif
#ifdef MONITORX
#define INSTR_MONITOR asm volatile("monitorx" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("xor %%rbx, %%rbx; mwaitx; jnc 1f; inc %%rbx; 1: nop" : "=b"(carry) : "a"(-1), "d"(-1), "c"(0));
#define INSTR_NAME "MONITORX"
#endif
#ifdef MONITOR
#define INSTR_MONITOR asm volatile("monitor" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_WAIT asm volatile("mwait" : : "a"(0), "c"(0));
#define INSTR_NAME "MONITOR"
#endif
#ifdef MONITOR_INTEL
#define INSTR_MONITOR asm volatile("monitor" : : "a"(test + OFFSET), "c"(0), "d"(0));
#define INSTR_NAME "MONITOR_INTEL"
#endif

#ifndef INSTR_MONITOR
#error "No monitor/mwait instructions defined! Use -DUMONITOR, -DMONITORX, -DMONITOR, or -DMONITOR_INTEL to choose a monitoring instruction!"
#endif


#define TIMER_S  (TIMER / (1000000))
#define TIMER_US (TIMER % (1000000))


struct itimerval timer = {
    .it_interval = {.tv_sec = TIMER_S, .tv_usec = TIMER_US},
    .it_value = {.tv_sec = TIMER_S, .tv_usec = TIMER_US}
};


typedef void (*trigger_t)(char*, char*, char*);
typedef void (*setup_t)();

typedef struct {
    trigger_t trigger;
    const char* name;
    int success;
    setup_t setup;
    setup_t teardown;
    setup_t check;
    int execute;
} test_t;


void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}


int can_execute(setup_t f) {
    for(int i = 0; i < 32; i++) {
        signal(i, trycatch_segfault_handler);
    }
    if(!setjmp(trycatch_buf)) {
        f();
        return 1;
    }
    return 0;
}



void write_speculative2(void* ptr) {
    speculation_start(s);
//         maccess(0);
        *(char*)ptr = 3;
    speculation_end(s);
}

void write_speculative_no_fault(void* ptr, int value) {
    speculation_start(s);
        *(char*)ptr = 3;
    speculation_end(s);
}

__attribute__((aligned(4096))) char test[4096 * 256];

// ---- test functions ----
void flush_addr(char* addr, char* alias, char* dpm) {
    flush(addr);
}
void flush_alias(char* addr, char* alias, char* dpm) {
    flush(alias);
}
void flush_dpm(char* addr, char* alias, char* dpm) {
    flush(dpm);
}
void flush_other_dpm(char* addr, char* alias, char* dpm) {
    flush(dpm + 4096);
}
void write_addr(char* addr, char* alias, char* dpm) {
    (*(volatile char*)addr) = 2;
}
void write_alias(char* addr, char* alias, char* dpm) {
    (*(volatile char*)alias) = 2;
}
void write_dpm(char* addr, char* alias, char* dpm) {
    (*(volatile char*)dpm) = 2;
}
void write_other_dpm(char* addr, char* alias, char* dpm) {
    (*(volatile char*)(DPM)) = 2;
}
void write_spec_addr(char* addr, char* alias, char* dpm) {
    write_speculative2(addr);
}
void write_spec_alias(char* addr, char* alias, char* dpm) {
    write_speculative2(alias);
}
void write_spec_dpm(char* addr, char* alias, char* dpm) {
    write_speculative2(dpm);
}
void write_spec_other_dpm(char* addr, char* alias, char* dpm) {
    write_speculative2(dpm + 4096);
}
void access_addr(char* addr, char* alias, char* dpm) {
    maccess(addr);
}
void write_fault_addr(char* addr, char* alias, char* dpm) {
    maccess(dpm);
    (*(volatile char*)addr) = 2;
}
void clwb_addr(char* addr, char* alias, char* dpm) {
    asm volatile("clwb 0(%0)" : : "r"(addr));
}
void clwb_alias(char* addr, char* alias, char* dpm) {
    asm volatile("clwb 0(%0)" : : "r"(alias));
}
void clwb_dpm(char* addr, char* alias, char* dpm) {
    asm volatile("clwb 0(%0)" : : "r"(dpm));
}
void clzero_addr(char* addr, char* alias, char* dpm) {
    asm volatile("clzero" : : "a"(addr));
}
void clzero_alias(char* addr, char* alias, char* dpm) {
    asm volatile("clzero" : : "a"(alias));
}
void clzero_dpm(char* addr, char* alias, char* dpm) {
    asm volatile("clzero" : : "a"(dpm));
}
void flushopt_dpm(char* addr, char* alias, char* dpm) {
    asm volatile("clflushopt 0(%0)" : : "r"(dpm));
}
void prefetchw_addr(char* addr, char* alias, char* dpm) {
    asm volatile("prefetchw 0(%0)" : : "r"(addr));
}
void prefetchw_alias(char* addr, char* alias, char* dpm) {
    asm volatile("prefetchw 0(%0)" : : "r"(alias));
}
void prefetchw_dpm(char* addr, char* alias, char* dpm) {
    asm volatile("prefetchw 0(%0)" : : "r"(dpm));
}

// ---- setup / teardown functions ----
void mark_uc() {
    ptedit_entry_t entry = ptedit_resolve(test, 0);
    int uc_mt = ptedit_find_first_mt(PTEDIT_MT_UC);
    if (uc_mt == -1) {
        printf("No UC MT available!\n");
    }
    entry.pte = ptedit_apply_mt(entry.pte, uc_mt);
    entry.valid = PTEDIT_VALID_MASK_PTE;
    ptedit_update(test, 0, &entry);
    test[OFFSET] = 3;
}

void mark_wb() {
    ptedit_entry_t entry = ptedit_resolve(test, 0);
    int uc_wb = ptedit_find_first_mt(PTEDIT_MT_WB);
    if (uc_wb == -1) {
        printf("No WB MT available!\n");
    }
    entry.pte = ptedit_apply_mt(entry.pte, uc_wb);
    entry.valid = PTEDIT_VALID_MASK_PTE;
    ptedit_update(test, 0, &entry);
}


void mark_ro() {
	mprotect(test, 4096, PROT_READ);
}

void mark_rw() {
	mprotect(test, 4096, PROT_READ|PROT_WRITE);
}


// ---- check functions ----
void clwb_check() {
    clwb_addr(test, NULL, NULL);
}
void clzero_check() {
    clzero_addr(test, NULL, NULL);
}

test_t tests[] = {
    {.trigger = access_addr, .name = "Access"},
    {.trigger = write_addr, .name = "Write"},
    {.trigger = write_addr, .name = "Write R/O", .setup = mark_ro, .teardown = mark_rw},
    {.trigger = write_alias, .name = "Write alias"},
    {.trigger = write_dpm, .name = "Write DPM"},
    {.trigger = write_other_dpm, .name = "Write unrelated DPM"},
    {.trigger = write_spec_addr, .name = "Write speculatively"},
    {.trigger = write_spec_alias, .name = "Write alias speculatively"},
    {.trigger = write_spec_dpm, .name = "Write DPM speculatively"},
    {.trigger = write_spec_other_dpm, .name = "Write unrelated DPM speculatively"},
    {.trigger = write_fault_addr, .name = "Write after fault"},
    {.trigger = flush_addr, .name = "Flush"},
    {.trigger = flush_alias, .name = "Flush alias"},
    {.trigger = flush_dpm, .name = "Flush DPM"},
    {.trigger = flush_dpm, .name = "Flush unrelated DPM"},
    {.trigger = flushopt_dpm, .name = "FlushOpt DPM"},
    {.trigger = write_addr, .name = "Write UC", .setup = mark_uc, .teardown = mark_wb},
    {.trigger = write_spec_addr, .name = "Write UC speculatively", .setup = mark_uc, .teardown = mark_wb},
    {.trigger = write_alias, .name = "Write alias w/ monitored address UC", .setup = mark_uc, .teardown = mark_wb},
    {.trigger = write_spec_alias, .name = "Write alias speculatively w/ monitored address UC", .setup = mark_uc, .teardown = mark_wb},
    {.trigger = flush_addr, .name = "Flush UC", .setup = mark_uc, .teardown = mark_wb},
    {.trigger = clwb_addr, .name = "CLWB", .check = clwb_check},
    {.trigger = clwb_alias, .name = "CLWB alias", .check = clwb_check},
    {.trigger = clwb_dpm, .name = "CLWB DPM", .check = clwb_check},
    {.trigger = clzero_addr, .name = "CLZERO", .check = clzero_check},
    {.trigger = clzero_alias, .name = "CLZERO alias", .check = clzero_check},
    {.trigger = clzero_dpm, .name = "CLZERO DPM", .check = clzero_check},
    {.trigger = clzero_addr, .name = "CLZERO R/O", .setup = mark_ro, .teardown = mark_rw},
    {.trigger = prefetchw_addr, .name = "PREFETCHW"},
    {.trigger = prefetchw_alias, .name = "PREFETCHW alias"},
    {.trigger = prefetchw_dpm, .name = "PREFETCHW DPM"},
    {.trigger = prefetchw_addr, .name = "PREFETCHW R/O", .setup = mark_ro, .teardown = mark_rw},
};

volatile int testcase = 0, triggering = 0;

void* keystroke_thread(void* dummy) {
    pin_to_core(CORE_ATTACKER);
    printf("[~] Waiting for start (PID: %d)\n", getpid());
    test[OFFSET] = 1;

    size_t p = get_physical_address(test);
    if(p < 4096) {
            printf("Could not get physical address\n");
            exit(1);
    }
    size_t phys = p + DPM;
    size_t alias = ptedit_pmap(p, 4096);
    printf("Test: 0x%016zx\nPhys: 0x%016zx\nDPM:  0x%016zx\n2nd:  0x%016zx\n\n", test + OFFSET, p + OFFSET, phys + OFFSET, alias + OFFSET);

    while(1) {
        while(!triggering);
        printf("[+] Do the access!\n");
        asm volatile("mfence");
        size_t c = 0;
        while(triggering) {
            if(!setjmp(trycatch_buf)) {
                tests[testcase].trigger(test + OFFSET, alias + OFFSET, phys + OFFSET);
            }
        }
        asm volatile("mfence");
    }

}

volatile size_t cnt = 0, cnt_carry = 0;
volatile size_t sum_no_trigger = 0, sum_trigger = 0, samples = 0;

void sigalarm(int num) {
    if(samples == 0) {
        do {
            printf("Testing: '%s'\n", tests[testcase].name);
            if(tests[testcase].setup) tests[testcase].setup();
        } while(0);
    }
    printf("    Count: %zd (Carry: %zd)\n", cnt, cnt_carry);
    if(samples && samples < AVG_NO_TRIGGER) {
        sum_no_trigger += cnt;
    }
    if(samples > AVG_NO_TRIGGER) {
        triggering = 1;
    }
    if(samples > AVG_NO_TRIGGER + SKIP_TRIGGER && samples < AVG_NO_TRIGGER + SKIP_TRIGGER + AVG_TRIGGER) {
        sum_trigger += cnt;
    } 
    if(samples > AVG_NO_TRIGGER + SKIP_TRIGGER + AVG_TRIGGER) {
        samples = 0;
        printf("No trigger: %zd\n", sum_no_trigger / AVG_NO_TRIGGER);
        printf("   trigger: %zd\n", sum_trigger / AVG_TRIGGER);
        printf("\n");
        if((sum_trigger / AVG_TRIGGER) > 2 * (sum_no_trigger / AVG_NO_TRIGGER)) tests[testcase].success = 1;
        triggering = 0;
        sum_no_trigger = 0;
        sum_trigger = 0;
        if(tests[testcase].teardown) tests[testcase].teardown();
        do {
            testcase++;
            if(testcase >= sizeof(tests) / sizeof(tests[0])) {
                printf("\n==================================================================\n\n");
                printf("Working triggers for %s:\n", INSTR_NAME);
                for(int i = 0; i < testcase; i++) {
                    if(tests[i].success) {
                        printf(" + %s\n", tests[i].name);
                    }
                }
                exit(0);
            }
        } while(!tests[testcase].execute);
    } else {
        samples++;
    }
    
    cnt = 0;
    cnt_carry = 0;
}

size_t monitor_mwait(){
    size_t a = 0, d = 0, carry = 0;
    INSTR_MONITOR
    asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
    a = (d << 32) | a;
    asm volatile("mfence");
    size_t wakeup = a + 50000;
    asm volatile("mwait; jnc 1f; inc %0; 1: nop" : "=r"(carry) : "a"(0), "b"(wakeup & 0xffffffff), "d"((wakeup >> 32)), "c"(2));

    return carry;
}

int main(int argc, char *argv[]) {
#ifdef MONITOR_INTEL
    if (r0e_init()) {
        printf("Could not initialize r0e\n");
        return 1;
    }
#else
    if(ptedit_init()) {
        printf("Could not initialize PTEditor\n");
        return 1;
    }
#endif
    
    pthread_t p;
    pthread_create(&p, NULL, keystroke_thread, NULL);
    sched_yield();

    pin_to_core(CORE_VICTIM);

    for(int testcase = 0; testcase < sizeof(tests) / sizeof(tests[0]); testcase++) {
        if(tests[testcase].check) {
            tests[testcase].execute = can_execute(tests[testcase].check);
        } else {
            tests[testcase].execute = 1;
        }
    }
    
    
    memset(test, 1, sizeof(test));
    signal(SIGALRM, sigalarm);
    setitimer(ITIMER_REAL, &timer, NULL);
    signal(SIGSEGV, trycatch_segfault_handler);

    while(1) {

#ifdef MONITOR_INTEL
        cnt_carry += r0e_call(monitor_mwait);
#else
        INSTR_MONITOR
        size_t carry = 0;
        INSTR_WAIT
        cnt_carry += carry;
#endif
        cnt++;
    }


    return 0;
}

