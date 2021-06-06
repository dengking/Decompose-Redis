# antirez [An update about Redis developments in 2019](http://antirez.com/news/126)

## Multi threading

There are two possible multi threading supports that Redis could get. I believe the user is referring to “memcached alike” multithreading, that is the ability to scale a single **Redis instance** to multiple threads in order to increase the operations per second it can deliver in things like `GET` or `SET` and other simple commands. This involves making the I/O, command parsing and so forth multi threaded. So let’s call this thing “**I/O threading**”.

Another multi threaded approach is to, instead, allow slow commands to be executed in a different thread, so that other clients are not blocked. We’ll call this threading model “**Slow commands threading**”.

Well, that’s the plan: I/O threading is not going to happen in Redis AFAIK, because after much consideration I think it’s a lot of complexity without a good reason. Many Redis setups are **network or memory bound** actually. Additionally I really believe in a share-nothing setup, so the way I want to scale Redis is by improving the support for multiple Redis instances to be executed in the same host, especially via Redis Cluster. The things that will happen in 2019 about that are two:

A) Redis Cluster multiple instances will be able to orchestrate(精心编制) to make a judicious use of the disk of the local instance, that is, let’s avoid an AOF rewrite at the same time.

B) We are going to ship a Redis Cluster proxy as part of the Redis project, so that users are able to abstract away a cluster without having a good implementation of the Cluster protocol client side.

### Redis VS Memcached

Another thing to note is that Redis is not Memcached, but, like memcached, is an in-memory system. To make multithreaded an in-memory system like memcached, with a very simple data model, makes a lot of sense. A multi-threaded on-disk store is mandatory. A multi-threaded complex in-memory system is in the middle where things become ugly: Redis clients are not isolated, and data structures are complex. A thread doing LPUSH need to serve other threads doing LPOP. There is less to gain, and a lot of complexity to add.

> NOTE: 
>
> 一、翻译: "另外需要注意的是Redis不是Memcached，但是像memcached一样，是一个内存系统。 使用非常简单的数据模型制作多线程内存系统（如memcached）非常有意义。 必须使用多线程磁盘存储。 一个多线程复杂的内存系统正处于丑陋的中间：Redis客户端不是孤立的，数据结构也很复杂。 执行LPUSH的线程需要服务于执行LPOP的其他线程。 获得的收益较少，添加的复杂性也很高。"
>
> 二、上面这段话是作者对Redis 支持 multithread 的思考、tradeoff，作者使用Memcached来作为对照(Memcached是multithread的)
>
> 1、Redis的data model是比Memcached要复杂的，因此，当Redis考虑支持multithread的时候，就需要考虑可能出现的对data structures的并发操作，作者列举了一个例子: "A thread doing LPUSH need to serve other threads doing LPOP"，即多个thread同时对一个data structure进行操作，那如何进行同步呢？显然这是比较复杂的，需要考虑非常多的情况。
>
> 2、因此，是否采用multithread是需要进行tradeoff的:
>
> a、能够获得多大的性能提升
>
> b、实现的复杂度、维护性
>
> 三、从目前(2021-06-05)的Redis的实现来看，它采用的是一种折中的方案:
>
> 具体参见 zhihu [Redis 6.0 多线程IO处理过程详解](https://zhuanlan.zhihu.com/p/144805500) 。

What instead I *really want* a lot is slow operations threading, and with the Redis modules system we already are in the right direction. However in the future (not sure if in Redis 6 or 7) we’ll get key-level locking in the module system so that threads can completely acquire control of a key to process slow operations. Now modules can implement commands and can create a reply for the client in a completely separated way, but still to access the shared data set a global lock is needed: this will go away.

> NOTE: 
>
> "相反，我真正想要的是慢速操作线程，而使用Redis模块系统，我们已经朝着正确的方向前进了。 但是在将来（不确定是否在Redis 6或7中）我们将在模块系统中获得key-level 锁定，以便线程可以完全获得对key的控制以处理慢速操作。 现在，模块可以实现命令，并且可以以完全独立的方式为客户端创建回复，但仍然需要访问共享数据集，需要全局锁定：这将消失。"



## Data structures

Now Redis has Streams, starting with Redis 5. For Redis 6 and 7 what is planned is, to start, to make what we have much more memory efficient by changing the implementations of certain things. However to add new data structures there are a lot of considerations to do. It took me years to realize how to fill the gap, with streams, between lists, pub/sub and sorted sets, in the context of time series and streaming. I really want Redis to be a set of orthogonal data structures that the user can put together, and not a set of *tools* that are ready to use. Streams are an abstract log, so I think it’s a very worthwhile addition. However other things I’m not completely sure if they are worth to be inside the core without a very long consideration. Anyway in the latest years there was definitely more stress in adding new data structures. HyperLogLogs, more advanced bit operations, streams, blocking sorted set operations (ZPOP* and BZPOP*), and streams are good examples.

