# Redis deploy and topology

本章对Redis的deploy方式以及对应的topology进行分析。

参考内容:

1、rtfm [Redis: replication, part 1 – an overview. Replication vs Sharding. Sentinel vs Cluster. Redis topology.](https://rtfm.co.ua/en/redis-replication-part-1-overview-replication-vs-sharding-sentinel-vs-cluster-redis-topology/)

其中总结了Redis的所有可能的部署方式，比较全面

2、amazon [**Amazon ElastiCache for Redis**](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/SelectEngine.html)

其中总结了Redis sentinel、Redis cluster

## Sentinel VS cluster

stackoverflow [Redis sentinel vs clustering](https://stackoverflow.com/questions/31143072/redis-sentinel-vs-clustering)

### fnordig [Redis Sentinel & Redis Cluster - what?](https://fnordig.de/2015/06/01/redis-sentinel-and-redis-cluster/)

#### Redis Sentinel

It will **monitor** your master & slave instances, **notify** you about changed behaviour, handle **automatic failover** in case a master is down and act as a **configuration provider**, so your clients can find the current master instance.

> NOTE: 
>
> 上面这段话，介绍了"configuration provider"的功能

#### Redis Cluster

Redis Cluster is a **data sharding** solution with **automatic management**, **handling failover** and **replication**.

