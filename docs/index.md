Redis is simple but powerful, it can be a great learning material. I decompose it into modules and conquer each. It can be divide into modules as below



## Redis data structure



## Redis network module



## Redis event library



## Redis memory management





# TODO

## Cache replacement policies

在阅读redis的源代码的时候， 发现了如下的定义：

```
typedef struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; /* lru time (relative to server.lruclock) */
    int refcount;
    void *ptr;
} robj;
```

其中的LRU引起了我的注意，遂Google了一下，发现了如下内容：

- [Cache replacement policies](https://en.wikipedia.org/wiki/Cache_replacement_policies)
- 

## Redis Persistence

https://redis.io/topics/persistence

## [Redis Sentinel Documentation](https://redis.io/topics/sentinel)

### clients service discovery

https://en.wikipedia.org/wiki/Service_discovery

https://www.nginx.com/blog/service-discovery-in-a-microservices-architecture/

https://stackoverflow.com/questions/37148836/what-is-service-discovery-and-why-do-you-need-it

## redis pub/sub

[How to use Pub/sub with hiredis in C++?](https://stackoverflow.com/questions/11641741/how-to-use-pub-sub-with-hiredis-in-c)

[Add code example for pub/sub](https://github.com/redis/hiredis/issues/55)

[pub/sub sample](https://code.msdn.microsoft.com/pubsub-sample-385d5286/sourcecode?fileId=130365&pathId=4287888)

## redis source code

c没有destructor，我看redis的source code中存在着大量的指针，那么当redis进程退出执行的时候，是否需要执行清理工作呢？还是说完全依靠OS来执行这些清理；

redis使用引用计数来实现自动内存管理

## redis信号处理

redis-server向自己发送信号是否使用的是raise？

参加APUE10.9



## stream oriented

出自：https://redis.io/topics/protocol

Why TCP called stream oriented protocol?

### byte stream vs character stream



# consistent hashing

https://redis.io/topics/cluster-tutorial

https://en.wikipedia.org/wiki/Consistent_hashing