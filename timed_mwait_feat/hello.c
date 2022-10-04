
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <asm/mwait.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("R.Y.");
MODULE_DESCRIPTION("Timed MWAIT");
MODULE_VERSION("0.01");

#define PIN_CORE 1

#define OFFSET (64 * 21)
 __attribute__((aligned(4096))) char test[4096 * 32];

static struct task_struct *mwait_thread_st;

static int mwait_thread_fn(void *unused)
{
    allow_signal(SIGKILL);
    while (!kthread_should_stop())
    {
        if (signal_pending(mwait_thread_st))
            break;

        uint64_t high, low;
        uint64_t a, d;
        uint64_t sum = 0, cnt = 0, mcnt = 0;
        int offset = 5000;

        while(!kthread_should_stop()){
                __monitor(test+OFFSET, 0, 0);
                asm volatile("mfence");
                asm volatile("rdtscp" : "=a"(a), "=d"(d) :: "rcx");
                a = (d << 32) | a;
                asm volatile("mfence");
                uint64_t wakeup = a + offset;

                // __mwait(0x0, 0);
                // The maximum waiting time is an implicit 64-bit timestamp-counter value 
                // stored in the EDX:EBX register pair.
                // Test timeout by changing "b" => (0xffffffff) or "d"((wakeup >> 32) + 1) 
		        // ECX bit 31 indicate whether timeout feature is used
		        // EAX [0:3] indicate sub C-state; [4:7] indicate C-states e.g., 0=>C1, 1=>C2 ...
                asm volatile("mwait" :: "a"(0), "b"(wakeup & 0xffffffff), "d"((wakeup >> 32)), "c"(2));
                asm volatile("rdtscp" : "=a"(low), "=d"(high) :: "rcx");

                uint64_t real = (high << 32) | low;
                int diff = (int)(real - wakeup);

                if(diff > 0) {
                        cnt++;
                        sum += diff;
                        mcnt++;
                }
                if(cnt == 1000) {
                        mcnt = 0;
                        sum = 0;
                }
                if(cnt == 2000) {
                        printk("Avg: %3d (Offset: %4d) -> Measured offset: %4d\n", (int)(sum / mcnt), offset, offset + (int)(sum / mcnt));
                        cnt = 0;
                        sum = 0;
                        offset++;  
			do_exit(0);
                }
        }
    }
    printk(KERN_INFO "Mwait Thread Stopping\n");
    do_exit(0);
    return 0;
}

// Module Initialization
static int __init test_start(void)
{
    printk(KERN_INFO "Creating Thread\n");

    mwait_thread_st = kthread_create(mwait_thread_fn, NULL, "mwait_thread");
    if (mwait_thread_st)
        printk("Mwait Thread Created successfully\n");
    else
        printk(KERN_INFO "Mwait Thread creation failed\n");
    kthread_bind(mwait_thread_st, PIN_CORE);

    wake_up_process(mwait_thread_st);

    return 0;
}

static void __exit test_end(void)
{
    printk(KERN_INFO "END TIMED MWAIT TEST\n");
}

module_init(test_start);
module_exit(test_end);
