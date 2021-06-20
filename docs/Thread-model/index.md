# Redis thread model

## 参考文章

antirez [An update about Redis developments in 2019](http://antirez.com/news/126)

一、在这篇文章中，作者给出了Redis可能的multiple thread:

1、“**I/O threading**”

2、“**Slow commands threading**”

二、这篇文章中，作者以Memcached来对比，讨论Redis实现multiple thread时，需要考虑的各种内容、各种tradeoff

### Redis 6 multiple thread implementation

alibabacloud [Improving Redis Performance through Multi-Thread Processing](https://www.alibabacloud.com/blog/improving-redis-performance-through-multi-thread-processing_594150)

zhihu [Redis 6.0 多线程IO处理过程详解](https://zhuanlan.zhihu.com/p/144805500)

通过上述两篇文章的内容，基本上可以掌握Redis 6 multiple thread implementation。



## TODO


https://blog.cloudflare.com/io_submit-the-epoll-alternative-youve-never-heard-about/

### toutiao [Redis为什么又引入了多线程？难道作者也逃不过“真香定理”？](https://www.toutiao.com/a6816914695023231500/?wid=1623047095160)

#### 三、为什么引入多线程？

刚刚说了一堆使用单线程的好处，现在话锋一转，又要说为什么要引入多线程，别不适应。引入多线程说明Redis在有些方面，单线程已经不具有优势了。

因为读写网络的read/write系统调用在Redis执行期间占用了大部分CPU时间，如果把网络读写做成多线程的方式对性能会有很大提升。

Redis 的多线程部分只是用来处理网络数据的读写和协议解析，执行命令仍然是单线程。之所以这么设计是不想 Redis 因为多线程而变得复杂，需要去控制 key、lua、事务，LPUSH/LPOP 等等的并发问题。

### zhihu [Redis6 多线程剖析](https://zhuanlan.zhihu.com/p/118450227)
