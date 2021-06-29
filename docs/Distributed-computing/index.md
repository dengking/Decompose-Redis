# Distributed computing: sentinel、cluster

Redis sentinel、Redis cluster都属于distributed system，其中使用了很多distributed computing中的技术，我们可以将Redis作为学习distributed computing的一个非常好的案例，也可以使用distributedcomputing的理论来对它进行分析；

## Redis sentinel、Redis cluster

Redis sentinel、Redis cluster都属于distributed system存在着诸多共同之处，可以认为: Redis cluster集成了Redis sentinel的功能。



|                       | Redis sentinel              | Redis cluster               |
| --------------------- | --------------------------- | --------------------------- |
| Consensus protocol    | raft                        | raft                        |
|                       | current epoch、config epoch | current epoch、config epoch |
| 下线                  |                             |                             |
| system/topology state | `sentinelState`             | `clusterState`              |
| auto discover         | HELLO channel               | gossip                      |



### stackoverflow [Redis sentinel vs clustering](https://stackoverflow.com/questions/31143072/redis-sentinel-vs-clustering)



### fnordig [Redis Sentinel & Redis Cluster - what?](https://fnordig.de/2015/06/01/redis-sentinel-and-redis-cluster/)

#### Redis Sentinel

It will **monitor** your master & slave instances, **notify** you about changed behaviour, handle **automatic failover** in case a master is down and act as a **configuration provider**, so your clients can find the current master instance.

> NOTE: 
>
> 上面这段话，介绍了"configuration provider"的功能

#### Redis Cluster

Redis Cluster is a **data sharding** solution with **automatic management**, **handling failover** and **replication**.



### See also

amazon [**Amazon ElastiCache for Redis**](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/SelectEngine.html)

其中总结了Redis sentinel、Redis cluster



## CAP

Redis的两种部署方式(sentinel、cluster)都属于distributed system，都涉及CAP:

consistency、availability、partition；

