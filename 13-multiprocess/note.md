# 多进程编程

[TOC]

- 复制进程映像的fork系统调用和替换进程映像的exec系统调用
- 僵尸进程以及如何避免僵尸进程
- 进程间通信(IPC, Inter-Process Communication)的最简单方式:管道
- 3种System V进程间通信方式: 信号量、消息队列和共享内存。它们都是由AT&T System V2版本的UNIX引入的，所以统称为System V IPC。
- 在进程间传递文件描述符的通用方法: 通过UNIX本地域socket传递特殊的辅助数据（关于辅助数据，参考5.8.3小节）

## fork系统调用

```cpp
#include <sys/types.h>
#include <unistd.h>
pid_t fork();
```

返回两次，父进程返回子进程的pid，子进程返回0。失败时返回-1并设置errno。

fork函数复制当前进程，在内核进程表中创建一个新的进程表项。新的进程表项有很多属性和原进程相同，比如堆指针、栈指针和标志寄存器的值。但也有许多属性被赋予了新的值，比如该进程的PPID被设置成原进程的PID，信号位图被清除（原进程设置的信号处理函数不再对新进程起作用）。

子进程的代码与父进程完全相同，同时它还会复制父进程的数据（堆数据、栈数据和静态数据）。数据的复制采用的是所谓的写时复制（copy on writte），即只有在任一进程（父进程或子进程）对数据执行了写操作时，复制才会发生（先是缺页中断，然后操作系统给子进程分配内存并复制父进程的数据）。即便如此，如果我们在程序中分配了大量内存，那么使用fork时也应当十分谨慎，尽量避免没必要的内存分配和数据复制。

此外，创建子进程后，父进程中打开的文件描述符默认在子进程中也是打开的，且文件描述符的引用计数加1。不仅如此，父进程的用户根目录、当前工作目录等变量的引用计数均会加1。

## exec系列系统调用

替换当前进程映像

```cpp
#include <unistd.h>
extern char **environ;
int execl(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execle(const char *path, const char *arg, ..., char *const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
```

- path参数指定可执行文件的完整路径
- file参数可以接受文件名，该文件的具体位置则在环境变量PATH中搜寻
- arg接受可变参数，argv则接受参数数组，它们都会被传递给新程序（path或file指定的程序）的main函数
- envp参数用于设置新程序的环境变量。如果未设置它，则新程序将使用由全局变量environ指定的环境变量。
- 一般情况下，exec函数是不返回的，除非出错。它出错时返回-1，并设置errno。如果没出错，则原程序中exec调用之后的代码都不会执行，因为此时原程序已经被exec的参数指定的程序完全替换（包括代码和数据）
- exec函数不会关闭原程序打开的文件描述符，除非该文件描述符被设置了类似SOCK_CLOEXEC的属性

## 处理僵尸进程

父进程一般需要跟踪子进程的退出状态。因此，当子进程结束运行时，内核不会立即释放该进程的进程表表项，以满足父进程后续对该子进程退出信息的查询（如果父进程还在运行）。在子进程结束运行之后，父进程读取其退出状态之前，我们称该子进程处于僵尸态。另外一种使子进程进入僵尸态的情况是：父进程结束或者异常终止，而子进程继续运行。此时子进程的PPID将被操作系统设置为1，即init进程。init进程接管了该子进程，并等待它结束。在父进程退出之后，子进程退出之前，该子进程处于僵尸态。

```cpp
#include <sys/types.h>
#include <sys/wait.h>
pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);
```

wait函数将阻塞进程，直到该进程的某个子进程结束运行为止。它返回结束运行的子进程的PID，并将该子进程的退出状态信息存储于stat_loc参数指向的内存中。`sys/wait.h`头文件中定义了几个宏帮助解释状态信息

|宏|含义|
|-|-|
|WIFEXITED(stat_val)    |如果子进程正常结束返回非0值|
|WEXITSTATUS(stat_val)  |如果WIFEXITED非0，返回子进程的退出码|
|WIFSIGNALED(stat_val)  |如果子进程因一个未捕获的信号而终止，返回非0值|
|WTERMSIG(stat_val)     |如果WIFSIGNALED非0，返回一个信号值|
|WIFSTOPPED(stat_val)   |如果子进程意外终止，返回一个非0值|
|WSTOPSIG(stat_val)     |如果WIFSTOPPED非0，返回一个信号值|

waitpid只等待由pid参数指定的子进程。如果pid取值为-1，那么它就和wait函数相同，即等待任意一个子进程结束。stat_loc参数的含义和wait函数的stat_loc参数相同。options参数可以控制waitpid函数的行为。该参数最常用的取值是WNOHANG。当options的取值是WNOHANG时，waitpid调用将是非阻塞的：如果pid指定的目标子进程还没有结束或意外终止，则waitpid立即返回0；如果目标子进程确实正常退出了，则waitpid返回该子进程的PID。waitpid调用失败时返回-1并设置errno。

要在子进程退出，即收到SIGCHLD后调用该函数

```cpp
static void handle_child(int sig) {
    pid_t pid;
    int stat;
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        /*对结束的子进程进行善后处理*/
    }
}
```

## 管道

fork调用之后两个管道文件描述符（fd[0]和fd[1]）都保持打开。一对这样的文件描述符只能保证父、子进程间一个方向的数据传输，父进程和子进程必须有一个关闭fd[0]，另一个关闭fd[1]。第6章中还介绍过，socket编程接口提供了一个创建全双工管道的系统调用：socketpair。

管道只能用于有关联的两个进程（比如父、子进程）间的通信。而下面要讨论的3种System V IPC能用于无关联的多个进程之间的通信，因为它们都使用一个全局唯一的键值来标识一条信道。不过，有一种特殊的管道称为FIFO[^1]（First In First Out，先进先出），也叫命名管道。它也能用于无关联进程之间的通信。因为FIFO管道在网络编程中使用不多，所以本书不讨论它。

## 信号量

### 信号量原语

P(wait/passeren传递)、V(signal/vrijgeven释放)

- P(SV) 如果SV大于0就减1，如果为0就挂起
- V(SV) 如果有挂起进程就唤醒，否则SV加1

### semget系统调用

创建一个新的信号集，或获取一个已存在的信号集。

```cpp
#include <sys/sem.h>
int semget(key_t key, int num_sems, int sem_flags);
```

- key 键值，用于标识一个全局唯一的信号量集，类似文件名，通过信号量通信的进程要使用相同的键值来创造/获取该信号量
- num_sems 指定信号量集中的信号量数目，如果获取信号量集可以设置为0
- sem_flags 指定一组标志，低端的9bit是信号量的权限，格式和含义与系统调用open的mode相同。还可以与`IPC_CREAT`做或以创建新的信号量集，此时即使已存在也不会发生错误。`IPC_EXCL`为exclusive，可确保创建新信号集，信号集已存在时返回-1并置`EEXIST`
- 返回正整数值，是信号量集的标识符；失败时返回-1，并设errno

创建信号集后，关联的内核数据结构将被创建并被初始化:

```cpp
#include <sys/sem.h>
struct ipc_perm {
    key_t key;/*键值*/
    uid_t uid;/*所有者的有效用户ID*/
    gid_t gid;/*所有者的有效组ID*/
    uid_t cuid;/*创建者的有效用户ID*/
    gid_t cgid;/*创建者的有效组ID*/
    mode_t mode;/*访问权限*/
    /*省略其他填充字段*/
};
struct semid_ds {
    struct ipc_perm sem_perm;/*信号量的操作权限*/
    unsigned long int sem_nsems;/*该信号量集中的信号量数目*/
    time_t sem_otime;/*最后一次调用semop的时间*/
    time_t sem_ctime;/*最后一次调用semctl的时间*/
    /*省略其他填充字段*/
};
```

semget对semid_ds结构体的初始化包括:

- 将sem_perm.cuid和sem_perm.uid设置为调用进程的有效用户ID。
- 将sem_perm.cgid和sem_perm.gid设置为调用进程的有效组ID。
- 将sem_perm.mode的最低9位设置为sem_flags参数的最低9位。
- 将sem_nsems设置为num_sems。
- 将sem_otime设置为0。
- 将sem_ctime设置为当前的系统时间。

### semop系统调用

改变信号量的值，即执行P、V操作。先介绍与每个信号量关联的一些重要的内核变量，semop对信号量的操作实际上就是对这些内核变量的操作:

```cpp
#include <sys/sem.h>

unsigned short semval; /*信号量的值*/
unsigned short semzcnt; /*等待信号量值变为0的进程数量*/
unsigned short semncnt; /*等待信号量值增加的进程数量*/
pid_t sempid; /*最后一次执行semop操作的进程ID*/

int semop(int sem_id, struct sembuf *sem_ops, size_t num_sem_ops);

struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};
```

- sem_id semget返回的信号量集标识符
- sem_ops 指向一个sembuf结构体类型的数组
  - sem_num 集合中信号量的编号
  - sem_flg
    - IPC_NOWAIT 无论信号量操作是否成功，semop调用都将立即返回，这类似于非阻塞I/O操作
    - SEM_UNDO 当进程退出时取消正在进行的semop操作
  - sem_op 指定操作类型 可为正整数、0、负整数
    - 大于0 `semval += sem_op`要求进程有写权限，若设置`SEM_UNDO`，则更新semadj变量(用以跟踪进程对信号量的修改情况)
    - 等于0 是一个"等待0"(wait-for-zero)操作，要求进程有读权限。如果此时信号量的值是0，则调用立即成功返回。如果信号量的值不是0，则semop失败返回或者阻塞进程以等待信号量变为0。
      - 在信号量不为0时，IPC_NOWAIT标志被指定时，semop立即返回一个错误，并设置errno为EAGAIN。
      - IPC_NOWAIT未被指定时，semzcnt值+1，进程阻塞直至:
        - semval变为0，semzcnt-=1；
        - 所在信号量集被进程移除，返回-1，errno置EIDRM；
        - 调用被信号中断，返回-1，errno被设置为EINTR，同时系统将该信号量的semzcnt-=1。
    - 小于0 表示对信号量值进行减操作，即期望获得信号量，要求调用进程对被操作信号量集拥有写权限。
      - 如果信号量的值semval大于或等于sem_op的绝对值，则semop操作成功，调用进程立即获得信号量，并且系统将该信号量的semval值减去sem_op的绝对值。此时如果设置了SEM_UNDO标志，则系统将更新进程的semadj变量。
      - 如果semval小于sem_op的绝对值，如果IPC_NOWAIT被指定，返回-1并置EAGAIN，否则semncnt+=1，进程阻塞直到:
        - semval>=abs(sem_op)，此时semncnt-=1，且semval-=abs(sem_op)，如果SEM_UNDO标志被设置，则系统更新semadj变量
        - 所在信号量集被进程移除，返回-1，errno置EIDRM；
        - 调用被信号中断，返回-1，errno被设置为EINTR，同时系统将该信号量的semncnt-=1。
- num_sem_ops 要执行操作的个数，即数组元素的个数
- semop依次对每个sem_ops数组成员进行操作，且是原子操作
- 成功返回0，失败返回-1并设置errno。失败时数组中的所有操作都不被执行

### semctl系统调用

对信号量进行直接控制

```cpp
#include <sys/sem.h>
int semctl(int sem_id,int sem_num,int command, ...);

union semun {
    int val; /*用于SETVAL命令*/
    struct semid_ds *buf; /*用于IPC_STAT和IPC_SET命令*/
    unsigned short *array; /*用于GETALL和SETALL命令*/
    struct seminfo *__buf; /*用于IPC_INFO命令*/
};
struct seminfo {
    int semmap;/*Linux内核没有使用*/
    int semmni;/*系统最多可以拥有的信号量集数目*/
    int semmns;/*系统最多可以拥有的信号量数目*/
    int semmnu;/*Linux内核没有使用*/
    int semmsl;/*一个信号量集最多允许包含的信号量数目*/
    int semopm;/*semop一次最多能执行的sem_op操作数目*/
    int semume;/*Linux内核没有使用*/
    int semusz;/*sem_undo结构体的大小*/
    int semvmx;/*最大允许的信号量值*/
    int semaem; /*最多允许的UNDO次数（带SEM_UNDO标志的semop操作的次数）*/
};
```

- sem_id 信号量集标识符
- sem_num 信号量编号
- command 要执行的命令
- semun 第四个参数的推荐格式
- 失败时返回-1，并设置errno

支持的命令如下表所示

|命令|含义|成功时的返回值|
|-|-|-|
|IPC_STAT|将信号量集关联的内核数据结构复制到semun.buf中|0|
|IPC_SET|将semun.buf中的部分成员复制到信号量集关联的内核数据结构中，同时内核数据中的semid_ds.sem_ctime被更新|0|
|IPC_RMID|立即移除信号量集，唤醒所有等待该信号量集的进程(semop返回错误，并设置errno为EIDRM)|0|
|IPC_INFO|获取系统信号量资源配置信息，结果存在semun.__buf，含义见注释|内核信号量集数组中已被使用的项的最大索引值|
|SEM_INFO|与IPC_INFO类似，但semusz被设置为系统目前拥有的信号量集数目，semaem被设置为系统目前拥有的信号量数目|^|
|SEM_STAT|与IPC_STAT类似，但sem_id不表示信号量集标识符，而用来表示内核信号量集数组的索引|内核信号量数组中索引值为sem_id的信号量集的标识符|
|GETALL|将信号量集中的所有信号量导出到semun.array中|0|
|GETNCNT|获取信号量的semncnt值|信号量的semncnt值|
|GETPID|获取信号量的sempid值|信号量的sempid值|
|GETVAL|获取信号量的semval值|信号量的semval值|
|GETZCNT|获取信号量的semzcnt值|信号量的semzcnt值|
|SETALL|用semun.array中的数据填充由sem_id标识的信号量集中的所有信号量的semval值，同时内核数据中的semid_ds.sem_ctime被更新|0|
|SETVAL|将信号量的semval值更新为semun.val，同时内核数据中的semid_ds.sem_ctime被更新|0|

注意，GETNCNT、GETPID、GETVAL、GETZCNT和SETVAL操作的是单个信号量，它是由标识符sem_id指定的信号量集中的第sem_num个信号量；而其他操作针对的是整个信号量集，此时semctl的参数sem_num被忽略。

### 特殊键值IPC_PRIVATE

semget的调用者可以给其key参数传递一个特殊的键值IPC_PRIVATE（其值为0），这样无论该信号量是否已经存在，semget都将创建一个新的信号量。使用该键值创建的信号量并非像它的名字声称的那样是进程私有的。其他进程，尤其是子进程，也有方法来访问这个信号量。

## 共享内存

最高效的IPC机制，因为不涉及数据传输，需要与其他IPC方式一起使用来同步进程对共享内存的访问，避免产生竞态条件。

### shmget系统调用

创建一段新的共享内存，或获取已存在的共享内存

```cpp
#include <sys/shm.h>
int shmget(key_t key, size_t size, int shmflg);
```

- key 键值，用于标识一段全局唯一的共享内存
- size 共享内存大小，单位是字节。若获取已存在内存可以不指定
- shmflg 类似sem_flags，支持两个额外标志:
  - SHM_HUGETLB 类似mmap的MAP_HUGETLB标志，系统将使用大页面来为共享内存分配空间
  - SHM_NORESERVE 类似mmap的MAP_NORESERVE标志，不为共享内存保留交换分区(swap空间)。这样当物理内存不足时，对该共享内存执行写操作将触发SIGSEGV信号
- 成功返回正整数，是共享内存的标识符；失败返回-1并设置errno

创建共享内存时，所有字节都被初始化为0，关联的内核数据结构shmid_ds将被初始化。

```cpp
struct shmid_ds {
    struct ipc_perm shm_perm; /*共享内存的操作权限*/
    size_t shm_segsz; /*共享内存大小，单位是字节*/
    __time_t shm_atime; /*对这段内存最后一次调用shmat的时间*/
    __time_t shm_dtime; /*对这段内存最后一次调用shmdt的时间*/
    __time_t shm_ctime; /*对这段内存最后一次调用shmctl的时间*/
    __pid_t shm_cpid; /*创建者的PID*/
    __pid_t shm_lpid; /*最后一次执行shmat或shmdt操作的进程的PID*/
    shmatt_t shm_nattach; /*目前关联到此共享内存的进程数量*/
    /*省略一些填充字段*/
};
```

shmget对shmid_ds结构体的初始化包括:

- 将shm_perm.cuid和shm_perm.uid设置为调用进程的有效用户ID。
- 将shm_perm.cgid和shm_perm.gid设置为调用进程的有效组ID。
- 将shm_perm.mode的最低9位设置为shmflg参数的最低9位。
- 将shm_segsz设置为size。
- 将shm_lpid、shm_nattach、shm_atime、shm_dtime设置为0。
- 将shm_ctime设置为当前的时间。

### shmat和shmdt系统调用

共享内存被创建/获取之后，我们不能立即访问它，而是需要先将它关联到进程的地址空间中。使用完共享内存之后，我们也需要将它从进程地址空间中分离。(把物理内存块索引添加到进程的虚拟页表中)

```cpp
#include <sys/shm.h>

void *shmat(int shm_id, const void *shm_addr, int shmflg);
int shmdt(const void *shm_addr);
```

- shm_id 由shmget调用返回的共享内存标识符
- shm_addr 指定将共享内存关联到进程的哪块地址空间，最终的效果还受到shmflg参数的可选标志SHM_RND的影响:
  - 如果shm_addr为NULL，则被关联的地址由操作系统选择。这是推荐的做法，以确保代码的可移植性。
  - 如果shm_addr非空，并且SHM_RND标志未被设置，则共享内存被关联到addr指定的地址处。
  - 如果shm_addr非空，并且设置了SHM_RND标志，则被关联的地址是`[shm_addr-(shm_addr%SHMLBA)]`。SHMLBA的含义是“段低端边界地址倍数”(Segment Low Boundary Address Multiple)，它必须是内存页面大小(PAGE_SIZE)的整数倍。现在的Linux内核中，它等于一个内存页大小。SHM_RND的含义是圆整(round)，即将共享内存被关联的地址向下圆整到离shm_addr最近的SHMLBA的整数倍地址处。
- 除了SHM_RND标志外，shmflg参数还支持如下标志:
  - SHM_RDONLY。进程仅能读取共享内存中的内容。若没有指定该标志，则进程可同时对共享内存进行读写操作（当然还需要在创建共享内存的时候指定其读写权限）。
  - SHM_REMAP。如果地址shmaddr已经被关联到一段共享内存上，则重新关联。
  - SHM_EXEC。它指定对共享内存段的执行权限。对共享内存而言，执行权限实际上和读权限是一样的。
- shmat成功时返回共享内存被关联到的地址，失败则返回(void*)-1并设置errno。shmat成功时，将修改内核数据结构shmid_ds的部分字段，如下:
  - 将shm_nattach加1。
  - 将shm_lpid设置为调用进程的PID。
  - 将shm_atime设置为当前的时间。
- shmdt函数将关联到shm_addr处的共享内存从进程中分离。它成功时返回0，失败则返回-1并设置errno。shmdt在成功调用时将修改内核数据结构shmid_ds的部分字段，如下:
  - 将shm_nattach减1。
  - 将shm_lpid设置为调用进程的PID。
  - 将shm_dtime设置为当前的时间。

### shmctl系统调用

控制共享内存的某些属性

```cpp
#include <sys/shm.h>
int shmctl(int shm_id, int command, struct shmid_ds *buf);
```

- shmid 由shmget调用返回的共享内存标识符
- command 指定要执行的命令

    |命令|含义|成功时返回值|
    |-|-|-|
    |IPC_STAT|将共享内存相关的内核数据结构复制到buf中|0|
    |IPC_SET|将buf中部分成员复制到内核数据结构中，同时shm_ctime被更新|0|
    |IPC_RMID|将共享内存打上删除标记，当最后一个使用它的进程调用shmdt分离时，共享内存将被删除|0|
    |IPC_INFO|获取系统共享内存资源配置信息，结果存在buf中，应用程序需要将buf转换成shminfo结构体读取信息|内核共享内存信息数组中已被使用项的最大索引值|
    |SHM_INFO|与IPC_INFO类似，但返回的是已分配的共享内存占用系统资源的信息|^|
    |SHM_STAT|与IPC_STAT类似，但shm_id表示内核数据结构数组索引|内核共享内存信息数组中索引值为shm_id的共享内存的标识符|
    |SHM_LOCK|禁止共享内存被移动至交换分区|0|
    |SHM_UNLOCK|允许共享内存被移动至交换分区|0|

- 失败返回-1，并置errno

### 共享内存的POSIX方法

6.5节中介绍过mmap函数。利用它的MAP_ANONYMOUS标志我们可以实现父、子进程之间的匿名内存共享。通过打开同一个文件，mmap也可以实现无关进程之间的内存共享。Linux提供了另外一种利用mmap在无关进程之间共享内存的方式。这种方式无须任何文件的支持，但它需要先使用如下函数来创建或打开一个POSIX共享内存对象:

```cpp
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
int shm_open(const char *name, int oflag, mode_t mode);
```

使用方法与open系统调用完全相同

- name 指定要创建/打开的共享内存对象。从可移植性的角度考虑，该参数应该使用"/somename"的格式：以"/"开始，后接多个字符，且这些字符都不是"/"；以"＼0"结尾，长度不超过NAME_MAX（通常是255）。
- oflag参数指定创建方式。它可以是下列标志中的一个或者多个的按位或:
  - O_RDONLY。以只读方式打开共享内存对象。
  - O_RDWR。以可读、可写方式打开共享内存对象。
  - O_CREAT。如果共享内存对象不存在，则创建之。此时mode参数的最低9位将指定该共享内存对象的访问权限。共享内存对象被创建的时候，其初始长度为0。
  - O_EXCL。和O_CREAT一起使用，如果由name指定的共享内存对象已经存在，则shm_open调用返回错误，否则就创建一个新的共享内存对象。
  - O_TRUNC。如果共享内存对象已经存在，则把它截断，使其长度为0。
- shm_open调用成功时返回一个文件描述符。该文件描述符可用于后续的mmap调用，从而将共享内存关联到调用进程。shm_open失败时返回-1，并设置errno。
- 和打开的文件最后需要关闭一样，由shm_open创建的共享内存对象使用完之后也需要被删除。这个过程是通过如下函数实现的:

    ```cpp
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    int shm_unlink(const char *name);
    ```

    该函数将name参数指定的共享内存对象标记为等待删除。当所有使用该共享内存对象的进程都使用ummap将它从进程中分离之后，系统将销毁这个共享内存对象所占据的资源。

如果代码中使用了上述POSIX共享内存函数，则编译的时候需要指定链接选项-lrt。

### 共享内存实例

见shm.cpp

注意:

- 虽然我们使用了共享内存，但每个子进程都只会往自己所处理的客户连接所对应的那一部分读缓存中写入数据，所以我们使用共享内存的目的只是为了“共享读”。因此，每个子进程在使用共享内存的时候都无须加锁。这样做符合“聊天室服务器”的应用场景，同时提高了程序性能。
- 我们的服务器程序在启动的时候给数组users分配了足够多的空间，使得它可以存储所有可能的客户连接的相关数据。同样，我们一次性给数组sub_process分配的空间也足以存储所有可能的子进程的相关数据。这是牺牲空间换取时间的又一例子。

## 消息队列

在两个进程之间传递二进制块数据的一种简单有效的方式。每个数据块都有一个特定的类型，接收方可以根据类型来有选择地接收数据，而不一定像管道和命名管道那样必须以先进先出的方式接收数据。

### msgget系统调用

创建一个消息队列，或者获取一个已有的消息队列

```cpp
#include <sys/msg.h>
int msgget(key_t key, int msgflg);
```

- key参数是一个键值，用来标识一个全局唯一的消息队列。
- msgflg参数的使用和含义与semget系统调用的sem_flags参数相同。
- msgget成功时返回一个正整数值，它是消息队列的标识符。msgget失败时返回-1，并设置errno。

如果msgget用于创建消息队列，则与之关联的内核数据结构msqid_ds将被创建并初始化。msqid_ds结构体的定义如下:

```cpp
struct msqid_ds {
  struct ipc_perm msg_perm; /*消息队列的操作权限*/
  time_t msg_stime;         /*最后一次调用msgsnd的时间*/
  time_t msg_rtime;         /*最后一次调用msgrcv的时间*/
  time_t msg_ctime;         /*最后一次被修改的时间*/
  unsigned long__msg_cbytes;/*消息队列中已有的字节数*/
  msgqnum_t msg_qnum;       /*消息队列中已有的消息数*/
  msglen_t msg_qbytes;      /*消息队列允许的最大字节数*/
  pid_t msg_lspid;          /*最后执行msgsnd的进程的PID*/
  pid_t msg_lrpid;          /*最后执行msgrcv的进程的PID*/
};
```

### msgsnd系统调用

把一条消息添加到消息队列中

```cpp
#include <sys/msg.h>
int msgsnd(int msqid, const void *msg_ptr, size_t msg_sz, int msgflg);

struct msgbuf {
    long mtype;
    char mtext[512];
};
```

- msqid 消息队列标识符
- msg_ptr 指向一个准备发送的消息，类型为msgbuf
  - mtype 指定类型，必须是正整数(语义由用户定义?)
  - mtext 消息数据
- msg_sz 数据部分的长度，可以为0，表示没有消息数据
- msgflg 控制msgsnd的行为，通常仅支持IPC_NOWAIT标志，即以非阻塞的方式发送消息。默认情况下，发送消息时如果消息队列满了，则msgsnd将阻塞。若IPC_NOWAIT标志被指定，则msgsnd将立即返回并设置errno为EAGAIN。
- 处于阻塞状态的msgsnd调用可能被如下两种异常情况所中断:
  - 消息队列被移除。此时msgsnd调用将立即返回并设置errno为EIDRM。
  - 程序接收到信号。此时msgsnd调用将立即返回并设置errno为EINTR。
- msgsnd成功时返回0，失败则返回-1并设置errno。msgsnd成功时将修改内核数据结构msqid_ds的部分字段，如下所示:
  - 将msg_qnum加1。
  - 将msg_lspid设置为调用进程的PID。
  - 将msg_stime设置为当前的时间。

### msgrcv系统调用

从消息队列中获取信息

```cpp
#include <sys/msg.h>
int msgrcv(int msqid, void *msg_ptr, size_t msg_sz, long int msgtype, int msgflg);
```

- msqid 由msgget调用返回的消息队列标识符。
- msg_ptr 用于存储接收的消息
- msg_sz参数指的是消息数据部分的长度。
- msgtype参数指定接收何种类型的消息。我们可以使用如下几种方式来指定消息类型:
  - msgtype等于0。读取消息队列中的第一个消息。
  - msgtype大于0。读取消息队列中第一个类型为msgtype的消息（除非指定了标志MSG_EXCEPT，见后文）
  - msgtype小于0。读取消息队列中第一个类型值比msgtype的绝对值小的消息。
- msgflg 控制msgrcv函数的行为。它可以是如下一些标志的按位或:
  - IPC_NOWAIT。如果消息队列中没有消息，则msgrcv调用立即返回并设置errno为ENOMSG。
  - MSG_EXCEPT。如果msgtype大于0，则接收消息队列中第一个非msgtype类型的消息。
  - MSG_NOERROR。如果消息数据部分的长度超过了msg_sz，就将它截断。
- 处于阻塞状态的msgrcv调用还可能被如下两种异常情况所中断:
  - 消息队列被移除。此时msgrcv调用将立即返回并设置errno为EIDRM。
  - 程序接收到信号。此时msgrcv调用将立即返回并设置errno为EINTR。
- msgrcv成功时返回0，失败则返回-1并设置errno。msgrcv成功时将修改内核数据结构msqid_ds的部分字段，如下所示:
- 将msg_qnum减1。
- 将msg_lrpid设置为调用进程的PID。
- 将msg_rtime设置为当前的时间。

### msgctl系统调用

控制消息队列的某些属性

```cpp
#include <sys/msg.h>
int msgctl(int msqid, int cmd, struct msqid_ds *buf);
```

- msqid 由msgget调用返回的共享内存标识符
- command 指定要执行的命令

    |命令|含义|成功时返回值|
    |-|-|-|
    |IPC_STAT|将消息队列对应内核数据结构复制到buf中|0|
    |IPC_SET|将buf中部分成员复制到相关内核数据结构中，同时msg_ctime被更新|0|
    |IPC_RMID|立即移除消息队列，唤醒所有等待读消息和写消息的进程(立即返回并设errno为EIDRM)|0|
    |IPC_INFO|获取系统消息队列资源配置信息，将结果存在(msginfo *)buf中。|内核数据结构数组已使用项的最大索引值|
    |MSG_INFO|与IPC_INFO类似，但返回已分配的消息队列占用的资源信息|^|
    |MSG_STAT|与IPC_STAT类似，但msqid是内核数据队列数组的索引|该数据结构的标识符|

- 函数失败时返回-1并设置errno

## IPC命令

上述3种System V IPC进程间通信方式都使用一个全局唯一的键值（key）来描述一个共享资源。当程序调用semget、shmget或者msgget时，就创建了这些共享资源的一个实例。Linux提供了ipcs命令，以观察当前系统上拥有哪些共享资源实例。在ArchLinux上:

```shell
> sudo ipcs
--------- 消息队列 -----------
键        msqid      拥有者  权限     已用字节数 消息      

------------ 共享内存段 --------------
键        shmid      拥有者  权限     字节     连接数  状态      
0x00000000 32769      awei       606        9078480    2          目标       
0x00000000 32770      awei       606        9078480    2          目标       
0x00000000 32775      awei       606        7679028    2          目标       
0x00000000 32776      awei       606        7679028    2          目标       
0x00000000 32777      awei       606        37200      2          目标       
0x00000000 32778      awei       606        37200      2          目标       
0x00000000 32779      awei       606        87120      2          目标       
0x00000000 32780      awei       606        87120      2          目标       
0x00000000 32781      awei       606        139392     2          目标       
0x00000000 32782      awei       606        139392     2          目标       
0x00000000 32783      awei       606        222108     2          目标       
0x00000000 32784      awei       606        222108     2          目标       
0x00000000 32785      awei       606        241188     2          目标       
0x00000000 32786      awei       606        241188     2          目标       
0x00000000 32787      awei       606        256620     2          目标       
0x00000000 32788      awei       606        256620     2          目标       
0x00000000 32789      awei       606        104544     2          目标       
0x00000000 32790      awei       606        104544     2          目标       
0x00000000 32791      awei       606        264222     2          目标       
0x00000000 32792      awei       606        264222     2          目标       
0x00000000 32793      awei       606        227484     2          目标       
0x00000000 32794      awei       606        227484     2          目标       
0x00000000 32795      awei       606        286416     2          目标       
0x00000000 32796      awei       606        286416     2          目标       
0x00000000 32797      awei       606        230442     2          目标       
0x00000000 32798      awei       606        230442     2          目标       
0x00000000 31         awei       777        14745600   2                       
0x00000000 33         awei       606        3075300    2          目标       
0x00000000 34         awei       606        3075300    2          目标       
0x00000000 32806      awei       600        524288     2          目标       

--------- 信号量数组 -----------
键        semid      拥有者  权限     nsems     
0xe93c17d9 0          awei       666        1         
0xe67f3a0e 1          awei       600        1         
0xd0b52db4 2          awei       600        1         
0x619f9126 3          awei       600        1         
```

输出结果分段显示了系统拥有的共享内存、信号量和消息队列资源。

此外，我们可以使用ipcrm命令来删除遗留在系统中的共享资源。

## 在进程间传递文件描述符

由于fork调用之后，父进程中打开的文件描述符在子进程中仍然保持打开，所以文件描述符可以很方便地从父进程传递到子进程。需要注意的是，传递一个文件描述符并不是传递一个文件描述符的值，而是要在接收进程中创建一个新的文件描述符，并且该文件描述符和发送进程中被传递的文件描述符指向内核中相同的文件表项。

那么如何把子进程中打开的文件描述符传递给父进程呢？或者更通俗地说，如何在两个不相干的进程之间传递文件描述符呢？在Linux下，我们可以利用UNIX域socket在进程间传递特殊的辅助数据，以实现文件描述符的传递[^2]。代码清单13-5给出了一个实例，它在子进程中打开一个文件描述符，然后将它传递给父进程，父进程则通过读取该文件描述符来获得文件的内容。

见ipc_fd.cpp

[^1]: 虽然这种特殊的管道被专门命名为FIFO，但并不是只有这种管道才遵循先进先出的原则，其实所有的管道都遵循先进先出的原则。

[^2]: W. Richard Stevens，UNIX Network Programming Volume 1：The Sockets Networking API\[M].3rd ed
Boston：Addison Wesley，2003.
