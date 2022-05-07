# Event and handler

一、完全event-driven，每种event都有对应的handler、callback

二、state machine、idempotent幂等

1、这是在阅读 [Redis源码解析：23sentinel(四)故障转移流程](https://www.cnblogs.com/gqtcgq/p/7247046.html) 时，其中 `sentinelFailoverStateMachine`

2、幂等，由于是 periodical event，因此会被不断地调用，因此就需要考虑idempotent幂等，可以看到，Redis中，很多的 periodical event handler都是考虑了idempotent，实现方式有:

state machine: sentinel

## file event

### `readQueryFromClient` handler

在创建client的时候设置

```c
client *createClient(int fd) {
    client *c = zmalloc(sizeof(client));

    /* passing -1 as fd it is possible to create a non connected client.
     * This is useful since all the commands needs to be executed
     * in the context of a client. When commands are executed in other
     * contexts (for instance a Lua script) we need a non connected client. */
    if (fd != -1) {
        anetNonBlock(NULL,fd);
        anetEnableTcpNoDelay(NULL,fd);
        if (server.tcpkeepalive)
            anetKeepAlive(NULL,fd,server.tcpkeepalive);
        if (aeCreateFileEvent(server.el,fd,AE_READABLE,
            readQueryFromClient, c) == AE_ERR)
        {
            close(fd);
            zfree(c);
            return NULL;
        }
    }
    
}
```
#### Redis `aeFileEvent` and `struct client`

server端接受到了client端的请求后才会创建`struct client` instance；

client端的socket file descriptor有对应的`aeFileEvent` 

上述就展示了client和event之间的关联




#### `readQueryFromClient`的实现分析

###### `readQueryFromClient`实现Redis Protocol



`readQueryFromClient`-->`processInputBufferAndReplicate`

`processInputBufferAndReplicate`-->`processInputBuffer`
                                -->`replicationFeedSlavesFromMasterStream`

`processInputBuffer`-->`processInlineBuffer`
                    -->`processMultibulkBuffer`




### `acceptTcpHandler`

```c
void initServer(void) {
    /* Create an event handler for accepting new connections in TCP and Unix
     * domain sockets. */
    for (j = 0; j < server.ipfd_count; j++) {
        if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE,
            acceptTcpHandler,NULL) == AE_ERR)
            {
                serverPanic(
                    "Unrecoverable error creating server.ipfd file event.");
            }
    }
    if (server.sofd > 0 && aeCreateFileEvent(server.el,server.sofd,AE_READABLE,
        acceptUnixHandler,NULL) == AE_ERR) serverPanic("Unrecoverable error creating server.sofd file event.");

    /* Register a readable event for the pipe used to awake the event loop
     * when a blocked client in a module needs attention. */
    if (aeCreateFileEvent(server.el, server.module_blocked_pipe[0], AE_READABLE,
        moduleBlockedClientPipeReadable,NULL) == AE_ERR) {
            serverPanic(
                "Error registering the readable event for the module "
                "blocked clients subsystem.");
    }


}
```



### `acceptUnixHandler`



### `moduleBlockedClientPipeReadable`





### replication

#### `replication.c:syncWithMaster`

```C++
    int fd;

    fd = anetTcpNonBlockBestEffortBindConnect(NULL,
        server.masterhost,server.masterport,NET_FIRST_BIND_ADDR);
    if (fd == -1) {
        serverLog(LL_WARNING,"Unable to connect to MASTER: %s",
            strerror(errno));
        return C_ERR;
    }

    if (aeCreateFileEvent(server.el,fd,AE_READABLE|AE_WRITABLE,syncWithMaster,NULL) ==
            AE_ERR)
    {
        close(fd);
        serverLog(LL_WARNING,"Can't create readable event for SYNC");
        return C_ERR;
    }
```



## time event

从目前redis的实现来看，`aeCreateTimeEvent`所创建的time event有：

- `server.c:serverCron`
- `module.c:moduleTimerHandler`

### `serverCron`

`serverCron`实现主要的按照time进行poll，它包含如下的一些poll：



#### `replicationCron`



#### poll backgroud process的状态

它会poll background saving process的状态，在它完成的时候，会调用`backgroundSaveDoneHandler`或者`backgroundRewriteDoneHandler`。参见《`redis-code-analysis-background process.md`》。



#### `clusterCron`



#### `databasesCron`

This function handles 'background' operations we are required to do incrementally in Redis databases, such as active key expiring, resizing, rehashing



#### `clientsCron`

> This function is called by `serverCron()` and is used in order to perform operations on clients that are important to perform constantly. For instancewe use this function in order to disconnect clients after a timeout, including clients blocked in some blocking command with a non-zero timeout.The function makes some effort to process all the clients every second, even if this cannot be strictly guaranteed, since `serverCron()` may be called with an actual frequency lower than `server.hz` in case of latency events like slowcommands.It is very important for this function, and the functions it calls, to be very fast: sometimes Redis has tens of hundreds of connected clients, and the default `server.hz` value is 10, so sometimes here we need to process thousandsof clients per second, turning this function into a source of latency.

