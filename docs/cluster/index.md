# Redis cluster



## 实现概述

redis cluster的结构是decentralized ，所以每个node为了知道其他node的信息必须要将它们保存下来，这就是为什么每个node都需要有一个`clusterState`，并且redis cluster是[High-availability cluster](https://en.wikipedia.org/wiki/High-availability_cluster)，即它需要支持failover，所以cluster需要有能够侦探cluster是否异常的能力，所以它需要不断地进行heartbeat，然后将得到的cluster中的其他node的状态保存起来；而raft算法的cluster结构是有centralized的，它是基于leader-follower的，不是一个decentralized的结构；

redis中，当cluster解决master election采用的是raft算法的实现思路；

