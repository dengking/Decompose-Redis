# 入口

从`cluster_announce_port`入手；发现了`cluster.c:clusterInit`中使用了它；`cluster.c:clusterInit`在`server.c:initServer`中被调用。

## 如何开启redis cluster？

在[Redis cluster tutorial](https://redis.io/topics/cluster-tutorial)中有介绍：

> # Redis Cluster configuration parameters
>
> We are about to create an example cluster deployment. Before we continue, let's introduce the configuration parameters that Redis Cluster introduces in the `redis.conf` file. Some will be obvious, others will be more clear as you continue reading.
>
> - **cluster-enabled <yes/no>**: If yes enables Redis Cluster support in a specific Redis instance. Otherwise the instance starts as a stand alone instance as usual.

在`config.c`中也是根据此配置项来初始化`struct redisServer.c:cluster_enabled`成员变量的，该成员变量表示是否启动redis cluster；

在`server.c:initServer`中有如下code：

```c
if (server.cluster_enabled) clusterInit();
```

显然，只有在配置文件中开启了cluster后，redis server才会在启动的时候执行`clusterInit()`。

关于如何构建redis cluster，参见[Redis系列九：redis集群高可用](https://www.cnblogs.com/leeSmall/p/8414687.html)。



# 数据结构

## `server.c:struct clusterState` 集群的状态

`server.c:struct clusterState`就是[replicated state machines](https://en.wikipedia.org/wiki/State_machine_replication)。



### 成员变量`currentEpoch`





## `server.c:struct clusterNode` 集群节点

### 成员变量`configEpoch`

对于每个集群中的node而言，其配置是cluster最最关心的，所以给他取名中带有`config`。



## 成员变量`currentEpoch` VS 成员变量`configEpoch`

在 [Redis Cluster Specification](https://redis.io/topics/cluster-spec)中有如下介绍：

- The `currentEpoch` and `configEpoch` fields of the sending node that are used to mount the distributed algorithms used by Redis Cluster (this is explained in detail in the next sections). If the node is a slave the `configEpoch` is the last known `configEpoch` of its master.

显然两者的相同点就是:mount the distributed algorithms used by Redis Cluster



在[Redis源码解析：27集群(三)主从复制、故障转移](https://www.cnblogs.com/gqtcgq/p/7247042.html)中有对`currentEpoch`和`configEpoch`有着非常详细的介绍，其实从它们的用途来看，就可以明白为什么`currentEpoch`置于`server.c:struct clusterNode`而`configEpoch`置于`server.c:struct clusterState`中。



