# Redis client



## `CLIENT_SLAVE` 和 `CLIENT_MASTER`

### 在master中为client添加上 `CLIENT_SLAVE` 标识
slave instance向master instance发送`SYNC`或者`PSYNC`到master instance，所以master需要执行这两个command，这两个command的执行函数是:`replication.c:syncCommand`



`replication.c:masterTryPartialResynchronization`




当master执行对slave的full resynchronize的时候



## 如何缓存用户发送过来的command

具体的实现在`networking.c:processMultibulkBuffer`

其实在阅读了[Redis Protocol specification](https://redis.io/topics/protocol)后，知道在redis server中，一个非常重要的问题就是要将用户发送过来的command给保存起来，那在`struct client`中所采用的data structure是什么呢？以下是对这个问题的分析：

[Redis Protocol specification](https://redis.io/topics/protocol)中描述了，client发送过来的command是以RESP Arrays的方式打包的，以下是一个example：

```c
*5\r\n
:1\r\n
:2\r\n
:3\r\n
:4\r\n
$6\r\n
foobar\r\n
```

可以看到，它其实非常类似于一个二维数组；

与此对应的是在`struct client`中也使用一个二维array来保存，在`struct client`中使用如下字段来保存：

```c
robj **argv;            /* Arguments of current command. */
```

`struct client`字段`int multibulklen;       /* Number of multi bulk arguments left to read. */`对应的是RESP Arrays中所指定的 number of elements in the array，所以据此就可以知道了`argv`中的元素的个数了；



由于底层的网络实现，client发送过来的command是可能需要多次调用`read`系统调用才能够完整地读取的，然后才能够准确地理解command的含义；所以我们需要一个cache来保存用户发送过来的stream，保存这个stream的就是`struct client`字段`sds querybuf;` `struct client`字段`size_t qb_pos;`则记录的是`querybuf`空闲空间的起点，也就是说read得到的数据从`querybuf`开始写入到`querybuf`中。



`qb_pos`记录的是已经处理的数据的长度



### `struct client`字段`sds querybuf`

千万不要小看了`querybuf`，其实它涉及到了非常多的问题，稍有不慎就会导致严重后果：

- [Buffer overflow](https://en.wikipedia.org/wiki/Buffer_overflow)
- malloc和free `querybuf`

###  `struct client`字段`robj **argv`

该字段其实和`querybuf`非常类似，本质上都是由于缓存；



## 如何判断与client之间的网络连接已经被对方断开？

今天在阅读redis的代码的时候，看到了如下这段让我感到非常疑惑的代码：

```c
void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
	......
        
    nread = read(fd, c->querybuf+qblen, readlen);
    if (nread == -1) {
        if (errno == EAGAIN) {
            return;
        } else {
            serverLog(LL_VERBOSE, "Reading from client: %s",strerror(errno));
            freeClientAsync(c);
            return;
        }
    } else if (nread == 0) {
        serverLog(LL_VERBOSE, "Client closed connection");
        freeClientAsync(c);
        return;
    } else if (c->flags & CLIENT_MASTER) {
        /* Append the query buffer to the pending (not applied) buffer
         * of the master. We'll use this buffer later in order to have a
         * copy of the string applied by the last command executed. */
        c->pending_querybuf = sdscatlen(c->pending_querybuf,
                                        c->querybuf+qblen,nread);
    }

    ......

}
```

为什么没有再读到数据了，就表示这个client已经close了connection？难道redis要每次在client没有数据可读的情况下，就将这个client给关闭掉？这样岂不是会导致这个client下一次进行IO的时候又要重新连接到redis server？

```c
if (nread == 0) {
        serverLog(LL_VERBOSE, "Client closed connection");
        freeClientAsync(c);
        return;
    }
```





### how to judge whether a socket peer close the connection in linux





#### how redis server judge client has close the network connection

https://github.com/andymccurdy/redis-py/issues/1140

https://redis.io/topics/clients

https://github.com/antirez/redis/issues/2948