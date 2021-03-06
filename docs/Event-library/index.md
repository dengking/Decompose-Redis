# Redis event library: ae



## [what does "ae" mean, short for?](https://redis-db.narkive.com/zj43Vnl5/what-does-ae-mean-short-for)

看了其中的回答，我觉得可能性更大的是**async events**



## 功能/需求

1、需要监控、轮训的事件

2、当事件发生时，需要执行的callback

3、能够cross plateform(Unix-like OS)

4、file、timer

## Implementation概述

1、需要依赖于OS提供的IO multiplex/polling system call来同时对多个file descriptor进行监控，这在`Low-level-IO-multiplex`章节进行描绘

2、基于low level IO multiplex中提供的abstract interface，ae建立起了比较灵活的、强大的、cross plateform的event library



## Callback

```c
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);
```



## ae data structure

ae的data structure主要定义在[`ae.h`](https://github.com/antirez/redis/blob/unstable/src/ae.h) 文件中。



### `struct aeFileEvent`

```c
/* File event structure */
typedef struct aeFileEvent {
    int mask; /* one of AE_(READABLE|WRITABLE|BARRIER) */
    aeFileProc *rfileProc; // 函数指针
    aeFileProc *wfileProc; // 函数指针
    void *clientData;
} aeFileEvent;
```

1、`struct aeFileEvent`描述的是需要监控的file event:

2、每个`struct aeFileEvent`object，都有一个对应的file descriptor、file；既然是file，那么显然对于file event，它的触发条件就是: readable、writeable；可以通过成员`mask`来控制到底是由readable、writeable来触发

3、显然，由`struct aeFileEvent`记录它的callback: `rfileProc`、`wfileProc`

4、需要注意的是，在`aeFileEvent`中并没有file descriptor成员变量，那它是如何和file descriptor进行管理的呢？

a、ae是将file descriptor作为索引，将`aeFileEvent`对象按照该索引存放如`struct aeEventLoop.events`中；

b、OS poll system call会保存file descriptor，并且在触发的时候，会传回用户传入的file descriptor

5、`mask`的功能是什么？描述的是监控的是readable、writeable



### `struct aeFiredEvent`



```C++
/* A fired event */
typedef struct aeFiredEvent {
    int fd;
    int mask;
} aeFiredEvent;
```

1、描述的是触发的event

2、可以看到，其中包含了file descriptor

### `struct aeEventLoop`

```c
/* State of an event based program */
typedef struct aeEventLoop {
    int maxfd;   /* highest file descriptor currently registered */
    int setsize; /* max number of file descriptors tracked */
    long long timeEventNextId;
    time_t lastTime;     /* Used to detect system clock skew */
    aeFileEvent *events; /* Registered events */
    aeFiredEvent *fired; /* Fired events */
    aeTimeEvent *timeEventHead;
    int stop; // 是否停止
    void *apidata; /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
    aeBeforeSleepProc *aftersleep;
} aeEventLoop;
```

1、`ae`暴露出的`aeCreateFileEvent`方法允许用户添加`FileEvent`

2、可以看到在`aeCreateFileEvent`中会更新`maxfd`成员变量，从而使`maxfd`始终记录最大的file descriptor

3、`apidata`是典型的type erasure，用于C中实现polymorphism



## File descriptors

1、在前面的"`struct aeFileEvent`"章节中，进行了说明



## redis-ae-application

1、voidcn [把redis源码的linux网络库提取出来，自己封装成通用库使用（★firecat推荐★）](http://www.voidcn.com/article/p-agorceqr-brv.html)