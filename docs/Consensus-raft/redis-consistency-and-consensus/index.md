

# redis 最终一致性
[Redis源码解析：25集群(一)握手、心跳消息以及下线检测](https://www.cnblogs.com/gqtcgq/p/7247044.html)中提及Gossip是一个[**最终一致性算法**](https://en.wikipedia.org/wiki/Eventual_consistency)。

[Redis Sentinel Documentation](https://redis.io/topics/sentinel)中提及:

> In general Redis + Sentinel as a whole are a an **eventually consistent system** where the merge function is **last failover wins**, and the data from old masters are discarded to replicate the data of the current master, so there is always a window for losing acknowledged writes. This is due to Redis asynchronous replication and the discarding nature of the "virtual" merge function of the system. Note that this is not a limitation of Sentinel itself, and if you orchestrate the failover with a strongly consistent replicated state machine, the same properties will still apply. There are only two ways to avoid losing acknowledged writes:


# redis cluster structure

raft算法其实包含了多方面的设计，或者说这个算法解决了多个问题：
- Consensus问题，即cluster中的多个node对某个value达成共识 
- consistency，保证数据的一致性


redis cluster的结构是decentralized ，所以每个node为了知道其他node的信息必须要将它们保存下来，这就是为什么每个node都需要有一个`clusterState`，并且redis cluster是[High-availability cluster](https://en.wikipedia.org/wiki/High-availability_cluster)，即它需要支持failover，所以cluster需要有能够侦探cluster是否异常的能力，所以它需要不断地进行heartbeat，然后将得到的cluster中的其他node的状态保存起来；而raft算法的cluster结构是有centralized的，它是基于leader-follower的，不是一个decentralized的结构；

redis中，当cluster解决master election采用的是raft算法的实现思路；

# redis replication的consistency model

在[Replication](https://redis.io/topics/replication)中对这个问题进行了非常详细的介绍；



## Redis raft

[redis集群实现(五) sentinel的架构与raft协议](https://blog.csdn.net/sanwenyublog/article/details/53385616)

[redis集群实现(五) 集群一致性的保证-raft协议](http://www.voidcn.com/article/p-oajejwod-bcx.html)

[把 raft 做进 redis 的思考](https://zhuanlan.zhihu.com/p/28800722)

[Redis中算法之——Raft算法](https://www.cnblogs.com/tangtangde12580/p/8302185.html)

这篇文章收录了