[TOC]



# replication在cluster mode disabled和cluster mode enabled的情况下对比分析



## 实现分析

在`server.h:struct redisServer`中有成员变量`list *slaves`，显然对于master而言，这个成员变量是用于保存它的所有的slaves的；

在`cluster.h:struct clusterNode`中有成员变量`struct clusterNode **slaves`，显然这是用于保存这个node的所有的slaves的；

看到了这些，我想到的一个问题是：难道两种模式下，replication的实现是不同的？

需要注意的是，两者的类型是不一样的，`server.h:struct redisServer`的成员变量`list *slaves`中保存的元素的类型是`struct client`，而`cluster.h:struct clusterNode`的成员变量`struct clusterNode **slaves`中保存的元素的类型是`struct clusterNode`。

其实分析到这里，我猜测：cluster模型下的replication应该是基于普通的replication实现的，在cluster的实现中，应该会对普通的replication进行一定地封装，cluster的实现中，应该主要侧重于对cluster中node之间的关系的维护等；

 

## 构建分析

cluster中，指定某个节点为另外一个节点的replication：

[CLUSTER REPLICATE node-id](https://redis.io/commands/cluster-replicate)



非cluster中，指定一个instance为另外一个instance的replication：

[SLAVEOF host port](https://redis.io/commands/slaveof)