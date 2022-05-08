# Redis  RDB implementation



## zhihu [Redis 持久化 RDB 原理](https://zhuanlan.zhihu.com/p/345233479)



### 四. RDB 原理

我们看下RDB过程中监控下系统调用情况：

```java
[root@localhost 7000]# ps -ef |grep redis
root       371  5245  0 14:15 pts/1    00:00:00 grep --color=auto redis
root     30969     1  1 13:42 ?        00:00:25 redis-server 0.0.0.0:7000
root     31617  5434  0 13:55 pts/2    00:00:00 redis-cli -p 7000

[root@localhost 7000]# strace -tt -f -o output.log -p 30969
```

#### save命令

strace命令会把 redis-server进程执行的系统调用给输出到output.log 中，然后我们再执行save命令，观察输出：

```java
30969 14:05:43.842462 read(8, "*1\r\n$4\r\nsave\r\n", 16384) = 14
30969 14:05:43.842663 open("temp-30969.rdb", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 10
30969 14:05:43.842955 fstat(10, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
30969 14:05:43.843127 mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f7f74a25000
30969 14:05:43.843457 write(10, "REDIS0008\372\tredis-ver\0064.0.10\372\nred"..., 4096) = 4096
30969 14:05:43.843848 write(10, "127.1711.1781.2321\303\22D\21\1AA\340\377\0\340\377\0\340"..., 4096) = 4096
30969 14:05:43.844230 write(10, "\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377"..., 4096) = 4096
30969 14:05:43.844547 write(10, "\340 \0\1AA\0\020127.231"..., 4096) = 4096
30969 14:05:43.844894 write(10, "71.721\303\fA\204\1AA\340\377\0\340o\0\1AA\0\020127.1691"..., 4096) = 4096
30969 14:05:43.845259 write(10, "\36GS\1AA\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0\340\16\0\1A"..., 4096) = 4096
30969 14:05:43.845591 write(10, ".1251.1801\303\36Gf\1AA\340\377\0\340\377\0\340\377\0\340\377\0\340\377\0"..., 4096) = 4096
....
....
30969 14:05:45.271929 fsync(10)         = 0
30969 14:05:45.372375 close(10)         = 0
30969 14:05:45.372606 munmap(0x7f7f74a25000, 4096) = 0
30969 14:05:45.372821 rename("temp-30969.rdb", "7000_dump.rdb") = 0
30969 14:05:45.373023 open("/usr/local/app/redis-cluster/7000/redis.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 10
30969 14:05:45.373221 lseek(10, 0, SEEK_END) = 1117342
30969 14:05:45.373381 stat("/etc/localtime", {st_mode=S_IFREG|0644, st_size=528, ...}) = 0
30969 14:05:45.373665 fstat(10, {st_mode=S_IFREG|0644, st_size=1117342, ...}) = 0
30969 14:05:45.373819 mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f7f74a25000
30969 14:05:45.374029 write(10, "30969:M 20 Jan 14:05:45.373 * DB"..., 47) = 47
30969 14:05:45.374196 close(10)         = 0
30969 14:05:45.374363 munmap(0x7f7f74a25000, 4096) = 0
```

从上面日志我们分析save执行RDB的几个重要的步骤：

1. read save : 读入save命令 。
2. open temp-30969.rdb ： 打开一个临时文件。
3. fstat ： 获取临时文件的详细信息。
4. mmap ： 将进程的一段地址 映射到 文件上，这样直接改进程的内存就直接反映到文件上了。
5. write ： 写redis数据到临时文件，每次写4096个字节**（4KB）**。
6. fsync ： 将上面write的数据实际写入磁盘， 这个 **fsync 需要好好讲一讲。下面会有介绍。**
7. close ： 关闭文件。
8. 将步骤4的 映射解除。
9. rename ： 重命名临时文件为 你指定的RDB文件名称。
10. open ：写一些日志到日志文件。发现写的内容是 30969:M 20 Jan 14:05:45.373 * DB...

#### bgsave命令

上面是save命令的结果，save是同步的，是需要阻塞主进程的（线上不要用）， bgsave据说是开的子进程来执行RDB的，我们来看下bgsave 的系统调用过程：

```java
30969 15:54:54.071907 read(8, "*1\r\n$6\r\nBGSAVE\r\n", 16384) = 16
30969 15:54:54.071955 pipe([9, 10])     = 0
30969 15:54:54.071989 fcntl(9, F_GETFL) = 0 (flags O_RDONLY)
30969 15:54:54.072014 fcntl(9, F_SETFL, O_RDONLY|O_NONBLOCK) = 0
30969 15:54:54.072044 clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f7f74a1ba10) = 5246
30969 15:54:54.072559 open("/usr/local/app/redis-cluster/7000/redis.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 11
30969 15:54:54.072602 lseek(11, 0, SEEK_END) = 1117483
30969 15:54:54.072632 stat("/etc/localtime", {st_mode=S_IFREG|0644, st_size=528, ...}) = 0
30969 15:54:54.072672 fstat(11, {st_mode=S_IFREG|0644, st_size=1117483, ...}) = 0
30969 15:54:54.072697 mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f7f74a25000
30969 15:54:54.072729 write(11, "30969:M 20 Jan 15:54:54.072 * Ba"..., 68) = 68
30969 15:54:54.072758 close(11)         = 0
30969 15:54:54.073053 write(8, "+Background saving started\r\n", 28) = 28
30969 15:54:54.073169 epoll_wait(5,  <unfinished ...>
5246  15:54:54.073311 set_robust_list(0x7f7f74a1ba20, 24) = 0
5246  15:54:54.073435 close(7)          = 0
5246  15:54:54.073514 open("temp-5246.rdb", O_WRONLY|O_CREAT|O_TRUNC, 0666) = 7
5246  15:54:54.073630 fstat(7, {st_mode=S_IFREG|0644, st_size=0, ...}) = 0
5246  15:54:54.073658 mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f7f74a25000
5246  15:54:54.074319 write(7, "REDIS0008\372\tredis-ver\0064.0.10\372\nred"..., 4096) = 4096
..... (这里是写redis数据到临时文件)
.....
.....
5246  15:54:54.807921 write(7, "\0\1AA\0\017127.01.371.1611\303'J\241\1AA\340\377\0\340"..., 2637) = 2637
5246  15:54:54.807952 fsync(7 <unfinished ...>
5246  15:54:54.925819 close(7)          = 0
5246  15:54:54.925904 munmap(0x7f7f74a25000, 4096) = 0
5246  15:54:54.925969 rename("temp-5246.rdb", "7000_dump.rdb") = 0
5246  15:54:54.926044 open("/usr/local/app/redis-cluster/7000/redis.log", O_WRONLY|O_CREAT|O_APPEND, 0666) = 7
5246  15:54:54.926122 lseek(7, 0, SEEK_END) = 1117551
。。。。。（这里是写日志，我就不列出来了）
5246  15:54:54.927601 close(7)          = 0
5246  15:54:54.927623 munmap(0x7f7f74a25000, 4096) = 0
5246  15:54:54.927651 write(10, "\0\0\0\0\0\0\0\0\0@\242\0\0\0\0\0xV4\22z\332}\301", 24) = 24
5246  15:54:54.927681 exit_group(0)     = ?
5246  15:54:54.927888 +++ exited with 0 +++
```

从上面日志我们分析bgsave执行RDB的几个重要的步骤：

1. **read save** : 读入BGSAVE命令 。
2. **pipe** : 建立与redis主进程通信的管道，9是读管道，10是写管道。
3. **fcntl(9, F_GETFL)** ： 获取文件描述符状态，flags O_RDONLY 是只读状态。
4. **fcntl(9, F_SETFL, O_RDONLY|O_NONBLOCK)** ： 设置文件描述符状态，nonblocking非阻塞状态。也就是设置读管道为非阻塞。
5. **clone** ： 克隆一个子进程执行RDB，pid为5246 。
6. **open ~ close(11)** : 这一块就是写日志。
7. **write(8)** : 8是第一步read的输出重定向，也就是向客户端输出

```java
127.0.0.1:7000> BGSAVE
Background saving started
```

\8. **open ~ fsync** : 打开临时文件 temp-5246.rdb ， 写入 redis 所有key ，value 。

\9. **rename** : 重命名临时文件7000_dump.rdb ， 也就是你指定RDB配置的文件。

\10. **write 10**: 10为我们第二步的主进程写管道，也就是向redis主进程发送 \0\0\0\0\0\0\0\0\0@\242\0\0\0\0\0xV4\22z\332}\301 ， 我估计是结束标志。

\11. **exit_group** ： 退出子进程中的所有线程 。

#### redis是使用clone来建子进程

```java
clone(child_stack=NULL, flags=CLONE_CHILD_CLEARTID|CLONE_CHILD_SETTID|SIGCHLD, child_tidptr=0x7f7f74a1ba10)=5246
```