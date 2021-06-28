# csdn [彻底搞懂缓存穿透，缓存击穿，缓存雪崩](https://blog.csdn.net/qq_42875345/article/details/107006899)

## 一般用redis常规的写法：

1：客户端发起请求。
2：判断redis中是否有数据，如果有返回给客户端，没有则请求数据库,查出数据返回给客户端。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20200714220657728.png)

## 带来的问题一：缓存穿透

1：如果此时有人恶意的攻击呢？发起几十亿万条redis和mysql中都不存在的数据，请求访问你的网站，数据库不就挂了。

### 解决办法1：使用布隆过滤器

redis中没有数据，请求布隆过滤器拦截请求mysql和redis中不存在的key的请求。如果布隆过滤器中对应的key，在请求数据库，没有则返回一个非法访问

![在这里插入图片描述](https://img-blog.csdnimg.cn/20200714220753361.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzQyODc1MzQ1,size_16,color_FFFFFF,t_70)

### 解决办法2：缓存空数据

在客户端请求redis时发现没有key，接着请求mysql发现也没有key，此时就把key缓存起来，value设置为null。（**缺点只适合单一key多次访问数据库的情况**）

## 带来的问题二：缓存击穿

客户端访问数据的时候，redis中没有数据，mysql中有数据，相当于直接跳过了redis。

### 为何会发生

用户访问这条数据的时候，热点数据过期时间刚好到了。

### 问题

1：如果此时这条数据很热门，秒级有几十亿万次的访问量，数据库不就挂了。

### 解决

1：设置热点数据永远不过期。

2：发现redis中没有数据，加入分布式锁（拦截请求），接着查询redis，查询数据库后，把这条数据重新加入到缓存，释放锁，之后剩余的请求，请求redis时就可以查到数据了

```Java
public Item_kill getItemKill(int id) {
        /**
         * 布隆过滤器解决缓存穿透
         */
        if (!bloomFilter.mightContain(id)) {
            log.warn("非法秒杀商品id" + id);
            return null;
        }
        /**
         * 从缓存中获取秒杀商品的信息，如果此时热点数据恰好过期了呢？
         */
        Item_kill itemKill = (Item_kill) redisTemplate.opsForValue().get(CACHE_PRE + "[" + id + "]");
        if (itemKill != null) {
            return itemKill;
        }
        /**
         * 定义唯一标识key
         */
        String key = new StringBuffer().append(id).toString();
        RLock lock = redissonClient.getLock(key);
        boolean ok = false;
        Item_kill item_kill = null;
        try {
            ok = lock.tryLock(30, 10, TimeUnit.SECONDS);
            if(ok){
                /**
                 * 从缓存中获取秒杀商品的信息
                 */
                itemKill = (Item_kill) redisTemplate.opsForValue().get(CACHE_PRE + "[" + id + "]");
                if (itemKill != null) {
                    return itemKill;
                }
                item_kill = itemKillMapper.selectByPrimaryKey(id);
                /**
                 * 将热点数据重新加入到缓存，解决热点数据失效的问题
                 */
                redisTemplate.opsForValue().set(CACHE_PRE + "[" + id + "]",item_kill);
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }finally {
            lock.unlock();
        }
        return item_kill;
    }

```

3：做接口的限流降级（举个例子）

> NOTE: 
>
> 其实就是拦截请求

接口限流（这样的话，这个登录的接口就最大支持20个人同时访问了）

```Java
@RestController
public class logController {
    @PostMapping("/login")
    @Limit(maxLimit = 20)
    public R login() {

    }
}
```

## 三：缓存雪崩

### 介绍：

redis挂了，所有的请求都达到了数据库

### 解决

1：使用缓存集群，保证缓存高可用

使用 Redis Sentinel 和 Redis Cluster 实现高可用缓存

2：使用Hystrix

做一些限流或者熔断的兜底策略
