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
