# Redis DB

一、DB提供了一定的隔离

在cnblogs [【原创】那些年用过的Redis集群架构（含面试解析）](https://www.cnblogs.com/rjzheng/p/10360619.html) 中，提及了Redis db

问题2:Redis的多数据库机制，了解多少？
`正常版`：Redis支持多个数据库，并且每个数据库的数据是隔离的不能共享，单机下的redis可以支持16个数据库（db0 ~ db15）
`高调版`: 在Redis Cluster集群架构下只有一个数据库空间，即db0。因此，我们没有使用Redis的多数据库功能！

