

# [`aeCreateEventLoop`](https://github.com/antirez/redis/blob/unstable/src/ae.c) 

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
    eventLoop->timeEventHead = NULL; // aeTimeEvent和aeFileEvent不同，使用linked list来保存它
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