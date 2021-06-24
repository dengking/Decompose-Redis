在阅读[Redis源码解析：27集群(三)主从复制、故障转移](https://www.cnblogs.com/gqtcgq/p/7247042.html)的时候，其中提及：

> 理解Redis集群中的故障转移，必须要理解纪元(epoch)在分布式Redis集群中的作用，Redis集群使用RAFT算法中类似term的概念，在Redis集群中这被称之为纪元(epoch)。纪元的概念在介绍哨兵时已经介绍过了，在Redis集群中，纪元的概念和作用与哨兵中的纪元类似。Redis集群中的纪元主要是两种：currentEpoch和configEpoch。

上面提到了redis哨兵，redis哨兵的文档在[Redis Sentinel Documentation](https://redis.io/topics/sentinel)中介绍，其中的确有epoch的概念，并且redis哨兵所实现的high availability，其实是依赖于故障转移功能的，而redis cluster也能够提供故障转移(failover)功能，并且它的实现也借鉴了redis sentinel的实现，这说明redis cluster集成了redis sentinel的部分功能，并且实现上也是借鉴的redis sentinel；

阅读[gqtc ](https://home.cnblogs.com/u/gqtcgq/)的[redis系列博客](https://www.cnblogs.com/gqtcgq/category/1043761.html)中，关于sentinel的文章，我能够从中看到一些redis cluster实现中同样使用的技术：



|                           sentinel                           |                           cluster                            |
| :----------------------------------------------------------: | :----------------------------------------------------------: |
| [Redis源码解析：21sentinel(二)定期发送消息、检测主观下线](https://www.cnblogs.com/gqtcgq/p/7247048.html) | [Redis源码解析：25集群(一)握手、心跳消息以及下线检测](https://www.cnblogs.com/gqtcgq/p/7247044.html) |
| [Redis源码解析：22sentinel(三)客观下线以及故障转移之选举领导节点](https://www.cnblogs.com/gqtcgq/p/7247047.html) | [Redis源码解析：27集群(三)主从复制、故障转移](https://www.cnblogs.com/gqtcgq/p/7247042.html) |
| [Redis源码解析：23sentinel(四)故障转移流程](https://www.cnblogs.com/gqtcgq/p/7247046.html) |                                                              |



其实现在想想本质上redis sentinel和redis cluster都是distribution system，并且它们需要实现的功能是存在一定的重复的，所以实现上的重复是非常正常的；

