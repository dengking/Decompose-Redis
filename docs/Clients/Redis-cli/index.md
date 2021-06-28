# redis-cli



## [redis-cli, the Redis command line interface](https://redis.io/topics/rediscli)

`redis-cli` is the Redis command line interface, a simple program that allows to send commands to Redis, and read the replies sent by the server, directly from the terminal.

It has two main modes: an **interactive mode** where there is a REPL ([Read Eval Print Loop](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop)) where the user types commands and get replies; and another mode where the command is sent as arguments of `redis-cli`, executed, and printed on the standard output.

> NOTE: 第二个mode所指的是如下这种使用方式：
> ```
> redis-cli KEYS "prefix:*" | xargs redis-cli DEL
> ```

In interactive mode, `redis-cli` has basic line editing capabilities to provide a good typing experience.

However `redis-cli` is not just that. There are options you can use to launch the program in order to put it into special modes, so that `redis-cli` can definitely do more complex tasks, like simulate a slave and print the **replication stream** it receives from the **master**, check the **latency** of a **Redis server** and show statistics or even an ASCII-art spectrogram of latency samples and frequencies, and many other things.

This guide will cover the different aspects of `redis-cli`, starting from the simplest and ending with the more advanced ones.

***SUMMARY*** : redis-cli不仅仅只有上述的mode，还有其他的mode，参见[Special modes of operation](#Special modes of operation)

If you are going to use Redis extensively, or if you already do, chances are you happen to use `redis-cli` a lot. Spending some time to familiarize with it is likely a very good idea, you'll see that you'll work more effectively with Redis once you know all the tricks of its command line interface.

## Command line usage



### Host, port, password and database



### Getting input from other programs



### Continuously run the same command



### Mass insertion of data using `redis-cli`



### CSV output



### Running Lua scripts





## Interactive mode



### Handling connections and reconnections



### Editing, history and completion



### Running the same command N times



### Showing help about Redis commands



## Special modes of operation

So far we saw two main modes of `redis-cli`.

- Command line execution of Redis commands.
- Interactive "REPL-like" usage.



However the CLI performs other auxiliary tasks related to Redis that are explained in the next sections:

- Monitoring tool to show continuous stats about a Redis server.
- Scanning a Redis database for very large keys.
- Key space scanner with pattern matching.
- Acting as a [Pub/Sub](https://redis.io/topics/pubsub) client to subscribe to channels.
- Monitoring the commands executed into a Redis instance.
- Checking the [latency](https://redis.io/topics/latency) of a Redis server in different ways.
- Checking the scheduler latency of the local computer.
- Transferring RDB backups from a remote Redis server locally.
- Acting as a Redis slave for showing what a slave receives.
- Simulating [LRU](https://redis.io/topics/lru-cache) workloads for showing stats about keys hits.
- A client for the Lua debugger.

## 实现分析

“interactive mode ”的入口函数是：`repl`

"Command line execution of Redis commands"的 "Running Lua scripts"的入口函数是:`evalMode`



redis所支持的所有的command都在"help.h"中定义.



### 如何组织所有的command？

如何组织所有的command和command对应的执行函数？