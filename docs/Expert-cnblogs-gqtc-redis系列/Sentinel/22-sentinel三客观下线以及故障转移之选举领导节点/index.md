# cnblogs [Redis源码解析：22sentinel(三)客观下线以及故障转移之选举领导节点](https://www.cnblogs.com/gqtcgq/p/7247047.html)

## 八：判断实例是否客观下线

当前哨兵一旦监测到某个主节点实例主观下线之后，就会向其他哨兵发送”is-master-down-by-addr”命令，询问其他哨兵是否也认为该主节点主观下线了。如果有超过quorum个哨兵（包括当前哨兵）反馈，都认为该主节点主观下线了，则当前哨兵就将该主节点实例标记为客观下线。

### 1：发送”is-master-down-by-addr”命令

### 2：其他哨兵收到”is-master-down-by-addr”命令后的处理

### 3：哨兵收到其他哨兵的”is-master-down-by-addr”命令回复信息后的处理

### 4：判断实例是否客观下线

## 九：故障转移流程之选举领导节点

### 1：故障转移流程

### 2：选举领导节点原理

### 3：判断是否开始故障转移

### 4：开始新一轮的故障转移流程

### 5：发送”is-master-down-by-addr”命令进行拉票

### 6：其他哨兵收到”is-master-down-by-addr”命令后进行投票

### 7：哨兵收到其他哨兵的”is-master-down-by-addr”命令回复信息后的处理

### 8：统计投票
