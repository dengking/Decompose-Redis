# Redis event library

本文阅读Redis官方文档中给出的关于Redis event library的文档。



## Redis [event library](https://redis.io/topics/internals-eventlib)

> NOTE: 
> 1、这一段解释了为什么Redis需要event library
>
> 2、同时这段话解释清楚了如下内容:
>
> a、**network server** 
>
> b、**non-blocking** 
>
> 

### Why is an Event Library needed at all?

Let us figure it out through a series of Q&As.



Q: What do you expect a **network server** to be doing all the time? 

A: Watch for inbound connections on the port its listening and accept them.



Q: Calling [accept](http://man.cx/accept(2) accept) yields a descriptor. What do I do with it?

A: Save the descriptor and do a **non-blocking** read/write operation on it.



Q: Why does the read/write have to be **non-blocking**?

A: If the **file operation** ( even a socket in Unix is a file ) is blocking how could the server for example accept other connection requests when its blocked in a file I/O operation.

> NOTE: 解释清楚了为什么需要non-blocking

Q: I guess I have to do many such **non-blocking** operations on the **socket** to see when it's ready. Am I right?

A: Yes. That is what an **event library** does for you. Now you get it.



Q: How do Event Libraries do what they do?

A: They use the operating system's [polling](http://www.devshed.com/c/a/BrainDump/Linux-Files-and-the-Event-Poll-Interface/) facility along with **timers**.

> NOTE : 
>
> 1、上述的**timer**让我想到了我最近在programming的时候使用[`wait_until`](https://en.cppreference.com/w/cpp/thread/condition_variable/wait_until) 来替代`sleep`
>
> 2、 [polling](http://www.devshed.com/c/a/BrainDump/Linux-Files-and-the-Event-Poll-Interface/) 的链接的内容已经消失了，其实就是IO multiplex



Q: So are there any open source event libraries that do what you just described? 

A: Yes. `libevent` and `libev` are two such event libraries that I can recall off the top of my head.



Q: Does Redis use such open source event libraries for handling socket I/O?

A: No. For various [reasons](http://groups.google.com/group/redis-db/browse_thread/thread/b52814e9ef15b8d0/) Redis uses its own event library.



## [Redis Event Library](https://redis.io/topics/internals-rediseventlib)

> NOTE: 
>
> 1、这篇文章重要讲解Redis event library的implementation

Redis implements its own event library. The event library is implemented in `ae.c`.

The best way to understand how the **Redis event library** works is to understand how Redis uses it.



### Event Loop Initialization

> NOTE: 
>
> 1、redis的event loop中共定义了两类event：`aeFileEvent`，`aeTimeEvent`
>
> 2、需要处理listening port，从而建立新连接；需要处理已经建立的连接上的event
>
> 3、从后面的描绘来看，Event Loop Initialization主要做了如下内容:
>
> a、`aeCreateEventLoop`: 创建event loop 对象
>
> b、`aeCreateTimeEvent`: 注册 `serverCron`
>
> c、`aeCreateFileEvent`: 注册 listening port

`initServer` function defined in `redis.c` initializes the numerous fields of the `redisServer` structure variable. One such field is the **Redis event loop** `el`:

> NOTE: 
> 1、意思是: `redisServer` structure 有一个filed: **Redis event loop** `el`
>
> 2、`initServer` 函数顾名思义: 初始化 server的

```c
aeEventLoop *el
```

#### `aeEventLoop`

> NOTE: 
>
> 1、所有需要持续监控、poll的对象，都需要将它的descriptor添加都`server.el` (`struct aeEventLoop`)
>
> 

`initServer` initializes `server.el` field by calling `aeCreateEventLoop` defined in `ae.c`. The definition of `aeEventLoop` is below:

```c
typedef struct aeEventLoop
{
    int maxfd;
    long long timeEventNextId;
    aeFileEvent events[AE_SETSIZE]; /* Registered events */
    aeFiredEvent fired[AE_SETSIZE]; /* Fired events */
    aeTimeEvent *timeEventHead;
    int stop;
    void *apidata; /* This is used for polling API specific data */
    aeBeforeSleepProc *beforesleep;
} aeEventLoop;
```

#### `aeCreateEventLoop`

`aeCreateEventLoop` first `malloc`s `aeEventLoop` structure then calls `ae_epoll.c:aeApiCreate`.

#### `aeApiCreate` 

> NOTE:
>
> 1、下面的讲述重要是以Linux epoll为例的
>
> 2、Linux system call一般都是返回的resource handle，下面的"`epfd` that holds the `epoll` file descriptor returned by a call from [`epoll_create`](http://man.cx/epoll_create(2)) "就是典型的这种模式，这里的"`epoll` file descriptor "就是resource handle
>
> 3、`aeApiCreate` 就是调用OS的IO multiplex system call，以Linux OS为例，就是  [`epoll_create`](http://man.cx/epoll_create(2)) 
>
> 4、"`aeApiCreate` `malloc`s `aeApiState` " 返回的`aeApiState`  object存放在哪里？从后面的描述来看，是放在 `server.el` 中，那对应的是哪个field呢？是 `apidata` 字段

`aeApiCreate` `malloc`s `aeApiState` that has two fields - `epfd` that holds the `epoll` file descriptor returned by a call from [`epoll_create`](http://man.cx/epoll_create(2)) and `events` that is of type `struct epoll_event` define by the Linux `epoll` library. The use of the `events`field will be described later.

Next is `ae.c:aeCreateTimeEvent`. But before that `initServer` call `anet.c:anetTcpServer` that creates and returns a *listening descriptor*. The descriptor listens on *port 6379* by default. The returned *listening descriptor* is stored in `server.fd` field.

> NOTE: listening descriptor存放在`server.fd`，而不是和`epoll` file descriptor一起存放在`epfd`中；



#### `aeCreateTimeEvent`

`aeCreateTimeEvent` accepts the following as parameters:

1、`eventLoop`: This is `server.el` in `redis.c`

2、milliseconds: The number of milliseconds from the current time after which the timer expires.

3、`proc`: Function pointer. Stores the address of the function that has to be called after the timer expires.

4、`clientData`: Mostly `NULL`.

5、`finalizerProc`: Pointer to the function that has to be called before the timed event is removed from the list of timed events.

`initServer` calls `aeCreateTimeEvent` to add a **timed event** to `timeEventHead` field of `server.el`. `timeEventHead` is a pointer to a list of such timed events. The call to `aeCreateTimeEvent` from `redis.c:initServer` function is given below:

```c
aeCreateTimeEvent(server.el /*eventLoop*/, 1 /*milliseconds*/, serverCron /*proc*/, NULL /*clientData*/, NULL /*finalizerProc*/);
```

`redis.c:serverCron` performs many operations that helps keep Redis running properly.



#### `aeCreateFileEvent`

The essence of `aeCreateFileEvent` function is to execute [`epoll_ctl`](http://man.cx/epoll_ctl) system call which adds a watch for `EPOLLIN` event on the *listening descriptor* create by `anetTcpServer` and associate it with the `epoll` descriptor created by a call to `aeCreateEventLoop`.

Following is an explanation of what precisely `aeCreateFileEvent` does when called from `redis.c:initServer`.

`initServer` passes the following arguments to `aeCreateFileEvent`:

1、`server.el`: The event loop created by `aeCreateEventLoop`. The `epoll` descriptor is got from `server.el`.

2、`server.fd`: The *listening descriptor* that also serves as an index to access the relevant file event structure from the `eventLoop->events` table and store extra information like the callback function.

3、`AE_READABLE`: Signifies that `server.fd` has to be watched for `EPOLLIN` event.

4、`acceptHandler`: The function that has to be executed when the event being watched for is ready. This function pointer is stored in `eventLoop->events[server.fd]->rfileProc`.

This completes the initialization of Redis event loop.



### Event Loop Processing

`ae.c:aeMain` called from `redis.c:main` does the job of processing the **event loop** that is initialized in the previous phase.

`ae.c:aeMain` calls `ae.c:aeProcessEvents` in a while loop that processes **pending time and file events**.



#### `aeProcessEvents`

`ae.c:aeProcessEvents` looks for the time event that will be pending in the smallest amount of time by calling `ae.c:aeSearchNearestTimer` on the **event loop**. In our case there is only one **timer event** in the **event loop** that was created by `ae.c:aeCreateTimeEvent`.

Remember, that **timer event** created by `aeCreateTimeEvent` has by now probably elapsed because it had a expiry(过期) time of one millisecond. Since, the timer has already expired the seconds and microseconds fields of the `tvp` `timeval`structure variable is initialized to zero.

#### `aeApiPoll` 

The `tvp` structure variable along with the event loop variable is passed to `ae_epoll.c:aeApiPoll`.

`aeApiPoll` functions does a [`epoll_wait`](http://man.cx/epoll_wait) on the `epoll` descriptor and populates the `eventLoop->fired` table with the details:

1、`fd`: The descriptor that is now ready to do a read/write operation depending on the mask value.

2、`mask`: The read/write event that can now be performed on the corresponding descriptor.

`aeApiPoll` returns the number of such file events ready for operation. Now to put things in context, if any client has requested for a connection then `aeApiPoll` would have noticed it and populated the `eventLoop->fired` table with an entry of the descriptor being the *listening descriptor* and mask being `AE_READABLE`.

Now, `aeProcessEvents` calls the `redis.c:acceptHandler` registered as the callback. `acceptHandler` executes [accept](http://man.cx/accept) on the *listening descriptor* returning a *connected descriptor* with the client. `redis.c:createClient` adds a file event on the *connected descriptor* through a call to `ae.c:aeCreateFileEvent` like below:

```c
if (aeCreateFileEvent(server.el, c->fd, AE_READABLE,
    readQueryFromClient, c) == AE_ERR) {
    freeClient(c);
    return NULL;
}
```

`c` is the `redisClient` structure variable and `c->fd` is the connected descriptor.

Next the `ae.c:aeProcessEvent` calls `ae.c:processTimeEvents`



#### `processTimeEvents`

`ae.processTimeEvents` iterates over list of time events starting at `eventLoop->timeEventHead`.

For every timed event that has elapsed `processTimeEvents` calls the registered callback. In this case it calls the only timed event callback registered, that is, `redis.c:serverCron`. The callback returns the time in milliseconds after which the callback must be called again. This change is recorded via a call to `ae.c:aeAddMilliSeconds` and will be handled on the next iteration of `ae.c:aeMain` while loop.

That's all.