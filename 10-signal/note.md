# 信号

[TOC]

信号是由用户、系统或者进程发送给目标进程的信息，以通知目标进程某个状态的改变或系统异常。Linux信号可由如下条件产生：

- 对前台进程可通过输入特殊终端字符发送信号。比如输入`Ctrl+C`通常会发送一个中断信号
- 系统异常 比如浮点异常和非法内存段访问
- 系统状态变化 比如alarm时钟到期将引起SIGALARM信号
- 运行kill命令或调用kill函数

服务器必须处理(或至少忽略)一些常见信号，避免异常终止。

本章先讨论如何在程序中发送和处理信号，然后讨论Linux支持的信号种类，并详细探讨与网络编程相关的几个。

## Linux信号概述

### 发送信号

给其他进程发送信号的API是kill函数。

```cpp
#include <sys/type.h>
#include <signal.h>
int kill(pid_t pid, int sig);
```

1. pid 目标进程号

    |pid参数|含义|
    |-|-|
    |pid > 0|信号发送给PID为pid的进程|
    |pid = 0|发送给本进程组内其他进程|
    |pid = -1|发送给除init进程外所有进程，但发送者需要有对应权限|
    |pid < -1|发送给组ID为-pid的进程组的所有成员|

2. sig 信号值，为0则不发送任何信号
3. 成功返回0，失败返回-1并设errno

    |errno|含义|
    |-|-|
    |EINVAL|无效的信号|
    |EPERM|没有权限发送给任何一个目标进程|
    |ESRCH|目标进程或进程组不存在|

### 信号处理方式

收到信号需要定义接收函数处理。

```cpp
#include <signal.h>
typedef void (*__sighandler_t)(int);
```

整型参数指示信号类型。处理函数应该是可重入的，所有严禁调用不安全的函数。

忽略信号和信号的默认处理方式:

```cpp
#include <bits/signum.h>
#define SIG_DFL ((__sighandler_t)0)
#define SIG_IGN ((__sighandler_t)1)
```

默认处理方式种类: 结束进程(Term)、忽略信号(Ign)、结束进程并产生核心转储文件(Core)、暂停进程(Stop)、继续进程(Cont)

### Linux信号

可用信号都定义在`<bits/signum.h>`中，包括标准信号和POSIX实时信号。本书仅讨论标准信号

| 信号      | 起源 | 默认行为 | 含义 |
| --------- | ---- | -------- | ---- |
| SIGHUP    | POSIX | Term    | 控制终端挂起 |
| SIGINT    | ANSI | Term     | 键盘输入以中断进程(Ctrl+C) |
| SIGQUIT   | POSIX | Core    | 键盘输入使进程推出(Ctrl+\\) |
| SIGILL    | ANSI | Core     | 非法指令 |
| SIGTRAP   | POSIX | Core    | 断点陷阱，用于调试 |
| SIGABRT   | ANSI | Core     | 进程调用abort函数时生成该信号 |
| SIGIOT    | 4.2BSD | Core   | 和SIGABRT相同 |
| SIGBUS    | 4.2BSD | Core   | 总线错误，错误内存访问 |
| SIGFPE    | ANSI | Core     | 浮点异常 |
| SIFKILL   | POSIX | Term    | 终止一个进程，该信号不可被捕获或忽略 |
| SIGUSR1   | POSIX | Term    | 用户自定义信号一 |
| SIGSEGV   | ANSI | Core     | 非法内存段引用 |
| SIGUSR2   | POSIX | Term    | 用户自定义信号二 |
| SIGPIPE   | POSIX | Term    | 往读端被关闭的管道或socket连接中写数据 |
| SIGALRM   | POSIX | Term    | 由alarm或setitimer设置的实时闹钟超时引起 |
| SIGTERM   | ANSI | Term     | 终止进程，kill命令默认发送的信号 |
| SIGSTKFLT | Linux | Term    | 早期的Linux使用该信号来报告数学协处理器栈错误 |
| SIGCLD    | System V | Ign  | 和SIGCHLD相同 |
| SIGCHLD   | POSIX | Ign     | 子进程状态发生变化(退出或暂停) |
| SIGCONT   | POSIX | Cont    | 启动被暂停的进程(Ctrl+Q) ，如果不处于暂停状态则忽略|
| SIGSTOP   | POSIX | Stop    | 暂停进程(Ctrl+S)，不可被捕获或忽略 |
| SIGTSTP   | POSIX | Stop    | 挂起进程(Ctrl+Z) |
| SIGTTIN   | POSIX | Stop    | 后台进程试图从终端读取输入 |
| SIGTTOU   | POSIX | Stop    | 后台进程试图从终端输出内容 |
| SIGURG    | 4.2BSD | Ign    | socket连接上收到紧急数据 |
| SIGXCPU   | 4.2BSD | Core   | 进程的CPU使用时间超过其软限制 |
| SIGXFSZ   | 4.2BSD | Core   | 文件尺寸超过其软限制 |
| SIGVTALRM | 4.2BSD | Term   | 与SIGALRM类似，但只统计本进程用户空间代码的运行时间 |
| SIGPROF   | 4.2BSD | Term   | 与SIGALRM类似，同时统计用户代码和内核的运行时间 |
| SIGWINCH  | 4.3BSD | Ign    | 终端窗口大小发生变化 |
| SIGPOLL   | System V | Term | 与SIGIO类似 |
| SIGIO     | 4.2BSD | Term   | IO就绪，比如socket上发生可读、可写事件。TCP服务器可触发SIGIO的条件很多，故无法在TCP服务器中使用。可用在UDP服务器中，但也非常少见 |
| SIGPWR    | System V | Term | 对使用UPS的系统，当电池电量过低时，该信号被触发 |
| SIGSYS    | POSIX | Core    | 非法系统调用 |
| SIGUNUSED |      | Core     | 保留，通常和SIGSYS效果相同 |

### 中断系统调用

如果程序在执行处于阻塞状态的系统调用时接收到信号，并且我们为该信号设置了信号处理函数，则默认情况下系统调用将被中断，并且errno被设置为EINTR。我们可以使用sigaction函数（见后文）为信号设置SA_RESTART标志以自动重启被该信号中断的系统调用。

对于默认行为是暂停进程的信号（比如SIGSTOP、SIGTTIN），如果我们没有为它们设置信号处理函数，则它们也可以中断某些系统调用（比如connect、epoll_wait）。POSIX没有规定这种行为，这是Linux独有的。

## 信号函数

### signal系统调用

为一个信号设置处理函数

```cpp
#include <signal.h>
_sighandler_t signal(int sig, _sighandler_t _handler);
```

- sig 信号类型
- _handler_t 处理函数指针
- 返回前一次调用的_handler或SIG_DFL
- 出错返回SIG_ERR，并设置errno

### sigaction系统调用

更健壮的接口

```cpp
#include <signal.h>
int sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
```

- sig 信号类型
- act 新信号处理方式
- oact 旧处理方式(不为NULL的话)
- struct sigaction

    ```cpp
    struct sigaction {
        #ifdef __USE_POSIX 199309
        union {
            _sighandler_t sa_handler;
            void (*sa_sigaction)(int, siginfo_t*, oid*);
        } _sigaction_handler;
        #define sa_handler __sigaction_handler.sa_handler
        #define sa_sigaction __sigaction_handler.sa_sigaction
        #else
        _sighandler_t sa_handler;
        #endif
        _sigset_t sa_mask;
        int sa_flags;
        void (*sa_restorer)(void);
    };
    ```

  - sa_handler 指定信号处理函数
  - sa_mask 设置进程的信号掩码(在原有信号掩码的基础上增加信号掩码)，是信号集(_sigset_t)类型，以后介绍
  - sa_flags 设置程序收到信号时的行为

    |选项|含义|
    |-|-|
    |SA_NOCLDSTOP|如果sig参数是SIGCHILD，则子进程暂停时不生成SIGCHILD信号|
    |SA_NOCLDWAIT|如果sig参数是SIGCHILD，则子进程结束时不生成僵尸进程|
    |SA_SIGINFO|使用sa_sigaction作为信号处理函数，能给进程提供更多相关信息|
    |SA_ONSTACK|调用由sigaltstack函数设置的可选信号栈上的信号处理函数|
    |SA_RESTART|重新调用被该信号终止的系统调用|
    |SA_NODEFER|当接收到信号并进入其信号处理函数时，不屏蔽该信号。默认情况下不允许以防止竞态条件|
    |SA_RESETHAND|信号处理函数执行完后，恢复该信号的默认处理方式|
    |SA_INTERRUPT|中断系统调用|
    |SA_NOMASK|同SA_NODEFER|
    |SA_ONESHOT|同SA_RESETHAND|
    |SA_STACK|同SA_ONSTACK|

  - sa_restorer 已过时，最好别用
- 成功返回0，失败返回-1并设置errno

## 信号集

### 信号集函数

Linux使用sigset_t表示一组信号，实际为位图。并提供了操作信号集的函数。

```cpp
#include <bits/sigset.h>
#define _SIGSET_NWORDS (1024/(8*sizeof(unsigned long int)))
typedef struct {
    unsigned long int __val[_SIGSET_NWORDS];
} __sigset_t;

#include <signal.h>
int sigemptyset(sigset_t *_set); //清空信号集
int sigfillset(sigset_t *_set); //设置所有信号
int sigaddset(sigset_t *_set, int _signo); //添加至信号集
int sigdelset(sigset_t *_set, int _signo); //从信号集删除
int sigismember(const sigset_t *_set, int _signo); //测试是否在信号集中
```

### 进程信号掩码

设置或查看进程的信号掩码

```cpp
#include <signal.h>
int sigprocmask(int _how, const sigset_t *_set, sigset_t *_oset);
```

- _set、_oset 新旧信号掩码，可以分别传NULL
- _how 设置掩码方式
  - SIG_BLOCK 求并集
  - SIG_UNBLOCK 求与`~_set`的交集，即_set指定的不被屏蔽
  - SIG_SETMASK 直接设置
- 成功返回0，失败-1并设置errno

### 被挂起的信号

如果给进程发送一个被屏蔽的信号，则操作系统将该信号设置为进程的一个被挂起的信号。如果我们取消对被挂起信号的屏蔽，则它能立即被进程接收到。如下函数可以获得进程当前被挂起的信号集：

```cpp
#include <signal.h>
int sigpending(sigset_t *set);
```

多次接收被挂起的信号只反映一次，取消屏蔽后也只触发一次。

成功返回0，失败返回-1并设置errno。

要始终清楚地知道进程在每个运行时刻的信号掩码，以及如何适当地处理捕获到的信号。在多进程、多线程环
境中，我们要以进程、线程为单位来处理信号和信号掩码。我们不能设想新创建的进程、线程具有和父进程、主线程完全相同的信号特征。比如，fork调用产生的子进程将继承父进程的信号掩码，但具有一个空的挂起信号集。

## 统一事件源

信号是一种异步事件：信号处理函数和程序的主循环是两条不同的执行路线。很显然，信号处理函数需要尽可能快地执行完毕，以确保该信号不被屏蔽（前面提到过，为了避免一些竞态条件，信号在处理期间，系统不会再次触发它）太久。一种典型的解决方案是：把信号的主要处理逻辑放到程序的主循环中，当信号处理函数被触发时，它只是简单地通知主循环程序接收到信号，并把信号值传递给主循环，主循环再根据接收到的信号值执行目标信号对应的逻辑代码。信号处理函数通常使用管道来将信号“传递”给主循环：信号处理函数往管道的写端写入信号值，主循环则从管道的读端读出该信号值。那么主循环怎么知道管道上何时有数据可读呢?这很简单，我们只需要使用I/O复用系统调用来监听管道的读端文件描述符上的可读事件。如此一来，信号事件就能和其他I/O事件一样被处理，即统一事件源。

很多优秀的I/O框架库和后台服务器程序都统一处理信号和I/O事件，比如Libevent I/O框架库和xinetd超
级服务。`ues.cpp`给出了统一事件源的一个简单实现。

## 网络编程相关信号

### SIGHUP

当挂起进程的控制终端时，SIGHUP信号将被触发。对于没有控制终端的网络后台程序而言，它们通常利用SIGHUP信号来强制服务器重读配置文件。

### SIGPIPE

默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号。我们需要在代码中捕获并处理该信号，或者至少忽略它，因为程序接收到SIGPIPE信号的默认行为是结束进程，而我们绝对不希望因为错误的写操作而导致程序退出。引起SIGPIPE信号的写操作将设置errno为EPIPE。

第5章提到，我们可以使用send函数的MSG_NOSIGNAL标志来禁止写操作触发SIGPIPE信号。在这种情况下，我们应该使用send函数反馈的errno值来判断管道或者socket连接的读端是否已经关闭。

此外，我们也可以利用I/O复用系统调用来检测管道和socket连接的读端是否已经关闭。以poll为例，当管道的读端关闭时，写端文件描述符上的POLLHUP事件将被触发；当socket连接被对方关闭时，socket上的POLLRDHUP事件将被触发。

### SIGURG

在Linux环境下，内核通知应用程序带外数据到达主要有两种方法：一种是第9章介绍的I/O复用技术，select等系统调用在接收到带外数据时将返回，并向应用程序报告socket上的异常事件；另一种就是使用SIGURG信号。
