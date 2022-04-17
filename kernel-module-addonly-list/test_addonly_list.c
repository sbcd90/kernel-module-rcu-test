#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/kthread.h>

static LIST_HEAD(rl);
static spinlock_t listLock;

static struct task_struct *thread1;
static struct task_struct *thread2;

struct data {
    char content[64];
    struct list_head nxt;
    struct rcu_head rcu;
};

static void addDelay(void) {
    ktime_t start, stop;
    s64 secs = 0;

    start = ktime_get();
    while (((unsigned int) secs) < 30000) {
        stop = ktime_get();

        secs = ktime_to_ms(ktime_sub(stop, start));
    }
}

static void doSomething(struct data *p) {
    printk(KERN_INFO "%s", p->content);
}

static void listReader(int i) {
    struct data *p;

    printk(KERN_INFO "start reader%d", i);
    list_for_each_entry_rcu(p, &rl, nxt, true) {
        doSomething(p);
    }
    addDelay();
    printk(KERN_INFO "end reader%d", i);
}

static void listAdder(const char *content, int i) {
    struct data *q;

    printk(KERN_INFO "start adder%d", i);
    addDelay();
    q = kzalloc(sizeof(struct data), GFP_KERNEL);
    if (q == NULL) {
        return;
    }

    strncpy(q->content, content, sizeof(q->content));

    spin_lock(&listLock);
    list_add_rcu(&(q->nxt), &rl);
    spin_unlock(&listLock);
    printk(KERN_INFO "end adder%d", i);
}

static int adderThread(void *args) {
    for (int i=0; i < 10; ++i) {
        char buffer[64];

        char *num = kasprintf(GFP_KERNEL, "%d", i);
        if (!num) {
            printk(KERN_INFO "kasprintf");
        } else {
            strcat(strcpy(buffer, "Golko"), num);
            listAdder(buffer, i);
        }
    }
    return 0;
}

static int readerThread(void *args) {
    for (int i=0; i < 10; ++i) {
        listReader(i);
    }
    return 0;
}

static int addonly_list_test_rcu_init(void) {
    spin_lock_init(&listLock);

    printk(KERN_INFO "addonly list - init module");
    thread1 = kthread_create(readerThread, NULL, "reader thread");
    if (thread1) {
        printk(KERN_INFO "start reader thread");
        wake_up_process(thread1);
    }
    msleep(15000);
    thread2 = kthread_create(adderThread, NULL, "adder thread");
    if (thread2) {
        printk(KERN_INFO "start adder thread");
        wake_up_process(thread2);
    }
    return 0;
}

static void addonly_list_test_rcu_exit(void) {
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

module_init(addonly_list_test_rcu_init);
module_exit(addonly_list_test_rcu_exit);
MODULE_LICENSE("GPL");