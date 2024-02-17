# 多线程编程

[TOC]

本书所有线程相关的例程使用的线程库都是NPTL。

本章要讨论的线程相关的内容都属于POSIX线程（简称pthread）标准，而不局限于NPTL实现，具体包括:

- 创建线程和结束线程。
- 读取和设置线程属性。
- POSIX线程同步方式：POSIX信号量、互斥锁和条件变量。

在本章的最后，我们还将介绍在Linux环境下，库函数、进程、信号与多线程程序之间的相互影响。

## Linux线程概述

### 线程模型

线程的实现方式可分为三种模式：完全在用户空间实现、完全由内核调度和双层调度（two level scheduler）。

完全在用户空间实现的线程无须内核的支持，内核甚至根本不知道这些线程的存在。线程库负责管理所有执行线程，比如线程的优先级、时间片等。线程库利用longjmp来切换线程的执行，使它们看起来像是“并发”执行的。但实际上内核仍然是把整个进程作为最小单位来调度的。换句话说，一个进程的所有执行线程共享该进程的时间片，它们对外表现出相同的优先级。因此，对这种实现方式而言，N=1，即M个用户空间线程对应1个内核线程，而该内核线程实际上就是进程本身。完全在用户空间实现的线程的优点是：创建和调度线程都无须内核的干预，因此速度相当快。并且由于它不占用额外的内核资源，所以即使一个进程创建了很多线程，也不会对系统性能造成明显的影响。其缺点是：对于多处理器系统，一个进程的多个线程无法运行在不同的CPU上，因为内核是按照其最小调度单位来分配CPU的。此外，线程的优先级只对同一个进程中的线程有效，比较不同进程中的线程的优先级没有意义。早期的伯克利UNIX线程就是采用这种方式实现的。

完全由内核调度的模式将创建、调度线程的任务都交给了内核，运行在用户空间的线程库无须执行管理任务，这与完全在用户空间实现的线程恰恰相反。二者的优缺点也正好互换。较早的Linux内核对内核线程的控制能力有限，线程库通常还要提供额外的控制能力，尤其是线程同步机制，不过现代Linux内核已经大大增强了对线程的支持。完全由内核调度的这种线程实现方式满足M:N=1:1，即1个用户空间线程被映射为1个内核线程。

双层调度模式是前两种实现模式的混合体：内核调度M个内核线程，线程库调度N个用户线程。这种线程实现方式结合了前两种方式的优点：不但不会消耗过多的内核资源，而且线程切换速度也较快，同时它可以充分利用多处理器的优势。

### Linux线程库

现代Linux上默认使用的线程库是NPTL。用户可以使用如下命令来查看当前系统上所使用的线程库:

```shell
> getconf GNU_LIBPTHREAD_VERSION
NPTL 2.39
```

LinuxThreads线程库的内核线程是用clone系统调用创建的进程模拟的。clone系统调用和fork系统调用的作用类似：创建调用进程的子进程。不过我们可以为clone系统调用指定CLONE_THREAD标志，这种情况下它创建的子进程与调用进程共享相同的虚拟地址空间、文件描述符和信号处理函数，这些都是线程的特点。不过，用进程来模拟内核线程会导致很多语义问题，比如:

- 每个线程拥有不同的PID，因此不符合POSIX规范。
- Linux信号处理本来是基于进程的，但现在一个进程内部的所有线程都能而且必须处理信号。
- 用户ID、组ID对一个进程中的不同线程来说可能是不一样的。
- 程序产生的核心转储文件不会包含所有线程的信息，而只包含产生该核心转储文件的线程的信息。
- 由于每个线程都是一个进程，因此系统允许的最大进程数也就是最大线程数。

LinuxThreads线程库一个有名的特性是所谓的管理线程。它是进程中专门用于管理其他工作线程的线程。其作用包括:

- 系统发送给进程的终止信号先由管理线程接收，管理线程再给其他工作线程发送同样的信号以终止它们。
- 当终止工作线程或者工作线程主动退出时，管理线程必须等待它们结束，以避免僵尸进程。
- 如果主线程先于其他工作线程退出，则管理线程将阻塞它，直到所有其他工作线程都结束之后才唤醒它。
- 回收每个线程堆栈使用的内存。

管理线程的引入，增加了额外的系统开销。并且由于它只能运行在一个CPU上，所以LinuxThreads线程库也不能充分利用多处理器系统的优势。

要解决LinuxThreads线程库的一系列问题，不仅需要改进线程库，最主要的是需要内核提供更完善的线程支持。因此，Linux内核从2.6版本开始，提供了真正的内核线程。新的NPTL线程库也应运而生。相比LinuxThreads，NPTL的主要优势在于:

- 内核线程不再是一个进程，因此避免了很多用进程模拟内核线程导致的语义问题。
- 摒弃了管理线程，终止线程、回收线程堆栈等工作都可以由内核来完成。
- 由于不存在管理线程，所以一个进程的线程可以运行在不同的CPU上，从而充分利用了多处理器系统的优势。
- 线程的同步由内核来完成。隶属于不同进程的线程之间也能共享互斥锁，因此可实现跨进程的线程同步。

## 创建线程和结束线程

下面讨论创建和结束进程的基础API。Linux上它们都定义在`<pthread.h>`中

1. pthread_create 创建一个线程

    ```cpp
    #include <pthread.h>
    int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
    #include <bits/pthreadtypes.h>
    typedef unsigned long int pthread_t;
    ```

    - thread 新线程标识符，是整型类型。几乎所有Linux的资源标识符都是整型数，如socket、各种System V IPC标识符等
    - attr 设置新线程的属性，传递NULL表示使用默认线程属性。线程拥有许多属性，将在后面讨论
    - start_routine 新线程将运行的函数
    - arg 即将运行的函数的参数
    - 成功时返回0，失败时返回错误码
    - 一个用户可以打开的线程数量不能超过RLIMIT_NPROC软资源限制（见表7-1）。此外，系统上所有用户能创建的线程总数也不得超过`/proc/sys/kernel/threads-max`内核参数所定义的值。

2. pthread_exit

    线程一旦被创建好，内核就可以调度内核线程来执行start_routine函数指针所指向的函数了。线程函数在结束时最好调用如下函数，以确保安全、干净地退出:

    ```cpp
    #include <pthread.h>
    void pthread_exit(void *retval);
    ```

    pthread_exit函数通过retval参数向线程的回收者传递其退出信息。它执行完之后不会返回到调用者，而且永远不会失败。

3. pthread_join

    所有线程都可以调用pthread_join函数来回收其他线程（前提是目标线程是可回收的，见后文），即等待其他线程结束，这类似于回收进程的wait和waitpid系统调用。pthread_join的定义如下:

    ```cpp
    #include <pthread.h>
    int pthread_join(pthread_t thread, void **retval);
    ```

    - thread 目标线程的标识符
    - retval 目标线程返回的退出信息
    - 该函数会一直阻塞，直到被回收的线程结束为止
    - 该函数成功时返回0，失败则返回错误码。

    |错误码|描述|
    |-|-|
    |EDEADLK|可能引起死锁，比如两个线程互相调用join，或一个线程对自己调用join|
    |EINVAL|目标线程不可回收，或已有其他线程在回收该线程|
    |ESRCH|目标线程不存在|

4. pthread_cancel

    有时候我们希望异常终止一个线程，即取消线程

    ```cpp
    #include <pthread.h>
    int pthread_cancel(pthread_t thread);
    ```

    - thread 目标线程的标识符
    - 该函数成功时返回0，失败则返回错误码
    - 不过，接收到取消请求的目标线程可以决定是否允许被取消以及如何取消，这分别由如下两个函数完成:

        ```cpp
        #include <pthread.h>
        int pthread_setcancelstate(int state,int *oldstate);
        int pthread_setcanceltype(int type,int *oldtype);
        ```

      - 第一个参数分别用于设置线程的取消状态（是否允许取消）和取消类型（如何取消）
      - 第二个参数则分别记录线程原来的取消状态和取消类型。
      - state参数有两个可选值:
        - PTHREAD_CANCEL_ENABLE，允许线程被取消。它是线程被创建时的默认取消状态。
        - PTHREAD_CANCEL_DISABLE，禁止线程被取消。这种情况下，如果一个线程收到取消请求，则它会将请求挂起，直到该线程允许被取消。
      - type参数也有两个可选值:
        - PTHREAD_CANCEL_ASYNCHRONOUS，线程随时都可以被取消。它将使得接收到取消请求的目标线程立即采取行动。
        - PTHREAD_CANCEL_DEFERRED，允许目标线程推迟行动，直到它调用了下面几个所谓的取消点函数中的一个: pthread_join、pthread_testcancel、pthread_cond_wait、pthread_cond_timedwait、sem_wait和sigwait。根据POSIX标准，其他可能阻塞的系统调用，比如read、wait，也可以成为取消点。不过为了安全起见，我们最好在可能会被取消的代码中调用pthread_testcancel函数以设置取消点。
      - pthread_setcancelstate和pthread_setcanceltype成功时返回0，失败则返回错误码。

## 线程属性

pthread_attr_t结构体定义了一套完整的线程属性:

```cpp
#include <bits/pthreadtypes.h>
#define __SIZEOF_PTHREAD_ATTR_T 56
typedef union {
    char __size[__SIZEOF_PTHREAD_ATTR_T];
    long int __align;
} pthread_attr_t;
```

可见，各种线程属性全部包含在一个字符数组中。线程库定义了一系列函数来操作pthread_attr_t类型的变量，以方便我们获取和设置线程属性:

```cpp
#include <pthread.h>
/*初始化线程属性对象*/
int pthread_attr_init(pthread_attr_t *attr);
/*销毁线程属性对象。被销毁的线程属性对象只有再次初始化之后才能继续使用*/
int pthread_attr_destroy(pthread_attr_t *attr);
/*下面这些函数用于获取和设置线程属性对象的某个属性*/
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate);
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate);
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr);
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr);
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize);
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize);
int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize);
int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize);
int pthread_attr_getguardsize(const pthread_attr_t *__attr ,size_t *guardsize);
int pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize);
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param);
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param);
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy);
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy);
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit);
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit);
int pthread_attr_getscope(const pthread_attr_t *attr,int *scope);
int pthread_attr_setscope(pthread_attr_t *attr, int scope);
```

每个线程属性的含义:

- detachstate，线程的脱离状态。它有PTHREAD_CREATE_JOINABLE和PTHREAD_CREATE_DETACH两个可选值。前者指定线程是可以被回收的，后者使调用线程脱离与进程中其他线程的同步。脱离了与其他线程同步的线程称为“脱离线程”。脱离线程在退出时将自行释放其占用的系统资源。线程创建时该属性的默认值是PTHREAD_CREATE_JOINABLE。此外，我们也可以使用pthread_detach函数直接将线程设置为脱离线程
- stackaddr和stacksize，线程堆栈的起始地址和大小。一般来说，我们不需要自己来管理线程堆栈，因为Linux默认为每个线程分配了足够的堆栈空间（一般是8 MB）。我们可以使用`ulimit -s`命令来查看或修改这个默认值
- guardsize，保护区域大小。如果guardsize大于0，则系统创建线程的时候会在其堆栈的尾部额外分配guardsize字节的空间，作为保护堆栈不被错误地覆盖的区域。如果guardsize等于0，则系统不为新创建的线程设置堆栈保护区。如果使用者通过pthread_attr_setstackaddr或pthread_attr_setstack函数手动设置线程的堆栈，则guardsize属性将被忽略
- schedparam，线程调度参数。其类型是sched_param结构体。该结构体目前还只有一个整型类型的成员——sched_priority，该成员表示线程的运行优先级
- schedpolicy，线程调度策略。该属性有SCHED_FIFO、SCHED_RR和SCHED_OTHER三个可选值，其中SCHED_OTHER是默认值，不支持优先级。SCHED_RR表示采用轮转算法（round-robin）调度，SCHED_FIFO表示使用先进先出的方法调度，这两种调度方法都具备实时调度功能，但只能用于以超级用户身份运行的进程
- inheritsched，是否继承调用线程的调度属性。该属性有PTHREAD_INHERIT_SCHED和PTHREAD_EXPLICIT_SCHED两个可选值。前者表示新线程沿用其创建者的线程调度参数，这种情况下再设置新线程的调度参数属性将没有任何效果。后者表示调用者要明确地指定新线程的调度参数
- scope，线程间竞争CPU的范围，即线程优先级的有效范围。POSIX标准定义了该属性的PTHREAD_SCOPE_SYSTEM和PTHREAD_SCOPE_PROCESS两个可选值，前者表示目标线程与系统中所有线程一起竞争CPU的使用，后者表示目标线程仅与其他隶属于同一进程的线程竞争CPU的使用。目前Linux只支持PTHREAD_SCOPE_SYSTEM这一种取值

## POSIX信号量

接下来我们讨论3种专门用于线程同步的机制: POSIX信号量、互斥量和条件变量。

在Linux上，信号量API有两组。一组是第13章讨论过的System V IPC信号量，另外一组是我们现在要讨论的POSIX信号量。这两组接口很相似，但不保证能互换。

POSIX信号量函数的名字都以sem_开头，并不像大多数线程函数那样以pthread_开头。常用的POSIX信号量函数是下面5个:

```cpp
#include <semaphore.h>

int sem_init(sem_t *sem, int pshared, unsigned int value);
int sem_destroy(sem_t *sem);
int sem_wait(sem_t *sem);
int sem_trywait(sem_t *sem);
int sem_post(sem_t *sem);
```

- sem 指向被操作的信号量
- sem_init 用于初始化一个未命名的信号量（POSIX信号量API支持命名信号量，不过本书不讨论它）
  - pshared 指定信号量的类型。如果其值为0，就表示这个信号量是当前进程的局部信号量，否则该信号量就可以在多个进程之间共享
  - value参数指定信号量的初始值。此外，初始化一个已经被初始化的信号量将导致不可预期的结果
- sem_destroy函数用于销毁信号量，以释放其占用的内核资源。如果销毁一个正被其他线程等待的信号量，则将导致不可预期的结果。
- sem_wait函数以原子操作的方式将信号量的值减1。如果信号量的值为0，则sem_wait将被阻塞，直到这个信号量具有非0值。
- sem_trywait与sem_wait函数相似，不过它始终立即返回，而不论被操作的信号量是否具有非0值，相当于sem_wait的非阻塞版本。当信号量的值非0时，sem_trywait对信号量执行减1操作。当信号量的值为0时，它将返回-1并设置errno为EAGAIN。
- sem_post函数以原子操作的方式将信号量的值加1。当信号量的值大于0时，其他正在调用sem_wait等待信号量的线程将被唤醒。
- 上面这些函数成功时返回0，失败则返回-1并设置errno。

## 互斥锁

互斥锁（也称互斥量）可以用于保护关键代码段，以确保其独占式的访问，这有点像一个二进制信号量。当进入关键代码段时，我们需要获得互斥锁并将其加锁，这等价于二进制信号量的P操作；当离开关键代码段时，我们需要对互斥锁解锁，以唤醒其他等待该互斥锁的线程，这等价于二进制信号量的V操作。

### 互斥锁基础API

```cpp
#include <pthread.h>
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);
```

- mutex 指向要操作的目标互斥锁，互斥锁的类型是pthread_mutex_t结构体
- init 用于初始化互斥锁。mutexattr参数指定互斥锁的属性。如果将它设置为NULL，则表示使用默认属性。互斥锁的属性将在下一小节讨论。除了这个函数外，还可以使用如下方式来初始化一个互斥锁: `pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;`该宏实际上只是把互斥锁的各个字段都初始化为0
- destroy 用于销毁互斥锁，以释放其占用的内核资源。销毁一个已经加锁的互斥锁将导致不可预期的后果
- lock 以原子操作的方式给一个互斥锁加锁。如果已经锁上，则阻塞直到被解锁
- trylock 非阻塞版本，立即返回，如果已被锁则返回错误码EBUSY。这两个lock函数是针对普通锁而言的。后面我们将看到，对于其他类型的锁而言，这两个加锁函数会有不同的行为
- unlock 解锁，如果其他线程在等待，则会获得该锁
- 成功时返回0，失败则返回错误码

### 互斥锁属性

pthread_mutexattr_t结构体定义了一套完整的互斥锁属性。线程库提供了一系列函数来操作pthread_mutexattr_t类型的变量，以方便我们获取和设置互斥锁属性。这里我们列出其中一些主要的函数:

```cpp
#include <pthread.h>
/*初始化互斥锁属性对象*/
int pthread_mutexattr_init(pthread_mutexattr_t*attr);
/*销毁互斥锁属性对象*/
int pthread_mutexattr_destroy(pthread_mutexattr_t*attr);
/*获取和设置互斥锁的pshared属性*/
int pthread_mutexattr_getpshared(const pthread_mutexattr_t*attr,int*pshared);
int pthread_mutexattr_setpshared(pthread_mutexattr_t*attr,int pshared);
/*获取和设置互斥锁的type属性*/
int pthread_mutexattr_gettype(const pthread_mutexattr_t*attr,int*type);
int pthread_mutexattr_settype(pthread_mutexattr_t*attr,int type);
```

- pshared 指定是否允许跨进程共享互斥锁，其可选值有两个:
  - PTHREAD_PROCESS_SHARED。互斥锁可以被跨进程共享。
  - PTHREAD_PROCESS_PRIVATE。互斥锁只能被和锁的初始化线程隶属于同一个进程的线程共享。
- type 指定互斥锁的类型。Linux支持如下4种类型的互斥锁:
  - PTHREAD_MUTEX_NORMAL，普通锁。这是互斥锁默认的类型。当一个线程对一个普通锁加锁以后，其余请求该锁的线程将形成一个等待队列，并在该锁解锁后按优先级获得它。这种锁类型保证了资源分配的公平性。但这种锁也很容易引发问题：一个线程如果对一个已经加锁的普通锁再次加锁，将引发死锁；对一个已经被其他线程加锁的普通锁解锁，或者对一个已经解锁的普通锁再次解锁，将导致不可预期的后果。
  - PTHREAD_MUTEX_ERRORCHECK，检错锁。一个线程如果对一个已经加锁的检错锁再次加锁，则加锁操作返回EDEADLK。对一个已经被其他线程加锁的检错锁解锁，或者对一个已经解锁的检错锁再次解锁，则解锁操作返回EPERM。
  - PTHREAD_MUTEX_RECURSIVE，嵌套锁。这种锁允许一个线程在释放锁之前多次对它加锁而不发生死锁。不过其他线程如果要获得这个锁，则当前锁的拥有者必须执行相应次数的解锁操作。对一个已经被其他线程加锁的嵌套锁解锁，或者对一个已经解锁的嵌套锁再次解锁，则解锁操作返回EPERM。
  - PTHREAD_MUTEX_DEFAULT，默认锁。一个线程如果对一个已经加锁的默认锁再次加锁，或者对一个已经被其他线程加锁的默认锁解锁，或者对一个已经解锁的默认锁再次解锁，将导致不可预期的后果。这种锁在实现的时候可能被映射为上面三种锁之一。

### 死锁举例

死锁使得一个或多个线程被挂起而无法继续执行。略

## 条件变量

如果说互斥锁是用于同步线程对共享数据的访问的话，那么条件变量则是用于在线程之间同步共享数据的值。条件变量提供了一种线程间的通知机制：当某个共享数据达到某个值的时候，唤醒等待这个共享数据的线程。

```cpp
#include <pthread.h>
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *cond_attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
```

- cond 指向要操作的目标条件变量，类型是pthread_cond_t结构体
- init函数 用于初始化条件变量。cond_attr参数指定条件变量的属性。如果设置为NULL，则表示使用默认属性。条件变量的属性不多，而且和互斥锁的属性类型相似，故我们不再赘述。除了pthread_cond_init函数外，我们还可以使用如下方式来初始化一个条件变量:`pthread_cond_t cond = PTHREAD_COND_INITIALIZER;`该宏实际上只是把条件变量的各个字段都初始化为0。
- destroy函数用于销毁条件变量，以释放其占用的内核资源。销毁一个正在被等待的条件变量将失败并返回EBUSY
- broadcast函数 以广播的方式唤醒所有等待目标条件变量的线程
- signal函数 用于唤醒一个等待目标条件变量的线程。至于哪个线程将被唤醒，则取决于线程的优先级和调度策略。有时候我们可能想唤醒一个指定的线程，但pthread没有对该需求提供解决方法。不过我们可以间接地实现该需求：定义一个能够唯一表示目标线程的全局变量，在唤醒等待条件变量的线程前先设置该变量为目标线程，然后采用广播方式唤醒所有等待条件变量的线程，这些线程被唤醒后都检查该变量以判断被唤醒的是否是自己，如果是就开始执行后续代码，如果不是则返回继续等待。
- wait函数用于等待目标条件变量。mutex参数是用于保护条件变量的互斥锁，以确保pthread_cond_wait操作的原子性。在调用pthread_cond_wait前，必须确保互斥锁mutex已经加锁，否则将导致不可预期的结果。pthread_cond_wait函数执行时，首先把调用线程放入条件变量的等待队列中，然后将互斥锁mutex解锁。可见，从pthread_cond_wait开始执行到其调用线程被放入条件变量的等待队列之间的这段时间内，pthread_cond_signal和pthread_cond_broadcast等函数不会修改条件变量。换言之，pthread_cond_wait函数不会错过目标条件变量的任何变化[^7]。当pthread_cond_wait函数成功返回时，互斥锁mutex将再次被锁上。
- 上面这些函数成功时返回0，失败则返回错误码。

## 线程同步机制包装类

为了充分复用代码，同时由于后文的需要，将前面讨论的3种线程同步机制分别封装成3个类，实现在locker.hpp文件中

## 多线程环境

### 可重入函数

如果一个函数能被多个线程同时调用且不发生竞态条件，则我们称它是线程安全的（thread safe），或者说它是可重入函数。Linux库函数只有一小部分是不可重入的，比如5.1.4小节讨论的inet_ntoa函数，以及5.12.2小节讨论的getservbyname和getservbyport函数。关于Linux上不可重入的库函数的完整列表，请读者参考相关书籍，这里不再赘述。这些库函数之所以不可重入，主要是因为其内部使用了静态变量。不过Linux对很多不可重入的库函数提供了对应的可重入版本，这些可重入版本的函数名是在原函数名尾部加上_r。比如，函数localtime对应的可重入函数是localtime_r。在多线程程序中调用库函数，一定要使用其可重入版本，否则可能导致预想不到的结果。

### 线程和进程

如果一个多线程程序的某个线程调用了fork函数，那么新创建的子进程是否将自动创建和父进程相同数量的线程呢？答案是否。子进程只拥有一个执行线程，它是调用fork的那个线程的完整复制。并且子进程将自动继承父进程中互斥锁（条件变量与之类似）的状态。也就是说，父进程中已经被加锁的互斥锁在子进程中也是被锁住的。这就引起了一个问题：子进程可能不清楚从父进程继承而来的互斥锁的具体状态（是加锁状态还是解锁状态）。这个互斥锁可能被加锁了，但并不是由调用fork函数的那个线程锁住的，而是由其他线程锁住的。见fork_deadlock.cpp

pthread提供了一个专门的函数pthread_atfork，以确保fork调用后父进程和子进程都拥有一个清楚的锁状态:

```cpp
#include <pthread.h>
int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void));
```

该函数将建立3个fork句柄来帮助我们清理互斥锁的状态:

- prepare句柄将在fork调用创建出子进程之前被执行。它可以用来锁住所有父进程中的互斥锁
- parent句柄则是fork调用创建出子进程之后，而fork返回之前，在父进程中被执行。它的作用是释放所有在prepare句柄中被锁住的互斥锁
- child句柄是fork返回之前，在子进程中被执行。和parent句柄一样，child句柄也是用于释放所有在prepare句柄中被锁住的互斥锁。
- 该函数成功时返回0，失败则返回错误码

### 线程和信号

每个线程都可以独立地设置信号掩码，10.3.2小节讨论过设置进程信号掩码的函数sigprocmask，多线程环境下我们应该使用pthread_sigmask函数来设置进程信号掩码:

```cpp
#include <pthread.h>
#include <signal.h>
int pthread_sigmask(int how, const sigset_t *newmask, sigset_t *oldmask);
```

含义与sigpromask的参数完全相同，故不再赘述。成功返回0，失败返回错误码。

由于进程中的所有线程共享该进程的信号，所以线程库将根据线程掩码决定把信号发送给哪个具体的线程。因此，如果我们在每个子线程中都单独设置信号掩码，就很容易导致逻辑错误。此外，所有线程共享信号处理函数。也就是说，当我们在一个线程中设置了某个信号的信号处理函数后，它将覆盖其他线程为同一个信号设置的信号处理函数。这两点都说明，我们应该定义一个专门的线程来处理所有的信号。这可以通过如下两个步骤来实现:

1. 在主线程创建出其他子线程之前就调用pthread_sigmask来设置好信号掩码，所有新创建的子线程都将自动继承这个信号掩码。这样做之后，实际上所有线程都不会响应被屏蔽的信号了。
2. 在某个线程中调用如下函数来等待信号并处理之:

    ```cpp
    #include <signal.h>
    int sigwait(const sigset_t *set, int *sig);
    ```

- set 指定需要等待的信号的集合。我们可以简单地将其指定为在第1步中创建的信号掩码，表示在该线程中等待所有被屏蔽的信号。
- sig 指向用于存储该函数返回的信号值
- sigwait成功时返回0，失败则返回错误码。一旦sigwait正确返回，我们就可以对接收到的信号做处理了。很显然，如果我们使用了sigwait，就不应该再为信号设置信号处理函数了。这是因为当程序接收到信号时，二者中只能有一个起作用。

最后，pthread还提供了下面的方法，使得我们可以明确地将一个信号发送给指定的线程:

```cpp
#include <signal.h>
int pthread_kill(pthread_t thread, int sig);
```

- thread 指定目标线程
- sig 指定待发送的信号。如果sig为0，则pthread_kill不发送信号，但它任然会执行错误检查。我们可以利用这种方式来检测目标线程是否存在
- pthread_kill成功时返回0，失败则返回错误码

[^7]: Bradford Nichols，Dick Buttlar，Jackie Farrell. Pthreads Programming\[M].Sebastopol，CA：O'Reilly＆Associates，Inc.，1996
