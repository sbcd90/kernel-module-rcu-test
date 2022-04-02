kernel-module-rcu-test
======================

A simple project to show how read-copy-update synchronization mechanism in the linux kernel.

How spinlock works?
===================

[Here](./kernel-module-spinlock-test/) is the module.

```
make
sudo insmod test_spinlock.ko
sudo dmesg | less > log.txt
sudo rmmod test_spinlock.ko
```

How RCU works?
==============

[Here](./kernel-module-rcu-starter/) is the module.

```
make
sudo insmod test_rcu.ko
sudo dmesg | less > log.txt
sudo rmmod test_rcu.ko
```
