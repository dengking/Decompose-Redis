# cnblogs [Redis源码解析：20sentinel(一)初始化、建链](https://www.cnblogs.com/gqtcgq/p/7247050.html)

> NOTE: 
>
> 一、首先需要了解拓扑结构
>
> jianshu [Redis哨兵（Sentinel）模式](https://www.jianshu.com/p/06ab9daf921d)
>
> ![img](https://upload-images.jianshu.io/upload_images/11320039-3f40b17c0412116c.png?imageMogr2/auto-orient/strip|imageView2/2/w/747/format/webp)

 sentinel(哨兵)是redis的高可用解决方案。由一个或多个sentinel实例组成的分布式系统，可以监控任意多个主节点，以及它们属下的所有从节点。当某个主节点下线时，sentinel可以将下线主节点属下的某个从节点升级为新的主节点。

## 一：哨兵进程

## 二：数据结构

### `struct sentinelState`

在哨兵模式中，最主要的数据结构就是`sentinelState`。

> NOTE: 
>
> 在`sentinel.c`中定义

```C
/* Main state. */
struct sentinelState {
    uint64_t current_epoch;     /* Current epoch. */
    dict *masters;      /* Dictionary of master sentinelRedisInstances.
                           Key is the instance name, value is the
                           sentinelRedisInstance structure pointer. */
    int tilt;           /* Are we in TILT mode? */
    int running_scripts;    /* Number of scripts in execution right now. */
    mstime_t tilt_start_time;   /* When TITL started. */
    mstime_t previous_time;     /* Last time we ran the time handler. */
    list *scripts_queue;    /* Queue of user scripts to execute. */
    char *announce_ip;      /* IP addr that is gossiped to other sentinels if
                               not NULL. */
    int announce_port;      /* Port that is gossiped to other sentinels if
                               non zero. */
} sentinel;
```

#### `sentinelState::masters`

在`sentinelState`结构中，最主要的成员就是字典`masters`。该字典中记录当前哨兵所要监控和交互的所有实例。这些实例包括主节点、从节点和其他哨兵。



`masters`字典以主节点的名字为key，以主节点实例结构`sentinelRedisInstance`为value。主节点的名字通过解析配置文件得到。

> NOTE: 
>
> 一、"主节点的名字通过解析配置文件得到"是什么意思？
>
> 参见下面的"初始化"章节可知: 用户需要在sentinel的配置文件中，指定由它监控的master节点；

### `struct sentinelRedisInstance`

而`sentinelRedisInstance`结构的定义如下：

````C
typedef struct sentinelRedisInstance {
    int flags;      /* See SRI_... defines */
    char *name;     /* Master name from the point of view of this sentinel. */
    char *runid;    /* run ID of this instance. */
    uint64_t config_epoch;  /* Configuration epoch. */
    sentinelAddr *addr; /* Master host. */
    redisAsyncContext *cc; /* Hiredis context for commands. */
    redisAsyncContext *pc; /* Hiredis context for Pub / Sub. */
    int pending_commands;   /* Number of commands sent waiting for a reply. */
    mstime_t cc_conn_time; /* cc connection time. */
    mstime_t pc_conn_time; /* pc connection time. */
    ...

    /* Master specific. */
    dict *sentinels;    /* Other sentinels monitoring the same master. */
    dict *slaves;       /* Slaves for this master instance. */
    ...

    /* Slave specific. */
    ...
    struct sentinelRedisInstance *master; /* Master instance if it's slave. */
    ...
    
    /* Failover */
    ...
    struct sentinelRedisInstance *promoted_slave; 
    ...
} sentinelRedisInstance;
````

> NOTE: 
>
> 一、在`sentinel.c`中定义；
>
> 在最新版的Redis中，上述结构体已经改变了

在哨兵模式中，所有的主节点、从节点以及哨兵实例，都是由`sentinelRedisInstance`结构表示的。

### TCP连接

哨兵会与其监控的所有主节点、该主节点下属的所有从节点，以及与之监控相同主节点的其他哨兵之间建立TCP连接。

哨兵与主节点和从节点之间会建立两个TCP连接，分别用于发送命令和订阅HELLO频道；

> NOTE: 
>
> 一、对应了如下两个`redisAsyncContext`
>
> ```C
> redisAsyncContext *cc; /* Hiredis context for commands. */
> redisAsyncContext *pc; /* Hiredis context for Pub / Sub. */
> ```
>
> 在最新版Redis中，它们定义在:
>
> ```C++
>  struct instanceLink 
> ```
>
> 

哨兵与其他哨兵之间只建立一个发送命令的TCP连接（因为哨兵本身不支持订阅模式）；

#### `sentinelRedisInstance::cc`、`sentinelRedisInstance::pc`

哨兵与其他节点进行通信，使用的是Hiredis中的异步方式。因此，`sentinelRedisInstance`结构中的`cc`，就是用于命令连接的异步上下文；而其中的`pc`，就是用于订阅连接的异步上下文；

### 主节点实例: `sentinelRedisInstance::sentinels`、`sentinelRedisInstance::slaves`

 除了公共部分，不同类型的实例还会有自己特有的属性。比如对于**主节点实例**而言，它的特有属性有：

1、`sentinels`字典：用于记录监控相同主节点其他哨兵实例。该字典以哨兵名字为key，以哨兵实例`sentinelRedisInstance`结构为key；

2、`slaves`字典：用于记录该主节点实例的所有从节点实例。该字典以从节点名字为key，以从节点实例`sentinelRedisInstance`结构为key；



因此总结而言就是：`sentinelState`结构中的字典`masters`中，记录了本哨兵要监控的所有主节点实例，而在表示每个主节点实例的`sentinelRedisInstance`结构中，字典`sentinels`中记录了监控该主节点的其他哨兵实例，字典`slaves`记录了该主节点的所有下属从节点。

这种设计方式非常巧妙，以**主节点**为核心，将当前哨兵所监控的实例进行分组，每个主节点及其属下的从节点和哨兵，组成一个监控单位，不同监控单位之间的流程是相互隔离的。

> NOTE: 
>
> 一、这是自然而然的设计，因为Redis sentinel的目的就是对主节点监控、automatic failover；因此，就需要知道这个主节点的从节点、其他监控这个主节点的sentinel；显然它们就构成了一个监控单位；显然这个监控单位的核心是"主节点"，这个监控单位就是为了保证这个主节点的HA的；
>
> 二、"不同监控单位之间的流程是相互隔离的"
>
> 需要注意的是，在Redis cluster被标准化之前，民间也是有cluster方案的；在民间的cluster方案中，就会同时存在多个master；

### 从节点实例

对于**从节点实例**而言，`sentinelRedisInstance`结构中也有一些它所各有的属性，比如`master`指针，就指向了它的主节点的`sentinelRedisInstance`结构；



`sentinelRedisInstance`结构中还包含与故障转移相关的属性，这在分析哨兵的故障转移流程的代码时会介绍。

## 三：初始化

在哨兵模式下，启动时必须指定一个配置文件，这也是哨兵模式和redis服务器不同的地方，哨兵模式不支持命令行方式的参数配置。

> NOTE: 
>
> 主要是指定需要由它监控的master

```shell
sentinel monitor mymaster 127.0.0.1 6379 2
sentinel down-after-milliseconds mymaster 60000
sentinel failover-timeout mymaster 180000
sentinel parallel-syncs mymaster 1

sentinel monitor resque 192.168.1.3 6380 4
sentinel down-after-milliseconds resque 10000
sentinel failover-timeout resque 180000
sentinel parallel-syncs resque 5
```



在配置文件中，只需要指定主节点的名字、ip和port信息，而从节点和其他哨兵的信息，都是在信息交互的过程中自动发现的。

> NOTE:
>
> Redis cluster中，通过gossip来进行自动发现



### `sentinel.c:sentinelHandleConfiguration`

在源代码`sentinel.c`中，函数`sentinelHandleConfiguration`就是用于解析哨兵配置选项的函数。



```C
char *sentinelHandleConfiguration(char **argv, int argc) {
    sentinelRedisInstance *ri;

    if (!strcasecmp(argv[0],"monitor") && argc == 5) {
        /* monitor <name> <host> <port> <quorum> */
        int quorum = atoi(argv[4]);

        if (quorum <= 0) return "Quorum must be 1 or greater.";
        if (createSentinelRedisInstance(argv[1],SRI_MASTER,argv[2],
                                        atoi(argv[3]),quorum,NULL) == NULL)
        {
            switch(errno) {
            case EBUSY: return "Duplicated master name.";
            case ENOENT: return "Can't resolve master instance hostname.";
            case EINVAL: return "Invalid port number";
            }
        }
    }
    ...
}
```

上面的代码，就是根据参数值，直接调用`createSentinelRedisInstance`函数，创建一个`SRI_MASTER`标记的主节点实例。



### `sentinel.c:createSentinelRedisInstance`

`createSentinelRedisInstance`函数的代码如下：

```C
sentinelRedisInstance *createSentinelRedisInstance(char *name, int flags, char *hostname, int port, int quorum, sentinelRedisInstance *master) {
    sentinelRedisInstance *ri;
    sentinelAddr *addr;
    dict *table = NULL;
    char slavename[128], *sdsname;

    redisAssert(flags & (SRI_MASTER|SRI_SLAVE|SRI_SENTINEL));
    redisAssert((flags & SRI_MASTER) || master != NULL);

    /* Check address validity. */
    addr = createSentinelAddr(hostname,port);
    if (addr == NULL) return NULL;

    /* For slaves and sentinel we use ip:port as name. */
    if (flags & (SRI_SLAVE|SRI_SENTINEL)) {
        snprintf(slavename,sizeof(slavename),
            strchr(hostname,':') ? "[%s]:%d" : "%s:%d",
            hostname,port);
        name = slavename;
    }

    /* Make sure the entry is not duplicated. This may happen when the same
     * name for a master is used multiple times inside the configuration or
     * if we try to add multiple times a slave or sentinel with same ip/port
     * to a master. */
    if (flags & SRI_MASTER) table = sentinel.masters;
    else if (flags & SRI_SLAVE) table = master->slaves;
    else if (flags & SRI_SENTINEL) table = master->sentinels;
    sdsname = sdsnew(name);
    if (dictFind(table,sdsname)) {
        releaseSentinelAddr(addr);
        sdsfree(sdsname);
        errno = EBUSY;
        return NULL;
    }

    /* Create the instance object. */
    ri = zmalloc(sizeof(*ri));
    /* Note that all the instances are started in the disconnected state,
     * the event loop will take care of connecting them. */
    ri->flags = flags | SRI_DISCONNECTED;
    ri->name = sdsname;
    ri->runid = NULL;
    ri->config_epoch = 0;
    ri->addr = addr;
    ri->cc = NULL;
    ri->pc = NULL;
    ...
    dictAdd(table, ri->name, ri);
    return ri;
}
```

参数`name`表示该实例的名字，主节点的名字在配置文件中配置的；从节点和哨兵的名字由hostname和port组成；

如果该实例为主节点，则参数`master`为NULL，最终会将该实例存放到字典`sentinel.masters`中；

如果该实例为从节点或哨兵，则参数`master`不能为NULL，将该实例存放到字典`master->slaves`或`master->sentinels`中；

> NOTE:
>
> 一、参数`master` 指的是函数的入参`master`

如果字典中已经存在同名实例，则设置errno为EBUSY，并且返回NULL，表示创建实例失败；

因此，解析完哨兵的配置文件之后，就已经把所有要监控的主节点实例插入到字典`sentinel.masters`中了。下一步，就是开始向主节点进行TCP建链了。

## 四：哨兵进程的“主函数”

在介绍哨兵进程的各种流程之前，需要先了解一下哨兵进程的“主函数”。

> NOTE: 
>
> 从后面可知，"哨兵进程的“主函数”"就是`sentinelHandleRedisInstance`

### `sentinelTimer`

在redis服务器中的定时器函数`serverCron`中，每隔100ms就会调用一次`sentinelTimer`函数。该函数就是哨兵进程的主要处理函数，哨兵中的所有流程都是在该函数中处理的。

```C
void sentinelTimer(void) {
    sentinelCheckTiltCondition();
    sentinelHandleDictOfRedisInstances(sentinel.masters);
    sentinelRunPendingScripts();
    sentinelCollectTerminatedScripts();
    sentinelKillTimedoutScripts();

    /* We continuously change the frequency of the Redis "timer interrupt"
     * in order to desynchronize every Sentinel from every other.
     * This non-determinism avoids that Sentinels started at the same time
     * exactly continue to stay synchronized asking to be voted at the
     * same time again and again (resulting in nobody likely winning the
     * election because of split brain voting). */
    server.hz = REDIS_DEFAULT_HZ + rand() % REDIS_DEFAULT_HZ;
}
```

最后，修改`server.hz`，增加其随机性，以避免投票选举时发生冲突；

> NOTE: 
>
> ```C
>     /* We continuously change the frequency of the Redis "timer interrupt"
>      * in order to desynchronize every Sentinel from every other.
>      * This non-determinism avoids that Sentinels started at the same time
>      * exactly continue to stay synchronized asking to be voted at the
>      * same time again and again (resulting in nobody likely winning the
>      * election because of split brain voting). */
>     server.hz = REDIS_DEFAULT_HZ + rand() % REDIS_DEFAULT_HZ;
> ```
>
> 这是raft算法的"随机超时"机制，在下面文章中进行了介绍:
>
> 1、csdn [用动图讲解分布式 Raft](https://blog.csdn.net/jackson0714/article/details/113144730?spm=1001.2014.3001.5501)

### `sentinelHandleDictOfRedisInstances`

`sentinelHandleDictOfRedisInstances`函数，是处理该哨兵中保存的所有实例的函数。它的代码如下：

```C
void sentinelHandleDictOfRedisInstances(dict *instances) {
    dictIterator *di;
    dictEntry *de;
    sentinelRedisInstance *switch_to_promoted = NULL;

    /* There are a number of things we need to perform against every master. */
    di = dictGetIterator(instances);
    while((de = dictNext(di)) != NULL) {
        sentinelRedisInstance *ri = dictGetVal(de);

        sentinelHandleRedisInstance(ri);
        if (ri->flags & SRI_MASTER) {
            sentinelHandleDictOfRedisInstances(ri->slaves);
            sentinelHandleDictOfRedisInstances(ri->sentinels);
            if (ri->failover_state == SENTINEL_FAILOVER_STATE_UPDATE_CONFIG) {
                switch_to_promoted = ri;
            }
        }
    }
    if (switch_to_promoted)
        sentinelFailoverSwitchToPromotedSlave(switch_to_promoted);
    dictReleaseIterator(di);
}
```

函数的最后，如果针对某个主节点，发起了故障转移流程，并且流程已经到了最后一步，则会调用函数`sentinelFailoverSwitchToPromotedSlave`进行处理；

### `sentinelHandleRedisInstance`

`sentinelHandleRedisInstance`函数，就是相当于哨兵进程的“主函数”。有关实例的几乎所有动作，都在该函数中进行的。该函数的代码如下：

## 五：建链

哨兵对于其所监控的所有**主节点**，及其属下的所有**从节点**，都会建立两个**TCP连接**。一个用于发送命令，一个用于订阅其**HELLO频道**。而哨兵对于监控**同一主节点**的其他哨兵实例，只建立一个**命令连接**。

哨兵向主节点和从节点建立的**订阅连接**，主要是为了监控同一主节点的所有哨兵之间，能够相互发现，以及交换信息。

> NOTE: 
>
> 为什么要专门使用一个特殊的连接？

