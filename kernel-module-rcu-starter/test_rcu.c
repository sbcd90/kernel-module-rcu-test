#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/preempt.h>

struct book {
    int id;
    char name[64];
    char author[64];
    int borrow;
    struct list_head node;
    struct rcu_head rcu;
};

static LIST_HEAD(books);
static spinlock_t booksLock;

static void add_book(int id, const char *name, const char *author) {
    struct book *bk;

    bk = kzalloc(sizeof(struct book), GFP_KERNEL);
    if (!bk) {
        return;
    }
    bk->id = id;
    strncpy(bk->name, name, sizeof(bk->name));
    strncpy(bk->author, author, sizeof(bk->author));
    bk->borrow = 0;

    spin_lock(&booksLock);
    list_add_rcu(&(bk->node), &books);
    spin_unlock(&booksLock);
}

static void print_book(int id) {
    struct book *bk;

    rcu_read_lock();
    list_for_each_entry_rcu(bk, &books, node) {
        if (bk->id == id) {
            pr_info("id : %d, name : %s, author : %s, borrow : %d, addr : %lx\n",
            bk->id, bk->name, bk->author, bk->borrow, (unsigned long)bk);
            
            rcu_read_unlock();
            return;
        }
    }

    rcu_read_unlock();
    pr_info("not exist book\n");
}

static int is_borrowed_book(int id) {
    struct book *bk;

    rcu_read_lock();
    list_for_each_entry_rcu(bk, &books, node) {
        if (bk->id == id) {
            rcu_read_unlock();
            return bk->borrow;
        }
    }
    rcu_read_unlock();

    pr_err("not exist book\n");
    return -1;
}

static int borrow_book(int id) {
    struct book *bk = NULL;
    struct book *new_b = NULL;
    struct book *old_b =  NULL;

    rcu_read_lock();

    list_for_each_entry(bk, &books, node) {
        if (bk->id == id) {
            if (bk->borrow) {
                rcu_read_unlock();
                return -1;
            }

            old_b = bk;
            break;
        }
    }

    if (old_b == NULL) {
        rcu_read_unlock();
        return -1;
    }
    
    new_b = kzalloc(sizeof(struct book), GFP_ATOMIC);
    if (new_b == NULL) {
        rcu_read_unlock();
        return -1;
    }

    memcpy(new_b, old_b, sizeof(struct book));
    new_b->borrow = 1;

    spin_lock(&booksLock);
    list_replace_rcu(&old_b->node, &new_b->node);
    spin_unlock(&booksLock);

    rcu_read_unlock();

    synchronize_rcu();
    kfree(old_b);

    pr_info("borrow success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static int return_book(int id) {
    struct book *bk = NULL;
    struct book *new_bk = NULL;
    struct book *old_bk = NULL;

    rcu_read_lock();
    list_for_each_entry(bk, &books, node) {
        if (bk->id == id) {
            if (bk->borrow == 0) {
                rcu_read_unlock();
                return -1;
            }

            old_bk = bk;
            break;
        }
    }

    if (old_bk == NULL) {
        rcu_read_unlock();
        return -1;
    }

    new_bk = kzalloc(sizeof(struct book), GFP_ATOMIC);
    if (new_bk == NULL) {
        rcu_read_unlock();
        return -1;
    }

    memcpy(new_bk, old_bk, sizeof(struct book));
    new_bk->borrow = 0;

    spin_lock(&booksLock);
    list_replace_rcu(&old_bk->node, &new_bk->node);
    spin_unlock(&booksLock);

    rcu_read_unlock();

    synchronize_rcu();
    kfree(old_bk);

    pr_info("return success %d, preempt_count : %d\n", id, preempt_count());
	return 0;
}

static void test_example(void) {
    add_book(0, "book1", "jb");
    add_book(1, "book2", "jb");

    print_book(0);
    print_book(1);

    pr_info("book1 borrow : %d\n", is_borrowed_book(0));
    pr_info("book2 borrow : %d\n", is_borrowed_book(1));

    borrow_book(0);
    borrow_book(1);

    print_book(0);
    print_book(1);

    return_book(0);
    return_book(1);

    print_book(0);
    print_book(1);
}

static int list_test_rcu_init(void) {
    spin_lock_init(&booksLock);

    test_example();

    return 0;
}

static void list_test_rcu_exit(void) {
    return;
}

module_init(list_test_rcu_init);
module_exit(list_test_rcu_exit);
MODULE_LICENSE("GPL");