# oslab4
# 实验报告

## 练习1：分配并初始化一个进程控制块（需要编码）

alloc_proc函数（位于kern/process/proc.c中）负责分配并返回一个新的struct proc_struct结构，用于存储新建立的内核线程的管理信息。ucore需要对这个结构进行最基本的初始化，你需要完成这个初始化过程。
【提示】在alloc_proc函数的实现中，需要初始化的proc_struct结构中的成员变量至少包括：state/pid/runs/kstack/need_resched/parent/mm/context/tf/cr3/flags/name。
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
•	请说明proc_struct中struct context context和struct trapframe *tf成员变量含义和在本实验中的作用是啥？（提示通过看代码和编程调试可以判断出来）

### 设计实现过程

在`alloc_proc`函数的实现中，对`proc_struct`结构的成员进行初始化：

```c
proc->state = PROC_UNINIT;
proc->pid = -1;
proc->runs = 0;
proc->kstack = 0;
proc->need_resched = 0;
proc->parent = NULL;
proc->mm = NULL;
memset(&(proc->context), 0, sizeof(struct context));
proc->tf = NULL;
proc->cr3 = boot_cr3;
proc->flags = 0;
memset(proc->name, 0, PROC_NAME_LEN);
```


将所有的成员全部设置为 0，对应于指针则为 NULL，而state设置为进程的“初始”态、pid设置为未初始化值、cr3采用为uCore内核已经建立的页表，即设置为在uCore内核页表的起始地址boot_cr3（内核线程从属于同一个唯一的“大内核进程”—uCore内核）。

struct context context表示进程执行的上下文线程，其中包含了ra，sp，s0~s11共14个寄存器，以便在将来重新切换回来时能够还原寄存器的状态。并且只保存被调用者保存寄存器，这是因为线程的切换在一个函数当中，编译器会自动生成保存和恢复调用者保存寄存器的代码，在实际的进程切换过程中只需要保存被调用者保存寄存器。
tf里保存了进程的中断帧。当进程从用户空间跳进内核空间的时候，进程的执行状态被保存在了中断帧中，以便在中断处理完毕后能够恢复进程的执行。在本次实验中保存第1个内核线程 initproc 的中断帧。

## 练习2：为新创建的内核线程分配资源（需要编码）
创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用do_fork函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。因此，我们实际需要"fork"的东西就是stack和trapframe。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：
•	调用alloc_proc，首先获得一块用户信息块。
•	为进程分配一个内核栈。
•	复制原进程的内存管理信息到新进程（但内核线程不必做此事）
•	复制原进程上下文到新进程
•	将新进程添加到进程列表
•	唤醒新进程
•	返回新进程号
请在实验报告中简要说明你的设计实现过程。请回答如下问题：
•	请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

### 设计实现过程
```c
    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    proc->parent = current;

    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }
    copy_thread(proc, stack, tf);
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        list_add(&proc_list, &(proc->list_link));
        nr_process++;
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);

    ret = proc->pid;
```
首先调用alloc_proc，分配进程控制块proc，然后将当前进程设置为新进程的父进程。
接下来调用setup_kstack 函数为新进程分配内核栈，并根据 clone_flags 复制或共享内存管理信息。
（struct Page *page = alloc_pages(KSTACKPAGE);proc->kstack = (uintptr_t)page2kva(page);)
使用copy_thread复制线程的状态，包括中断帧tf和上下文context等，新创建的进程内核栈上专门给中断帧的空间。并且禁止中断，防止在操作 proc_list 时被打断。
禁止中断后给进程分配 pid，并将新进程添加到 proc_list 和 hash_list 中，然后恢复中断状态。
最后唤醒新进程，把proc的state设为PROC_RUNNABLE，使其变为可运行状态，并设置返回值为新进程的 pid。


请说明ucore是否做到给每个新fork的线程一个唯一的id？


```c

#define MAX_PID                     (MAX_PROCESS * 2)

// get_pid - alloc a unique pid for process
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    //next_safe和last_pid两个变量，这里需要注意的是，它们是static全局变量，每一次调用这个函数的时候，这两个变量的数值之间的取值均是合法的 pid（也就是说没有被使用过）
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    //++last_pid>=MAX_PID,说明pid分到了最后一个，需要从1重新再来
    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list; //le等于线程的链表头
        while ((le = list_next(le)) != list) {//这里相当于循环链表的操作
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                //如果proc的pid与last_pid相等，即这个pid被其他进程占用，则将last_pid加1，重新检查
                //如果又发生了last_pid+1 >= next_safe，next_safe就要变为最后一个，重新循环
                //当然这里如果在前两者基础上出现了last_pid>=MAX_PID,相当于直接从整个区间重新再来
                if (++ last_pid >= next_safe) {
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            //如果last_pid<proc->pid<next_safe，确保最后能够找到这么一个满足条件的区间[last_pid,min(next_safe, proc->pid)) 尚未被占用，获得合法的pid范围，取区间第一个作为返回值
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

```

    MAX_PID 定义了最大的进程 ID，确保它大于 MAX_PROCESS，即最大的进程数量。
    使用 next_safe 和 last_pid 两个变量来跟踪可用的 pid 范围，确保每次分配的 pid 都是唯一的。
    通过 ++last_pid 来递增 pid，如果超过 MAX_PID，则重新从 1 开始。
    在循环链表中遍历进程，检查当前的 last_pid 是否已经被其他进程占用，如果是，则继续递增 last_pid 直到找到一个未被占用的 pid。
    如果出现 last_pid + 1 >= next_safe 的情况，重新设定 next_safe，确保下次分配的 pid 范围在合法区间内。
    如果 last_pid 小于当前进程的 pid 且小于 next_safe，则更新 next_safe 为当前进程的 pid，确保分配的 pid 在合法区间内。
    最终，返回分配的唯一 pid。

通过这种方式，ucore 确保了给每个新 fork 的线程分配一个唯一的进程 ID。

## 练习3：编写proc_run 函数（需要编码）

proc_run用于将指定的进程切换到CPU上运行。它的大致执行步骤包括：
•	检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
•	禁用中断。你可以使用/kern/sync/sync.h中定义好的宏local_intr_save(x)和local_intr_restore(x)来实现关、开中断。
•	切换当前进程为要运行的进程。
•	切换页表，以便使用新进程的地址空间。/libs/riscv.h中提供了lcr3(unsigned int cr3)函数，可实现修改CR3寄存器值的功能。
•	实现上下文切换。/kern/process中已经预先编写好了switch.S，其中定义了switch_to()函数。可实现两个进程的context切换。
•	允许中断。
请回答如下问题：
•	在本实验的执行过程中，创建且运行了几个内核线程？

### 设计实现过程

```c
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag);
        // {
        current = proc;
        //     load_esp0(next->kstack + KSTACKSIZE);
        lcr3(next->cr3);
        switch_to(&(prev->context), &(next->context));
        // }
        local_intr_restore(intr_flag);

```
1.	检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
2.	使用 local_intr_save 宏禁用中断，保证在切换过程中不被中断打断。
3.	将当前进程指针 current 指向要运行的进程 proc。
4.	使用 lcr3 函数切换页表，以便使用新进程的地址空间。
5.	调用 switch_to 函数进行上下文切换，将当前进程的上下文切换为要运行的进程的上下文。
6.	使用 local_intr_restore 宏允许中断，恢复中断状态。

在本实验的执行过程中，创建且运行了 2 个内核线程。

## 扩展练习 Challenge：
说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？

```c
/* intr_enable - enable irq interrupt */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }

/* intr_disable - disable irq interrupt */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }

```
在 RISC-V 中，处理器状态寄存器（sstatus）包含了一系列用于控制和监控处理器状态的标志位。具体来说，SSTATUS_SIE是 sstatus 寄存器中的一个位，用于控制中断的使能。当 SSTATUS_SIE 被设置（1）时，表示中断被允许，处理器可以响应中断；当 SSTATUS_SIE 被清除（0）时，表示中断被禁止，处理器不会响应中断。即SIE为0的时候，如果当程序在S态运行，将禁用全部中断。
intr_enable函数用于启用中断，通过设置SSTATUS_SIE标志位实现。
intr_disable函数用于禁用中断，通过清除SSTATUS_SIE标志位实现。

```c
static inline bool __intr_save(void) {//保存中断
    if (read_csr(sstatus) & SSTATUS_SIE) {//如果中断使能
        intr_disable();//禁止中断
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();//恢复中断
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

```
__intr_save函数用于保存中断状态。如果当前中断是使能状态，它会禁用中断，并返回1，表示中断被禁用。
__intr_restore函数用于根据保存的中断状态标志恢复中断。如果之前中断是被禁用的，它会启用中断。
然后，通过宏定义：
    local_intr_save(x)，它在一个do-while循环中调用__intr_save保存中断状态，并将结果存储在变量x中。
    local_intr_restore(x)，它调用__intr_restore根据之前保存的中断状态标志x来恢复中断状态。
（do { ... } while (0)主要目的是确保在使用宏的情况下，它可以像普通的语句一样正常工作。在 C 语言中，宏定义是一种简单的文本替换。然而，有时我们希望宏在使用时表现得像一个单独的语句，以避免潜在的问题）
这样，使用local_intr_save和local_intr_restore的地方，就能够在一段代码中临时禁用中断，执行一些关键操作，然后在恢复中断状态，确保中断状态不受这段代码的影响。








