kernel-module-phased-state-change-example
=========================================

- In this task, I try to figure out how RCU works in general in the linux kernel. In order to learn more about RCUs in general, I chose the `Phased state change` example provided by Paul in the [tutorial](https://linuxfoundation.org/webinars/unraveling-rcu-usage-mysteries/).

- In my example, there is a `common-case operation` & `maint operation`.
The `common-case operation` should be done `quickly` during `normal` phase & should be done `carefully` during `maint` phase.

Properties of RCUs
==================

- RCU allows `synchronization` between `multiple readers` & a `single updater`.
- `Readers` code inside `rcu_read_lock()` and `rcu_read_unlock()` cannot have blocking/sleeping code. 
- `synchronize_rcu()` is a `blocking call` in `updater` which makes the `updater` wait for all previously started `readers` to complete.
- explicit `spinlock` mechanism needs to be used to `synchronize` between `multi updaters`.

Phased state change example
===========================

- For the sake of simplicity, let us assume that `cco_carefully` and `cco_quickly` does the same thing. They loop for `1 min.`.

```
ktime_t start, stop;
s64 secs = 0;

start = ktime_get();
while (((unsigned int)secs) < 60000) {
    stop = ktime_get();

    secs = ktime_to_ms(ktime_sub(stop, start));
}
```

- Now, lets create 3 threads to simulate the behavior of Paul's example. 

```
printk(KERN_INFO "phased state change - init module - %llu", (ktime_get_ns() / 1000000000));
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
```

- The 1st thread starts a reader.

```
thread1 = kthread_create(cco, NULL, "cco1");
if (thread1) {
    printk(KERN_INFO "start cco1");
    wake_up_process(thread1);
}
```

- Now, as the flag `be_careful` is not set here, this should hit `cco_quickly`. A sleep time of `30 secs.` is provided thereafter. Please note, the `cco_quickly` task runs for `1 min`.

- As after `30 secs.` `maint` task thread is triggered, we see an `overlap between first reader & maint task`. So, even before first
reader is completed, the `first 2 lines of maint() is executed`.

```
printk(KERN_INFO "hit maint");
WRITE_ONCE(be_careful, true);
synchronize_rcu();
printk(KERN_INFO "hit do_maint");
do_maint();
synchronize_rcu();
WRITE_ONCE(be_careful, false);
printk(KERN_INFO "end maint");
```

- But, then `maint` operation cannot proceed to `do_maint()` till `1st reader is completed` because of `synchronize_rcu` call. Thus, `cco_quickly` finishes first & then only `do_maint()` can start.

- Now, as per our story, any `cco_task` happening during `do_maint()` phase must call `cco_carefully()`.

- So, in order to start a 2nd reader & overlap it with `do_maint()` we put a sleep of `1 min` & start a new thread for 2nd reader.

```
msleep(60000);

thread4 = kthread_create(cco, NULL, "cco2");
if (thread4) {
    printk(KERN_INFO "start cco2");
    wake_up_process(thread4);
}
```

- This reader will call `cco_carefully()` now.
- Due to the 2nd `synchronize_rcu` call in `maint()`, this reader now has to complete before the `maint()` function ends.

- The dmesg log verifies all of the above statements.

```
[   99.566511] phased state change - init module
[   99.566653] start cco1
[   99.566686] hit cco
[   99.566687] hit cco_quickly
[  101.133465] sched: RT throttling activated
[  122.651194] audit: type=1100 audit(1649028244.082:62): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:authentication grantors=pam_faillock,pam_permit,pam_faillock acct="sbcd90" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  122.651493] audit: type=1101 audit(1649028244.082:63): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:accounting grantors=pam_unix,pam_permit,pam_time acct="sbcd90" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  122.652578] audit: type=1110 audit(1649028244.085:64): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  122.654700] audit: type=1105 audit(1649028244.085:65): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:session_open grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  122.662011] audit: type=1106 audit(1649028244.095:66): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:session_close grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  122.662126] audit: type=1104 audit(1649028244.095:67): pid=684 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  131.102127] start maint
[  131.102161] hit maint
[  159.569091] end cco_quickly
[  159.601005] hit do_maint
[  179.620992] audit: type=1101 audit(1649028301.049:68): pid=931 uid=1000 auid=1000 ses=1 msg='op=PAM:accounting grantors=pam_unix,pam_permit,pam_time acct="sbcd90" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  179.621915] audit: type=1110 audit(1649028301.052:69): pid=931 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_env,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  179.624173] audit: type=1105 audit(1649028301.052:70): pid=931 uid=1000 auid=1000 ses=1 msg='op=PAM:session_open grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  179.630482] audit: type=1106 audit(1649028301.059:71): pid=931 uid=1000 auid=1000 ses=1 msg='op=PAM:session_close grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  179.630640] audit: type=1104 audit(1649028301.059:72): pid=931 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_env,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  192.555196] start cco2
[  192.555721] hit cco
[  192.555724] hit cco_carefully
[  192.558365] audit: type=1106 audit(1649028313.985:73): pid=677 uid=1000 auid=1000 ses=1 msg='op=PAM:session_close grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/0 res=success'
[  192.558371] audit: type=1104 audit(1649028313.985:74): pid=677 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/0 res=success'
[  206.643094] audit: type=1101 audit(1649028328.075:75): pid=942 uid=1000 auid=1000 ses=1 msg='op=PAM:accounting grantors=pam_unix,pam_permit,pam_time acct="sbcd90" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  206.643661] audit: type=1110 audit(1649028328.075:76): pid=942 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_env,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  206.645389] audit: type=1105 audit(1649028328.078:77): pid=942 uid=1000 auid=1000 ses=1 msg='op=PAM:session_open grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  206.651702] audit: type=1106 audit(1649028328.082:78): pid=942 uid=1000 auid=1000 ses=1 msg='op=PAM:session_close grantors=pam_systemd_home,pam_limits,pam_unix,pam_permit acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  206.651746] audit: type=1104 audit(1649028328.082:79): pid=942 uid=1000 auid=1000 ses=1 msg='op=PAM:setcred grantors=pam_faillock,pam_permit,pam_env,pam_faillock acct="root" exe="/usr/bin/sudo" hostname=? addr=? terminal=/dev/pts/1 res=success'
[  219.852212] end do_maint
[  252.556730] end cco_carefully
[  252.589445] end maint
```
