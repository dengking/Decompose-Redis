# amazon [Replication: Redis (Cluster Mode Disabled) vs. Redis (Cluster Mode Enabled)](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/Replication.Redis-RedisCluster.html)

Beginning with Redis version 3.2, you have the ability to create one of two distinct types of Redis clusters (API/CLI: replication groups). A Redis (cluster mode disabled) cluster always has a single shard (API/CLI: node group) with up to 5 read replica nodes. A Redis (cluster mode enabled) cluster has up to 90 shards with 1 to 5 read replica nodes in each.

![Image: Redis (cluster mode disabled) and Redis (cluster mode enabled) clusters](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/images/ElastiCache-NodeGroups.png)

*Redis (cluster mode disabled) and Redis (cluster mode enabled) clusters*

The following table summarizes important differences between Redis (cluster mode disabled) and Redis (cluster mode enabled) clusters.

**Comparing Redis (Cluster Mode Disabled) and Redis (Cluster Mode Enabled) Clusters**

| Feature                          | Redis (cluster mode disabled)                                | Redis (cluster mode enabled)                                 |
| :------------------------------- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| Modifiable                       | Yes. Supports adding and deleting replica nodes, and scaling up node type. | Limited. For more information, see [Upgrading Engine Versions](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/VersionManagement.html) and [Scaling Clusters in Redis (Cluster Mode Enabled)](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/scaling-redis-cluster-mode-enabled.html). |
| Data Partitioning                | No                                                           | Yes                                                          |
| Shards                           | 1                                                            | 1 to 90                                                      |
| Read replicas                    | 0 to 5 Important If you have no replicas and the node fails, you experience total data loss. | 0 to 5 per shard. Important If you have no replicas and a node fails, you experience loss of all data in that shard. |
| Multi-AZ with Automatic Failover | Yes, with at least 1 replica. Optional. On by default.       | Yes. Required.                                               |
| Snapshots (Backups)              | Yes, creating a single .rdb file.                            | Yes, creating a unique .rdb file for each shard.             |
| Restore                          | Yes, using a single .rdb file from a Redis (cluster mode disabled) cluster. | Yes, using .rdb files from either a Redis (cluster mode disabled) or a Redis (cluster mode enabled) cluster. |
| Supported by                     | All Redis versions                                           | Redis 3.2 and following                                      |
| Engine upgradeable               | Yes, with some limits. For more information, see [Upgrading Engine Versions](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/VersionManagement.html). | Yes, with some limits. For more information, see [Upgrading Engine Versions](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/VersionManagement.html). |
| Encryption                       | Versions 3.2.6 and 4.0.10 and later.                         | Versions 3.2.6 and 4.0.10 and later.                         |
| HIPAA Compliant                  | Version 3.2.6 and 4.0.10 and later.                          | Version 3.2.6 and 4.0.10 and later.                          |
| PCI DSS Compliant                | Version 3.2.6 and 4.0.10 and later.                          | Version 3.2.6 and 4.0.10 and later.                          |
| Online resharding                | N/A                                                          | Version 3.2.10 and later.                                    |



