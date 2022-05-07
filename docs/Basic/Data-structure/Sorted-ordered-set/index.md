# Sorted ordered set



## [redis](https://github.com/redis/redis)/[src](https://github.com/redis/redis/tree/unstable/src)/[**t_zset.c**](https://github.com/redis/redis/blob/unstable/src/t_zset.c)

> ZSETs are ordered sets using two data structures to hold the same elements in order to get O(log(N)) INSERT and REMOVE operations into a sorted data structure.
>
> The elements are added to a hash table mapping **Redis objects** to **scores**. At the same time the elements are added to a **skip list** mapping **scores** to **Redis objects** (so objects are sorted by scores in this "view").



## TODO

jameshfisher [How is the Redis sorted set implemented?](https://jameshfisher.com/2018/04/22/redis-sorted-set/)

