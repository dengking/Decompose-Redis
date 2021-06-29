# Redis cluster data sharding strategy

Redis cluster 采用的是 "hash slot" 的data sharding strategy，这在下面的文章中进行了说明:

1、redis doc [redis partition](https://redis.io/topics/partitioning)

2、redis [Redis Cluster Specification](https://redis.io/topics/cluster-spec)

3、csdn [Redis Cluster and Consistent Hashing](https://blog.csdn.net/xinzhongtianxia/article/details/81543838)

在这篇文章中也对Redis没有采用consistent hash进行了说明。

## Redis hash slot VS consistent hash

Redis采用的是hash slot，而不是consistent hash，我觉得Redis的hash  slot方案的优势是: 

**允许用户"keys-to-nodes map"进行灵活的控制(其实从另外一个角度来说，它的优势也是它的劣势)**

下面是对此的详细说明:

一、用户可以手动以**hash slot**为单位来在节点之间移动数据，从而实现

1、**load balance**；

二、在 "redis doc [redis partition](https://redis.io/topics/partitioning) # Data store or cache? "中也进行了介绍: 

> If Redis is used as a store, **a fixed keys-to-nodes map is used, so the number of nodes must be fixed and cannot vary**. Otherwise, a system is needed that is able to rebalance keys between nodes when nodes are added or removed, and currently only Redis Cluster is able to do this - Redis Cluster is generally available and production-ready as of [April 1st, 2015](https://groups.google.com/d/msg/redis-db/dO0bFyD_THQ/Uoo2GjIx6qgJ).



consistent hash的方案则完全是自动的，运维人员无法介入、无法对"keys-to-nodes map"进行控制。

### stackoverflow [does redis cluster use consistent hashing](https://stackoverflow.com/questions/50246763/does-redis-cluster-use-consistent-hashing)

I'm using redis cluster 3.0.1.

I think redis cluster use consistent hashing. The **hash slots** are similar to **virtual nodes** in **consistent hashing**. Cassandra's data distribution is almost the same as redis cluster, and [this article](https://docs.datastax.com/en/cassandra/3.0/cassandra/architecture/archDataDistributeHashing.html) said it's consistent hashing.

But [the redis cluster turorial](https://redis.io/topics/cluster-tutorial) said redis cluster does not use consistent hash.

> NOTE: 
>
> hash slot的确和virtual node类似，但是Redis的hash slot 和 node是可以由用户来进行指定的，而consistent hash则是无法的

#### [A](https://stackoverflow.com/a/50251141)

You are right, virtual nodes is quite simalar with hash slot.

But virtual nodes is not an original concept of consistent hashing, but more like a trick used by Cassandra based on consistent hashing. So it's also ok for redis to say not using consistent hashing.

> NOTE: 
>
> 从这段话可以看出，virtual node是Cassandra中使用的一个trick

So, don't bother with phraseology(术语).



## Redis doc [Partitioning: how to split data among multiple Redis instances](https://redis.io/topics/partitioning)

Partitioning is the process of splitting your data into multiple Redis instances, so that every instance will only contain a subset of your keys. The first part of this document will introduce you to the concept of partitioning, the second part will show you the alternatives for Redis partitioning.

> NOTE: 
>
> partition其实就是shard

###  Why partitioning is useful

> NOTE: 
>
> 其实就是distributed computing的优势





### Disadvantages of partitioning



### Partitioning basics

> NOTE: 
>
> 如何建立"key-node map"

#### Range partitioning

One of the simplest ways to perform partitioning is with **range partitioning**, and is accomplished by mapping ranges of objects into specific Redis instances. For example, I could say users from ID 0 to ID 10000 will go into instance **R0**, while users form ID 10001 to ID 20000 will go into instance **R1** and so forth.



#### Hash partitioning

### Different implementations of partitioning

Partitioning can be the responsibility of different parts of a software stack.

#### Client side partitioning



#### Proxy assisted partitioning



#### Query routing 

Redis Cluster implements an hybrid form of query routing, with the help of the client (the request is not directly forwarded from a Redis instance to another, but the client gets *redirected* to the right node).



### [Data store or cache?](https://redis.io/topics/partitioning#data-store-or-cache)

> NOTE: 
>
> Redis的定位是它不仅仅作为一个cache，它还可以作为data store，因此它的实现是需要考虑两者的需求的

Although partitioning in Redis is conceptually the same whether using Redis as a **data store** or as a **cache**, there is a significant limitation when using it as a data store. When Redis is used as a **data store**, a given key must always map to the same Redis instance. When Redis is used as a cache, if a given node is unavailable it is not a big problem if a different node is used, altering the key-instance map as we wish to improve the *availability* of the system (that is, the ability of the system to reply to our queries).

> NOTE: 
>
> 上面这段话的大意是: 从理论上来说，无论是将Redis作为 **data store** 或  **cache**，"partitioning"的本质都是相同的，上文然后以"将Redis作为data store"、"将Redis作为cache"分类讨论: 
>
> 一、当将Redis作为data store的时候，有一个非常重要的限制: 
>
> "a given key must always map to the same Redis instance"即"一个key，必须总是map到相同的Redis instance"，即key-instance map是不能够随意修改的，这就是后面所说的"a fixed keys-to-nodes map"
>
> 相反，当将Redis作为cache的时候，"if a given node is unavailable it is not a big problem if a different node is used, altering the key-instance map as we wish to improve the *availability* of the system (that is, the ability of the system to reply to our queries)"
>
> 括号中的内容是对 ***availability*** 进行解释的；
>
> 二、当将Redis作为cache的时候，可以随意的修改key-instance map，只要能够保证 *availability* 即可；
>
> 显然，通过上述分析可知: 由于Redis既需要满足data store的需求又需要满足cache的需求，因此Redis cluster没有采用consistent hash的方案，而是采用的hash slot的方案，这种方案允许用户对"keys-to-nodes map"进行灵活控制

Consistent hashing implementations are often able to switch to other nodes if the preferred node for a given key is not available. Similarly if you add a new node, part of the new keys will start to be stored on the new node.

The main concept here is the following:

1、If Redis is used as a cache **scaling up and down** using consistent hashing is easy.

2、If Redis is used as a store, **a fixed keys-to-nodes map is used, so the number of nodes must be fixed and cannot vary**. Otherwise, a system is needed that is able to rebalance keys between nodes when nodes are added or removed, and currently only Redis Cluster is able to do this - Redis Cluster is generally available and production-ready as of [April 1st, 2015](https://groups.google.com/d/msg/redis-db/dO0bFyD_THQ/Uoo2GjIx6qgJ).



### [Implementations of Redis partitioning](https://redis.io/topics/partitioning#implementations-of-redis-partitioning)

So far we covered Redis partitioning in theory, but what about practice? What system should you use?

#### Redis Cluster

Redis Cluster is the preferred way to get automatic sharding and high availability. It is generally available and production-ready as of [April 1st, 2015](https://groups.google.com/d/msg/redis-db/dO0bFyD_THQ/Uoo2GjIx6qgJ?_ga=2.11141108.1733593536.1624506756-1794781287.1624506756). You can get more information about Redis Cluster in the [Cluster tutorial](https://redis.io/topics/cluster-tutorial).

Once Redis Cluster is available, and if a Redis Cluster compliant client is available for your language, Redis Cluster will be the de facto standard for Redis partitioning.

Redis Cluster is a mix between *query routing* and *client side partitioning*.

#### Twemproxy

> NOTE: 
>
> 典型的client side partition

[Twemproxy is a proxy developed at Twitter](https://github.com/twitter/twemproxy) for the Memcached ASCII and the Redis protocol. It is single threaded, it is written in C, and is extremely fast. It is open source software released under the terms of the Apache 2.0 license.

Twemproxy supports automatic partitioning among multiple Redis instances, with optional node ejection(驱离) if a node is not available (this will change the keys-instances map, so you should use this feature only if you are using Redis as a cache).

It is *not* a **single point of failure** since you can start multiple proxies and instruct your clients to connect to the first that accepts the connection.

> NOTE: 
>
> 一、成组来避免**single point of failure** 

Basically Twemproxy is an intermediate layer between clients and Redis instances, that will reliably handle partitioning for us with minimal additional complexities.

You can read more about Twemproxy [in this antirez blog post](http://antirez.com/news/44).
