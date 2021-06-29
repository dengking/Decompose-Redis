# 流程



## cnblogs [Redis中算法之——Raft算法](https://www.cnblogs.com/tangtangde12580/p/8302185.html)

> NOTE: 
>
> 这篇文章讲述了Redis sentinel执行failover的 流程

Sentinel系统选举领头的方法是对Raft算法的领头选举方法的实现。

> NOTE: 
>
> leader election

在分布式系统中一致性是很重要的。1990年Leslie Lamport提出基于消息传递的一致性算法Paxos算法，解决分布式系统中就某个值或决议达成一致的问题。Paxos算法流程繁杂实现起来也比较复杂。

2013年斯坦福的Diego Ongaro、John Ousterhout两个人以易懂为目标设计一致性算法Raft。Raft一致性算法保障在任何时候一旦处于leader服务器当掉，可以在剩余正常工作的服务器中选举出新的Leader服务器更新新Leader服务器，修改从服务器的复制目标。

Sentinel是一个运行在特殊模式下的Redis服务器。它负责监视主服务器以及主服务器下的从服务器。当**领头Sentinel**认为主服务器已经进入主观下线状态，将对已下线的**主服务器**执行故障转移操作，该操作包括三个步骤：

（1）在已下线主服务器下的从服务器中挑选一个从服务器，将其转换为新的主服务器。

（2）让已下线主服务器属下的所有从服务器改为复制新的主服务器。

（3）将已下线主服务器成为新主服务器的从服务器。

Raft详解：http://www.cnblogs.com/likehua/p/5845575.html

分布式Raft算法：http://www.jdon.com/artichect/raft.html

分布式一致算法——Paxos：http://www.cnblogs.com/cchust/p/5617989.html