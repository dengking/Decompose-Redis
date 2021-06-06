# [redis-plus-plus](https://github.com/sewenew/redis-plus-plus)

对[hiredis](https://github.com/redis/hiredis)的wrapper，写的非常好。

## [Connection](https://github.com/sewenew/redis-plus-plus#connection)

### `class Connection`



> If the connection is broken, `Redis` reconnects to Redis server automatically.

### 如何实现的自动重连？

#### [Connection Failure](https://github.com/sewenew/redis-plus-plus#connection-failure)







## [Exception](https://github.com/sewenew/redis-plus-plus#exception)

See [errors.h](https://github.com/sewenew/redis-plus-plus/blob/master/src/sw/redis%2B%2B/errors.h) for details



## [Redis Sentinel](https://github.com/sewenew/redis-plus-plus#redis-sentinel)

> NOTE: 通过sentinel可以获取master、slave的地址，然后可以指定`Redis`连接到它们。

## 类图

```
`class Redis` has a `ConnectionPool _pool`

```

