# zhihu [Redis 集群中的纪元(epoch)](https://zhuanlan.zhihu.com/p/44658603)

> NOTE: 
>
> redis的epoch，非常类似于lamport clock



## **纪元（epoch）**

Redis Cluster 使用了类似于 ***Raft*** 算法 ***term***（任期）的概念称为 ***epoch***（纪元），用来给**事件**增加**版本号**。Redis 集群中的纪元主要是两种：***currentEpoch*** 和 ***configEpoch***。

## **currentEpoch**

这是一个**集群状态**相关的概念，可以当做记录**集群状态**变更的递增版本号。每个集群节点，都会通过 `server.cluster->currentEpoch` 记录当前的 ***currentEpoch***。

集群节点创建时，不管是 **master** 还是 **slave**，都置 **currentEpoch** 为 0。当前节点接收到来自其他节点的包时，如果发送者的 **currentEpoch**（消息头部会包含发送者的 ***currentEpoch***）大于当前节点的***currentEpoch***，那么当前节点会更新 **currentEpoch** 为发送者的 ***currentEpoch***。因此，集群中所有节点的 ***currentEpoch*** 最终会达成一致，相当于对**集群状态**的认知达成了一致。

## **currentEpoch 作用**

**currentEpoch** 作用在于，当集群的状态发生改变，某个节点为了执行一些动作需要寻求其他节点的同意时，就会增加 ***currentEpoch*** 的值。目前 ***currentEpoch*** 只用于 **slave** 的故障转移流程，这就跟**哨兵**中的`sentinel.current_epoch` 作用是一模一样的。当 ***slave A*** 发现其所属的 ***master***下线时，就会试图发起故障转移流程。首先就是增加 **currentEpoch** 的值，这个增加后的 **currentEpoch** 是所有集群节点中最大的。然后**slave A** 向所有节点发起拉票请求，请求其他 ***master*** 投票给自己，使自己能成为新的 ***master***。其他节点收到包后，发现发送者的 ***currentEpoch*** 比自己的 ***currentEpoch*** 大，就会更新自己的 ***currentEpoch***，并在尚未投票的情况下，投票给 ***slave A***，表示同意使其成为新的 ***master***。

## **configEpoch**

这是一个集群**节点配置**相关的概念，每个集群节点都有自己独一无二的 configepoch。所谓的**节点配置**，实际上是指节点所负责的槽位信息。

每一个 **master** 在向其他节点发送包时，都会附带其 **configEpoch** 信息，以及一份表示它所负责的 **slots** 信息。而 **slave** 向其他节点发送包时，其包中的 ***configEpoch*** 和负责槽位信息，是其 **master** 的 **configEpoch** 和负责的 ***slot*** 信息。节点收到包之后，就会根据包中的 ***configEpoch***和负责的 ***slots*** 信息，记录到相应节点属性中。

## **configEpoch 作用**

**configEpoch** 主要用于解决不同的节点的配置发生冲突的情况。举个例子就明白了：节点A 宣称负责 **slot 1**，其向外发送的包中，包含了自己的 **configEpoch** 和负责的 **slots** 信息。节点 C 收到 A 发来的包后，发现自己当前没有记录 ***slot 1*** 的负责节点（也就是 server.cluster->slots[1] 为 NULL），就会将 A 置为 ***slot 1*** 的负责节点（server.cluster->slots[1] = A），并记录节点 A 的 **configEpoch**。后来，节点 C 又收到了 B 发来的包，它也宣称负责 ***slot 1***，此时，如何判断 ***slot 1*** 到底由谁负责呢？

这就是 **configEpoch** 起作用的时候了，C 在 B 发来的包中，发现它的 ***configEpoch***，要比 A 的大，说明 B 是更新的配置。因此，就将 **slot 1** 的负责节点设置为 B（server.cluster->slots[1] = B）。在 **slave** 发起选举，获得足够多的选票之后，成功当选时，也就是 **slave** 试图替代其已经下线的旧 **master**，成为新的 **master** 时，会增加它自己的 ***configEpoch***，使其成为当前所有集群节点的 ***configEpoch*** 中的最大值。这样，该 ***slave*** 成为 ***master*** 后，就会向所有节点发送广播包，强制其他节点更新相关 ***slots*** 的负责节点为自己。

## **参考资料**

1. [Redis Cluster Specification](https://link.zhihu.com/?target=https%3A//redis.io/topics/cluster-spec)
2. [Redis cluster tutorial](https://link.zhihu.com/?target=https%3A//redis.io/topics/cluster-tutorial)
3. [Redis系列九：redis集群高可用](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/leeSmall/p/8414687.html)
4. [Redis源码解析：27集群(三)主从复制、故障转移](https://link.zhihu.com/?target=https%3A//www.cnblogs.com/gqtcgq/p/7247042.html)

