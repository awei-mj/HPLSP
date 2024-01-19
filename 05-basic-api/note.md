# Linux网络编程基础API

- socket地址api socket地址，即(ip,port)对
- socket基础api `<sys/socket.h>` 创建、命名、监听socket，接受、发起连接，读写数据等
- 网络信息api `<netdb.h>` 实现主机名和ip地址之间的转换以及服务名称和端口号之间的转换

## socket地址api

### 主机字节序、网络字节序

网络字节序总是大端，收到的主机根据自己机器的字节序决定如何转换。

Linux提供长短整型的转换:

```c
#include <netinet/in.h>
unsigned long int htonl(unsigned long int hostlong);
unsigned short int htons(unsigned short int hostshort);
unsigned long int ntohl(unsigned long int netlong);
unsigned short int ntohs(unsigned short int netshort);
```

### 通用socket地址

表示socket地址的数据结构:

```c
#include <bits/socket.h>
struct sockaddr {
    sa_family_t sa_family; //地址族类型，与协议族类型对应
    char sa_data[14];
};
```

|协议族|地址族|描述|地址值含义和长度|
|-|-|-|-|
|PF_UNIX|AF_UNIX|UNIX本地域协议族|文件的路径名，长度可达108字节|
|PF_INET|AF_INET|TCP/IPv4协议族|16bit端口号和32bitIPv4地址，共6字节|
|PF_INET6|AF_INET6|TCP/IPv6协议族|16bit端口号,32bit流标识,128bitIPv6地址,32bit范围ID，共26字节|

宏`PF_*`和`AF_*`定义与`<bits/socket.h>`中，且值相同，故通常混用。

旧数据结构无法完全容纳地址值，新结构有足够空间且内存对齐。

```c
#include <bits/socket.h>
struct sockaddr_storage
{
    sa_family_t sa_family;
    unsigned long int __ss_align;
    char __ss_padding[128-sizeof(__ss_align)];
}
```

### 专用socket地址

通用的太逆天了不好用，提供了专用结构体。

UNIX本地域:

```c
#include <sys/un.h>
struct sockaddr_un
{
    sa_family_t sin_family; /*地址族：AF_UNIX*/
    char sun_path[108];     /*文件路径名*/
};
```

TCP/IP:

```c
struct sockaddr_in
{
    sa_family_t sin_family;     /*地址族：AF_INET*/
    u_int16_t sin_port;         /*端口号，要用网络字节序表示*/
    struct in_addr sin_addr;    /*IPv4地址结构体，见下面*/
};
struct in_addr
{
    u_int32_t s_addr;           /*IPv4地址，要用网络字节序表示*/
};
struct sockaddr_in6
{
    sa_family_t sin6_family;    /*地址族：AF_INET6*/
    u_int16_t sin6_port;        /*端口号，要用网络字节序表示*/
    u_int32_t sin6_flowinfo;    /*流信息，应设置为0*/
    struct in6_addr sin6_addr;  /*IPv6地址结构体，见下面*/
    u_int32_t sin6_scope_id;    /*scope ID，尚处于实验阶段*/
};
struct in6_addr
{
    unsigned char sa_addr[16];  /*IPv6地址，要用网络字节序表示*/
};
```

所有专用socket地址（包括sockaddr_storage）类型的变量在实际使用时都需要转化为通用socket地址类型sockaddr（强制转换即可），因为所有socket编程接口使用的地址参数的类型都是sockaddr。

### IP地址转换函数

点分十进制字符串表示的IPv4地址和用网络字节序整数表示的IPv4地址之间的转换：

```c
#include <arpa/inet.h>
in_addr_t inet_addr(const char *strptr);
int inet_aton(const char *cp, struct in_addr *inp);
char *inet_ntoa(struct in_addr in);
```

inet_addr函数将用点分十进制字符串表示的IPv4地址转化为用网络字节序整数表示的IPv4地址。它失败时返回INADDR_NONE。

inet_aton函数完成和inet_addr同样的功能，但是将转化结果存储于参数inp指向的地址结构中。它成功时返回1，失败则返回0。

inet_ntoa函数将用网络字节序整数表示的IPv4地址转化为用点分十进制字符串表示的IPv4地址。但需要注意的是，该函数内部用一个静态变量存储转化结果，函数的返回值指向该静态内存，因此inet_ntoa是不可重入的。

同时适用于IPv4地址和IPv6地址的新函数:

```c
#include <arpa/inet.h>
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src,char *dst, socklen_t cnt);

#include <netinet/in.h>
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
```

inet_pton函数将用字符串表示的IP地址src（用点分十进制字符串表示的IPv4地址或用十六进制字符串表示的IPv6地址）转换成用网络字节序整数表示的IP地址，并把转换结果存储于dst指向的内存中。其中，af参数指定地址族，可以是AF_INET或者AF_INET6。inet_pton成功时返回1，失败则返回0并设置errno。

inet_ntop函数进行相反的转换，前三个参数的含义与inet_pton的参数相同，最后一个参数cnt指定目标存
储单元的大小。inet_ntop成功时返回目标存储单元的地址，失败则返回NULL并设置errno。

## 创建socket

socket是可读写、可控制、可关闭的文件描述符。

```c
#include <sys/types.h>
#include <sys/socket.h>
int socket(int domain, int type, int protocol);
```

domain参数代表底层协议族，可选PF_INET、PF_INET6和PF_UNIX。

type服务类型，主要有SOCK_STREAM(流服务)和SOCK_UGRAM(数据报服务)。对TCP/IP协议族而言，前者代表TCP协议，后者代表UDP协议。常用的还有SOCK_NONBLOCK和SOCK_CLOEXEC。它们分别表示将新创建的socket设为非阻塞的，以及用fork调用创建子进程时在子进程中关闭该socket。type之间可以取或。

protocol一般设为0，成功返回文件描述符，失败返回-1并设置errno。

## 命名socket

将一个socket与socket地址绑定称为给socket命名。在服务器程序中，我们通常要命名socket，因为只有命名后客户端才能知道该如何连接它。客户端则通常不需要命名socket，而是采用匿名方式，即使用操作系统自动分配的socket地址。命名socket的系统调用是bind:

```c
#include <sys/types.h>
#include <sys/socket.h>
int bind(int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);
```

sockfd是未命名的文件描述符，my_addr指向socket地址，addrlen表示socket地址的长度。
bind成功时返回0，失败则返回-1并设置errno，两种常见的errno：

- EACCES，被绑定的地址是受保护的地址，仅超级用户能够访问。比如普通用户将socket绑定到知名服务端口（端口号为0～1023）上时，bind将返回EACCES错误。
- EADDRINUSE，被绑定的地址正在使用中。比如将socket绑定到一个处于TIME_WAIT状态的socket地址。

## 监听socket

创建监听队列以存放待处理的客户连接的系统调用:

```c
#include <sys/socket.h>
int listen(int sockfd, int backlog);
```

backlog参数提示内核监听队列的最大长度。监听队列的长度如果超过backlog，服务器将不受理新的客户连接，客户端也将收到ECONNREFUSED错误信息。在内核版本2.2之前的Linux中，backlog参数是指所有处于半连接状态（SYN_RCVD）和完全连接状态（ESTABLISHED）的socket的上限。但自内核版本2.2之后，它只表示处于完全连接状态的socket的上限，处于半连接状态的socket的上限则由/proc/sys/net/ipv4/tcp_max_syn_backlog内核参数定义。backlog参数的典型值是5。

listen成功时返回0，失败则返回-1并设置errno。

代码实验，对tcp socket进行连接，完整连接最多有(backlog+1)个

```bash
$> ./backlog '0.0.0.0' 12345 5
$> telnet '127.0.0.1' 12345
$> ss -atn | grep 12345
LISTEN     6      5                                       0.0.0.0:12345                  0.0.0.0:*    
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:39456
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:59162
ESTAB      0      0                                     127.0.0.1:56708                127.0.0.1:12345
SYN-SENT   0      1                                     127.0.0.1:54778                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:47920                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:56708
ESTAB      0      0                                     127.0.0.1:59162                127.0.0.1:12345
SYN-SENT   0      1                                     127.0.0.1:37330                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:47920
ESTAB      0      0                                     127.0.0.1:39466                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:56712                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:56712
ESTAB      0      0                                     127.0.0.1:39456                127.0.0.1:12345
ESTAB      0      0                                     127.0.0.1:12345                127.0.0.1:39466
```

## 接受连接

从listen监听队列中接受一个连接的系统调用:

```c
#include <sys/types.h>
#include <sys/socket.h>
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

sockfd: 执行过listen的监听socket，addr: 远端socket地址，addrlen: socket地址长度

成功时返回新的连接socket，唯一标识了新接受的连接；失败时返回-1并设置errno。

现考虑: ESTABLISHED状态的连接对应的客户端掉线后，该accept是否成功？

答：accept只是从监听队列中取出连接，而不论连接处于何种状态（如上面的ESTABLISHED状态和CLOSE_WAIT状态），更不关心任何网络状况的变化。

## 发起连接

主动与服务器建立连接的系统调用:

```c
#include <sys/types.h>
#include <sys/socket.h>
int connect(int sockfd, const struct socketaddr *serv_addr, socklen_t addrlen);
```

成功时返回0，sockfd就唯一地标识了这个连接，客户端就可以通过读写sockfd来与服务器通信。connect失败则返回-1并设置errno。两种常见的errno:

- ECONNREFUSED，目标端口不存在，连接被拒绝。我们在3.5.1小节讨论过这种情况。
- ETIMEDOUT，连接超时。我们在3.3.3小节讨论过这种情况。

## 关闭连接

通过关闭普通文件描述符的系统调用来关闭socket，只有当fd的引用计数为0时，才真正关闭连接。

如果要立即终止，可通过shutdown系统调用:

```c
#include <unistd.h>
int close(int fd);

#include <sys/socket.h>
int shutdown(int sockfd, int howto); //成功返回0，失败返回-1并设置errno
```

|howto可选值|含义|
|-|-|
|SHUT_RD|关闭读，接受缓冲区数据被抛弃|
|SHUT_WR|关闭写，发送缓冲区数据发出后真正关闭连接|
|SHUT_RDWR|同时关|

## 数据读写

### TCP数据读写

可以用对文件的读写操作read、write，也有专用系统调用，增加了对数据读写的控制。其中TCP数据读写：

```c
#include <sys/types.h>
#include <sys/socket.h>
ssize_t recv(int sockfd, void *buf, size_t len, int flags); //返回实际读取到数据的长度，对方关闭连接时返回0，出错返回-1并置errno
ssize_t send(int sockfd, const void *buf, size_t len, int flags); //返回写入数据的长度，失败返回-1并置errno
```

flags一般置0，可选值如下，支持逻辑或：

|选项名|含义|send|recv|
|-|-|-|-|
|MSG_CONFIRM    |指示链路层监听对方回应，直到得到答复，仅能用于SOCK_DGRAM和SOCK_RAW类型的socket     |Y|N|
|MSG_DONTROUTE  |不查看路由表，直接将数据发给本地局域网内的主机，表示发送者知道目标主机在本地网络上   |Y|N|
|MSG_DONTWAIT   |此次操作是非阻塞的                                                             |Y|Y|
|MSG_MORE       |告知内核应用层有更多数据要发送，内核会等待新数据一并发送                           |Y|N|
|MSG_WAITALL    |仅在读到指定数量字节后才返回                                                    |N|Y|
|MSG_PEEK       |窥探读缓存中的数据，不会导致数据被清除                                           |N|Y|
|MSG_OOB        |发送或接收紧急(out of bound)数据                                               |Y|Y|
|MSG_NOSIGNAL   |往读端关闭的管道或socket连接中写数据时不引发SIGPIPE信号                          |Y|N|

### UDP数据读写

```c
#include <sys/types.h>
#include <sys/socket.h>
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
```

recvfrom读取sockfd上的数据，buf和len参数分别指定读缓冲区的位置和大小。因为UDP通信没有连接的概念，所以我们每次读取数据都需要获取发送端的socket地址，即参数src_addr所指的内容，addrlen参数则指定该地址的长度。

recvfrom/sendto系统调用也可以用于面向连接（STREAM）的socket的数据读写，只需要把最后两个参数都设置为NULL以忽略发送端/接收端的socket地址

### 通用读写

```c
#include <sys/socket.h>
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);
ssize_t sendmsg(int sockfd, struct msghdr *msg, int flags);

struct msghdr {
    void *msg_name;             /*socket地址*/
    socklen_t msg_namelen;      /*socket地址的长度*/
    struct iovec *msg_iov;      /*分散的内存块，见后文*/
    int msg_iovlen;             /*分散内存块的数量*/
    void *msg_control;          /*指向辅助数据的起始位置*/
    socklen_t msg_controllen;   /*辅助数据的大小*/
    int msg_flags;              /*复制函数中的flags参数，并在调用过程中更新*/
};

struct iovec
{
    void *iov_base;/*内存起始地址*/
    size_t iov_len;/*这块内存的长度*/
};
```

对iovec的策略:分散读、集中写

## 带外标记

```c
#include <sys/socket.h>
int sockatmark(int sockfd);
```

下一个数据是带外数据，返回1，可以用带MSG_OOB标志的recv接收数据。如果不是返回0。

## 地址信息函数

获得一个socket连接的本端socket地址和远端socket地址。

```c
#include <sys/socket.h>
int getsockname(int sockfd, struct sockaddr *address, socklen_t *address_len);
int getpeername(int sockfd, struct sockaddr *address, socklen_t *address_len);
```

成功时返回0，失败返回-1并设置errno。

## socket选项

读取和设置socket文件描述符属性

```c
#include <sys/socket.h>
// sockfd 目标socket, level执行操作协议(IPv4, IPv6, TCP) option_name 参数指定了选项的名字. 后面值和长度
// 成功时返回0 失败返回-1 设置errno
int getsockopt(int sockfd, int level, int option_name, void* option_value, socklen_t restrict option_len);
int setsockopt(int sockfd, int level, int option_name, const void* option_value, socklen_t option_len);
```

sockfd参数指定被操作的目标socket。level参数指定要操作哪个协议的选项（即属性），比如IPv4、IPv6、TCP等。option_name参数则指定选项的名字。我们在表5-5中列举了socket通信中几个比较常用的socket选项。option_value和option_len参数分别是被操作选项的值和长度，不同的选项具有不同类型的值。

<table>
  <th>level</th><th>option name</th><th>数据类型</th><th>说明</th>
  <tr>
    <td rowspan=14>SOL_SOCKET<br/>(通用socket选项，与协议无关)</td>
    <td>SO_DEBUG</td><td>int</td><td>打开调试信息</td>
  </tr>
  <tr>
    <td>SO_REUSEADDR</td><td>int</td><td>重用本地地址</td>
  </tr>
  <tr>
    <td>SO_TYPE</td><td>int</td><td>获取socket类型</td>
  </tr>
  <tr>
    <td>SO_ERROR</td><td>int</td><td>获取并清除socket错误状态</td>
  </tr>
  <tr>
    <td>SO_DONTROUTE</td><td>int</td><td>不查看路由表</td>
  </tr>
  <tr>
    <td>SO_RCVBUF</td><td>int</td><td>TCP接收缓冲区大小</td>
  </tr>
  <tr>
    <td>SO_SNDBUF</td><td>int</td><td>TCP发送缓冲区大小</td>
  </tr>
  <tr>
    <td>SO_KEEPALIVE</td><td>int</td><td>发送周期性保活报文以保持连接</td>
  </tr>
  <tr>
    <td>SO_OOBINLINE</td><td>int</td><td>带外数据存在普通数据队列中</td>
  </tr>
  <tr>
    <td>SO_LINGER</td><td>linger</td>linger<td>有数据发送则延迟关闭</td>
  </tr>
  <tr>
    <td>SO_RCVLOWAT</td><td>int</td><td>接收缓冲区低水位标记</td>
  </tr>
  <tr>
    <td>SO_SNDLOWAT</td><td>int</td><td>发送缓冲区低水位标记</td>
  </tr>
  <tr>
    <td>SO_RCVTIMEO</td><td>timeval</td><td>接收数据超时</td>
  </tr>
  <tr>
    <td>SO_SNDTIMEO</td><td>timeval</td><td>发送数据超时</td>
  </tr>
  <tr>
    <td rowspan=2>IPPROTO_IP(IPv4)</td>
    <td>IP_TOS</td><td>int</td><td>服务类型</td>
  </tr>
  <tr>
    <td>IP_TTL</td><td>int</td><td>存活时间</td>
  </tr>
  <tr>
    <td rowspan=4>IPPROTO_IP6(IPv6)</td>
    <td>IPV6_NEXTHOP</td><td>sockaddr_in6</td><td>下一跳IP地址</td>
  </tr>
  <tr>
    <td>IPV6_RECVPKTINFO</td><td>int</td><td>接收分组信息</td>
  </tr>
  <tr>
    <td>IPV6_DONTFRAG</td><td>int</td><td>禁止分片</td>
  </tr>
  <tr>
    <td>IPV6_RECVTCLASS</td><td>int</td><td>接收通信类型</td>
  </tr>
  <tr>
    <td rowspan=2>IPPROTO_TCP(TCP)</td>
    <td>TCP_MAXSEG</td><td>int</td><td>TCP最大报文段大小</td>
  </tr>
  <tr>
    <td>TCP_NODELAY</td><td>int</td><td>禁止Nagle算法</td>
  </tr>
</table>

部分选项只有在调用listen前针对监听socket才有效，使之继承到连接socket上: SO_DEBUG、SO_DONTROUTE、SO_KEEPALIVE、SO_LINGER、SO_OOBINLINE、SO_RCVBUF、SO_RCVLOWAT、SO_SNDBUF、SO_SNDLOWAT、TCP_MAXSEG和TCP_NODELAY。对客户端而言，应该在调用connect函数前设置。

### SO_REUSEADDR

强制使用被处于TIME_WAIT状态的连接占用的socket地址

```c
int sock = socket(PF_INET, SOCK_STREAM, 0);
assert(sock >= 0);
int reuse=1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
struct sockaddr_in address;
bzero(&address, sizeof(address));
address.sin_family = AF_INET;
inet_pton(AF_INET, ip, &address.sin_addr);
address.sin_port = htons(port);
int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
```

### SO_RCVBUF和SO_SNDBUF

设置缓冲区大小时，会将其值加倍，并且不得小于某个最小值。

### SO_RCVLOWAT和SO_SNDLOWAT

可读数据大于低水位标记时，I/O复用系统调用通知应用可以读取数据；可写同理。默认情况下为1字节

### SO_LINGER

控制close系统调用在关闭TCP连接时的行为。默认情况下close立即返回，TCP模块负责发送缓冲区中剩余数据。

```c
#include <sys/socket.h>
struct linger {
    int l_onoff;    //开启还是关闭该选项
    int l_linger;   //滞留时间
};
```

- l_onoff==0, 默认行为
- l_onoff!=0, l_linger==0, 立即返回，TCP模块丢弃残留数据，并发送一个复位报文段。
- l_onoff!=0, l_linger!=0, 阻塞socket会等待l_linger的时间，直到TCP模块发送完所有残留数据并得到对方的确认。如果未完成返回-1并设置errno为EWOULDBLOCK；非阻塞立即返回，根据返回值和errno判断残留数据是否发送完毕。

## 网络信息API

可以通过调用网络信息API完成主机名到API地址的转换和服务名称到端口的转换

### gethostbyname和gethostbyaddr

先在/etc/hosts里找主机，没找到再去问DNS服务器

```c
#include <netdb.h>
struct hostent *gethostbyname(const char *name);
struct hostent *gethostbyaddr(const void *addr, size_t len, int type);

struct hostent
{
    char *h_name;       /*主机名*/
    char **h_aliases;   /*主机别名列表，可能有多个*/
    int h_addrtype;     /*地址类型（地址族）*/
    int h_length;       /*地址长度*/
    char **h_addr_list  /*按网络字节序列出的主机IP地址列表*/
};
```

### getservbyname和getservbyport

通过读取/etc/services获取服务信息

```c
#include <netdb.h>
struct servent *getservbyname(const char *name, const char *proto);
struct servent *getservbyport(int port, const char *proto);

struct servent
{
    char *s_name;       /*服务名称*/
    char **s_aliases;   /*服务的别名列表，可能有多个*/
    int s_port;         /*端口号*/
    char *s_proto;      /*服务类型,通常是tcp或者udp*/
};
```

proto指定服务类型，传递"tcp"表示流服务，传递"udp"表示数据包服务，NULL表示所有类型

### getaddrinfo

既能通过主机名获得IP地址（内部使用的是gethostbyname函数），也能通过服务名获得端口号（内部使用的是getservbyname函数）。它是否可重入取决于其内部调用的gethostbyname和getservbyname函数是否是它们的可重入版本。

```c
#include <netdb.h>
int getaddrinfo(const char *hostname, const char *service, const struct addrinfo *hints, struct addrinfo **result);
void freeaddrinfo(struct addrinfo *res);

struct addrinfo
{
    int ai_flags;               /*见后文*/
    int ai_family;              /*地址族*/
    int ai_socktype;            /*服务类型，SOCK_STREAM或SOCK_DGRAM*/
    int ai_protocol;            /*具体的网络协议,通常设置为0*/
    socklen_t ai_addrlen;       /*socket地址ai_addr的长度*/
    char*ai_canonname;          /*主机的别名*/
    struct sockaddr*ai_addr;    /*指向socket地址*/
    struct addrinfo*ai_next;    /*指向下一个sockinfo结构的对象*/
};
```

hostname参数可以接收主机名，也可以接收字符串表示的IP地址（IPv4采用点分十进制字符串，IPv6则采用十六进制字符串）。同样，service参数可以接收服务名，也可以接收字符串表示的十进制端口号。hints参数是应用程序给getaddrinfo的一个提示，以对getaddrinfo的输出进行更精确的控制。hints参数可以被设置为NULL，表示允许getaddrinfo反馈任何可用的结果。result参数指向一个链表，该链表用于存储getaddrinfo反馈的结果。

ai_flags选项

|选项|含义|
|-|-|
|AI_PASSIVE|服务端是否会将取得的socket地址用于被动打开|
|AI_CANONNAME|告诉getaddrinfo返回主机的别名|
|AI_NUMERICHOST|hostname必须是字符串表示的ip地址，从而避免DNS查询|
|AI_NUMERICSERV|service参数使用十进制端口号|
|AI_V4MAPPED|如果ai_family设置为AF_INET6且没有v6地址结果，将v4地址映射为v6地址|
|AI_ALL|必须和上条一起使用，同时返回符合条件的v6地址和由v4映射得到的v6地址|
|AI_ADDRCONFIG|与V4MAPPED互斥|

调用结束后需要释放res

### getnameinfo

通过socket地址同时获得以字符串表示的主机名(gethostbyaddr)和服务名(getservbyport)。是否可重入由内部调用的函数决定。

```c
#include <netdb.h>

int getnameinfo(const struct sockaddr *sockaddr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags);
```

flags:

|选项|含义|
|-|-|
|NI_NAMEREQD|如果socket地址不能获得主机名，返回错误|
|NI_DGRAM|返回数据报服务|
|NI_NUMERICHOST|返回字符串表示的IP地址，而不是主机名|
|NI_NUMERICSERV|返回字符串表示的十进制端口号，而不是服务名|
|NI_NOFQDN|仅返回主机域名的第一部分|

这两个函数成功时返回0,失败时返回错误码

|选项|含义|
|-|-|
|EAI_AGAIN|调用临时失败，稍后再试|
|EAI_BADFLAGS|非法的flags值|
|EAI_FAIL|名称解析失败|
|EAI_FAMILY|不支持的ai_family参数|
|EAI_MEMORY|内存分配失败|
|EAI_NONAME|非法的主机名或服务名|
|EAI_OVERFLOW|用户提供的缓冲区溢出|
|EAI_SERVICE|没有支持的服务|
|EAI_SOCKTYPE|不支持的服务类型|
|EAI_SYSTEM|系统错误，错误值存在errno中|

Linux中strerror可转换errno，下述函数可将上表错误码转换成易读的字符串形式：

```c
#include <netdb.h>
const char *gai_strerror(int error);
```
