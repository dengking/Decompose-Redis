# github Redis [README.md](https://github.com/redis/redis#readme)

## Allocator

This default was picked because jemalloc has proven to have fewer fragmentation problems than libc malloc.

## Monotonic clock

By default, Redis will build using the POSIX `clock_gettime` function as the monotonic clock source. On most modern systems, the **internal processor clock** can be used to improve performance. Cautions can be found here: http://oliveryang.net/2015/09/pitfalls-of-TSC-usage/



## [Redis internals](https://github.com/antirez/redis)



### Source code layout

There are a few more directories but they are not very important for our goals here. We'll focus mostly on `src`, where the Redis implementation is contained, exploring what there is inside each file. The order in which files are exposed is the logical one to follow in order to disclose（公开） different layers of complexity incrementally.

> NOTE:  文件被公开的顺序是遵循的逻辑顺序，以便逐步地公开不同的复杂层。

### `server.h`

The simplest way to understand how a program works is to understand the **data structures** it uses. So we'll start from the main header file of Redis, which is `server.h`.

> NOTE:
>
> 上面这段话中所提及的观点是非常重要的，现在想想，对于linux OS，在我知道了其kernel中使用process control block来管理描述进程后，很多相关的概念理解起来就非常轻松了；

All the server configuration and in general all the shared state is defined in a global structure called `server`, of type `struct redisServer`. A few important fields in this structure are:

1、`server.db` is an array of Redis databases, where data is stored.

2、`server.commands` is the **command table**.

3、`server.clients` is a linked list of clients connected to the server.

4、`server.master` is a special client, the master, if the instance is a replica.

There are tons of other fields. Most fields are commented directly inside the structure definition.

Another important Redis data structure is the one defining a client. In the past it was called `redisClient`, now just `client`. The structure has many fields, here we'll just show the main ones:

```c
struct client {
    int fd;
    sds querybuf;
    int argc;
    robj **argv;
    redisDb *db;
    int flags;
    list *reply;
    char buf[PROTO_REPLY_CHUNK_BYTES];
    ... many other fields ...
}
```

The client structure defines a *connected client*:

1、The `fd` field is the **client socket file descriptor**.

2、`argc` and `argv` are populated with the command the client is executing, so that functions implementing a given **Redis command** can read the arguments.

3、`querybuf` accumulates the requests from the **client**, which are parsed by the Redis server according to the **Redis protocol** and executed by calling the implementations of the commands the client is executing.

> NOTE:
>
> 一、根据 `querybuf` 解析得到 `argc` and `argv` 



4、`reply` and `buf` are dynamic and static buffers that accumulate the replies the server sends to the client. These buffers are incrementally written to the socket as soon as the **file descriptor** is writable.

> NOTE: 
>
> 一、reply
>
> 1、`sds querybuf` 是 dynamic buffer
>
> 2、`char buf[PROTO_REPLY_CHUNK_BYTES]` 是 static buffer

As you can see in the **client structure** above, arguments in a command are described as `robj` structures. The following is the full `robj` structure, which defines a *Redis object*:

```c
typedef struct redisObject {
    unsigned type:4;
    unsigned encoding:4;
    unsigned lru:LRU_BITS; /* lru time (relative to server.lruclock) */
    int refcount;
    void *ptr;
} robj;
```

Basically this structure can represent all the basic Redis data types like strings, lists, sets, sorted sets and so forth. The interesting thing is that it has a `type` field, so that it is possible to know what type a given object has, and a `refcount`, so that the same object can be referenced in multiple places without allocating it multiple times. Finally the `ptr` field points to the actual representation of the object, which might vary even for the same type, depending on the `encoding` used.

> NOTE:
>
> 它是类似`std::variant`。

**Redis objects** are used extensively in the Redis internals, however in order to avoid the overhead of indirect accesses, recently in many places we just use plain dynamic strings not wrapped inside a **Redis object**.



### `server.c`

This is the entry point of the **Redis server**, where the `main()` function is defined. The following are the most important steps in order to startup the Redis server.

1、`initServerConfig()` setups the default values of the `server` structure.

2、`initServer()` allocates the data structures needed to operate, setup the **listening socket**, and so forth.

3、`aeMain()` starts the **event loop** which listens for new connections.

There are two special functions called periodically by the event loop:

1、`serverCron()` is called periodically (according to `server.hz` frequency), and performs tasks that must be performed from time to time, like checking for **timedout clients**.

2、`beforeSleep()` is called every time the event loop fired, Redis served a few requests, and is returning back into the event loop.

Inside `server.c` you can find code that handles other vital things of the Redis server:

1、`call()` is used in order to call a given command in the context of a given client.

> NOTE: 
>
> command的执行是in the context of a given client，这说明command是和client关联的；

2、`activeExpireCycle()` handles eviciton(驱逐) of keys with a time to live set via the `EXPIRE` command.

3、`freeMemoryIfNeeded()` is called when a new write command should be performed but Redis is out of memory according to the `maxmemory` directive.

4、The global variable `redisCommandTable` defines all the Redis commands, specifying the name of the command, the function implementing the command, the number of arguments required, and other properties of each command.



### `networking.c`

This file defines all the I/O functions with clients, masters and replicas (which in Redis are just special clients):

1、`createClient()` allocates and initializes a new client.

2、the `addReply*()` family of functions are used by commands implementations in order to append data to the **client structure**, that will be transmitted to the client as a **reply** for a given command executed.

3、`writeToClient()` transmits the data pending in the output buffers to the client and is called by the *writable event handler* `sendReplyToClient()`.

4、`readQueryFromClient()` is the *readable event handler* and accumulates data from read from the client into the query buffer.

5、`processInputBuffer()` is the entry point in order to parse the client query buffer according to the Redis protocol. Once commands are ready to be processed, it calls `processCommand()` which is defined inside `server.c` in order to actually execute the command.

6、`freeClient()` deallocates, disconnects and removes a client.



***SUMMARY*** : 显然，这个模块所描述的都是和network相关的实现，它们应该都是由event loop来进行调用；在`server.h`的`struct client`是支持上述操作的data structure；

***SUMMARY*** : 新版本的redis中的networking已经没有使用该文件了，而是使用的`anet.c`


### aof.c and rdb.c

As you can guess from the names these files implement the RDB and AOF persistence for Redis. Redis uses a persistence model based on the `fork()` system call in order to create a thread with the same (shared) memory content of the **main Redis thread**. This secondary thread dumps the content of the memory on disk. This is used by `rdb.c` to create the snapshots on disk and by `aof.c` in order to perform the AOF rewrite when the append only file gets too big.

The implementation inside `aof.c` has additional functions in order to implement an API that allows commands to append new commands into the AOF file as clients execute them.

The `call()` function defined inside `server.c` is responsible to call the functions that in turn will write the commands into the AOF.



### db.c

Certain Redis commands operate on specific data types, others are general. Examples of generic commands are `DEL` and `EXPIRE`. They operate on keys and not on their values specifically. All those generic commands are defined inside `db.c`.

Moreover `db.c` implements an API in order to perform certain operations on the **Redis dataset** without directly accessing the internal data structures.

The most important functions inside `db.c` which are used in many commands implementations are the following:

- `lookupKeyRead()` and `lookupKeyWrite()` are used in order to get a pointer to the value associated to a given key, or `NULL` if the key does not exist.
- `dbAdd()` and its higher level counterpart `setKey()` create a new key in a Redis database.
- `dbDelete()` removes a key and its associated value.
- `emptyDb()` removes an entire single database or all the databases defined.

The rest of the file implements the generic commands exposed to the client.



### object.c

The `robj` structure defining **Redis objects** was already described. Inside `object.c` there are all the functions that operate with **Redis objects** at a basic level, like functions to allocate new objects, handle the reference counting and so forth. Notable functions inside this file:

- `incrRefcount()` and `decrRefCount()` are used in order to increment or decrement an object reference count. When it drops to 0 the object is finally freed.
- `createObject()` allocates a new object. There are also specialized functions to allocate string objects having a specific content, like `createStringObjectFromLongLong()` and similar functions.

This file also implements the `OBJECT` command.

​	

### replication.c

This is one of the most complex files inside Redis, it is recommended to approach it only after getting a bit familiar with the rest of the code base. In this file there is the implementation of both the **master** and **replica** role of Redis.

One of the most important functions inside this file is `replicationFeedSlaves()` that writes commands to the clients representing replica instances connected to our master, so that the replicas can get the writes performed by the clients: this way their data set will remain synchronized with the one in the master.

This file also implements both the `SYNC` and `PSYNC` commands that are used in order to perform the first synchronization between masters and replicas, or to continue the replication after a disconnection.



### Other C files

- `t_hash.c`, `t_list.c`, `t_set.c`, `t_string.c` and `t_zset.c` contains the implementation of the Redis data types. They implement both an API to access a given data type, and the client commands implementations for these data types.
- `ae.c` implements the Redis event loop, it's a self contained library which is simple to read and understand.
- `sds.c` is the Redis string library, check <http://github.com/antirez/sds> for more information.
- `anet.c` is a library to use POSIX networking in a simpler way compared to the raw interface exposed by the kernel.
- `dict.c` is an implementation of a non-blocking hash table which rehashes incrementally.
- `scripting.c` implements Lua scripting. It is completely self contained from the rest of the Redis implementation and is simple enough to understand if you are familar with the Lua API.
- `cluster.c` implements the Redis Cluster. Probably a good read only after being very familiar with the rest of the Redis code base. If you want to read `cluster.c` make sure to read the [Redis Cluster specification](http://redis.io/topics/cluster-spec).



### Anatomy of a Redis command

All the Redis commands are defined in the following way:

```c
void foobarCommand(client *c) {
    printf("%s",c->argv[1]->ptr); /* Do something with the argument. */
    addReply(c,shared.ok); /* Reply something to the client. */
}
```

The command is then referenced inside `server.c` in the command table:

```c
{"foobar",foobarCommand,2,"rtF",0,NULL,0,0,0,0,0},
```

In the above example `2` is the number of arguments the command takes, while `"rtF"` are the **command flags**, as documented in the **command table** top comment inside `server.c`.

After the command operates in some way, it returns a **reply** to the client, usually using `addReply()` or a similar function defined inside `networking.c`.

There are tons of commands implementations inside the Redis source code that can serve as examples of actual commands implementations. To write a few toy commands can be a good exercise to familiarize with the code base.

There are also many other files not described here, but it is useless to cover everything. We want to just help you with the first steps. Eventually you'll find your way inside the Redis code base :-)

Enjoy!