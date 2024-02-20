# 系统监测工具

[TOC]

Linux提供了很多有用的工具，以方便开发人员调试和测评服务器程序。娴熟的网络程序员在开发服务器程序的整个过程中，都将不断地使用这些工具中的一个或者多个来监测服务器行为。某些工具更是黑客们常用的利器。

## tcpdump

转储网络流量，网络抓包。打印满足表达式的数据包信息

```shell
tcpdump [ "-AbdDefhHIJKlLnNOpqStuUvxX#" ] [ -B buffer_size ]
        [ -c count ] [ --count ] [ -C file_size ]
        [ -E spi@ipaddr algo:secret,...  ]
        [ -F file ] [ -G rotate_seconds ] [ -i interface ]
        [ --immediate-mode ] [ -j tstamp_type ] [ -m module ]
        [ -M secret ] [ --number ] [ --print ] [ -Q in|out|inout ]
        [ -r file ] [ -s snaplen ] [ -T type ] [ --version ]
        [ -V file ] [ -w file ] [ -W filecount ] [ -y datalinktype ]
        [ -z postrotate-command ] [ -Z user ]
        [ --time-stamp-precision=tstamp_precision ]
        [ --micro ] [ --nano ]
        [ expression ]
```

- -w 重定向输出到文件
- -r 从文件读取
- -V 读取文件列表
- -c 抓取的数据包数量
- -e 显示以太网帧头部信息
- -i interface 指定网络接口，如eth0
- -n 不解释域名和服务名
- -v verbose output
- -x 十六进制
- -X 十六进制+ASCII字符
- -XX 与-X相同，并打印以太网帧头部信息

expression:

- 类型 解释后面紧跟参数的含义
  - host 主机名(IP地址)
  - net CIDR法表示的网络地址
  - port 端口号
  - portrange 端口范围
  - e.g. 抓取整个1.2.3.0/255.255.255.0网络上的数据包 `tcpdump net 1.2.3.0/24`
- 方向
  - src 发送端
  - dst 目的端
  - e.g. 抓取进入端口13579的数据包 `tcpdump dst port 13579`
- 协议 指定目标协议 e.g. `tcpdump icmp`
- 支持逻辑操作符进行复杂运算，如and(&&), or(||), not(!)和括号(需要用单引号包裹或使用转义字符)
- 还允许直接使用数据包中的部分协议字段的内容来过滤数据包。比如，仅抓取TCP同步报文段，可使用如下命令: `tcpdump 'tcp[13]&2!=0'`(TCP头部第14个字节的第2位是同步标志)或`tcpdump 'tcp[tcpflags]&tcp-syn!=0'`

## lsof

列出当前系统打开的文件描述符的工具。通过它我们可以了解感兴趣的进程打开了哪些文件描述符，或者我们感兴趣的文件描述符被哪些进程打开了。

- -i，显示socket文件描述符。该选项的使用方法是:`lsof -i[46] [protocol] [@hostname|ipaddr] [:service|port]`
  - 4表示IPv4协议，6表示IPv6协议
  - protocol指定传输层协议，可以是TCP或者UDP
  - hostname指定主机名
  - ipaddr指定主机的IP地址
  - service指定服务名
  - port指定端口号
  - 比如，要显示所有连接到主机192.168.1.108的ssh服务的socket文件描述符，可以使用命令:`lsof -i @192.168.1.108:22`
  - 如果-i选项后不指定任何参数，则lsof命令将显示所有socket文件描述符。
- -u，显示指定用户启动的所有进程打开的所有文件描述符。
- -c，显示指定的命令打开的所有文件描述符。比如要查看websrv程序打开了哪些文件描述符，可以使用如下命令: `lsof -c websrv`
- -p，显示指定进程打开的所有文件描述符
- -t，仅显示打开了目标文件描述符的进程的PID
- 我们还可以直接将文件名作为lsof命令的参数，以查看哪些进程打开了该文件。

下面介绍一个实例：查看websrv服务器打开了哪些文件描述符。

```shell
$ps -ef | grep websrv #先获取websrv程序的进程号
awei        7272    4459  0 15:29 pts/2    00:00:00 ./webserver 0.0.0.0 1234
awei        7360    7281  0 15:29 pts/3    00:00:00 grep --color=auto --exclude-dir=.bzr --exclude-dir=CVS --exclude-dir=.git --exclude-dir=.hg --exclude-dir=.svn --exclude-dir=.idea --exclude-dir=.tox webserver
$sudo lsof -p 6346 #用-p选项指定进程号
lsof: WARNING: can't stat() fuse.portal file system /run/user/1000/doc
      Output information may be incomplete.
COMMAND    PID USER  FD      TYPE DEVICE SIZE/OFF     NODE NAME
webserver 7272 awei cwd       DIR  259,8     4096 32114164 /home/awei/Desktop/Code/vscode/hplsp/15-pool/target
webserver 7272 awei rtd       DIR  259,8     4096        2 /
webserver 7272 awei txt       REG  259,8   116904 32114207 /home/awei/Desktop/Code/vscode/hplsp/15-pool/target/webserver
webserver 7272 awei mem       REG  259,8  1948952 24264162 /usr/lib/libc.so.6
webserver 7272 awei mem       REG  259,8   728120 24264228 /usr/lib/libgcc_s.so.1
webserver 7272 awei mem       REG  259,8   960856 24264188 /usr/lib/libm.so.6
webserver 7272 awei mem       REG  259,8 21512744 24287086 /usr/lib/libstdc++.so.6.0.32
webserver 7272 awei mem       REG  259,8   220248 24264148 /usr/lib/ld-linux-x86-64.so.2
webserver 7272 awei   0u      CHR  136,2      0t0        5 /dev/pts/2
webserver 7272 awei   1u      CHR  136,2      0t0        5 /dev/pts/2
webserver 7272 awei   2u      CHR  136,2      0t0        5 /dev/pts/2
webserver 7272 awei   3u     IPv4 387796      0t0      TCP *:search-agent (LISTEN)
webserver 7272 awei   4u  a_inode   0,15        0     2102 [eventpoll:3]
```

lsof命令的输出内容相当丰富，其中每行内容都包含如下字段:

- COMMAND，执行程序所使用的终端命令（默认仅显示前9个字符）。
- PID，文件描述符所属进程的PID。
- USER，拥有该文件描述符的用户的用户名。
- FD，文件描述符的描述。其中cwd表示进程的工作目录，rtd表示用户的根目录，txt表示进程运行的程序代码，mem表示直接映射到内存中的文件（本例中都是动态库）。有的FD是以"数字+访问权限"表示的，其中数字是文件描述符的具体数值，访问权限包括r（可读）、w（可写）和u（可读可写）。在本例中，0u、1u、2u分别表示标准输入、标准输出和标准错误输出；3u表示处于LISTEN状态的监听socket；4u表示epoll内核事件表对应的文件描述符
- TYPE，文件描述符的类型。其中DIR是目录，REG是普通文件，CHR是字符设备文件，IPv4是IPv4类型的socket文件描述符，0000是未知类型。更多文件描述符的类型请参考lsof命令的man手册，这里不再赘述。
- DEVICE，文件所属设备。对于字符设备和块设备，其表示方法是"主设备号，次设备号"。由上文输出可见，测试机器上的程序文件和动态库都存放在设备"259,8"中。其中，"259"表示这是一个NVME硬盘(BLKEXT?)；"8"表示这是该硬盘上的第8个分区，即/dev/nvme1n1p1(我有两块硬盘，/dev/nvme0n1的从设备号是0,/dev/nvme0n1p1-/dev/nvme0n1p6为1-6,/dev/nvme1n1为7)。websrv程序的标准输入、标准输出和标准错误输出对应的设备是"136,2"。其中，"136"表示这是一个伪终端；"2"表示它是第2个伪终端，即/dev/pts/2。关于设备编号的更多细节，请参考[文档](http://www.kernel.org/pub/linux/docs/lanana/device-list/devices-2.6.txt)。对于FIFO类型的文件，比如管道和socket，该字段将显示一个内核引用目标文件的地址，或者是其i节点号。
- SIZE/OFF，文件大小或者偏移值。如果该字段显示为"0t*"或者"0x*"，就表示这是一个偏移值，否则就表示这是一个文件大小。对字符设备或者FIFO类型的文件定义文件大小没有意义，所以该字段将显示一个偏移值。
- NODE，文件的i节点号。对于socket，则显示为协议类型，比如"TCP"
- NAME，文件的名字
如果我们使用telnet命令向websrv服务器发起一个连接，则再次执行代码清单17-1中的lsof命令时，其输
出将多出如下一行:

```shell
webserver 7272 awei   5u     IPv4 430350      0t0      TCP localhost:search-agent->localhost:41342 (ESTABLISHED)
```

该输出表示服务器打开了一个IPv4类型的socket，其值是5，且它处于ESTABLISHED状态。该socket对
应的连接的本端socket地址是(127.0.0.1，1234)，远端socket地址则是(127.0.0.1，41342)。

## nc

nc（netcat）命令短小精干、功能强大，有着"瑞士军刀"的美誉。它主要被用来快速构建网络连接。我们可以让它以服务器方式运行，监听某个端口并接收客户连接，因此它可用来调试客户端程序。我们也可以使之以客户端方式运行，向服务器发起连接并收发数据，因此它可以用来调试服务器程序，此时它有点像telnet程序。

本机/该书使用的是OpenBSD的netcat，版本号为1.226

常用的选项包括：

- -4 使用IPv4
- -6 使用IPv6
- -i 设置数据包传送的时间间隔。
- -l 以服务器方式运行，监听指定的端口。nc命令默认以客户端方式运行。
- -k 重复接受并处理某个端口上的所有连接，必须与-l选项一起使用。
- -n 使用IP地址表示主机，而不是主机名；使用数字表示端口号，而不是服务名称。
- -p 当nc命令以客户端方式运行时，强制其使用指定的端口号。3.4.2小节中我们就曾使用过该选项。
- -s 设置本地主机发送出的数据包的IP地址。
- -C 将CR和LF两个字符作为行结束符。
- -U 使用UNIX本地域协议通信。
- -u 使用UDP协议。nc命令默认使用的传输层协议是TCP协议。
- -w 如果nc客户端在指定的时间内未检测到任何输入，则退出。
- -X 当nc客户端和代理服务器通信时，该选项指定它们之间使用的通信协议。目前nc支持的代理协议包括"4"(SOCKS v.4)，"5"(SOCKS v.5)和"connect"(HTTPS proxy)。nc默认使用的代理协议是SOCKS v.5
- -x 指定目标代理服务器的IP地址和端口号。比如，要从其他机器连接到本机上的clash代理服务器，并通过它来访问www.baidu.com的Web服务，可以使用如下命令:

    `> nc -x 192.168.1.10:7890 -X connect www.baidu.com 80`

- -z 扫描目标机器上的某个或某些服务是否开启（端口扫描）。比如，要扫描机器192.168.1.10上端口号在20～50之间的服务，可以使用如下命令:

    `> nc -z 192.168.1.10 20-50`

举例来说，我们可以使用如下方式来连接websrv服务器并向它发送数据:

```shell
> nc -C 127.0.0.1 1234 #监听端口是1234
GET http://localhost/a.html HTTP/1.1（回车）
Host:localhost（回车）
（回车）
HTTP/1.1 404 NOT FOUND
Content-Length:49
Connection:close

The requested file was not found on this server.
```

这里我们使用了-C选项，这样每次我们按下回车键向服务器发送一行数据时，nc客户端程序都会给服务器额外发送一个＜CR＞＜LF＞，而这正是websrv服务器期望的HTTP行结束符。发送完第三行数据之后，我们得到了服务器的响应，内容正是我们期望的：服务器没有找到被请求的资源文件a.html。可见，nc命令是一个很方便的快速测试工具，通过它我们能很快找出服务器的逻辑错误。

nc结束的太快，可能会收不到应答。

## strace

测试服务器性能的重要工具。它跟踪程序运行过程中执行的系统调用和接收到的信号，并将系统调用名、参数、返回值及信号名输出到标准输出或者指定的文件。

常用选项:

- -c，统计每个系统调用执行时间、执行次数和出错次数。
- -f，跟踪由fork调用生成的子进程。
- -t，在输出的每一行信息前加上时间信息。
- -e，指定一个表达式，用来控制如何跟踪系统调用（或接收到的信号，下同）。其格式是:

    `[qualifier=][!]value1[,value2]...`

  - qualifier可以是trace、abbrev、verbose、raw、signal、read和write中之一，默认是trace
  - value是用于进一步限制被跟踪的系统调用的符号或数值。它的两个特殊取值是all和none，分别表示跟踪所有由qualifier指定类型的系统调用和不跟踪任何该类型的系统调用。关于value的其他取值，我们简单地列举一些:
    - -e trace=set，只跟踪指定的系统调用。例如，-e trace=open，close，read，write表示只跟踪open、close、read和write这四种系统调用。
    - -e trace=file，只跟踪与文件操作相关的系统调用。
    - -e trace=process，只跟踪与进程控制相关的系统调用。
    - -e trace=network，只跟踪与网络相关的系统调用。
    - -e trace=signal，只跟踪与信号相关的系统调用。
    - -e trace=ipc，只跟踪与进程间通信相关的系统调用。
    - -e signal=set，只跟踪指定的信号。比如，-e signal=!SIGIO表示跟踪除SIGIO之外的所有信号。
    - -e read=set，输出从指定文件中读入的数据。例如，-e read=3，5表示输出所有从文件描述符3和5读入的数据。
- -o，将strace的输出写入指定的文件。

strace命令的每一行输出都包含这些字段:系统调用名称、参数和返回值。比如下面的示例:

```shell
> strace cat /dev/null
openat(AT_FDCWD, "/dev/null", O_RDONLY) = 3
```

这行输出表示：程序"cat/dev/null"在运行过程中执行了openat系统调用。open调用以只读的方式打开了文件/dev/null，然后返回了一个值为3的文件描述符。输出中次要信息被省略

系统调用发生错误时，strace命令将输出错误标识和描述，比如下面的示例:

```shell
> strace cat /foo/bar
openat(AT_FDCWD, "/foo/bar", O_RDONLY)  = -1 ENOENT (没有那个文件或目录)
```

strace命令对不同的参数类型将有不同的输出方式，比如:

- 对于C风格的字符串，strace将输出字符串的内容。默认的最大输出长度是32字节，过长的部分strace会使用"..."省略。比如，ls -l命令在运行过程中将读取/etc/passwd文件:

    ```shell
    > strace ls -l
    read(4, "root:x:0:0::/root:/usr/bin/zsh\nb"..., 4096) = 1946
    ```

    需要注意的是，文件名并不被strace当作C风格的字符串，其内容总是被完整地输出。

- 对于结构体，strace将用"{}"输出该结构体的每个字段，并用","将每个字段隔开。对于字段较多的结构体，strace将用"..."省略部分输出。比如:

    ```shell
    $strace ls -l /dev/null
    lstat64("/dev/null",{st_mode=S_IFCHR|0666,st_rdev=makedev(1,3),...})=0
    ```

    上面的strace输出显示，lstat64系统调用的第1个参数是字符串输入参数"/dev/null"；第二个参数则是stat结构体类型的输出参数（指针），strace仅显示了该结构体参数的两个字段：st_mode和st_rdev。需要注意的是，当系统调用失败时，输出参数将显示为传入前的值。

- 对于位集合参数（比如信号集类型sigset_t），strace将用"[]"输出该集合中所有被置1的位，并用空格
将每一项隔开。假设某个程序中有如下代码:

    ```cpp
    sigset_t set;
    sigemptyset(＆set);
    sigaddset(＆set,SIGQUIT);
    sigaddset(＆set,SIGUSR1);
    sigprocmask(SIG_BLOCK,＆set,NULL);
    ```

    则针对该程序的strace命令将输出如下内容：`rt_sigprocmask(SIG_BLOCK,[QUIT USR1],NULL,8)=0`

    针对其他参数类型的输出方式，请读者参考strace的man手册，这里不再赘述

- 对于程序接收到的信号，strace将输出该信号的值及其描述。比如，我们在一个终端上运行"sleep 100"命令，然后在另一个终端上使用strace命令跟踪该进程，接着用"Ctrl+C"终止"sleep 100"进程以观察strace的输出。具体操作如下:

    ```shell
    > sleep 100
    > ps -ef | grep sleep
    awei       13674   12966  0 17:43 pts/2    00:00:00 sleep 100
    > strace -p 13674
    strace: Process 13674 attached
    restart_syscall(<... resuming interrupted read ...>) = ? ERESTART_RESTARTBLOCK (Interrupted by signal)
    --- SIGINT {si_signo=SIGINT, si_code=SI_KERNEL} ---
    +++ killed by SIGINT +++
    ```

下面考虑一个使用strace命令的完整、具体的例子：查看websrv服务器在处理客户连接和数据时使用系统调用的情况。具体操作如下:

```shell
> ./webserver 0.0.0.0 12345
> ps -ef | grep websrv
awei       13864   12966  0 17:46 pts/2    00:00:00 ./webserver 0.0.0.0 12345
> sudo strace -p 13864
strace: Process 13864 attached
epoll_wait(4, 
```

可见，服务器当前正在执行epoll_wait系统调用以等待客户请求。值得注意的是，epoll_wait的第一个参数（标识epoll内核事件表的文件描述符）的值是4，这和前面lsof命令的输出一致。接下来用浏览器对服务器发起一个连接并发送HTTP请求，此时strace命令的输出如下所示:

```shell
epoll_wait(4, [{events=EPOLLIN, data={u32=3, u64=3}}], 10000, -1) = 1
accept(3, {sa_family=AF_INET, sin_port=htons(40794), sin_addr=inet_addr("127.0.0.1")}, [16]) = 5
setsockopt(5, SOL_SOCKET, SO_REUSEADDR, [1], 4) = 0
epoll_ctl(4, EPOLL_CTL_ADD, 5, {events=EPOLLIN|EPOLLRDHUP|EPOLLONESHOT|EPOLLET, data={u32=5, u64=5}}) = 0
fcntl(5, F_GETFL)                       = 0x2 (flags O_RDWR)
fcntl(5, F_SETFL, O_RDWR|O_NONBLOCK)    = 0
epoll_wait(4, [{events=EPOLLIN, data={u32=3, u64=3}}], 10000, -1) = 1
accept(3, {sa_family=AF_INET, sin_port=htons(40798), sin_addr=inet_addr("127.0.0.1")}, [16]) = 6
setsockopt(6, SOL_SOCKET, SO_REUSEADDR, [1], 4) = 0
epoll_ctl(4, EPOLL_CTL_ADD, 6, {events=EPOLLIN|EPOLLRDHUP|EPOLLONESHOT|EPOLLET, data={u32=6, u64=6}}) = 0
fcntl(6, F_GETFL)                       = 0x2 (flags O_RDWR)
fcntl(6, F_SETFL, O_RDWR|O_NONBLOCK)    = 0

epoll_wait(4, [{events=EPOLLIN, data={u32=5, u64=5}}], 10000, -1) = 1
recvfrom(5, "GET /index.html HTTP/1.1\r\nHost: "..., 2048, 0, NULL, NULL) = 731
recvfrom(5, 0x7c7c6340483f, 1317, 0, NULL, NULL) = -1 EAGAIN (资源暂时不可用)
futex(0x5e58b7b88300, FUTEX_WAKE_PRIVATE, 1) = 1

epoll_wait(4, [{events=EPOLLOUT, data={u32=5, u64=103732050132997}}], 10000, -1) = 1
writev(5, [{iov_base="HTTP/1.1 200 OK\r\nContent-Length:"..., iov_len=57}, {iov_base="<!DOCTYPE html>\n<html>\n<head>\n<m"..., iov_len=157}], 2) = 214
munmap(0x7c7c7665c000, 157)             = 0
epoll_ctl(4, EPOLL_CTL_MOD, 5, {events=EPOLLIN|EPOLLRDHUP|EPOLLONESHOT|EPOLLET, data={u32=5, u64=5}}) = 0
epoll_ctl(4, EPOLL_CTL_DEL, 5, NULL)    = 0
close(5)                                = 0

epoll_wait(4, [{events=EPOLLIN|EPOLLRDHUP, data={u32=6, u64=6}}], 10000, -1) = 1
epoll_ctl(4, EPOLL_CTL_DEL, 6, NULL)    = 0
close(6)                                = 0
epoll_wait(4, 
```

上面的输出分为四个部分，我们用空行将每个部分隔开。

第一部分从第一次epoll_wait系统调用开始。此次epoll_wait调用检测到了文件描述符3上的EPOLLIN事件。从代码清单17-1中lsof的输出来看，文件描述符3正是服务器的监听socket。因此，这个事件表示有新客户连接到来，于是websrv服务器对监听socket执行了accept调用，accept返回一个新的连接socket，其值为5。接着，服务器设置这个新socket上的SO_REUSEADDR属性，然后往epoll内核事件表中注册该socket上的EPOLLRDHUP和EPOLLONESHOT两个事件，最后设置新socket为非阻塞的。然后服务器又接受一个连接socket，是客户端请求的keep-alive连接，值为6。

第二部分从第二次epoll_wait系统调用开始。此次epoll_wait调用检测到了文件描述符5上的EPOLLIN事件，这表示客户端的第一行数据到达了，于是服务器执行了两次recvfrom系统调用来接收数据。第一次recvfrom调用读取到731字节的客户数据，即"GET /index.html HTTP/1.1\r\nHost: "...。第二次recvfrom调用则失败了，errno是EAGAIN，这表示目前没有更多的客户数据可读。此后，服务器调用了futex函数对互斥锁解锁，以唤醒等待互斥锁的线程。可见，POSIX线程库中的pthread_mutex_unlock函数在内部调用了futex函数。

第三部分中，epoll_wait调用检测到了文件描述符5上的EPOLLOUT事件，这表示工作线程正确地处理了客户请求，并准备好了待发送的数据，因此主线程开始执行writev系统调用往客户端写入HTTP应答，在应答完成后，释放从文件映射的共享内存。最后，服务器从epoll内核事件表中移除文件描述符5上的所有注册事件，并关闭该文件描述符。

第四部分中，客户端一段时间没有请求后，关闭长连接。

由此可见，strace命令使我们能够清楚地查看每次系统调用发生的时机，以及相关参数的值，这比用gdb调试更方便

## ss

原为net-tools包中的netstat，现被iproute2中的ss取代(arp被ip neighbor取代、ifconfig被ip addr和ip link取代、route被ip route取代)。

网络信息统计工具，主要利用其即显示TCP连接及其状态信息的功能。

常用选项:

- -r 尝试解析数字地址/端口
- -n 不解析服务名
- -a 结果中包含监听socket和非监听socket
- -l 结果只包含监听socket
- -p 显示socket所属进程
- -T 显示socket所属线程，语义包含-p
- -E 持续输出
- -4 显示IPv4连接
- -6 显示Ipv6连接
- -t 显示tcp连接
- -u 显示udp连接

```shell
> ./webserver 0.0.0.0 12345
> telnet localhost 12345
> ss -atn | grep 12345
State         Recv-Q    Send-Q                                  Local Address:Port                     Peer Address:Port
LISTEN     0      5                                       0.0.0.0:12345                  0.0.0.0:*    
ESTAB      0      0                                     127.0.0.1:51186                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:51186
```

输出字段:

- State socket的状态。对于无状态协议，比如UDP协议，这一字段将显示为空。而对面向连接的协议而言，netstat支持的State包括ESTABLISHED、SYN_SENT、SYN_RCVD、FIN_WAIT1、FIN_WAIT2、TIME_WAIT、CLOSE、CLOSE_WAIT、LAST_ACK、LISTEN、CLOSING、UNKNOWN。含义见TCP连接的三次握手、四次挥手。
- Recv-Q socket内核接收缓冲区中尚未被应用程序读取的数据量。
- Send-Q 未被对方确认的数据量。
- Local Address 本端的IP地址和端口号。
- Foreign Address 对方的IP地址和端口号。

上面的输出中，第1行表示本地socket地址0.0.0.0:12345处于LISTEN状态，并等待任何远端socket（用0.0.0.0:*表示）对它发起连接。第2行表示服务器和远端地址127.0.0.1:51186建立了一个连接。第3行只是从客户端的角度重复输出第2行信息表示的这个连接，因为我们是在同一台机器上运行服务器程序（webserver）和客户端程序（telnet）的。

在服务器程序开发过程中，我们一定要确保每个连接在任一时刻都处于我们期望的状态。因此我们应该习惯于使用netstat命令。

## vmstat

virtual memory statistics的缩写，它能实时输出系统的各种资源的使用情况，比如进程信息、内存使用、CPU使用率以及I/O使用情况。

常用的选项和参数包括:

- -f 显示系统自启动以来执行的fork次数。
- -s 显示内存相关的统计信息以及多种系统活动的数量（比如CPU上下文切换次数）。
- -d 显示磁盘相关的统计信息。
- -p 显示指定磁盘分区的统计信息。
- -S 使用指定的单位来显示。参数k、K、m、M分别代表1000、1024、1 000 000和1 048 576字节。
- delay 采样间隔（单位是s），即每隔delay的时间输出一次统计信息。
- count 采样次数，即共输出count次统计信息。

默认情况下，vmstat输出的内容相当丰富。请看下面的示例:

```shell
> vmstat 5 3 #每隔5秒输出一次结果，共输出3次
procs -----------memory---------- ---swap-- -----io---- -system-- -------cpu-------
 r  b 交换 空闲 缓冲 缓存   si   so    bi    bo   in   cs us sy id wa st gu
 3  0      0 16573180 443452 6529760    0    0   628   807 2208    2  1  1 98  0  0  0
 2  0      0 16587688 443488 6530632    0    0   246   130 1413 3479  1  0 99  0  0  0
 2  0      0 16594072 443504 6530772    0    0     0   139 1499 3715  1  0 99  0  0  0
```

第1行输出是自系统启动以来的平均结果，而后面的输出则是采样间隔内的平均结果。vmstat的每条输出都包含6个字段，它们的含义分别是:

- procs 进程信息
  - "r" 等待运行的进程数目
  - "b" 处于不可中断睡眠状态的进程数目
- memory 内存信息，各项的单位都是千字节（KB）
  - "swpd/交换" 虚拟内存的使用数量
  - "free/空闲" 空闲内存的数量
  - "buff/缓冲" 作为"buffer cache"的内存数量。从磁盘读入的数据可能被保持在"buffercache"中，以便下一次快速访问
  - "cache/缓存" 作为"page cache"的内存数量。待写入磁盘的数据将首先被放到"page cache"中，然后由磁盘中断程序写入磁盘
- swap 交换分区（虚拟内存）的使用信息，各项的单位都是KB/s
  - "si" 数据由磁盘交换至内存的速率
  - "so" 数据由内存交换至磁盘的速率。如果这两个值经常发生变化，则说明内存不足
- io 块设备的使用信息，单位是block/s
  - "bi" 从块设备读入块的速率
  - "bo" 向块设备写入块的速率
- system 系统信息
  - "in" 每秒发生的中断次数
  - "cs" 每秒发生的上下文切换（进程切换）次数
- cpu CPU使用信息
  - "us" 系统所有进程运行在用户空间的时间占CPU总运行时间的比例
  - "sy" 系统所有进程运行在内核空间的时间占CPU总运行时间的比例
  - "id" CPU处于空闲状态的时间占CPU总运行时间的比例
  - "wa" CPU等待I/O事件的时间占CPU总运行时间的比例
  - "st" 被虚拟机使用的时间占总时间的比例
  - "gu" 运行KVM guest code的时间占总时间的比例

不过，我们可以使用iostat命令获得磁盘使用情况的更多信息，也可以使用mpstat获得CPU使用情况的更多信息。vmstat命令主要用于查看系统内存的使用情况。

## sar

iproute2里的ifstat依托构式，原先的ifstat只在aur提供。选择sysstat中的sar查看接口流量，用法: `sar -n DEV (--iface=xxx) 1 3 #查看网络接口 间隔1秒 输出3次`

```shell
> sar -n DEV --iface=lo,wlan0 1 3
Linux 6.7.5-arch1-1 (awei)      2024年02月20日  _x86_64_        (24 CPU)

20时07分26秒     IFACE   rxpck/s   txpck/s    rxkB/s    txkB/s   rxcmp/s   txcmp/s  rxmcst/s   %ifutil
20时07分27秒        lo     13.00     13.00      1.43      1.43      0.00      0.00      0.00      0.00
20时07分27秒     wlan0      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00

20时07分27秒     IFACE   rxpck/s   txpck/s    rxkB/s    txkB/s   rxcmp/s   txcmp/s  rxmcst/s   %ifutil
20时07分28秒        lo      6.00      6.00      0.34      0.34      0.00      0.00      0.00      0.00
20时07分28秒     wlan0      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00

20时07分28秒     IFACE   rxpck/s   txpck/s    rxkB/s    txkB/s   rxcmp/s   txcmp/s  rxmcst/s   %ifutil
20时07分29秒        lo     32.00     32.00    141.98    141.98      0.00      0.00      0.00      0.00
20时07分29秒     wlan0      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00

平均时间:     IFACE   rxpck/s   txpck/s    rxkB/s    txkB/s   rxcmp/s   txcmp/s  rxmcst/s   %ifutil
平均时间:        lo     17.00     17.00     47.91     47.91      0.00      0.00      0.00      0.00
平均时间:     wlan0      0.00      0.00      0.00      0.00      0.00      0.00      0.00      0.00
```

含义:

- rxpck/s 每秒收到数据包数
- txpck/s 每秒发送数据包数
- rxkB/s 每秒收到千字节数
- txkB/s 每秒发送千字节数
- rxcmp/s 每秒收到的压缩数据包数(对于cslip等)
- txcmp/s 每秒发送的压缩数据包数
- rxmcst/s 每秒收到的多播数据包数
- %ifutil 网络接口的利用率百分比。对于半双工接口，使用rxkB/s和txkB/s之和作为接口速度的百分比来计算利用率。对于全双工，这是rxkB/S或txkB/S中的较大者。
- 仅当interval是0时会输出自系统启动开始的平均值

## mpstat

multi-processor statistics的缩写，它能实时监测多处理器系统上每个CPU的使用情况。mpstat命令和iostat命令通常都集成在包sysstat中，安装sysstat(12.7.5)即可获得这两个命令。mpstat命令的典型用法是

`mpstat [-P{|ALL}] [interval[count]]`

- 选项P 指定要监控的CPU号（0～CPU个数-1），其值“ALL”表示监听所有的CPU
- interval 采样间隔（单位是s），即每隔interval的时间输出一次统计信息
- count 采样次数，即共输出count次统计信息，但mpstat最后还会输出这count次采样结果的平均值。
- 与vmstat命令~~一样~~，mpstat命令输出的第一次结果**也不是**自系统启动以来的平均结果，~~而后面（count-1）次输出结果则是采样间隔内的平均结果~~。
