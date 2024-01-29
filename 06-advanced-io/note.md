# 高级I/O

[TOC]

不常用但特定条件下性能优秀的高级I/O函数:

- 用于创建文件描述符的函数，包括pipe、dup/dup2函数
- 用于读写数据的函数，包括readv/writev、sendfile、mmap/munmap、splice和tee函数
- 用于控制I/O行为和属性的函数，包括fcntl

## pipe函数

用于创建管道，实现进程间通信

```c
#include <unistd.h>
int pipe(int fd[2]); //成功返回0，并将一对打开的文件描述符填入数组；失败返回-1,设置errno
```

两个文件描述符构成管道的两端，往`fd[1]`写入的数据可以从`fd[0]`读出，且单向。如果要双向就建立两个管道。默认情况下一对文件描述符都是阻塞的，即读空与写满时会阻塞。若都设置为非阻塞会有不同行为。如果写端引用计数变为0，read操作将返回0，即读到EOF；反之写端的write操作将失败，并引发SIGPIPE信号。

管道内传输的数据是字节流，类似TCP。socket的基础API中的socketpair函数可以创建双向管道。

```c
#include <sys/types.h>
#include <sys/socket.h>

int socketpair(int domain, int type, int protocol, int fd[2]);
```

domain只能使用UNIX本地域协议族AF_UNIX，故仅能在本地使用，且既可读又可写。成功时返回0，失败时返回-1且设置errno。

## dup与dup2函数

将标准输入重定向到一个文件或将标准输出重定向到网络连接。

```c
#include <unistd.h>

int dup(int file_descriptor);
int dup2(int fd1, int fd2);
```

dup函数创建一个新的文件描述符，该新文件描述符和原有文件描述符file_descriptor指向相同的文件、管道或者网络连接，且返回值总是取系统当前可用的最小整数值。dup2和dup类似，不过它将返回第一个不小于fd2的整数值。失败时返回-1并设置errno。

用法:

```c
close(STDOUT_FILENO);
dup(connfd);
```

## readv函数和writev函数

分散读、集中写

```c
#include <sys/uio.h>
ssize_t readv(int fd, const struct iovec *vector, int count);
ssize_t writev(int fd, const struct iovec *vector, int count);
```

count是vector数组的长度，返回处理的字节数，失败返回-1,设置errno。相当于简化版的recvmsg和sendmsg。

HTTP应答的状态行、头部字段、空行和文档的内容可以使用writev函数一起写出。

## sendfile函数

在两个文件描述符之间直接传递数据（完全在内核中操作），从而避免内核缓冲区和用户缓冲区的数据拷贝，成为零拷贝。

```c
#include <sys/sendfile.h>
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```

offset代表从哪个位置开始读，count代表字节数。成功返回传输的字节数，失败返回-1并设errno。其中in_fd必须是支持类似mmap函数的描述符，即必须指向真实文件，不能是socket和管道；out_fd必须是一个socket。也就是说该函数几乎专门用于在网络上传输文件。

## mmap函数和munmap函数

mmap函数用于申请一段内存空间，可作为进程间通信的共享内存，也可将文件直接映射到其中。munmap用于释放该空间。

```c
#include <sys/mman.h>
void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset_t);
int munmap(void *start, size_t length);
```

- start允许用户使用某个特定地址作为起始地址，如果被设置为NULL则自动分配
- length指定长度
- prot用于设置访问权限，可取以下几个值的按位或:
  - PROT_READ 可读
  - PROT_WRITE 可写
  - PROT_EXEC 可执行
  - PROC_NONE 不能被访问
- flags参数控制内存段内容被修改后程序的行为，可取以下几个值的按位或:
  - MAP_SHARED 在进程间共享这段内存；或对内存段的修改将反映到被映射文件中
  - MAP_PRIVATE 私有、不会反映，与上条互斥
  - MAP_ANONYMOUS 不是从文件映射而来，内容被初始化为全0，最后两个参数将被忽略
  - MAP_FIXED 内存段必须位于start参数的指定地址处，start必须是内存页面大小(4096字节)的整数倍
  - MAP_HUGETLB 按照大内存页面分配空间，大小可通过`/proc/mcminfo`文件查看
- fd参数是被映射文件的对应描述符，一般通过open系统调用获得
- offset指定映射起始位置

mmap函数成功时返回目标区域指针，失败返回MAP_FAILED((void *)-1)，并设置errno

munmap函数成功时返回0，失败返回-1并设置errno

## splice函数

用于在两个文件描述符间移动数据，也是零拷贝操作。

```c
#include <fcntl.h>
ssize_t splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);
```

- fd_in 输入数据文件描述符，如果是管道，off_in必须被设置为NULL
- off_in 偏移量，为NULL表示当前位置
- fd_out、off_out与前两个参数意思相同
- len 移动数据的长度
- flags 控制标志，可为以下几个值的按位或:
  - SPLICE_F_MOVE 若合适移动页面而非拷贝，仅为给内核的提示，kernel 2.6.21后无效果，目前6.7应该可用
  - SPLICE_F_NONBLOCK 非阻塞，实际会受文件描述符本身阻塞状态影响
  - SPLICE_F_MORE 给内核的提示:后续的splice调用会读更多数据
  - SPLICE_F_GIFT 对SPLICE无效果
- fd_in和fd_out至少一个是管道文件描述符。成功时返回移动字节的数量，可能返回0；失败返回-1并设置errno，常见的errno值
  - EBADF 文件描述符有错
  - EINVAL 目标文件系统不支持splice，或目标文件以append模式打开，或两个都不是管道，或某个offset被用于不支持随机访问的设备
  - ENOMEM 内存不够
  - ESPIPE 管道文件描述符的偏移不是NULL

## tee函数

在两个管道文件描述符间复制数据，也是零拷贝。不消耗数据因此依然可以读。

```c
#include <fcntl.h>
ssize_t tee(int fd_in, int fd_out, size_t len, unsigned int flags);
```

与splice相似

## fcntl函数

能够实现对文件描述符的控制

```c
#include<fcntl.h>

int fcntl(int fd, int cmd, ... /* int arg */);
// 出错返回-1,成功时返回值依赖于cmd
```

1. 复制一个已有的描述符
   1. F_DUPFD 创建新fd，值大于等于`long arg`，返回fd
   2. F_DUPFD_CLOEXEC 同上，设置`close_on_exec`
2. 获取/设置文件描述符标志
   1. F_GETFD 获取标志
   2. F_SETFD 根据`long arg`设置标志
3. 获取/设置文件状态标志
   1. F_GETFL 获得状态标志
   2. F_SETFL 设置可更改的状态标志
4. 管理信号
   1. F_GETOWN 获得SIGIO和SIGURG信号的宿主进程的PID或进程组的组ID
   2. F_SETOWN 设置SIGIO和SIGURG信号的宿主进程的PID或进程组的组ID
   3. F_GETSIG 获取当应用程序被通知fd可读或可写时，由哪个信号通知，返回信号值(0表示SIGIO)
   4. F_SETSIG 设置当应用程序被通知fd可读或可写时，由哪个信号通知
5. 操作管道容量
   1. F_SETPIPE_SZ 设置由fd指定的管道的容量 `/proc/sys/fs/pipe-size-max`内核参数指定了容量上限，成功返回0
   2. F_SETPIPE_SZ 获取容量

在网络编程中，该函数常用于将一个文件描述符设为非阻塞的

```c
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);/*获取文件描述符旧的状态标志*/
    int new_option = old_option | O_NONBLOCK;/*设置非阻塞标志*/
    fcntl(fd, F_SETFL, new_option);
    return old_option;/*返回文件描述符旧的状态标志，以便*/
    /*日后恢复该状态标志*/
}
```

SIGIO和SIGURG两信号必须与某文件描述符关联才可使用: 可读或可写时出发SIGIO，socket上有oob数据可读时触发SIGURG。关联方法为使用该函数为fd指定宿主进程或进程组，被指定的进程或进程组将捕获这两个信号。使用SIGIO时还需要设置O_ASYNC标志。
