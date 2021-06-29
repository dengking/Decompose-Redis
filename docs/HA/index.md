# HA

Redis的HA需要由:

1、API

2、Sentinel cluster: automatic failover、leader election

3、Sentinel : sentinel本身是需要保证HA的，否则就会导致整个系统的single point of failure

3、Redis instance: replication: master、slave

协作来实现



## 流程

参见 `流程` 章节；

