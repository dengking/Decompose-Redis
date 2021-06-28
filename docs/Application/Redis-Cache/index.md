# Redis cache





## 参考文章: 

1、toutiao [阿里P8技术专家细究分布式缓存问题](https://www.toutiao.com/a6533812974807679495/)

cnblogs [Redis系列十：缓存雪崩、缓存穿透、缓存预热、缓存更新、缓存降级](https://www.cnblogs.com/leeSmall/p/8594542.html)

> NOTE: 两者其实为一篇

2、 csdn [彻底搞懂缓存穿透，缓存击穿，缓存雪崩](https://blog.csdn.net/qq_42875345/article/details/107006899)

这篇文章讲解得比较好

3、developpaper [Redis – cache avalanche, cache breakdown, cache penetration](https://developpaper.com/redis-cache-avalanche-cache-breakdown-cache-penetration/)

非常精简

4、ideras [Redis cache penetration & breakdown & avalanche](https://ideras.com/redis-cache-penetration-breakdown-avalanche/)

## 问题

### Cache penetration /  缓存穿透

缓存穿透是指用户查询数据，在数据库没有，自然在缓存中也不会有。这样就导致用户查询的时候，在缓存中找不到，每次都要去数据库再查询一遍，然后返回空（相当于进行了两次无用的查询）。这样请求就绕过缓存直接查数据库，这也是经常提的缓存命中率问题。

> toutiao [阿里P8技术专家细究分布式缓存问题](https://www.toutiao.com/a6533812974807679495/)

如果此时有人恶意的攻击呢？发起几十亿万条redis和mysql中都不存在的数据，请求访问你的网站，数据库不就挂了。

> csdn [彻底搞懂缓存穿透，缓存击穿，缓存雪崩](https://blog.csdn.net/qq_42875345/article/details/107006899)

### Cache breakdown / 缓存击穿

> csdn [彻底搞懂缓存穿透，缓存击穿，缓存雪崩](https://blog.csdn.net/qq_42875345/article/details/107006899)

### Cache avalanche / 缓存雪崩

缓存雪崩指的是: 所有的缓存都消失了，可能原因:

1、由于原有缓存失效，新缓存未到期间(例如：我们设置缓存时采用了相同的过期时间，在同一时刻出现大面积的缓存过期)，所有原本应该访问缓存的请求都去查询数据库了，而对数据库CPU和内存造成巨大压力，严重的会造成数据库宕机。从而形成一系列连锁反应，造成整个系统崩溃。

2、Redis宕机了



## 解决思路

看了各种解决方案，其实，目标都是一致的: 

避免大批量的请求直接进入到数据库，而造成数据库压力过大而宕机；

所以解决思路就是: 拦截请求；

在不同的场景中，拦截的方法不同，需要结合场景进行分析。



## developpaper [Redis – cache avalanche, cache breakdown, cache penetration](https://developpaper.com/redis-cache-avalanche-cache-breakdown-cache-penetration/)

### 2. Cache penetration(穿透)

#### concept

Cache penetration refers to the continuous request for data that does not exist in the cache, then all requests fall into the database layer. In the case of high concurrency, it will directly affect the business of the whole system, and even lead to system crash

#### process

1、Get data from cache according to key;

2、If the data is not empty, it will be returned directly;

3、If the data is empty, query the database;

4、If the data queried from the database is not empty, it will be put into the cache (set the expiration time)

Repeat the third step continuously. For example, if you want to check the information of a product and pass in '- 1', all requests will be directly placed in the database

#### solve

1、Set the null object cache

If the data retrieved from the database is null, it will also be cached. However, setting a shorter cache time can slow down the cache penetration to a certain extent

2、Bulon filter
In essence, bloom filter is a highly efficient probabilistic algorithm and data structure, which is mainly used to determine whether an element exists in a set.

//Use
You can add the true and correct key to the filter after adding it in advance. Each time you query again, first confirm whether the key to be queried is in the filter. If not, the key is illegal, and there is no need for subsequent query steps.

### 3. Cache avalanche(雪崩)

#### concept

Cache avalanche refers to that a large number of data in the cache are expired at the same time or the redis service is hung up, and the huge amount of query data causes the database pressure to be too large.

#### process

1、 The information of commodities is cached in the mall for 24 hours

2、 At 0 o'clock the next day, there was a rush sale

3、 At the beginning of the rush purchase, all caches are invalid, and all commodity information queries fall into the database at the same time, which may lead to excessive pressure

#### solve

1、You can cache data by category and add different cache times

2、At the same time of caching, add a random number to the cache time, so that all caches will not fail at the same time

3、For the problem of the redis service hanging up, we can realize the high availability master-slave architecture of redis, and implement the persistence of redis. When the redis is down, read the local cache data, and restore the redis service to load the persistent data

### 4. Buffer breakdown(击穿)

#### concept

Cache breakdown is similar to cache avalanche, but it is not a massive cache failure Cache breakdown means that a key value in the cache continuously receives a large number of requests, and at the moment when the key value fails, a large number of requests fall on the database, which may lead to excessive pressure on the database

### process

1、 There is a cache corresponding to key1, key2, etc

2、 Key1 is a hot commodity, constantly carrying the request

3、 The expiration time of key1 is up, and the request falls on the database instantly, which may cause excessive pressure

### solve

1、Add distributed locks or distributed queues

In order to avoid a large number of concurrent requests falling on the underlying storage system,  istributed locks or distributed queues are used to ensure the cache single thread write. In the lock method, first get the lock from the cache again (to prevent another thread from acquiring the lock first, and the lock has been written to the cache), and then the DB write cache is not checked. (of course, you can poll the cache until it times out in a thread that does not acquire a lock.)

2、 Add timeout flag

An attribute is added to the cached object to identify the timeout time. When the data is obtained, the internal marking time of the data is checked to determine whether the timeout is fast. If so, a thread is initiated asynchronously (concurrency is controlled) to actively update the cache.

3、 In addition, there is a crude method. If the real-time performance of your hot data is relatively low, you can set the hot data not to expire in the hot period, and expire in the low peak period of access, such as in the early hours of every day.
