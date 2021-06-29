# Redis 最终一致性

素材: 

一、cnblogs [Redis源码解析：25集群(一)握手、心跳消息以及下线检测](https://www.cnblogs.com/gqtcgq/p/7247044.html)

其中提及Gossip是一个[**最终一致性算法**](https://en.wikipedia.org/wiki/Eventual_consistency)。

二、[Redis Sentinel Documentation](https://redis.io/topics/sentinel)中提及:

> In general Redis + Sentinel as a whole are a an **eventually consistent system** where the merge function is **last failover wins**, and the data from old masters are discarded to replicate the data of the current master, so there is always a window for losing acknowledged writes. This is due to Redis asynchronous replication and the discarding nature of the "virtual" merge function of the system. Note that this is not a limitation of Sentinel itself, and if you orchestrate the failover with a strongly consistent replicated state machine, the same properties will still apply. There are only two ways to avoid losing acknowledged writes:



三、Redis replication的consistency model

在redis [Replication](https://redis.io/topics/replication) 中对这个问题进行了非常详细的介绍:

> Synchronous replication of certain data can be requested by the clients using the [WAIT](https://redis.io/commands/wait) command. However [WAIT](https://redis.io/commands/wait) is only able to ensure that there are the specified number of acknowledged copies in the other Redis instances, it does not turn a set of Redis instances into a CP system with strong consistency: acknowledged writes can still be lost during a failover, depending on the exact configuration of the Redis persistence. However with [WAIT](https://redis.io/commands/wait) the probability of losing a write after a failure event is greatly reduced to certain hard to trigger failure modes.



四、raft eventual consistency