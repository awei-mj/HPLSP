# I/O 复用

[TOC]

I/O复用使程序能同时监听多个文件描述符，对提高程序性能至关重要。通常网络程序需要使用I/O复用的情况:

- 客户端程序要同时处理多个socket。比如本章将要讨论的非阻塞connect技术
- 客户端程序要同时处理用户输入和网络连接。比如将要讨论的聊天室程序
- TCP服务器要同时处理监听socket和连接socket。I/O复用使用最多的场合
- 服务器要同时处理TCP和UDP请求，比如将要讨论的回射服务器
- 服务器要同时监听多个端口，或处理多种服务。比如将要讨论的xinetd服务器

I/O复用虽然能同时监听多个文件描述符，但它本身是阻塞的。并且当多个文件描述符同时就绪时，如果不采取额外的措施，程序就只能按顺序依次处理其中的每一个文件描述符，这使得服务器程序看起来像是串行工作的。如果要实现并发，只能使用多进程或多线程等编程手段。

Linux下实现I/O复用的系统调用主要有select、poll和epoll，本章将依次讨论并介绍几个实例。

## select系统调用

用途是：在一段指定时间内，监听用户感兴趣的文件描述符上的可读、可写和异常等事件。

### select API

```cpp
#include <sys/select.h>
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
```

1. nfds参数指定被监听的文件描述符的总数。它通常被设置为select监听的所有文件描述符中的最大值加1，因为文件描述符是从0开始计数的。
2. 中间3个参数分别指向可读、可写和异常等事件对应的文件描述符集合。应用程序调用select函数时，通过这3个参数传入感兴趣的文件描述符。调用返回时，内核将修改他们以告知应用程序哪些已就绪。fd_set结构体定义如下:

    ```cpp
    #include <typesizes.h>
    #define __FD_SETSIZE 1024
    #include <sys/select.h>
    #define FD_SETSIZE __FD_SETSIZE
    typedef long int __fd_mask;
    #undef __NFDBITS
    #define __NFDBITS (8*(int)sizeof(__fd_mask))
    typedef struct {
        #ifdef __USE_XOPEN
        __fd_mask fds_bits[__FD_SETSIZE / __NFDBITS];
        #define __FDS_BITS(set) ((set)->fds_bits)
        #else
        __fd_mask __fds_bits[__FD_SETSIZE / __NFDBITS];
        #define __FDS_BITS(set) ((set)->__fds_bits)
        #endif
    } fd_set;
    ```

   由以上定义可见，fd_set结构体仅包含一个整型数组，该数组的每个元素的每一位（bit）标记一个文件
描述符。fd_set能容纳的文件描述符数量由FD_SETSIZE指定，这就限制了select能同时处理的文件描述符的
总量。

   由于位操作过于烦琐，我们应该使用下面的一系列宏来访问fd_set结构体中的位：

    ```cpp
    #include <sys/select.h>
    FD_ZERO(fd_set *fdset); /*清除fdset的所有位*/
    FD_SET(int fd, fd_set *fdset); /*设置fdset的位fd*/
    FD_CLR(int fd, fd_set *fdset); /*清除fdset的位fd*/
    int FD_ISSET(int fd, fd_set *fdset); /*测试fdset的位fd是否被设置*/
    ```

3. timeout参数用于设置超时时间。采用指针是因为内核可以告知应用程序select等待了多久。但不能完全信任返回的timeout值，比如调用失败时该值不确定。如果都设置0，立即返回。如果传递NULL，将一直阻塞。

    ```cpp
    struct timeval {
        long tv_sec; /*秒数*/
        long tv_usec; /*微秒数*/
    }
    ```

4. 成功时返回就绪(可读、可写和异常)文件描述符的总数。超时期间内没有就绪的fd，返回0。select失败时返回-1并设置errno。等待期间收到信号，select立即返回-1，设errno为EINTR。

### 文件描述符就绪条件

哪些情况下文件描述符可被认为是可读:

- socket内核接收缓存区中的字节数大于或等于其低水位标记SO_RCVLOWAT。此时我们可以无阻塞地读该socket，并且读操作返回的字节数大于0。
- socket通信的对方关闭连接。此时对该socket的读操作将返回0。
- 监听socket上有新的连接请求。
- socket上有未处理的错误。此时我们可以使用getsockopt来读取和清除该错误。

哪些情况下文件描述符可被认为是可读:

- socket内核发送缓存区中的可用字节数大于或等于其低水位标记SO_SNDLOWAT。此时我们可以无阻塞地写该socket，并且写操作返回的字节数大于0。
- socket的写操作被关闭。对写操作被关闭的socket执行写操作将触发一个SIGPIPE信号。
- socket使用非阻塞connect连接成功或者失败(超时)之后。
- socket上有未处理的错误。此时我们可以使用getsockopt来读取和清除该错误。

网络程序中socket能处理的异常情况: 接收到带外数据。

### 处理带外数据

略

## poll系统调用

在指定时间内轮询一定数量的文件描述符，以测试其中是否有就绪者。

```cpp
#include <poll.h>
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

1. fds pollfd结构类型的数组，指定所有感兴趣的文件描述符上发生的可读、可写和异常事件

    ```c
    struct pollfd {
        int fd; //文件描述符
        short events; //注册的事件
        short revents; //实际发生的事件，由内核填充
    };
    ```

    events成员告知poll监听哪些事件，是一系列事件的按位或。

    | 事件       | 描述                                             | 是否可作为输入 | 是否可作为输出 |
    | ---------- | ------------------------------------------------ | :------------: | :------------: |
    | POLLIN     | 数据(包括普通数据和优先数据)可读                 |       T        |       T        |
    | POLLRDNORM | 普通数据可读                                     |       T        |       T        |
    | POLLRDBAND | 优先级带数据可读(Linux不支持)                    |       T        |       T        |
    | POLLPRI    | 高优先级数据可读，比如TCP带外数据                |       T        |       T        |
    | POLLOUT    | 数据(包括普通数据和优先数据)可写                 |       T        |       T        |
    | POLLWRNORM | 普通数据可写                                     |       T        |       T        |
    | POLLWRBAND | 优先级带数据可写                                 |       T        |       T        |
    | POLLRDIIUP | TCP连接被对方关闭，或对方关闭了写操作。由GNU引入 |       T        |       T        |
    | POLLERR    | 错误                                             |       F        |       T        |
    | POLLHUP    | 挂起。比如管道的写端被关闭后，读端将收到该事件   |       F        |       T        |
    | POLLNVAL   | 文件描述符没有打开                               |       F        |       T        |

    一些事件是将POLLIN和POLLOUT分得更细致，由XOPEN规范定义，但Linux不完全支持。

    通常需要根据recv返回的数据判断收到有效数据还是对方关闭连接，但kernel 2.6.17开始增加了POLLRDHUP事件。

2. nfds 被监听事件集合fds的大小，类型为`unsigned long int`
3. timeout参数指定poll的超时值，单位是毫秒。当timeout为-1时，poll调用将永远阻塞，直到某个事件发生；当timeout为0时，poll调用将立即返回。
4. poll系统调用的返回值的含义与select相同。

## epoll系列系统调用

### 内核事件表

### epoll_wait函数

### LT和ET模式

### EPOLLONESHOT事件

## 三组I/O复用事件的比较
