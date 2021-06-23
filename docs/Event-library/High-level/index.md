# High level interface and implementation

## Source code

[`ae.h`](https://github.com/antirez/redis/blob/unstable/src/ae.h) 

[`ae.c`](https://github.com/antirez/redis/blob/unstable/src/ae.c) 



## Multiplex on time and file event 

参考 [aeProcessEvents](https://github.com/antirez/redis/blob/unstable/src/ae.c) 的source code可知: 

> Note that we want call `select()` even if there are no file events to process as long as we want to process time events, in order to sleep until the next time event is ready to fire.

查看下面的代码，可以发现：`aeApiPoll`是将time even和file event杂糅起来了，它能够在这两种事件上进行multiplex；查看APUE的14.4.1 select and pselect Functions中关于select系统调用可知，该系统调用是支持用户设置一个timeout的；由于文件事件是由OS进行管理，而时间事件是有ae库自己来进行维护，所以下面的代码会先自己来查找出需要处理的时间事件的最短的超时时间，然后将该时间作为select系统调用的超时时间；



## ae data structure



| data structure       |              |                                 |
| -------------------- | ------------ | ------------------------------- |
| `struct aeFileEvent` | `aeFileProc` | `#define AE_FILE_EVENTS (1<<0)` |
| `struct aeTimeEvent` | `aeTimeProc` | `#define AE_TIME_EVENTS (1<<1)` |
|                      |              |                                 |



### Callback function type



```c
typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);
typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);
typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);
typedef void aeBeforeSleepProc(struct aeEventLoop *eventLoop);
```



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
    aeFileEvent *events; /* Registered events */ // 在select 中会使用它
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

3、`apidata`是典型的type erasure，用于C中实现generic programming



## High level interface

### [aeMain](https://github.com/antirez/redis/blob/unstable/src/ae.c)

```C
void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        aeProcessEvents(eventLoop, AE_ALL_EVENTS|AE_CALL_AFTER_SLEEP);
    }
}

```

这就是event loop、main loop



### [aeProcessEvents](https://github.com/antirez/redis/blob/unstable/src/ae.c)

Process every **pending time event**, then every **pending file event**  (that may be registered by **time event callbacks** just processed).  Without special flags the function sleeps until some **file event** fires, or when the next **time event** occurs (if any).

If flags is 0, the function does nothing and returns. 

If flags has `AE_ALL_EVENTS` set, all the kind of events are processed. 

If flags has `AE_FILE_EVENTS` set, **file events** are processed. 

If flags has `AE_TIME_EVENTS` set, time events are processed. 

If flags has `AE_DONT_WAIT` set,  the function returns ASAP until all the events that's possible to process without to wait are processed(`AE_DONT_WAIT` 的含义是do not wait，它的含义是使event loop不去等待). 

If flags has `AE_CALL_AFTER_SLEEP` set, the aftersleep callback is called.

The function returns the number of events processed. 

```c
int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    int processed = 0, numevents;

    /* Nothing to do? return ASAP */
    if (!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS)) return 0;

    /* Note that we want call select() even if there are no
     * file events to process as long as we want to process time
     * events, in order to sleep until the next time event is ready
     * to fire. */
    // 查看下面的代码，可以发现：aeApiPoll是将time even和file event杂糅起来了，它能够在这两种事件上进行multiplex；查看APUE的14.4.1 select and pselect Functions中关于select系统调用可知，该系统调用是支持用户设置一个timeout的；由于文件事件是由OS进行管理，而时间事件是有ae库自己来进行维护，所以下面的代码会先自己来查找出需要处理的时间事件的最短的超时时间，然后将该时间作为select系统调用的超时时间；
    if (eventLoop->maxfd != -1 ||
        ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        // eventLoop->maxfd != -1 表示有文件事件
        //  ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT)) 表示要处理时间事件
        int j;
        aeTimeEvent *shortest = NULL;
        struct timeval tv, *tvp;

        if (flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
            shortest = aeSearchNearestTimer(eventLoop);
        if (shortest) { // 最近需要处理的事件
            long now_sec, now_ms;

            aeGetTime(&now_sec, &now_ms);
            tvp = &tv;

            /* How many milliseconds we need to wait for the next
             * time event to fire? */
            long long ms =
                (shortest->when_sec - now_sec)*1000 +
                shortest->when_ms - now_ms;

            if (ms > 0) {
                tvp->tv_sec = ms/1000;
                tvp->tv_usec = (ms % 1000)*1000;
            } else {
                tvp->tv_sec = 0;
                tvp->tv_usec = 0;
            }
        } else { // 表示没有时间事件
            /* If we have to check for events but need to return
             * ASAP because of AE_DONT_WAIT we need to set the timeout
             * to zero */
            if (flags & AE_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            } else {
                /* Otherwise we can block */
                tvp = NULL; /* wait forever */
            }
        }

        /* Call the multiplexing API, will return only on timeout or when
         * some event fires. */
        numevents = aeApiPoll(eventLoop, tvp);

        /* After sleep callback. */
        if (eventLoop->aftersleep != NULL && flags & AE_CALL_AFTER_SLEEP)
            eventLoop->aftersleep(eventLoop);

        for (j = 0; j < numevents; j++) {
            aeFileEvent *fe = &eventLoop->events[eventLoop->fired[j].fd];
            int mask = eventLoop->fired[j].mask;
            int fd = eventLoop->fired[j].fd;
            int fired = 0; /* Number of events fired for current fd. */

            /* Normally we execute the readable event first, and the writable
             * event laster. This is useful as sometimes we may be able
             * to serve the reply of a query immediately after processing the
             * query.
             *
             * However if AE_BARRIER is set in the mask, our application is
             * asking us to do the reverse: never fire the writable event
             * after the readable. In such a case, we invert the calls.
             * This is useful when, for instance, we want to do things
             * in the beforeSleep() hook, like fsynching a file to disk,
             * before replying to a client. */
            int invert = fe->mask & AE_BARRIER;

            /* Note the "fe->mask & mask & ..." code: maybe an already
             * processed event removed an element that fired and we still
             * didn't processed, so we check if the event is still valid.
             *
             * Fire the readable event if the call sequence is not
             * inverted. */
            if (!invert && fe->mask & mask & AE_READABLE) {
                fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                fired++;
            }

            /* Fire the writable event. */
            if (fe->mask & mask & AE_WRITABLE) {
                if (!fired || fe->wfileProc != fe->rfileProc) {
                    fe->wfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            /* If we have to invert the call, fire the readable event now
             * after the writable one. */
            if (invert && fe->mask & mask & AE_READABLE) {
                if (!fired || fe->wfileProc != fe->rfileProc) {
                    fe->rfileProc(eventLoop,fd,fe->clientData,mask);
                    fired++;
                }
            }

            processed++;
        }
    }
    /* Check time events */
    if (flags & AE_TIME_EVENTS)
        processed += processTimeEvents(eventLoop);

    return processed; /* return the number of processed file/time events */
}
```

看了上述代码，可以发现，`aeApiPoll`是将time even和file event杂糅起来了，它能够在这两种事件上进行multiplex。



### [`aeCreateEventLoop`](https://github.com/antirez/redis/blob/unstable/src/ae.c) 

```C
aeEventLoop *aeCreateEventLoop(int setsize) {
    aeEventLoop *eventLoop;
    int i;

    if ((eventLoop = zmalloc(sizeof(*eventLoop))) == NULL) goto err;
    eventLoop->events = zmalloc(sizeof(aeFileEvent)*setsize); // 首先就一次性为所有的aeFileEvent分配空间
    eventLoop->fired = zmalloc(sizeof(aeFiredEvent)*setsize);
    if (eventLoop->events == NULL || eventLoop->fired == NULL) goto err;
    eventLoop->setsize = setsize;
    eventLoop->lastTime = time(NULL);
    eventLoop->timeEventHead = NULL; // aeTimeEvent和aeFileEvent不同，使用double linked list来保存它
    eventLoop->timeEventNextId = 0;
    eventLoop->stop = 0;
    eventLoop->maxfd = -1;
    eventLoop->beforesleep = NULL;
    eventLoop->aftersleep = NULL;
    if (aeApiCreate(eventLoop) == -1) goto err;
    /* Events with mask == AE_NONE are not set. So let's initialize the
     * vector with it. */
    for (i = 0; i < setsize; i++)
        eventLoop->events[i].mask = AE_NONE;
    return eventLoop;

err:
    if (eventLoop) {
        zfree(eventLoop->events);
        zfree(eventLoop->fired);
        zfree(eventLoop);
    }
    return NULL;
}
```

入参`setsize`是原来初始化`struct aeEventLoop`的`setsize`字段的，其含义需要查看`ae.h`中定义的`struct aeEventLoop`，其中对其进行了解释；在redis中，`aeCreateEventLoop`是在`server.c`的`initServer`函数中调用，传入的值为`server.maxclients+CONFIG_FDSET_INCR`



### [`aeCreateTimeEvent`](https://github.com/antirez/redis/blob/unstable/src/ae.c)  

```C
long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc)
{
    long long id = eventLoop->timeEventNextId++;
    aeTimeEvent *te;

    te = zmalloc(sizeof(*te));
    if (te == NULL) return AE_ERR;
    te->id = id;
    aeAddMillisecondsToNow(milliseconds,&te->when_sec,&te->when_ms);
    te->timeProc = proc;
    te->finalizerProc = finalizerProc;
    te->clientData = clientData;
    te->prev = NULL;
    te->next = eventLoop->timeEventHead;
    if (te->next)
        te->next->prev = te;
    eventLoop->timeEventHead = te; // push_front
    return id;
}
```



### [`aeCreateFileEvent`](https://github.com/antirez/redis/blob/unstable/src/ae.c) 

```c
int aeCreateFileEvent(aeEventLoop *eventLoop, int fd, int mask,
        aeFileProc *proc, void *clientData)
{
    if (fd >= eventLoop->setsize) {
        errno = ERANGE;
        return AE_ERR;
    }
    aeFileEvent *fe = &eventLoop->events[fd];

    if (aeApiAddEvent(eventLoop, fd, mask) == -1)
        return AE_ERR;
    fe->mask |= mask;
    if (mask & AE_READABLE) fe->rfileProc = proc;
    if (mask & AE_WRITABLE) fe->wfileProc = proc;
    fe->clientData = clientData;
    if (fd > eventLoop->maxfd)
        eventLoop->maxfd = fd;
    return AE_OK;
}
```



1、ae是否支持在event loop已经启动的情况下，继续添加event？

肯定是可以的，因为:

a、accept后，需要创建新的的network connection，因此需要将这些network connection添加到ae中，ae后续需要对这些file descriptor进行监控

### `aeSearchNearestTimer`

`````c++
/* Search the first timer to fire.
 * This operation is useful to know how many time the select can be
 * put in sleep without to delay any event.
 * If there are no timers NULL is returned.
 *
 * Note that's O(N) since time events are unsorted.
 * Possible optimizations (not needed by Redis so far, but...):
 * 1) Insert the event in order, so that the nearest is just the head.
 *    Much better but still insertion or deletion of timers is O(N).
 * 2) Use a skiplist to have this operation as O(1) and insertion as O(log(N)).
 */
static aeTimeEvent *aeSearchNearestTimer(aeEventLoop *eventLoop)
{
    aeTimeEvent *te = eventLoop->timeEventHead;
    aeTimeEvent *nearest = NULL;

    while(te) { // 通过打擂台的方式，选择出的nearest是linked list中，时间值最小的，即最近的
        if (!nearest || te->when_sec < nearest->when_sec ||
                (te->when_sec == nearest->when_sec &&
                 te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}
`````

