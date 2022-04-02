#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

static struct task_struct *thread1;
static struct task_struct *thread2;

DEFINE_SPINLOCK(etx_spinlock);

int etx_global_variable = 0;

bool kthread_should_stop(void) {
    if (etx_global_variable == 1000) {
        return true;
    }
    return false;
}

int thread_function1(void *args) {
    while (!kthread_should_stop()) {
        spin_lock(&etx_spinlock);
        ++etx_global_variable;
        printk(KERN_INFO "in thread1 function- %d\n", etx_global_variable);
        spin_unlock(&etx_spinlock);
        msleep(1000);
    }
    return 0;
}

int thread_function2(void *args) {
    while (!kthread_should_stop()) {
        spin_lock(&etx_spinlock);
        ++etx_global_variable;
        printk(KERN_INFO "in thread2 function- %d\n", etx_global_variable);
        spin_unlock(&etx_spinlock);
        msleep(50);
    }
    return 0;
}

static int test_spinlock_module_init(void) {
    thread1 = kthread_create(thread_function1, NULL, "thread1");
    if (thread1) {
        printk(KERN_INFO "start thread1");
        wake_up_process(thread1);
    }

    thread2 = kthread_create(thread_function2, NULL, "thread2");
    if (thread2) {
        printk(KERN_INFO "start thread2");
        wake_up_process(thread2);
    }
    return 0;
}

static void test_spinlock_module_exit(void) {
    int ret1, ret2;
    ret1 = kthread_stop(thread1);
    if (!ret1) {
        printk(KERN_INFO "thread1 stopped");
    }

    ret2 = kthread_stop(thread2);
    if (!ret2) {
        printk(KERN_INFO "thread2 stopped");
    }
    return;
}

module_init(test_spinlock_module_init);
module_exit(test_spinlock_module_exit);
MODULE_LICENSE("GPL");