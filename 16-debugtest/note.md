# 服务器调制、调试和测试

[TOC]

从系统的角度来优化、改进服务器，这包括3个方面的内容：系统调制、服务器调试和压力测试

Linux平台的一个优秀特性是内核微调，即我们可以通过修改文件的方式来调整内核参数。

在服务器的开发过程中，我们可能碰到各种意想不到的错误。一种调试方法是用tcpdump抓包，正如本书前面章节介绍的那样。不过这种方法主要用于分析程序的输入和输出。对于服务器的逻辑错误，更方便的调试方法是使用gdb调试器。

编写压力测试工具通常被认为是服务器开发的一个部分。压力测试工具模拟现实世界中高并发的客户请求，以测试服务器在高压状态下的稳定性。

## 最大文件描述符数

文件描述符是服务器程序的宝贵资源，几乎所有的系统调用都是和文件描述符打交道。系统分配给应用程序的文件描述符数量是有限制的，所以我们必须总是关闭那些已经不再使用的文件描述符，以释放它们占用的资源。比如作为守护进程运行的服务器程序就应该总是关闭标准输入、标准输出和标准错误这3个文件描述符。

Linux对应用程序能打开的最大文件描述符数量有两个层次的限制：用户级限制和系统级限制。用户级限制是指目标用户运行的所有进程总共能打开的文件描述符数；系统级的限制是指所有用户总共能打开的文件描述符数。

```shell
# 最常用的查看用户级文件描述符数限制的方法:
> ulimit -n
# 将用户级文件描述符数限制设定为max-file-number:
> ulimit -SHn max-file-number
```

永久修改用户级文件描述符数限制，可以在/etc/security/limits.conf文件中加入如下两项：

> *hard nofile max-file-number
> \*soft nofile max-file-number

第一行是指系统的硬限制，第二行是软限制。见7.4节

要修改系统级文件描述符数限制，则可以使用如下命令: `sysctl-w fs.file-max=max-file-number`

永久更改系统级文件描述符数限制，则需要在/etc/sysctl.conf文件中添加如下一项:

> fs.file-max=max-file-number

然后通过执行`sysctl -p`命令使更改生效

## 调整内核参数

几乎所有的内核模块，包括内核核心模块和驱动程序，都在/proc/sys文件系统下提供了某些配置文件以供用户调整模块的属性和行为。通常一个配置文件对应一个内核参数，文件名就是参数的名字，文件的内容是参数的值。我们可以通过命令sysctl-a查看所有这些内核参数。本节将讨论其中和网络编程关系较为紧密的部分内核参数。

### /proc/sys/fs目录下的部分文件

该目录下的内核参数都与文件系统相关。对于服务器程序来说，其中最重要的是如下两个参数:

- /proc/sys/fs/file-max，系统级文件描述符数限制。直接修改这个参数和16.1节讨论的修改方法有相同的效果（不过这是临时修改）。一般修改/proc/sys/fs/file-max后，应用程序需要把/proc/sys/fs/node-max设置为新/proc/sys/fs/file-max值的3～4倍，否则可能导致i节点数不够用。
- /proc/sys/fs/epoll/max_user_watches，一个用户能够往epoll内核事件表中注册的事件的总量。它是指该用户打开的所有epoll实例总共能监听的事件数目，而不是单个epoll实例能监听的事件数目。往epoll内核事
件表中注册一个事件，在32位系统上大概消耗90字节的内核空间，在64位系统上则消耗160字节的内核空间。所以，这个内核参数限制了epoll使用的内核内存总量

### /proc/sys/net目录下的部分文件

内核中网络模块的相关参数都位于该目录下，其中和TCP/IP协议相关的参数主要位于如下三个子目录中: core、ipv4和ipv6。在前面的章节中，我们已经介绍过这些子目录下的很多参数的含义，现在再总结一下和服务器性能相关的部分参数。

- /proc/sys/net/core/somaxconn，指定listen监听队列里，能够建立完整连接从而进入ESTABLISHED状态的socket的最大数目。读者不妨修改该参数并重新运行代码清单5-3，看看其影响。
- /proc/sys/net/ipv4/tcp_max_syn_backlog，指定listen监听队列里，能够转移至ESTAB-LISHED或者SYN_RCVD状态的socket的最大数目。
- /proc/sys/net/ipv4/tcp_wmem，它包含3个值，分别指定一个socket的TCP写缓冲区的最小值、默认值和最大值。
- /proc/sys/net/ipv4/tcp_rmem，它包含3个值，分别指定一个socket的TCP读缓冲区的最小值、默认值和最大值。在代码清单3-6中，我们正是通过修改这个参数来改变接收通告窗口大小的。
- /proc/sys/net/ipv4/tcp_syncookies，指定是否打开TCP同步标签（syncookie）。同步标签通过启动cookie来防止一个监听socket因不停地重复接收来自同一个地址的连接请求（同步报文段），而导致listen监听队列溢出（所谓的SYN风暴）

除了通过直接修改文件的方式来修改这些系统参数外，我们也可以使用sysctl命令来修改它们。这两种修改方式都是临时的。永久的修改方法是在/etc/sysctl.conf文件中加入相应网络参数及其数值，并执行`sysctl -p`使之生效，就像修改系统最大允许打开的文件描述符数那样

## gdb调试

假设读者懂得基本的gdb调试方法，比如设置断点，查看变量等

### 调试多进程程序

如果一个进程通过fork系统调用创建了子进程，gdb会继续调试原来的进程，子进程则正常运行。那么该如何调试子进程呢？常用的方法有如下两种

1. 单独调试子进程

    ```shell
    > ./cgisrv 127.0.0.1 12345
    > ps -ef | grep cgisrv
    shuang 4182 3601 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4183 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4184 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4185 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4186 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4187 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4188 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4189 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    shuang 4190 4182 0 12:25 pts/4 00:00:00./cgisrv 127.0.0.1 12345
    > gdb
    (gdb)attach 4183/*将子进程4183附加到gdb调试器*/
    Attaching to process 4183
    Reading symbols from/home/shuang/codes/pool_process/cgisrv...done.
    Reading symbols from/usr/lib/libstdc++.so.6...Reading symbols from/usr/lib/debug/usr/lib/libstdc++.so.6.0.16.debug...done.
    done.
    Loaded symbols for/usr/lib/libstdc++.so.6
    Reading symbols from/lib/libm.so.6...(no debugging symbols found)...done.
    Loaded symbols for/lib/libm.so.6
    Reading symbols from/lib/libgcc_s.so.1...Reading symbols from/usr/lib/debug/lib/libgcc_s-4.6.2-20111027.so.1.debug...done.
    done.
    Loaded symbols for/lib/libgcc_s.so.1
    Reading symbols from/lib/libc.so.6...(no debugging symbols found)...done.
    Loaded symbols for/lib/libc.so.6
    Reading symbols from/lib/ld-linux.so.2...(no debugging symbols found)...done.
    Loaded symbols for/lib/ld-linux.so.2
    0x0047c416 in__kernel_vsyscall()
    (gdb)b processpool.h:264/*设置子进程中的断点*/
    Breakpoint 1 at 0x8049787:file processpool.h,line 264.
    (gdb)c
    Continuing.
    /*接下来从另一个终端使用telnet 127.0.0.1 12345来连接服务器并发送一些数据，调试器就按照我们预期的，在断点处暂停*/
    Breakpoint 1,processpool＜cgi_conn＞::run_child(this=0x9a47008)at processpool.h:264
    264 users[sockfd].process();
    (gdb)bt
    #0 processpool＜cgi_conn＞::run_child(this=0x9a47008)at processpool.h:264
    #1 0x080491fe in processpool＜cgi_conn＞::run(this=0x9a47008)at processpool.h:169
    #2 0x08048ef9 in main(argc=3,argv=0xbfbc0b74)at main.cpp:138
    (gdb)
    ```

2. 使用调试器选项follow-fork-mode

    该选项允许我们选择程序在执行fork系统调用后是继续调试父进程还是调试子进程。其用法如下: `(gdb)set follow-fork-mode mode`。其中，mode的可选值是parent和child，分别表示调试父进程和子进程。

    ```shell
    > gdb ./cgisrv
    (gdb)set follow-fork-mode child
    (gdb)b processpool.h:264
    Breakpoint 1 at 0x8049787:file processpool.h,line 264.
    (gdb)r 127.0.0.1 12345
    Starting program:/home/shuang/codes/pool_process/cgisrv 127.0.0.1 12345
    [New process 4148]
    send request to child 0
    [Switching to process 4148]
    Breakpoint 1,processpool＜cgi_conn＞::run_child(this=0x804c008)at processpool.h:264
    264 users[sockfd].process();
    Missing separate debuginfos,use:debuginfo-install glibc-2.14.90-24.fc16.6.i686
    (gdb)bt
    #0 processpool＜cgi_conn＞::run_child(this=0x804c008)at processpool.h:264
    #1 0x080491fe in processpool＜cgi_conn＞::run(this=0x804c008)at processpool.h:169
    #2 0x08048ef9 in main(argc=3,argv=0xbffff4e4)at main.cpp:138
    (gdb)
    ```

### 调试多线程程序

gdb有一组命令可辅助多线程程序的调试。下面我们仅列举其中常用的一些:

- info threads，显示当前可调试的所有线程。gdb会为每个线程分配一个ID，我们可以使用这个ID来操作对应的线程。ID前面有"*"号的线程是当前被调试的线程。
- thread ID，调试目标ID指定的线程。
- set scheduler-locking\[off|on|step]。调试多线程程序时，默认除了被调试的线程在执行外，其他线程也在继续执行，但有的时候我们希望只让被调试的线程运行。这可以通过这个命令来实现。该命令设置scheduler-locking的值：off表示不锁定任何线程，即所有线程都可以继续执行，这是默认值；on表示只有当前被调试的线程会继续执行；step表示在单步执行的时候，只有当前线程会执行。

举例来说，如果要依次调试Web服务器（名为websrv）的父线程和子线程:

```shell
> gdb ./websrv
(gdb)b main.cpp:130/*设置父线程中的断点*/
Breakpoint 1 at 0x80498d3:file main.cpp,line 130.
(gdb)b threadpool.h:105/*设置子线程中的断点*/
Breakpoint 2 at 0x804a10b:file threadpool.h,line 105.
(gdb)r 127.0.0.1 12345
Starting program:/home/webtop/codes/pool_thread/websrv 127.0.0.1 12345
[Thread debugging using libthread_db enabled]
Using host libthread_db library"/lib/libthread_db.so.1".
create the 0th thread
[New Thread 0xb7fe1b40(LWP 5756)]
/*从另一个终端使用telnet 127.0.0.1 12345来连接服务器并发送一些数据，调试器就按照我们预期的，在断点处暂停*/
Breakpoint 1,main(argc=3,argv=0xbffff4e4)at main.cpp:130
130 if(users[sockfd].read())
(gdb)info threads/*查看线程信息。当前被调试的是主线程，其ID为1*/
Id Target Id Frame
2 Thread 0xb7fe1b40(LWP 5756)"websrv"0x00111416 in__kernel_vsyscall()
*1 Thread 0xb7fe3700(LWP 5753)"websrv"main(argc=3,argv=0xbffff4e4)at main.cpp:130
(gdb)set scheduler-locking on/*不执行其他线程，锁定调试对象*/
(gdb)n/*下面的操作都将执行父线程的代码*/
132 pool-＞append(users+sockfd);
(gdb)n
103 for(int i=0;i＜number;i++)
(gdb)
94 while(true)
(gdb)
96 int number=epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
(gdb)
^C
Program received signal SIGINT,Interrupt.
0x00111416 in__kernel_vsyscall()
(gdb)thread 2/*将调试切换到子线程，其ID为2*/
[Switching to thread 2(Thread 0xb7fe1b40(LWP 5756))]
#0 0x00111416 in__kernel_vsyscall()
(gdb)bt/*显示子线程的调用栈*/
#0 0x00111416 in__kernel_vsyscall()
#1 0x44d91c05 in sem_wait@@GLIBC_2.1()from/lib/libpthread.so.0
#2 0x08049aff in sem::wait(this=0x804e034)at locker.h:24
#3 0x0804a0db in threadpool＜http_conn＞::run(this=0x804e008)at threadpool.h:98
#4 0x08049f8f in threadpool＜http_conn＞::worker(arg=0x804e008)at threadpool.h:89
#5 0x44d8bcd3 in start_thread()from/lib/libpthread.so.0
#6 0x44cc8a2e in clone()from/lib/libc.so.6
(gdb)n/*下面的操作都将执行子线程的代码*/
Single stepping until exit from function__kernel_vsyscall,
which has no line number information.
0x44d91c05 in sem_wait@@GLIBC_2.1()from/lib/libpthread.so.0
(gdb)
```

关于调试进程池和线程池程序的一个不错的方法，是先将池中的进程个数或线程个数减少至1，以观察程序的逻辑是否正确，比如上面就是这样做的；然后逐步增加进程或线程的数量，以调试进程或线程的同步是否正确。

## 压力测试

压力测试程序有很多种实现方式，比如I/O复用方式，多线程、多进程并发编程方式，以及这些方式的结合使用。不过，单纯的I/O复用方式的施压程度是最高的，因为线程和进程的调度本身也是要占用一定CPU时间的。因此，我们将使用epoll来实现一个通用的服务器压力测试程序。见`stress_test.cpp`

上述内容应该有问题，CPU单核有性能上限，单纯使用单线程进行压力测试会导致压力不足。且创建socket连接时需要逐个创建，较为耗时。

```shell
> ./websrv 192.168.1.108 12345 #在ernest-laptop上执行，监听端口12345
> ./stress_test 192.168.1.108 12345 1000 #在Kongming20上执行
```
