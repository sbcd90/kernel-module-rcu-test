#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rculist.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/kthread.h>

static bool be_careful;

static struct task_struct *thread1;
static struct task_struct *thread2;
static struct task_struct *thread4;

static void cco_carefully(void) {
    ktime_t start, stop;
    s64 secs = 0;

    start = ktime_get();
    while (((unsigned int)secs) < 60000) {
        stop = ktime_get();

        secs = ktime_to_ms(ktime_sub(stop, start));
    }
    printk(KERN_INFO "end cco_carefully");
}

static void cco_quickly(void) {
    ktime_t start, stop;
    s64 secs = 0;

    start = ktime_get();
    while (((unsigned int)secs) < 60000) {
        stop = ktime_get();

        secs = ktime_to_ms(ktime_sub(stop, start));
    }
    printk(KERN_INFO "end cco_quickly");
}

static void do_maint(void) {
    msleep(60000);
    printk(KERN_INFO "end do_maint");
}

static int cco(void *args) {
    printk(KERN_INFO "hit cco");
    rcu_read_lock();
    if (READ_ONCE(be_careful)) {
        printk(KERN_INFO "hit cco_carefully");
        cco_carefully();
    } else {
        printk(KERN_INFO "hit cco_quickly");
        cco_quickly();
    }
    rcu_read_unlock();
    return 0;
}

static int maint(void *args) {
    printk(KERN_INFO "hit maint");
    WRITE_ONCE(be_careful, true);
    synchronize_rcu();
    printk(KERN_INFO "hit do_maint");
    do_maint();
    synchronize_rcu();
    WRITE_ONCE(be_careful, false);
    printk(KERN_INFO "end maint");
    return 0;
}

static int test_phased_state_change_init(void) {
    printk(KERN_INFO "phased state change - init module");
    thread1 = kthread_create(cco, NULL, "cco1");
    if (thread1) {
        printk(KERN_INFO "start cco1");
        wake_up_process(thread1);
    }
    msleep(30000);

    thread2 = kthread_create(maint, NULL, "maint");
    if (thread2) {
        printk(KERN_INFO "start maint");
        wake_up_process(thread2);
    }
    msleep(60000);

    thread4 = kthread_create(cco, NULL, "cco2");
    if (thread4) {
        printk(KERN_INFO "start cco2");
        wake_up_process(thread4);
    }
    return 0;
}

static void test_phased_state_change_exit(void) {
    int ret1, ret2, ret4;

    ret1 = kthread_stop(thread1);
    if (!ret1) {
        printk(KERN_INFO "thread1 stopped");
    }

    ret2 = kthread_stop(thread2);
    if (!ret2) {
        printk(KERN_INFO "thread2 stopped");
    }

    ret4 = kthread_stop(thread4);
    if (!ret4) {
        printk(KERN_INFO "thread4 stopped");
    }
    return;
}

module_init(test_phased_state_change_init);
module_exit(test_phased_state_change_exit);
MODULE_LICENSE("GPL");