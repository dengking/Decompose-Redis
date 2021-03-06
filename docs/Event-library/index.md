# Redis event library: ae



## [what does "ae" mean, short for?](https://redis-db.narkive.com/zj43Vnl5/what-does-ae-mean-short-for)

看了其中的回答，我觉得可能性更大的是**async events**



## 功能/需求

1、需要监控、轮训的事件

2、当事件发生时，需要执行的callback

3、能够cross plateform(Unix-like OS)

4、file event、time event

## Implementation概述

1、需要依赖于OS提供的IO multiplex/polling system call来同时对多个file descriptor进行监控，这在`Low-level-IO-multiplex`章节进行描述。

2、基于low level IO multiplex中提供的abstract interface，ae基于自身需求建立起了比较灵活的、强大的、cross plateform的event library，并提供了high level interface，Redis的其他部分就是使用的high level interface，这在 `High-level` 章节进行了描述。

3、single thread同时multiplex on time and file event

Redis ae充分利用了各种OS IO multiplex所提供的"multiplex on time and file event"特性，实现了multiplex on time and file event，关于这一点，参见:

a、 voidcn [把redis源码的linux网络库提取出来，自己封装成通用库使用（★firecat推荐★）](http://www.voidcn.com/article/p-agorceqr-brv.html)

> Redis网络库是一个单线程EPOLL模型，也就是说接收连接和处理读写请求包括**定时器任务**都被这一个线程包揽，真的是又当爹又当妈，但是效率一定比多线程差吗？不见得。
>
> 单线程的好处有： 
>
> 1：避免线程切换带来的上下文切换开销。 
>
> 2：单线程避免了锁的争用。 
>
> 3：对于一个内存型数据库，如果不考虑数据持久化，也就是读写物理磁盘，不会有阻塞操作，内存操作是非常快的。

b、`High-level` 章节中， 对这个问题进行了说明

5、Redis ae是典型的采用reactor pattern，参见:

a、zhihu [I/O多路复用技术（multiplexing）是什么？](https://www.zhihu.com/question/28594409)



## Documentation

在《redis设计与实现》的第12章事件中对redis的event library进行了介绍。

### Redis official doc: redis.io [Redis Internals documentation](https://redis.io/topics/internals)

#### Redis Event Library

Read [event library](https://redis.io/topics/internals-eventlib) to understand what an event library does and why its needed.

[Redis event library](https://redis.io/topics/internals-rediseventlib) documents the implementation details of the event library used by Redis.

> NOTE: 
> [event library](https://redis.io/topics/internals-eventlib)、[Redis event library](https://redis.io/topics/internals-rediseventlib)  收录于 `Redis-event-library-office-doc` 章节中



## redis-ae-application

1、voidcn [把redis源码的linux网络库提取出来，自己封装成通用库使用（★firecat推荐★）](http://www.voidcn.com/article/p-agorceqr-brv.html)