# redis [Redis cluster tutorial](https://redis.io/topics/cluster-tutorial)

This document is a gentle introduction to Redis Cluster, that does not use complex to understand **distributed systems** concepts. It provides instructions about how to setup a cluster, test, and operate it, without going into the details that are covered in the [Redis Cluster specification](https://redis.io/topics/cluster-spec) but just describing how the system behaves from the point of view of the user.

However this tutorial tries to provide information about the **availability** and **consistency** characteristics of **Redis Cluster** from the point of view of the final user, stated in a simple to understand way.

Note this tutorial requires Redis version 3.0 or higher.

If you plan to run a serious Redis Cluster deployment, the more formal specification is a suggested reading, even if not strictly required. However it is a good idea to start from this document, play with **Redis Cluster** some time, and only later read the specification.



## Redis Cluster 101

Redis Cluster provides a way to run a Redis installation where data is **automatically sharded across multiple Redis nodes**.

Redis Cluster also provides **some degree of availability during partitions**, that is in practical terms the ability to continue the operations when some nodes fail or are not able to communicate. However the cluster stops to operate in the event of larger failures (for example when the majority of masters are unavailable).

***SUMMARY*** : 上面这段话中的partition的含义参见[Network partition](https://en.wikipedia.org/wiki/Network_partition)，关于availability，参见[High availability](https://en.wikipedia.org/wiki/High_availability)

So in practical terms, what do you get with Redis Cluster?

- The ability to **automatically split your dataset among multiple nodes**.
- The ability to **continue operations when a subset of the nodes are experiencing failures** or are unable to communicate with the rest of the cluster.

## Redis Cluster TCP ports

Every **Redis Cluster node** requires two TCP connections open. The normal Redis TCP port used to serve clients, for example 6379, plus the port obtained by adding 10000 to the **data port**, so 16379 in the example.

This second *high* port is used for the **Cluster bus**, that is a node-to-node communication channel using a **binary protocol**. The **Cluster bus** is used by nodes for failure detection, configuration update, failover authorization and so forth. Clients should never try to communicate with the **cluster bus port**, but always with the normal **Redis command port**, however make sure you open both ports in your firewall, otherwise **Redis cluster nodes** will be not able to communicate.

***SUMMARY*** : `struct redisServer`的成员`port`就表示上述的redis command port，成员`cluster_announce_port`就表示上述的cluster bus port。

The command port and cluster bus port offset is fixed and is always 10000.

Note that for a **Redis Cluster** to work properly you need, for each node:

1. The **normal client communication port** (usually 6379) used to communicate with clients to be open to all the clients that need to reach the cluster, plus all the other cluster nodes (that use the **client port** for keys migrations).
2. The **cluster bus port** (the client port + 10000) must be reachable from all the other cluster nodes.



If you don't open both TCP ports, your cluster will not work as expected.

The **cluster bus** uses a different, binary protocol, for node to node **data exchange**, which is more suited to exchange information between nodes using little bandwidth and processing time.

## Redis Cluster and Docker

Currently Redis Cluster does not support NATted environments and in general environments where IP addresses or TCP ports are remapped.

Docker uses a technique called *port mapping*: programs running inside Docker containers may be exposed with a different port compared to the one the program believes to be using. This is useful in order to run multiple containers using the same ports, at the same time, in the same server.

In order to make Docker compatible with Redis Cluster you need to use the **host networking mode** of Docker. Please check the `--net=host` option in the [Docker documentation](https://docs.docker.com/engine/userguide/networking/dockernetworks/) for more information.



## Redis Cluster data sharding

**Redis Cluster** does not use **consistent hashing**, but a different form of **sharding** where every key is conceptually part of what we call an **hash slot**.

There are 16384 **hash slots** in **Redis Cluster**, and to compute what is the hash slot of a given key, we simply take the `CRC16` of the key modulo 16384.

Every node in a Redis Cluster is responsible for a subset of the hash slots, so for example you may have a cluster with 3 nodes, where:

- Node A contains hash slots from 0 to 5500.
- Node B contains hash slots from 5501 to 11000.
- Node C contains hash slots from 11001 to 16383.

This allows to add and remove nodes in the cluster easily. For example if I want to add a new node D, I need to move some **hash slot** from nodes A, B, C to D. Similarly if I want to remove node A from the cluster I can just move the **hash slots** served by A to B and C. When the node A will be empty I can remove it from the cluster completely.

Because moving **hash slots** from a node to another does not require to stop operations, adding and removing nodes, or changing the percentage of **hash slots** hold by nodes, does not require any downtime（停机时间）.

**Redis Cluster** supports multiple key operations as long as all the keys involved into a single command execution (or whole transaction, or Lua script execution) all belong to the same **hash slot**. The user can force multiple keys to be part of the same hash slot by using a concept called *hash tags*.

**Hash tags** are documented in the **Redis Cluster specification**, but the gist（要点是） is that if there is a substring between {} brackets in a key, only what is inside the string is hashed, so for example `this{foo}key` and `another{foo}key` are guaranteed to be in the same **hash slot**, and can be used together in a command with multiple keys as arguments.

## Redis Cluster master-slave model

In order to remain available when a subset of **master nodes** are failing or are not able to communicate with the majority of nodes, **Redis Cluster** uses a **master-slave model** where every **hash slot** has from 1 (the master itself) to N replicas (N-1 additional slaves nodes).

In our example cluster with nodes A, B, C, if node B fails the cluster is not able to continue, since we no longer have a way to serve **hash slots** in the range 5501-11000.

However when the cluster is created (or at a later time) we add a **slave node** to every **master**, so that the final cluster is composed of A, B, C that are **masters nodes**, and A1, B1, C1 that are **slaves nodes**, the system is able to continue if node B fails.

Node B1 replicates B, and B fails, the cluster will promote node B1 as the new master and will continue to operate correctly.

However note that if nodes B and B1 fail at the same time **Redis Cluster** is not able to continue to operate.



## Redis Cluster consistency guarantees

Redis Cluster is not able to guarantee **strong consistency**. In practical terms this means that under certain conditions it is possible that Redis Cluster will lose writes that were acknowledged by the system to the client.

The first reason why Redis Cluster can lose writes is because it uses **asynchronous replication**. This means that during writes the following happens:

- Your client writes to the master B.
- The master B replies OK to your client.
- The master B propagates the write to its slaves B1, B2 and B3.

As you can see B does not wait for an **acknowledge** from B1, B2, B3 before replying to the client, since this would be a prohibitive（禁止的） latency penalty for Redis, so if your client writes something, B acknowledges the write, but crashes before being able to send the write to its slaves, one of the slaves (that did not receive the write) can be promoted to master, losing the write forever.

This is **very similar to what happens** with most databases that are configured to flush data to disk every second, so it is a scenario you are already able to reason about because of past experiences with traditional database systems not involving distributed systems. Similarly you can improve consistency by forcing the database to flush data on disk before replying to the client, but this usually results into prohibitively low performance. That would be the equivalent of **synchronous replication** in the case of Redis Cluster.

Basically there is a trade-off to take between **performance** and **consistency**.

**Redis Cluster** has support for **synchronous writes** when absolutely needed, implemented via the [WAIT](https://redis.io/commands/wait) command, this makes losing writes a lot less likely, however note that **Redis Cluster** does not implement **strong consistency** even when **synchronous replication** is used: it is always possible under more complex failure scenarios that a **slave** that was not able to receive the write is elected as **master**.

There is another notable scenario where **Redis Cluster** will lose writes, that happens during a **network partition** where a client is isolated with a minority of instances including at least a master.

***SUMMARY*** : what is network partition？

Take as an example our 6 nodes cluster composed of A, B, C, A1, B1, C1, with 3 masters and 3 slaves. There is also a client, that we will call Z1.

After a partition occurs, it is possible that in one side of the partition we have A, C, A1, B1, C1, and in the other side we have B and Z1.

Z1 is still able to write to B, that will accept its writes. If the partition heals in a very short time, the cluster will continue normally. However if the partition lasts enough time for B1 to be promoted to master in the majority side of the partition, the writes that Z1 is sending to B will be lost.

Note that there is a **maximum window** to the amount of writes Z1 will be able to send to B: if enough time has elapsed for the majority side of the partition to elect a slave as master, every master node in the minority side stops accepting writes.

This amount of time is a very important configuration directive of Redis Cluster, and is called the **node timeout**.

After **node timeout** has elapsed, a **master node** is considered to be failing, and can be replaced by one of its replicas. Similarly after **node timeout** has elapsed without a **master node** to be able to sense the majority of the other master nodes, it enters an error state and stops accepting writes.

还有另一个值得注意的情况是，Redis群集将丢失写入，这种情况发生在网络分区中，其中客户端与少数实例（至少包括主服务器）隔离。

以6个节点簇为例，包括A，B，C，A1，B1，C1，3个主站和3个从站。还有一个客户，我们称之为Z1。

在发生分区之后，可能在分区的一侧有A，C，A1，B1，C1，在另一侧有B和Z1。

Z1仍然可以写入B，它将接受其写入。如果分区在很短的时间内恢复，群集将继续正常运行。但是，如果分区持续足够的时间使B1在分区的多数侧被提升为主，则Z1发送给B的写入将丢失。

请注意，Z1将能够发送到B的写入量存在最大窗口：如果分区的多数方面已经有足够的时间将从属设备选为主设备，则少数端的每个主节点都会停止接受写入。

这段时间是Redis Cluster的一个非常重要的配置指令，称为节点超时。

节点超时过后，主节点被视为失败，可以由其中一个副本替换。类似地，在节点超时已经过去而主节点无法感知大多数其他主节点之后，它进入错误状态并停止接受写入。