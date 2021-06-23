# Low level IO multiplex



## Cross-plateform and static polymorphism

1、Redis ae是能够支持多个Unix-like OS的，这就是本节标题"cross-plateform"的含义

2、由于各个Unix-like OS的IO multiplex/polling system call并不统一，因此Redis ae在各种implementation上建立了一层抽象层(interface)

3、通过static polymorphism来实现interface到implementation的dispatch



## Static polymorphism

Static polymorphism的实现依赖于如下:

1、plateform detection macro

2、void pointer type erasure(generic programming)

```C++
/* State of an event based program */
typedef struct aeEventLoop {

    void *apidata; /* This is used for polling API specific data */
};
```



3、建立abstraction(interface)、上层依赖于abstraction而不是implementation

## Interface

### `struct aeApiState` 

保存IO multiplex/polling object的resource handle

### `int aeApiCreate(aeEventLoop *eventLoop)`

重要是创建 `struct aeApiState` object，并将`struct aeApiState` object存放到`eventLoop.apidata`。

它会调用OS IO multiplex/polling system call。

### `int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask)`

添加一个event。

### `int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp)` 

顾名思义，进行轮训。





## Implementation

redis的ae的IO multiplex实现:



1、[`ae_evport.c`](https://github.com/antirez/redis/blob/unstable/src/ae_evport.c) 

2、[`ae_epoll.c`](https://github.com/antirez/redis/blob/unstable/src/ae_epoll.c) 

3、[`ae_kqueue.c`](https://github.com/antirez/redis/blob/unstable/src/ae_kqueue.c) 

4、[`ae_select.c`](https://github.com/antirez/redis/blob/unstable/src/ae_select.c) 



在`ae.c`中有如下代码：

```c
/* Include the best multiplexing layer supported by this system.
 * The following should be ordered by performances, descending. */
#ifdef HAVE_EVPORT
#include "ae_evport.c"
#else
    #ifdef HAVE_EPOLL
    #include "ae_epoll.c"
    #else
        #ifdef HAVE_KQUEUE
        #include "ae_kqueue.c"
        #else
        #include "ae_select.c"
        #endif
    #endif
#endif
```

在以上四个文件中，都提供了系统的API和data structure；这是c中实现类似于静态多态性的一种方式；



### `epoll`

查看了一下source code，Redis在使用`epoll`的时候，并没有指定edge trigger、level trigger，根据下面文章的内容可知，epoll默认是level trigger，也就是说，Redis使用的是level trigger。

1、chinaunix [epoll默认模式(LT），几乎总是触发EPOLL-OUT事件，怎么解决？](http://bbs.chinaunix.net/thread-1682066-1-1.html)

2、[epoll_ctl(2)](https://man7.org/linux/man-pages/man2/epoll_ctl.2.html) 

> Requests edge-triggered notification for the associated file descriptor.  The default behavior for epoll is level-triggered.  See epoll(7) for more detailed information about edge-triggered and level-triggered notification.



#### `struct aeApiState`

```C
#include <sys/epoll.h>

typedef struct aeApiState {
    int epfd; // epoll instance的file descriptor
    struct epoll_event *events; // 用于epoll_wait中，用于接收已经出发的事件 每个file descriptor都有一个对应的epoll_event，它是一个dynamic array
} aeApiState;
```

#### `aeApiCreate`

```C
static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;
    state->events = zmalloc(sizeof(struct epoll_event)*eventLoop->setsize);
    if (!state->events) {
        zfree(state);
        return -1;
    }
    state->epfd = epoll_create(1024); /* 1024 is just a hint for the kernel */
    if (state->epfd == -1) {
        zfree(state->events);
        zfree(state);
        return -1;
    }
    eventLoop->apidata = state;
    return 0;
}
```

#### `aeApiPoll`

```C++
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, numevents = 0;

    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval;
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j;

            if (e->events & EPOLLIN) mask |= AE_READABLE;
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE;
            if (e->events & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE;
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE;
            eventLoop->fired[j].fd = e->data.fd;
            eventLoop->fired[j].mask = mask;
        }
    }
    return numevents;
}
```



### `select`

下面以[`select`](https://en.wikipedia.org/wiki/Select_(Unix)) 为例来进行说明(因为APUE中所介绍的就是`select)。



#### `struct aeApiState`

```c
#include <sys/select.h>
#include <string.h>

typedef struct aeApiState {
    fd_set rfds, wfds; // rfds表示的是read file descriptor，wfds表示的是write file descriptor
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;
} aeApiState;
```





#### `aeApiCreate`

```c
static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    eventLoop->apidata = state; //设置apidata，从上面可以，目前 仅仅关注的是read和write
    return 0;
}
```





#### `aeApiPoll`

```c
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, j, numevents = 0;

    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    if (retval > 0) {
        for (j = 0; j <= eventLoop->maxfd; j++) { // process的file descriptor是依次递增的，所以此处可以使用for循环；
            int mask = 0;
            aeFileEvent *fe = &eventLoop->events[j];

            if (fe->mask == AE_NONE) continue;
            if (fe->mask & AE_READABLE && FD_ISSET(j,&state->_rfds))
                mask |= AE_READABLE;
            if (fe->mask & AE_WRITABLE && FD_ISSET(j,&state->_wfds))
                mask |= AE_WRITABLE;
            eventLoop->fired[numevents].fd = j;
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;
}
```



