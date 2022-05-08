# redis [Redis Cluster Specification](https://redis.io/topics/cluster-spec)

Welcome to the **Redis Cluster Specification**. Here you'll find information about algorithms and design rationales of **Redis Cluster**. This document is a work in progress as it is continuously synchronized with the actual implementation of Redis.



## Main properties and rationales of the design

### Redis Cluster goals

Redis Cluster is a distributed implementation of Redis with the following goals, in order of importance in the design:

1、High performance and linear scalability up to 1000 nodes. There are no proxies, **asynchronous replication** is used, and no merge operations are performed on values.

> NOTE: 
>
> 一、Redis都是使用的**asynchronous replication** 
>
> 二、关于 "no merge operations are performed on values"，参见 "Why merge operations are avoided" 章节

2、Acceptable degree of **write safety**: the system tries (in a best-effort way) to retain all the writes originating from clients connected with the majority of the **master nodes**. Usually there are small **windows** where acknowledged writes can be lost. Windows to lose acknowledged writes are larger when clients are in a **minority partition**.

> NOTE: 可接受的写安全性:系统尝试(以最佳方式)保留来自连接到大多数主节点的客户机的所有写。通常有一些小窗口，在那里可以丢失已确认的写操作。当客户端位于少数分区时，丢失已确认写操作的窗口会更大。

3、Availability: Redis Cluster is able to survive partitions where the majority of the **master nodes** are reachable and there is at least one reachable **slave** for every **master node** that is no longer reachable. Moreover using *replicas migration*, **masters** no longer replicated by any **slave** will receive one from a **master** which is covered by multiple slaves.

> NOTE: 
>
> 一、可用性:Redis集群能够在大多数主节点都可访问的分区中存活，并且对于每个不再可访问的主节点，至少有一个可访问的从属节点。此外，使用副本迁移，不再由任何奴隶复制的主人将从一个由多个奴隶覆盖的主人那里接收一个。
>
> 二、"replicas migration"指的是Redis cluster将一个拥有多个slave的master的其中一个slave指定为另外一个没有slave的master的slave

What is described in this document is implemented in Redis 3.0 or greater.



### Implemented subset

Redis Cluster implements all the single key commands available in the non-distributed version of Redis. Commands performing complex multi-key operations like Set type unions or intersections are implemented as well as long as the keys all belong to the **same node**.

Redis Cluster implements a concept called **hash tags** that can be used in order to force certain keys to be stored in the same node. However during manual reshardings, multi-key operations may become unavailable for some time while single key operations are always available.

Redis Cluster does not support multiple databases like the stand alone version of Redis. There is just database 0 and the [SELECT](https://redis.io/commands/select) command is not allowed.

> NOTE: 
>
> why?

### Clients and Servers roles in the Redis Cluster protocol

In Redis **Cluster nodes** are responsible for holding the data, and taking the **state** of the cluster, including mapping keys to the right nodes. **Cluster nodes** are also able to auto-discover other nodes, detect non-working nodes, and promote slave nodes to master when needed in order to continue to operate when a failure occurs.

> NOTE: 实现上，`cluster.h:struct clusterNode`用于描述cluster node，`cluster.h:struct clusterState`用于描述the state of the cluster；正如上面这段话中所描述的，每个cluster node都需要使用`cluster.h:struct clusterState`来记录the state of the cluster。

To perform their tasks all the **cluster nodes** are connected using a **TCP bus** and a **binary protocol**, called the **Redis Cluster Bus**. Every node is connected to every other node in the cluster using the **cluster bus**. Nodes use a **gossip protocol** 

1、to propagate information about the **cluster** in order to discover new nodes, 

2、to send **ping packets** to make sure all the other nodes are working properly, and 

3、to send **cluster messages** needed to signal specific conditions. 

The **cluster bus** is also used in order to propagate **Pub/Sub messages** across the cluster and to orchestrate **manual failovers** when requested by users (**manual failovers** are failovers which are not initiated by the Redis Cluster failure detector, but by the system administrator directly).

> NOTE: 
>
> 一、cluster node之间的连接方式是怎样的？
>
> "Every node is connected to every other node in the cluster using the **cluster bus**"
>
> 节点之间是相互连接的
>
> 上面说node use a gossip protocol，那能否据此推断出是两两互联？关于这个问题参见下面的topology。
>
> 二、总的来说，cluster node之间的通信:
>
> 1、**Redis Cluster Bus**
>
> 2、**gossip protocol** 

Since **cluster nodes** are not able to **proxy** requests, clients may be redirected to other nodes using redirection errors `-MOVED` and `-ASK`. The client is in theory free to send requests to all the nodes in the cluster, getting redirected if needed, so the client is not required to hold the state of the cluster. However clients that are able to cache the map between **keys** and **nodes** can improve the performance in a sensible way.



### Write safety

**Redis Cluster** uses **asynchronous replication** between nodes, and **last failover wins** implicit merge function. This means that the last elected master dataset eventually replaces all the other replicas. There is always a **window of time** when it is possible to lose writes during partitions. However these windows are very different in the case of a client that is connected to the majority of masters, and a client that is connected to the minority of masters.

> NOTE: 
>
> 一、Redis Cluster使用节点之间的异步复制，上次故障转移赢得隐式合并功能。 这意味着最后选出的主数据集最终将替换所有其他副本。 在分区期间可能会丢失写入时，总会有一个时间窗口。 然而，在连接到大多数主设备的客户端和连接到少数主设备的客户端的情况下，这些窗口是非常不同的。
>
> 二、"**last failover wins** implicit merge function"是什么含义呢？
>
> "the last elected master dataset eventually replaces all the other replicas" 即 所有的数据以最终"the last elected master"的数据为准



**Redis Cluster** tries harder to retain（保持） writes that are performed by clients connected to the **majority of masters**, compared to writes performed in the **minority side**. 

> NOTE: 
>
> 需要注意原文的写作思路: 原文是根据 majority partition 和 minority partition 来进行分类讨论的

#### In the majority partitions

The following are examples of scenarios that lead to loss of acknowledged writes received in the majority partitions during failures:

> NOTE: 
>
> "acknowledged writes" 指的是 master接收到了write请求

1、A write may reach a master, but while the master may be able to reply to the client, the write may not be propagated to slaves via the **asynchronous replication** used between **master** and **slave nodes**. If the **master** dies without the write reaching the slaves, the write is lost forever if the **master** is unreachable for a long enough period that one of its slaves is promoted. This is usually hard to observe in the case of a total, sudden failure of a master node since masters try to reply to clients (with the acknowledge of the write) and slaves (propagating the write) at about the same time. However it is a real world **failure mode**.

> NOTE: 
>
> 一、上面这段话其实描述地并不简介，让人难以抓住重要意思，其实场景非常简单:
>
> master接收到了write，并"reply to the client"，但是 "the write may not be propagated to slaves via the **asynchronous replication** used between **master** and **slave nodes**"；如果此时 "the **master** dies without the write reaching the slaves"，显然会进入failover流程，那么就存在这样的可能性:
>
> 一个没有同步刚刚的write的slave，被选举为新的master，那么 "the write is lost forever if the **master** is unreachable for a long enough period that one of its slaves is promoted"
>
> 在 csdn [Redis Cluster 会丢数据吗？](https://blog.csdn.net/duysh/article/details/103304134) 中，对这个场景有分析
>
> 二、思考: 如何保证不丢失呢？synchronous replication

2、Another theoretically possible failure mode where writes are lost is the following:

- A master is unreachable because of a partition.
- It gets failed over by one of its slaves.
- After some time it may be reachable again.
- A client with an **out-of-date routing table** may write to the old master before it is converted into a slave (of the new master) by the cluster.

The second failure mode is unlikely to happen because master nodes unable to communicate with the majority of the other masters for enough time to be failed over will no longer accept writes, and when the partition is fixed writes are still refused for a small amount of time to allow other nodes to inform about configuration changes. This failure mode also requires that the client's routing table has not yet been updated.

> NOTE: 
>
> 
>
> 一、"**master nodes** unable to communicate with the majority of the other masters for enough time to be failed over will no longer accept writes"
>
> 主语是 **master nodes**
>
> 定语是 "unable to communicate with the majority of the other masters for enough time to be failed over"
>
> 
>
> 二、在 "csdn [Redis Cluster 会丢数据吗？](https://blog.csdn.net/duysh/article/details/103304134) # 网络分区" 段中，给出了比较好的图示
>
> 
>
> 三、在partition恢复后，"writes are still refused for a small amount of time to allow other nodes to inform about configuration changes"
>
> 是谁继续"refuse write"？

#### In the minority side of a partition 

Writes targeting the minority side of a partition have a larger window in which to get lost. For example, Redis Cluster loses a non-trivial number of writes on partitions where there is a minority of masters and at least one or more clients, since all the writes sent to the masters may potentially get lost if the masters are failed over in the majority side.

Specifically, for a master to be failed over it must be unreachable by the majority of masters for at least `NODE_TIMEOUT`, so if the partition is fixed before that time, no writes are lost. When the partition lasts for more than `NODE_TIMEOUT`, all the writes performed in the minority side up to that point may be lost. However the minority side of a Redis Cluster will start refusing writes as soon as `NODE_TIMEOUT` time has elapsed without contact with the majority, so there is a maximum window after which the minority becomes no longer available. Hence, no writes are accepted or lost after that time.

> NOTE: 
>
> 感觉这种情况和前面的"2"是类似的

### Availability

> NOTE: 
>
> 一、"Redis Cluster is not available in the minority side of the partition"
>
> 需要对它进行解释，结合前面对内容、"csdn [Redis Cluster 会丢数据吗？](https://blog.csdn.net/duysh/article/details/103304134) # 网络分区" 中的内容，可知: 在 `NODE_TIMEOUT` 后， "the minority side of the partition"就不可用了
>
> 

Redis Cluster is not available in the minority side of the partition. In the majority side of the partition assuming that there are at least the majority of masters and a slave for every unreachable master, the cluster becomes available again after `NODE_TIMEOUT` time plus a few more seconds required for a slave to get elected and failover its master (failovers are usually executed in a matter of 1 or 2 seconds).

> NOTE: 
>
> 一、需要注意的是: majority side of the partition能够恢复是有前提条件的:
>
> 1、the majority of masters
>
> 2、slave for every unreachable master

This means that Redis Cluster is designed to survive failures of a few nodes in the cluster, but it is not a suitable solution for applications that require availability in the event of large net splits.

In the example of a cluster composed of N master nodes where every node has a single slave, the majority side of the cluster will remain available as long as a single node is partitioned away, and will remain available with a probability of `1-(1/(N*2-1))` when two nodes are partitioned away (after the first node fails we are left with `N*2-1` nodes in total, and the probability of the only master without a replica to fail is `1/(N*2-1))`.

For example, in a cluster with 5 nodes and a single slave per node, there is a `1/(5*2-1) = 11.11%` probability that after two nodes are partitioned away from the majority, the cluster will no longer be available.

> NOTE: 
>
> 在 segmentfault [Redis Cluster availability 分析](https://segmentfault.com/a/1190000039234661) 中，对上述计算过程进行了说明:
>
> 举个例子，包含 N 个 master 的集群，每个 master 有唯一 slave。
> 单个 node 出现故障，cluster 仍然可用，第二个 node 再出现故障，集群仍然可用的概率是 `1-(1/(N*2-1)`。
>
> 计算方式如下，第一个 node fail 后，集群剩下 `N*2-1` 个健康节点，此时 orphan master 恰好 fail 的概率是 `1/(N*2-1)`。
>
> > NOTE: orphan master 指的是没有slave的master，下面进行了说明
>
> 套用公式，假设 N = 5，那么，2 个节点从 majority partition 出去，集群不可用的概率是 11.11%。

Thanks to a Redis Cluster feature called **replicas migration** the Cluster availability is improved in many real world scenarios by the fact that replicas migrate to orphaned masters (masters no longer having replicas). So at every successful failure event, the cluster may reconfigure the slaves layout in order to better resist the next failure.

> NOTE: 
>
> 在  segmentfault [Redis Cluster availability 分析](https://segmentfault.com/a/1190000039234661) 中，对 **replicas migration** 的过程进行了详细说明

### Performance

In Redis Cluster nodes don't proxy commands to the right node in charge for a given key, but instead they redirect clients to the right nodes serving a given portion of the key space.

Eventually clients obtain an up-to-date representation of the cluster and which node serves which subset of keys, so during normal operations clients directly contact the right nodes in order to send a given command.

Because of the use of asynchronous replication, nodes do not wait for other nodes' acknowledgment of writes (if not explicitly requested using the [WAIT](https://redis.io/commands/wait) command).

Also, because multi-key commands are only limited to *near* keys, data is never moved between nodes except when resharding.

Normal operations are handled exactly as in the case of a single Redis instance. This means that in a Redis Cluster with N master nodes you can expect the same performance as a single Redis instance multiplied by N as the design scales linearly. At the same time the query is usually performed in a single round trip, since clients usually retain persistent connections with the nodes, so latency figures are also the same as the single standalone Redis node case.

Very high performance and scalability while preserving weak but reasonable forms of data safety and availability is the main goal of Redis Cluster.



### Why merge operations are avoided

> NOTE: 
>
> 一、在 "Write safety"中介绍了，Redis采用的是"**last failover wins** implicit merge function"
>
> 二、这一段则是介绍: 为什么Redis要这样做，通过原文的内容可知: 因为Redis的data type是比较复杂的、Redis的数据量是非常大的，如果采用merge的方式，那么复杂度将是非常高的，将造成 "bottleneck"

Redis Cluster design avoids conflicting versions of the same key-value pair in multiple nodes as in the case of the Redis data model this is not always desirable. Values in Redis are often very large; it is common to see lists or sorted sets with millions of elements. Also data types are semantically(语义) complex. Transferring and merging these kind of values can be a major bottleneck and/or may require the non-trivial involvement of application-side logic, additional memory to store meta-data, and so forth.

There are no strict technological limits here. CRDTs or synchronously replicated state machines can model complex data types similar to Redis. However, the actual run time behavior of such systems would not be similar to Redis Cluster. Redis Cluster was designed in order to cover the exact use cases of the non-clustered Redis version.

## Overview of Redis Cluster main components

### Keys distribution model

The **key space** is split into 16384 **slots**, effectively setting an upper limit for the cluster size of 16384 master nodes (however the suggested max size of nodes is in the order of ~ 1000 nodes).

> NOTE: Each master node in a cluster handles a subset of the 16384 hash slots. cluster中的所有node均分这16384个slot，每个node至少需要分得一个slot，所以cluster size的上限就是16374. `struct clusterNode`的`slots`成员变量保存这个node的所有的slot。

Each master node in a cluster handles a subset of the 16384 hash slots. The cluster is **stable** when there is no **cluster reconfiguration** in progress (i.e. where **hash slots** are being moved from one node to another). When the cluster is stable, a single **hash slot** will be served by a single node (however the serving node can have one or more slaves that will replace it in the case of net splits or failures, and that can be used in order to scale read operations where reading stale(旧) data is acceptable).

The base algorithm used to map keys to **hash slots** is the following (read the next paragraph for the hash tag exception to this rule):

```c++
HASH_SLOT = CRC16(key) mod 16384
```

The `CRC16` is specified as follows:

- Name: XMODEM (also known as ZMODEM or CRC-16/ACORN)
- Width: 16 bit
- Poly: 1021 (That is actually $x^{16} + x^{12} + x^{5} + 1$)
- Initialization: 0000
- Reflect Input byte: False
- Reflect Output CRC: False
- Xor constant to output CRC: 0000
- Output for "123456789": 31C3

14 out of 16 CRC16 output bits are used (this is why there is a modulo 16384 operation in the formula above).

In our tests CRC16 behaved remarkably well in distributing different kinds of keys evenly across the 16384 slots.

**Note**: A reference implementation of the CRC16 algorithm used is available in the Appendix A of this document.



> NOTE: 在代码中是由`cluser.c:keyHashSlot`函数实现的key space到hash slot的映射的；



### Keys hash tags

There is an exception for the computation of the hash slot that is used in order to implement **hash tags**. Hash tags are a way to ensure that multiple keys are allocated in the same hash slot. This is used in order to implement multi-key operations in Redis Cluster.

In order to implement hash tags, the hash slot for a key is computed in a slightly different way in certain conditions. If the key contains a "{...}" pattern only the substring between `{` and `}` is hashed in order to obtain the hash slot. However since it is possible that there are multiple occurrences of `{` or `}` the algorithm is well specified by the following rules:

- IF the key contains a `{` character.
- AND IF there is a `}` character to the right of `{`
- AND IF there are one or more characters between the first occurrence of `{` and the first occurrence of `}`.

Then instead of hashing the key, only what is between the first occurrence of `{` and the following first occurrence of `}`is hashed.

Examples:

- The two keys `{user1000}.following` and `{user1000}.followers` will hash to the same hash slot since only the substring `user1000` will be hashed in order to compute the hash slot.
- For the key `foo{}{bar}` the whole key will be hashed as usually since the first occurrence of `{` is followed by `}` on the right without characters in the middle.
- For the key `foo{{bar}}zap` the substring `{bar` will be hashed, because it is the substring between the first occurrence of `{` and the first occurrence of `}` on its right.
- For the key `foo{bar}{zap}` the substring `bar` will be hashed, since the algorithm stops at the first valid or invalid (without bytes inside) match of `{` and `}`.
- What follows from the algorithm is that if the key starts with `{}`, it is guaranteed to be hashed as a whole. This is useful when using binary data as key names.

Adding the hash tags exception, the following is an implementation of the `HASH_SLOT` function in Ruby and C language.

Ruby example code:

```python
def HASH_SLOT(key)
    s = key.index "{"
    if s
        e = key.index "}",s+1
        if e && e != s+1
            key = key[s+1..e-1]
        end
    end
    crc16(key) % 16384
end
```

C example code:

```c
unsigned int HASH_SLOT(char *key, int keylen) {
    int s, e; /* start-end indexes of { and } */

    /* Search the first occurrence of '{'. */
    for (s = 0; s < keylen; s++)
        if (key[s] == '{') break;

    /* No '{' ? Hash the whole key. This is the base case. */
    if (s == keylen) return crc16(key,keylen) & 16383;

    /* '{' found? Check if we have the corresponding '}'. */
    for (e = s+1; e < keylen; e++)
        if (key[e] == '}') break;

    /* No '}' or nothing between {} ? Hash the whole key. */
    if (e == keylen || e == s+1) return crc16(key,keylen) & 16383;

    /* If we are here there is both a { and a } on its right. Hash
     * what is in the middle between { and }. */
    return crc16(key+s+1,e-s-1) & 16383;
}
```



### Cluster nodes attributes

#### node ID /  node name 

> NOTE: 
>
> 需要注意，node ID就是node name

Every node has a unique name in the cluster. The **node name** is the hex representation of a 160 bit random number, obtained the first time a node is started (usually using `/dev/urandom`). The node will save its ID in the **node configuration file**, and will use the same ID forever, or at least as long as the node configuration file is not deleted by the system administrator, or a *hard reset* is requested via the [CLUSTER RESET](https://redis.io/commands/cluster-reset) command.

> NOTE: 
>
> 一、显然，node name需要保证不能够冲突；`cluster.c:struct clusterNode`的`name`成员变量就表示它的node name。
>
> 二、如何保证生成的node name不冲突呢？

The **node ID** is used to identify every node across the whole cluster. It is possible for a given node to change its IP address without any need to also change the **node ID**. The cluster is also able to detect the change in IP/port and **reconfigure** using the **gossip protocol** running over the cluster bus.

> NOTE: reconfigure cluster

The **node ID** is not the only information associated with each node, but is the only one that is always **globally consistent**（不会再改变）. Every node has also the following set of information associated. Some information is about the cluster configuration detail of this specific node, and is eventually consistent across the cluster. Some other information, like the last time a node was pinged, is instead local to each node.

Every node maintains the following information about other nodes that it is aware of in the cluster: The node ID, IP and port of the node, a set of flags, what is the master of the node if it is flagged as `slave`, last time the node was pinged and the last time the pong was received, the current *configuration epoch* of the node (explained later in this specification), the link state and finally the set of hash slots served.

> NOTE: 
>
> 一、cluster中的每个node还需要保存其他node的一些配置信息；那是哪些node呢？是cluster中的所有其他的node还是仅仅那些和它connect的node？关于这个问题，需要阅读下面的Cluster topology章节

A detailed [explanation of all the node fields](http://redis.io/commands/cluster-nodes) is described in the [CLUSTER NODES](https://redis.io/commands/cluster-nodes) documentation.

The [CLUSTER NODES](https://redis.io/commands/cluster-nodes) command can be sent to any node in the cluster and provides the state of the cluster and the information for each node according to the local view the queried node has of the cluster.

The following is sample output of the [CLUSTER NODES](https://redis.io/commands/cluster-nodes) command sent to a master node in a small cluster of three nodes.

### The Cluster bus

Every Redis Cluster node has an additional TCP port for receiving incoming connections from other Redis Cluster nodes. This port is at a fixed offset from the normal TCP port used to receive incoming connections from clients. To obtain the Redis Cluster port, 10000 should be added to the normal commands port. For example, if a Redis node is listening for client connections on port 6379, the Cluster bus port 16379 will also be opened.

Node-to-node communication happens exclusively using the **Cluster bus** and the **Cluster bus protocol**: a binary protocol composed of frames of different types and sizes. The **Cluster bus binary protocol** is not publicly documented since it is not intended for external software devices to talk with Redis Cluster nodes using this protocol. However you can obtain more details about the Cluster bus protocol by reading the `cluster.h` and `cluster.c` files in the Redis Cluster source code.



###  Cluster topology

**Redis Cluster** is a full mesh（网格） where every node is connected with every other node using a TCP connection.

> NOTE: 
>
> 关于full mesh，参见https://www.webopedia.com/TERM/M/mesh.html

In a cluster of N nodes, every node has N-1 outgoing（传出） TCP connections, and N-1 incoming（传入） connections.

> NOTE: 
>
> 每个node需要连接到cluster中的剩余N-1个node，同时cluster中的另外的N-1个node也要连接到它；
>
> slave是否也算是cluster中的node？slave是否也需要和cluster中的另外的node interconnect呢？

These TCP connections are kept alive all the time and are not created on demand. When a node expects a pong reply in response to a ping in the cluster bus, before waiting long enough to mark the node as unreachable, it will try to refresh the connection with the node by reconnecting from scratch.

While Redis Cluster nodes form a full mesh, **nodes use a gossip protocol and a configuration update mechanism in order to avoid exchanging too many messages between nodes during normal conditions**, so the number of messages exchanged is not exponential.

> NOTE: 
>
> 如何实现的呢？在后面的"Fault Tolerance # Heartbeat and gossip messages"章节中， 进行了说明:
>
> > Usually a node will ping a few random nodes every second so that the total number of ping packets sent (and pong packets received) by each node is a constant amount regardless of the number of nodes in the cluster.

### Nodes handshake

> NOTE: 
>
> 这一段其实告诉我们如何组建Redis cluster

Nodes always accept connections on the cluster bus port, and even reply to pings when received, even if the pinging node is not trusted. However, all other packets will be discarded by the receiving node if the sending node is not considered part of the cluster.

A node will accept another node as part of the cluster only in two ways:

1、If a node presents itself with a `MEET` message. A meet message is exactly like a [PING](https://redis.io/commands/ping) message, but forces the receiver to accept the node as part of the cluster. Nodes will send `MEET` messages to other nodes **only if** the system administrator requests this via the following command:

`CLUSTER MEET ip port`

> NOTE: 
>
> 一、[CLUSTER MEET ip port](https://redis.io/commands/cluster-meet)
>
> 二、显然，通过"CLUSTER MEET ip port" command来组建Redis cluster
>
> 三、这种方式是administrator来组建Redis cluster

2、A node will also register another node as part of the cluster if a node that is already trusted will gossip about this other node. So if A knows B, and B knows C, eventually B will send gossip messages to A about C. When this happens, A will register C as part of the network, and will try to connect with C.

> NOTE: 
>
> 一、这种方式是Redis node通过gossip协议来auto discover其他node，然后与它建立连接

This means that as long as we join nodes in any connected graph, they'll eventually form a fully connected graph automatically. This means that the cluster is able to auto-discover other nodes, but only if there is a trusted relationship that was forced by the system administrator.

> NOTE: 
>
> 一、必须要由administrator首先建立基本的连接，然后才能够实现全连接图

This mechanism makes the cluster more robust but prevents different Redis clusters from accidentally mixing after change of IP addresses or other network related events.



## Redirection and resharding



### MOVED Redirection

A Redis client is free to send queries to every node in the cluster, including **slave nodes**. The node will analyze the query, and if it is acceptable (that is, only a single key is mentioned in the query, or the multiple keys mentioned are all to the same **hash slot**) it will lookup what node is responsible for the **hash slot** where the key or keys belong.

If the **hash slot** is served by the node, the query is simply processed, otherwise the node will check its internal **hash slot to node map**（hash slot到node的映射关系）, and will reply to the client with a `MOVED` error, like in the following example:

```
GET x
-MOVED 3999 127.0.0.1:6381
```

The error includes the **hash slot** of the key (3999) and the `ip:port` of the instance that can serve the query. The client needs to reissue the query to the specified node's IP address and port. Note that even if the client waits a long time before reissuing the query, and in the meantime the cluster configuration changed, the destination node will reply again with a `MOVED` error if the **hash slot** 3999 is now served by another node. The same happens if the contacted node had no updated information.

So while from the point of view of the cluster nodes are identified by IDs we try to simplify our interface with the client just exposing a map between **hash slots** and **Redis nodes** identified by IP:port pairs.

The client is not required to, but should try to memorize that hash slot 3999 is served by 127.0.0.1:6381. This way once a new command needs to be issued it can compute the hash slot of the target key and have a greater chance of choosing the right node.

An alternative is to just refresh the whole **client-side cluster layout** using the [CLUSTER NODES](https://redis.io/commands/cluster-nodes) or [CLUSTER SLOTS](https://redis.io/commands/cluster-slots)commands when a `MOVED` redirection is received. When a redirection is encountered, it is likely multiple slots were reconfigured rather than just one, so updating the client configuration as soon as possible is often the best strategy.

Note that when the Cluster is stable (no ongoing changes in the configuration), eventually all the clients will obtain a map of hash slots -> nodes, making the cluster efficient, with clients directly addressing the right nodes without redirections, proxies or other single point of failure entities.

A client **must be also able to handle -ASK redirections** that are described later in this document, otherwise it is not a complete Redis Cluster client.

### Cluster live reconfiguration

> NOTE: 
>
> 一、这一段所描述的其实就是resharding
>
> 二、Redis使用的是hash tag，resharding的过程其实就是对hash slot的移动

Redis Cluster supports the ability to add and remove nodes while the cluster is running. Adding or removing a node is abstracted into the same operation: moving a hash slot from one node to another. This means that the same basic mechanism can be used in order to rebalance the cluster, add or remove nodes, and so forth.

1、To add a new node to the cluster an empty node is added to the cluster and some set of hash slots are moved from existing nodes to the new node.

2、To remove a node from the cluster the hash slots assigned to that node are moved to other existing nodes.

3、To rebalance the cluster a given set of hash slots are moved between nodes.

> NOTE: 
>
> 一、对Redis cluster进行CRUD:
>
> 1、选择节点
>
> 2、删除节点
>
> 3、修改节点
>
> 4、对节点进行查询

The core of the implementation is the ability to move **hash slots** around. From a practical point of view a hash slot is just a set of keys, so what Redis Cluster really does during *resharding* is to move keys from an instance to another instance. Moving a hash slot means moving all the keys that happen to hash into this hash slot.

To understand how this works we need to show the `CLUSTER` subcommands that are used to manipulate the ***slots translation table*** in a **Redis Cluster node**.

> NOTE: slots translation table应该是由`cluster.h:struct clusterState`的`migrating_slots_to` 、`importing_slots_from`、`slots`成员变量实现的；

The following subcommands are available (among others not useful in this case):

1、[CLUSTER ADDSLOTS](https://redis.io/commands/cluster-addslots) slot1 [slot2] ... [slotN]

2、[CLUSTER DELSLOTS](https://redis.io/commands/cluster-delslots) slot1 [slot2] ... [slotN]

3、[CLUSTER SETSLOT](https://redis.io/commands/cluster-setslot) slot NODE node

4、[CLUSTER SETSLOT](https://redis.io/commands/cluster-setslot) slot MIGRATING node

5、[CLUSTER SETSLOT](https://redis.io/commands/cluster-setslot) slot IMPORTING node

The first two commands, `ADDSLOTS` and `DELSLOTS`, are simply used to assign (or remove) slots to a Redis node. Assigning a slot means to tell a given **master node** that it will be in charge of storing and serving content for the specified hash slot.

After the hash slots are assigned they will propagate across the cluster using the gossip protocol, as specified later in the *configuration propagation* section.

> NOTE: 
>
> 一、是 "Hash slots configuration propagation" 段

#### `ADDSLOTS`

The `ADDSLOTS` command is usually used when a new cluster is created from scratch to assign each **master node** a subset of all the 16384 hash slots available.

####  `DELSLOTS`

The `DELSLOTS` is mainly used for manual modification of a cluster configuration or for debugging tasks: in practice it is rarely used.

#### `SETSLOT` 

The `SETSLOT` subcommand is used to assign a slot to a specific node ID if the `SETSLOT <slot> NODE` form is used. Otherwise the **slot** can be set in the two special states `MIGRATING` and `IMPORTING`. Those two special states are used in order to migrate a **hash slot** from one node to another.

1、When a slot is set as **MIGRATING**, the node will accept all queries that are about this **hash slot**, but only if the key in question exists, otherwise the query is forwarded using a `-ASK` redirection to the node that is target of the migration.

2、When a slot is set as **IMPORTING**, the node will accept all queries that are about this **hash slot**, but only if the request is preceded by an `ASKING` command. If the `ASKING` command was not given by the client, the query is redirected to the real hash slot owner via a `-MOVED` redirection error, as would happen normally.

#### Hash slot migration

Let's make this clearer with an example of **hash slot migration**. Assume that we have two Redis master nodes, called A and B. We want to move hash slot 8 from A to B, so we issue commands like this:

- We send B: CLUSTER SETSLOT 8 IMPORTING A
- We send A: CLUSTER SETSLOT 8 MIGRATING B

All the other nodes will continue to point clients to node "A" every time they are queried with a key that belongs to hash slot 8, so what happens is that:

- All queries about existing keys are processed by "A".
- All queries about non-existing keys in A are processed by "B", because "A" will redirect clients to "B".

This way we no longer create new keys in "A". In the meantime, a special program called `redis-trib` used during reshardings and Redis Cluster configuration will **migrate** existing keys in hash slot 8 from A to B. This is performed using the following command:

```
CLUSTER GETKEYSINSLOT slot count
```

The above command will return `count` keys in the specified **hash slot**. For every **key** returned, `redis-trib` sends node "A" a [MIGRATE](https://redis.io/commands/migrate) command, that will migrate the specified key from A to B in an **atomic way** (both instances are locked for the time (usually very small time) needed to migrate a key so there are no race conditions). This is how [MIGRATE](https://redis.io/commands/migrate)works:

```
MIGRATE target_host target_port key target_database id timeout
```

[MIGRATE](https://redis.io/commands/migrate) will connect to the **target instance**, send a serialized version of the key, and once an **OK code** is received, the old key from its own dataset will be deleted. From the point of view of an external client a **key** exists either in A or B at any given time.

In **Redis Cluster** there is no need to specify a database other than 0, but [MIGRATE](https://redis.io/commands/migrate) is a general command that can be used for other tasks not involving Redis Cluster. [MIGRATE](https://redis.io/commands/migrate) is optimized to be as fast as possible even when moving complex keys such as long lists, but in Redis Cluster reconfiguring the cluster where big keys are present is not considered a wise procedure if there are latency constraints in the application using the database.

When the migration process is finally finished, the `SETSLOT <slot> NODE <node-id>` command is sent to the two nodes involved in the migration in order to set the slots to their normal state again. The same command is usually sent to all other nodes to avoid waiting for the natural propagation of the new configuration across the cluster.

> NOTE: 移动key，是否需要移动该key对应的数据？需要的，要想完整地理解这一节的内容，需要阅读：[Redis系列九：redis集群高可用](https://www.cnblogs.com/leeSmall/p/8414687.html)

### ASK redirection





### Clients first connection and handling of redirections



### Multiple keys operations



### Scaling reads using slave nodes

> NOTE: 
>
> 这和读写分离有关，参见: 
>
> zhihu [redis需要读写分离吗？](https://www.zhihu.com/question/38768751)
>
> cnblogs [Redis读写分离技术解析](https://www.cnblogs.com/williamjie/p/11250713.html)
>
> 

Normally slave nodes will redirect clients to the authoritative master for the hash slot involved in a given command, however clients can use slaves in order to scale reads using the [READONLY](https://redis.io/commands/readonly) command.

[READONLY](https://redis.io/commands/readonly) tells a **Redis Cluster slave node** that the client is ok reading possibly stale data and is not interested in running write queries.

When the connection is in **readonly mode**, the cluster will send a redirection to the client only if the operation involves keys not served by the slave's master node. This may happen because:

1、The client sent a command about hash slots never served by the master of this slave.

2、The cluster was reconfigured (for example resharded) and the slave is no longer able to serve commands for a given hash slot.

When this happens the client should update its hashslot map as explained in the previous sections.

The readonly state of the connection can be cleared using the [READWRITE](https://redis.io/commands/readwrite) command.



## Fault Tolerance

### Heartbeat and gossip messages

Redis Cluster nodes continuously exchange ping and pong packets. Those two kind of packets have the same structure, and both carry important configuration information. The only actual difference is the **message type field**. We'll refer to the sum of ping and pong packets as *heartbeat packets*.

Usually nodes send **ping packets** that will trigger the receivers to reply with **pong packets**. However this is not necessarily true. It is possible for nodes to just send **pong packets** to send information to other nodes about their configuration, without triggering a reply. This is useful, for example, in order to broadcast a new configuration as soon as possible.

Usually a node will ping a few random nodes every second so that the total number of **ping packets** sent (and **pong packets** received) by each node is a constant amount regardless of the number of nodes in the cluster.

> NOTE: 
>
> 这是非常重要的，在 "Cluster topology" 章节中，就谈到了这个问题

However every node makes sure to ping every other node that hasn't sent a ping or received a pong for longer than half the `NODE_TIMEOUT` time. Before `NODE_TIMEOUT` has elapsed, nodes also try to reconnect the **TCP link** with another node to make sure nodes are not believed to be unreachable only because there is a problem in the current TCP connection.

> NOTE: 
>
> 一、需要注意的是，每个node不是每秒钟都去ping cluster中剩余的其他的所有的node，而是保证能够在half of the  `NODE_TIMEOUT`中能够ping到cluster中剩余的其他的所有的node，所以它需要做的是每秒钟只ping一部分。
>
> 二、为什么是half the `NODE_TIMEOUT` time？在下面的Failure detection章节中给出了这样做的原因。
>
> 

The number of messages globally exchanged can be sizable if `NODE_TIMEOUT` is set to a small figure and the number of nodes (N) is very large, since every node will try to ping every other node for which they don't have fresh information every half the `NODE_TIMEOUT` time.

For example in a 100 node cluster with a node timeout set to 60 seconds, every node will try to send 99 pings every 30 seconds, with a total amount of pings of 3.3 per second. Multiplied by 100 nodes, this is 330 pings per second in the total cluster.

There are ways to lower the number of messages, however there have been no reported issues with the bandwidth currently used by **Redis Cluster failure detection**, so for now the obvious and direct design is used. Note that even in the above example, the 330 packets per second exchanged are evenly divided among 100 different nodes, so the traffic each node receives is acceptable.



### Heartbeat packet content

Ping and pong packets contain a **header** that is common to all types of packets (for instance packets to request a **failover vote**), and a special **Gossip Section** that is specific of Ping and Pong packets.

The **common header** has the following information:

1、Node ID, a 160 bit pseudorandom string that is assigned the first time a node is created and remains the same for all the life of a Redis Cluster node.

2、The `currentEpoch` and `configEpoch` fields of the sending node that are used to mount the distributed algorithms used by Redis Cluster (this is explained in detail in the next sections). If the node is a slave the `configEpoch` is the last known `configEpoch` of its master.

3、The **node flags**, indicating if the node is a slave, a master, and other single-bit node information.

4、A bitmap of the hash slots served by the sending node, or if the node is a slave, a bitmap of the slots served by its master.

5、The sender **TCP base port** (that is, the port used by Redis to accept client commands; add 10000 to this to obtain the cluster bus port).

6、The state of the cluster from the point of view of the sender (down or ok).

7、The master node ID of the sending node, if it is a slave.



### Failure detection

**Redis Cluster failure detection** is used to recognize when a master or slave node is no longer reachable by the majority of nodes and then respond by promoting a slave to the role of master. When **slave promotion** is not possible the cluster is put in an **error state** to stop receiving queries from clients.

As already mentioned, every node takes a list of flags associated with other known nodes. There are two flags that are used for **failure detection** that are called `PFAIL` and `FAIL`. `PFAIL` means *Possible failure*, and is a non-acknowledged failure type. `FAIL` means that a node is failing and that this condition was confirmed by a majority of masters within a fixed amount of time.

> NOTE: 
>
> 阅读了下面的内容就能够明白最后这段话中关于`FAIL`的解释中的两个限制条件：
>
> 1、confirmed by a majority of masters 
>
> 2、within a fixed amount of time

#### PFAIL flag:

A node flags another node with the `PFAIL` flag when the node is not reachable for more than `NODE_TIMEOUT` time. Both master and slave nodes can flag another node as `PFAIL`, regardless of its type.

The concept of non-reachability for a Redis Cluster node is that we have an **active ping** (a ping that we sent for which we have yet to get a reply) pending for longer than `NODE_TIMEOUT`. For this mechanism to work the `NODE_TIMEOUT` must be large compared to the **network round trip time**. In order to add reliability during normal operations, nodes will try to reconnect with other nodes in the cluster as soon as half of the `NODE_TIMEOUT` has elapsed without a reply to a ping. This mechanism ensures that connections are kept alive so **broken connections** usually won't result in **false failure reports** between nodes.

#### FAIL flag:

The `PFAIL` flag alone is just local information every node has about other nodes, but it is not sufficient to trigger a **slave promotion**. For a node to be considered down（down表示fail了） the `PFAIL` condition needs to be escalated to a `FAIL` condition.

As outlined in the node heartbeats section of this document, every node sends **gossip messages** to every other node including the **state** of a few random known nodes. Every node eventually receives a set of node flags for every other node. This way every node has a mechanism to signal other nodes about **failure conditions** they have detected.

A `PFAIL` condition is escalated to a `FAIL` condition when the following set of conditions are met:

- Some node, that we'll call A, has another node B flagged as `PFAIL`.
- Node A collected, via gossip sections, information about the state of B from the point of view of the majority of masters in the cluster.
- The majority of masters signaled the `PFAIL` or `FAIL` condition within `NODE_TIMEOUT * FAIL_REPORT_VALIDITY_MULT` time. (The validity factor is set to 2 in the current implementation, so this is just two times the `NODE_TIMEOUT` time).

If all the above conditions are true, Node A will:

- Mark the node as `FAIL`.
- Send a `FAIL` message to all the reachable nodes.

The `FAIL` message will force every receiving node to mark the node in `FAIL` state, whether or not it already flagged the node in `PFAIL` state.

Note that *the FAIL flag is mostly one way*. That is, a node can go from `PFAIL` to `FAIL`, but a `FAIL` flag can only be cleared in the following situations:

- The node is already reachable and is a slave. In this case the `FAIL` flag can be cleared as slaves are not failed over.
- The node is already reachable and is a master not serving any slot. In this case the `FAIL` flag can be cleared as masters without slots do not really participate in the cluster and are waiting to be configured in order to join the cluster.
- The node is already reachable and is a master, but a long time (N times the `NODE_TIMEOUT`) has elapsed without any detectable slave promotion. It's better for it to rejoin the cluster and continue in this case.

It is useful to note that while the `PFAIL` -> `FAIL` transition uses a form of **agreement**, the agreement used is **weak**:

1. Nodes collect views of other nodes over some time period, so even if the majority of master nodes need to "agree", actually this is just **state** that we collected from different nodes at different times and we are not sure, nor we require, that at a given moment the majority of masters agreed. However we discard **failure reports** which are **old**, so the failure was signaled by the majority of masters within a **window of time**.
2. While every node detecting the `FAIL` condition will force that condition on other nodes in the cluster using the `FAIL` message, there is no way to ensure the message will reach all the nodes. For instance a node may detect the `FAIL` condition and because of a partition will not be able to reach any other node.

However the **Redis Cluster failure detection** has a **liveness requirement**: eventually all the nodes should agree about the state of a given node. There are two cases that can originate from split brain conditions. Either some minority of nodes believe the node is in `FAIL` state, or a minority of nodes believe the node is not in `FAIL` state. In both the cases eventually the cluster will have a single view of the state of a given node:

**Case 1**: If a majority of masters have flagged a node as `FAIL`, because of failure detection and the *chain effect* it generates, every other node will eventually flag the master as `FAIL`, since in the specified **window of time** enough failures will be reported.

**Case 2**: When only a minority of masters have flagged a node as `FAIL`, the **slave promotion** will not happen (as it uses a more formal algorithm that makes sure everybody knows about the promotion eventually) and every node will clear the `FAIL` state as per the `FAIL` state clearing rules above (i.e. no promotion after N times the `NODE_TIMEOUT` has elapsed).

**The FAIL flag is only used as a trigger to run the safe part of the algorithm** for the **slave promotion**. In theory a slave may act independently and start a **slave promotion** when its master is not reachable, and wait for the masters to refuse to provide the acknowledgment if the master is actually reachable by the majority. However the added complexity of the `PFAIL -> FAIL` state, the weak agreement, and the `FAIL` message forcing the propagation of the state in the shortest amount of time in the reachable part of the cluster, have practical advantages. Because of these mechanisms, usually all the nodes will stop accepting writes at about the same time if the cluster is in an error state. This is a desirable feature from the point of view of applications using Redis Cluster. Also erroneous election attempts initiated by slaves that can't reach its master due to local problems (the master is otherwise reachable by the majority of other master nodes) are avoided.



## Configuration handling, propagation, and failovers

> NOTE: 
>
> |                |                     |                                                              |
> | -------------- | ------------------- | ------------------------------------------------------------ |
> | `currentEpoch` |                     | Cluster current epoch 是 cluster 中所有的node的consensus共识 |
> | `configEpoch`  | Configuration epoch | 每个node的configuration                                      |
> |                |                     |                                                              |
>
> 

### Cluster current epoch

Redis Cluster uses a concept similar to the Raft algorithm "term". In Redis Cluster the term is called **epoch** instead, and it is used in order to give incremental versioning to events. When multiple nodes provide conflicting information, it becomes possible for another node to understand which state is the most up to date.

The `currentEpoch` is a 64 bit unsigned number.

At node creation every Redis Cluster node, both slaves and master nodes, set the `currentEpoch` to 0.

Every time a packet is received from another node, if the epoch of the sender (part of the cluster bus messages header) is greater than the local node epoch, the `currentEpoch` is updated to the sender epoch.

Because of these semantics, eventually all the nodes will agree to the greatest `configEpoch` in the cluster.

This information is used when the state of the cluster is changed and a node seeks agreement in order to perform some action.

Currently this happens only during **slave promotion**, as described in the next section. Basically the epoch is a **logical clock** for the cluster and dictates(表明) that given information wins over one with a smaller epoch.

> NOTE: 
>
> 一、raft算法
>
> 二、"epoch is a **logical clock** for the cluster"
>
> 三、consensus共识

### Configuration epoch

Every master always advertises(广为告知) its `configEpoch` in ping and pong packets along with a bitmap advertising the set of slots it serves.

The `configEpoch` is set to zero in masters when a new node is created.

A new `configEpoch` is created during **slave election**. Slaves trying to replace failing masters increment their epoch and try to get authorization from a majority of masters. When a slave is authorized, a new unique `configEpoch` is created and the slave turns into a master using the new `configEpoch`.

As explained in the next sections, the `configEpoch` helps to resolve conflicts when different nodes claim divergent（相异的） configurations (a condition that may happen because of network partitions and node failures).

Slave nodes also advertise(广为告知) the `configEpoch` field in ping and pong packets, but in the case of slaves the field represents the `configEpoch` of its master as of the last time they exchanged packets. This allows other instances to detect when a slave has an **old configuration** that needs to be updated (master nodes will not grant votes to slaves with an old configuration).

> NOTE: 上面这段话是比较不好理解的，它的意思是：Slave node在它们的ping and pong packets中也会带上`configEpoch`字段，但是在这种情况下，这个`configEpoch`字段

Every time the `configEpoch` changes for some known node, it is permanently stored in the `nodes.conf` file by all the nodes that receive this information. The same also happens for the `currentEpoch` value. These two variables are guaranteed to be saved and `fsync-ed` to disk when updated before a node continues its operations.

The `configEpoch` values generated using a simple algorithm during failovers are guaranteed to be new, incremental, and unique.



### Slave election and promotion

Slave election and promotion is handled by **slave nodes**, with the help of master nodes that vote for the slave to promote. A slave election happens when a master is in `FAIL` state from the point of view of at least one of its slaves that has the prerequisites（前提条件） in order to become a master.

In order for a slave to promote itself to master, it needs to start an election and win it. All the slaves for a given master can start an election if the master is in `FAIL` state, however only one slave will win the election and promote itself to master.

A slave starts an election when the following conditions are met:

- The slave's master is in `FAIL` state.
- The master was serving a non-zero number of slots.
- The slave **replication link** was disconnected from the master for no longer than a given amount of time, in order to ensure the promoted slave's data is reasonably fresh. This time is user configurable.

In order to be elected, the first step for a slave is to increment its `currentEpoch` counter, and request votes from master instances.

Votes are requested by the slave by broadcasting a `FAILOVER_AUTH_REQUEST` packet to every master node of the cluster. Then it waits for a maximum time of two times the `NODE_TIMEOUT` for replies to arrive (but always for at least 2 seconds).

Once a master has voted for a given slave, replying positively with a `FAILOVER_AUTH_ACK`, it can no longer vote for another slave of the same master for a period of `NODE_TIMEOUT * 2`. In this period it will not be able to reply to other authorization requests for the same master. This is not needed to guarantee safety, but useful for preventing multiple slaves from getting elected (even if with a different `configEpoch`) at around the same time, which is usually not wanted.

A slave discards any `AUTH_ACK` replies with an epoch that is less than the `currentEpoch` at the time the vote request was sent. This ensures it doesn't count votes intended for a previous election.

Once the slave receives ACKs from the majority of masters, it wins the election. Otherwise if the majority is not reached within the period of two times `NODE_TIMEOUT` (but always at least 2 seconds), the election is aborted and a new one will be tried again after `NODE_TIMEOUT * 4` (and always at least 4 seconds).

### Slave rank



### Masters reply to slave vote request





### Practical example of configuration epoch usefulness during partitions



### Hash slots configuration propagation



### UPDATE messages, a closer look



### How nodes rejoin the cluster



### Replica migration



### Replica migration algorithm



### configEpoch conflicts resolution algorithm



### Node resets



### Removing nodes from a cluster



## Publish/Subscribe





## Appendix





