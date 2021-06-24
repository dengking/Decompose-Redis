# [**Amazon ElastiCache Documentation**](https://docs.aws.amazon.com/elasticache/index.html) # [**Amazon ElastiCache for Redis**](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/SelectEngine.html)

> 一、其中的一些内容对于部署是有借鉴参考价值的。
>
> 二、原文的内容， 主要是基于Redis的两种部署方式来展开讨论的:
>
> | 部署方式                               | shard数量        |      |
> | -------------------------------------- | ---------------- | ---- |
> | Redis (cluster mode disabled) clusters | 一个shard        |      |
> | Redis (cluster mode enabled) clusters  | up to 500 shards |      |
>
> 



## [Choosing regions and availability zones](https://docs.aws.amazon.com/AmazonElastiCache/latest/red-ug/RegionsAndAZs.html)

AWS Cloud computing resources are housed in highly available data center facilities. To provide additional scalability and reliability, these data center facilities are located in different physical locations. These locations are categorized by *regions* and *Availability Zones*.

> NOTE: 
>
> 物理隔离



