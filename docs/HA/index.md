# HA

Redis的HA需要由如下三者协作来实现:

一、API

二、Sentinel cluster: 

功能:

1、下线检查、automatic failover(后面会进行详细介绍)

2、configuration provider

注意: 

sentinel本身是需要保证HA的，否则就会导致整个系统的single point of failure，所以需要使用sentinel cluster。

给API提供最新的master信息

三、Redis instance **replication**: master、slave之间需要进行replication





## 流程

### 参考文章

1、cnblogs [Redis中算法之——Raft算法](https://www.cnblogs.com/tangtangde12580/p/8302185.html)

2、cnblogs [gqtc](https://home.cnblogs.com/u/gqtcgq/) # [redis系列](https://www.cnblogs.com/gqtcgq/category/1043761.html)

[Redis源码解析：21sentinel(二)定期发送消息、检测主观下线](https://www.cnblogs.com/gqtcgq/p/7247048.html)

[Redis源码解析：22sentinel(三)客观下线以及故障转移之选举领导节点](https://www.cnblogs.com/gqtcgq/p/7247047.html)

[Redis源码解析：23sentinel(四)故障转移流程](https://www.cnblogs.com/gqtcgq/p/7247046.html)



### 总结

流程主要由Sentinel cluster来完成:

一、下线检查

二、Sentinel cluster通过raft算法选出Sentinel leader

三、由Sentinel leader从slave中，选择一个最优的作为新的master，然后执行一系列的其他操作

### cnblogs [Redis中算法之——Raft算法](https://www.cnblogs.com/tangtangde12580/p/8302185.html)

> NOTE: 
>
> 这篇文章讲述了Redis sentinel执行failover的 流程

Sentinel系统选举领头的方法是对Raft算法的领头选举方法的实现。

> NOTE: 
>
> leader election

在分布式系统中一致性是很重要的。1990年Leslie Lamport提出基于消息传递的一致性算法Paxos算法，解决分布式系统中就某个值或决议达成一致的问题。Paxos算法流程繁杂实现起来也比较复杂。

2013年斯坦福的Diego Ongaro、John Ousterhout两个人以易懂为目标设计一致性算法Raft。Raft一致性算法保障在任何时候一旦处于leader服务器当掉，可以在剩余正常工作的服务器中选举出新的Leader服务器更新新Leader服务器，修改从服务器的复制目标。

Sentinel是一个运行在特殊模式下的Redis服务器。它负责监视主服务器以及主服务器下的从服务器。当**领头Sentinel**认为主服务器已经进入主观下线状态，将对已下线的**主服务器**执行故障转移操作，该操作包括三个步骤：

（1）在已下线主服务器下的从服务器中挑选一个从服务器，将其转换为新的主服务器。

（2）让已下线主服务器属下的所有从服务器改为复制新的主服务器。

（3）将已下线主服务器成为新主服务器的从服务器。

Raft详解：http://www.cnblogs.com/likehua/p/5845575.html

分布式Raft算法：http://www.jdon.com/artichect/raft.html

分布式一致算法——Paxos：http://www.cnblogs.com/cchust/p/5617989.html

