# rtfm [Redis: replication, part 1 – an overview. Replication vs Sharding. Sentinel vs Cluster. Redis topology.](https://rtfm.co.ua/en/redis-replication-part-1-overview-replication-vs-sharding-sentinel-vs-cluster-redis-topology/)

Initially, it was planned to write one small post with an example how to create a **Redis replication** but as I read more and more details – I wanted to describe more and more about it, so eventually I split this post into two parts.

In this one – some quick overview, a brief explanation about differences in Redis data distribution types, topology examples.

In short terms but with links to detailed documentation and other useful posts on other resources.

[In the second part](https://rtfm.co.ua/en/redis-replication-part-2-master-slave-replication-and-redis-sentinel/) – a couple of examples of how to configure a simple replication and replication with Redis Sentinel.

[In the third part](https://rtfm.co.ua/en/redis-replication-part-3-redis-py-and-work-with-redis-sentinel-from-python/) – the `redis-py` library examples with Redis replication and Sentinel.



### Redis Replication vs Sharding

Redis supports two data sharing types *replication* (also known as *mirroring*, a data duplication), and *sharding* (also known as *partitioning*, a data segmentation). In this – **Redis Cluster** can use both methods simultaneously.



#### Replication

Is a data coping overall Redis nodes in a cluster which allows to make requests to one or more slave nodes and making data persistence if some of those nodes will go down, providing a **High Availability**.

Using this approach – the *read* requests will be faster.

See [Replication](https://rtfm.co.ua/goto/https://redis.io/topics/replication) and [Redis Cluster master-slave model](https://rtfm.co.ua/goto/https://redis.io/topics/cluster-tutorial#redis-cluster-master-slave-model).





#### Sharding

With the data segmentation – all data will be split into a few parts and this will improve each node’s performance as it will store only some part of the data and will not serve all requests.

Using this approach – the *write* requests will go faster.

See [Partitioning: how to split data among multiple Redis instances](https://rtfm.co.ua/goto/https://redis.io/topics/partitioning) and [Redis Cluster data sharding](https://rtfm.co.ua/goto/https://redis.io/topics/cluster-tutorial#redis-cluster-data-sharding).



### Redis Sentinel vs Redis Cluster



#### Redis Sentinel

Was added to Redis v.2.4 and basically is a monitoring service for master and slaves.

Also, can send notifications, automatically **switch** masters and slaves roles if a master is down and so on.

Might have a sense to be used for a bare master-slave replication (see below) without full clustering.

It works as a dedicated daemon using a `sentinel` binary or `redis-server` in the *sentinel* mode.

Will perform nodes reconfiguration if the master went down – will choose a new master from the slaves left.

Requires at least three Sentinel instances to have a quorum for a new master election and to decide if one of a Redis nodes goes down

> NOTE: 
>
> "quorum"的意思是"法定人数"



#### Redis Cluster

Was added to Redis v.3.0 and represents a full clustering solution for segmentation, replication, and its nodes management.

Will perform data synchronization, replication, manage nodes access persistence if some will go down.

The **Sentinel usage** in the **Redis Cluster** case doesn’t make sense as Cluster will do everything itself.

See [Redis Sentinel & Redis Cluster – what?](https://rtfm.co.ua/goto/https://fnordig.de/2015/06/01/redis-sentinel-and-redis-cluster/) and [Redis Sentinel Documentation](https://rtfm.co.ua/goto/https://redis.io/topics/sentinel).



### Redis topology

#### One Redis instance

[![Redis: репликация, часть 1 - обзор. Replication vs Sharding. Sentinel vs Cluster. Топология Redis.](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-30.png)](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-30.png)

The simplest and mor classical case.

Simple in running and configuration.

Limited by a host’s resources – its CPU and memory.

In case of such Redis instance will go down – all dependent services will be broken as well as there is no any **availability** or **fault tolerance** mechanisms.



#### Master-slave replication

[
![Redis: репликация, часть 1 - обзор. Replication vs Sharding. Sentinel vs Cluster. Топология Redis.](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-35-11.png)](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-35-11.png)

One master which has multitype slaves attached.

Data will be updated on this master and then the master will push those changes to its replicas.

Slaves can talk to the master only and can’t communicate with other slaves, but still can have their own slaves

Slaves are **read-only** nodes – no data changes can be performed there unless this wasn’t configured explicitly (see [the second part](https://rtfm.co.ua/en/redis-replication-part-2-master-slave-replication-and-redis-sentinel/) of this post).

In case of any node will go down – all data still will be available for clients as data is replicated over all nodes.

Simple in configuration but the *write* operations are limited by the master’s resources.

In case of the master will go down – you’ll have to manually reconfigure slaves and change slave to master roles for one on them.

Also, clients need to know which they have to use for writes operations.



#### Redis Sentinel

[
![Redis: репликация, часть 1 - обзор. Replication vs Sharding. Sentinel vs Cluster. Топология Redis.](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-42.png)](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-42.png)

Already described above but a few more words here.

Similarly to the Redis Replication – Sentinel has one Master instance which has a priority when deciding on a Redis master’s elections.

I.e. in case of one Redis Master and two slaves and if Sentinel Master works on the same host where Redis Master is running and this host will go down – Sentinel will choose Sentinel’s new Master instance and those two Sentinels instances need to decide which Redis slave will have to become a new Redis Master.

During this – a Sentinel’s Master will have more weight in such an election.

Keep in mind that not every Redis client able to work with Sentinel, all client can be found [here>>>](https://rtfm.co.ua/goto/https://redis.io/clients).



#### Redis Cluster

[![Redis: репликация, часть 1 - обзор. Replication vs Sharding. Sentinel vs Cluster. Топология Redis.](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-48.png)](https://rtfm.co.ua/wp-content/uploads/2019/03/screen-shot-2017-08-11-at-14-34-48.png)

And the most powerful solution – the Redis Cluster.

Has a few master instances and each can have one more – up to 1000 – slaves.

Will take care of data sharding, replication, synchronization, and failover operations.

Must have at least 6 Redis nodes – 3 for masters and three for slaves.

Can redirect clients requests to a necessary master or slave host – but the client must have an ability to work with Redis Cluster.

