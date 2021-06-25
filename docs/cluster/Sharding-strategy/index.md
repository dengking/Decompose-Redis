# Redis cluster data sharding strategy

Redis cluster 采用的是它 "hash slot" 的data sharding strategy，而不是consistent hash，我觉得Redis的hash  slot方案有如下优势:

1、运维人员可以手动以hash slot为单位来在节点之间移动数据，从而实现load balance；consistent hash的方案则完全是自动的，运维人员无法介入。





# redis doc [redis partition](https://redis.io/topics/partitioning)

https://redis.io/topics/partitioning



# redis cluster

https://redis.io/topics/cluster-tutorial

**Redis Cluster** does not use **consistent hashing**, but a different form of sharding where every key is conceptually part of what we call an hash slot.

