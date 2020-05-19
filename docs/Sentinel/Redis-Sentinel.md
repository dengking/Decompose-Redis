# Redis sentinel



## 官方文档[Redis Sentinel Documentation](https://redis.io/topics/sentinel)

Redis Sentinel also provides other collateral（附属的） tasks such as **monitoring**, **notifications** and acts as a **configuration provider** for clients.

This is the full list of Sentinel capabilities at a macroscopical（宏观的） level (i.e. the *big picture*):

- **Monitoring**. Sentinel constantly checks if your master and slave instances are working as expected.

- **Notification**. Sentinel can notify the **system administrator**, another computer programs, via an API, that something is wrong with one of the monitored Redis instances.

- **Automatic failover**. If a **master** is not working as expected, Sentinel can start a **failover process** where a slave is promoted to master, the other additional slaves are reconfigured to use the new master, and the applications using the Redis server informed about the new address to use when connecting.

  自动故障转移。 如果主服务器未按预期工作，Sentinel可以启动故障转移过程，其中从服务器升级为主服务器，其他其他服务器重新配置为使用新主服务器，并且使用Redis服务器的应用程序通知有关新服务器的地址。连接。

- **Configuration provider**. Sentinel acts as a source of authority for **clients service discovery**: clients connect to **Sentinels** in order to ask for the address of the **current Redis master** responsible for a given service. If a **failover** occurs, **Sentinels** will report the new address.



## Distributed nature of Sentinel

**Redis Sentinel** is a distributed system:

**Sentinel** itself is designed to run in a configuration where there are multiple Sentinel processes cooperating together. The advantage of having multiple Sentinel processes cooperating are the following:

1. Failure detection is performed when multiple Sentinels agree about the fact a given master is no longer available. This lowers the probability of **false positives**.
2. **Sentinel** works even if not all the Sentinel processes are working, making the system robust against failures. There is no fun in having a fail over system which is itself a single point of failure, after all.

The sum of Sentinels, Redis instances (masters and slaves) and clients connecting to Sentinel and Redis, are also a larger distributed system with specific properties. In this document concepts will be introduced gradually starting from basic information needed in order to understand the basic properties of Sentinel, to more complex information (that are optional) in order to understand how exactly Sentinel works.



# Quick Start

## Obtaining Sentinel

The current version of Sentinel is called **Sentinel 2**. It is a rewrite of the initial Sentinel implementation using stronger and simpler to predict algorithms (that are explained in this documentation).

A stable release of Redis Sentinel is shipped since Redis 2.8.

New developments are performed in the *unstable* branch, and new features sometimes are back ported into the latest stable branch as soon as they are considered to be stable.

Redis Sentinel version 1, shipped with Redis 2.6, is deprecated and should not be used.

## Running Sentinel

If you are using the `redis-sentinel` executable (or if you have a symbolic link with that name to the `redis-server`executable) you can run Sentinel with the following command line:

```shell
redis-sentinel /path/to/sentinel.conf
```

Otherwise you can use directly the `redis-server` executable starting it in **Sentinel mode**:

```shell
redis-server /path/to/sentinel.conf --sentinel
```

Both ways work the same.

However **it is mandatory** to use a configuration file when running **Sentinel**, as this file will be used by the system in order to save the **current state** that will be reloaded in case of restarts. Sentinel will simply refuse to start if no configuration file is given or if the configuration file path is not writable.

**Sentinels** by default run **listening for connections to TCP port 26379**, so for Sentinels to work, port 26379 of your servers **must be open** to receive connections from the IP addresses of the other **Sentinel instances**. Otherwise Sentinels can't talk and can't agree about what to do, so failover will never be performed.



## Fundamental things to know about Sentinel before deploying

1. You need at least three Sentinel instances for a robust deployment.
2. The three Sentinel instances should be placed into computers or virtual machines that are believed to fail in an **independent** way. So for example different physical servers or Virtual Machines executed on different availability zones.
3. Sentinel + Redis distributed system does not guarantee that **acknowledged writes** are retained(保持) during failures, since Redis uses **asynchronous replication**. However there are ways to deploy Sentinel that make the window to lose writes limited to certain moments, while there are other less secure ways to deploy it.
4. You need Sentinel support in your clients. Popular client libraries have Sentinel support, but not all.
5. There is no [HA](https://en.wikipedia.org/wiki/High-availability_cluster) setup which is safe if you don't test from time to time in development environments, or even better if you can, in production environments, if they work. You may have a misconfiguration that will become apparent only when it's too late (at 3am when your master stops working).
6. **Sentinel, Docker, or other forms of Network Address Translation or Port Mapping should be mixed with care**: Docker performs port remapping, breaking Sentinel auto discovery of other Sentinel processes and the list of slaves for a master. Check the section about Sentinel and Docker later in this document for more information.





## Configuring Sentinel

The Redis source distribution contains a file called `sentinel.conf` that is a self-documented example configuration file you can use to configure Sentinel, however a typical minimal configuration file looks like the following:

```
sentinel monitor mymaster 127.0.0.1 6379 2
sentinel down-after-milliseconds mymaster 60000
sentinel failover-timeout mymaster 180000
sentinel parallel-syncs mymaster 1

sentinel monitor resque 192.168.1.3 6380 4
sentinel down-after-milliseconds resque 10000
sentinel failover-timeout resque 180000
sentinel parallel-syncs resque 5
```

You only need to specify the masters to monitor, giving to each separated master (that may have any number of slaves) a different **name**. There is no need to specify **slaves**, which are **auto-discovered**. **Sentinel** will update the configuration automatically with additional information about slaves (in order to retain the information in case of restart). The configuration is also rewritten every time a slave is promoted to master during a failover and every time a new Sentinel is discovered.

The example configuration above, basically monitor two sets of **Redis instances**, each composed of a **master** and an undefined number of slaves. One set of instances is called `mymaster`, and the other `resque`.

The meaning of the arguments of `sentinel monitor` statements is the following:

```
sentinel monitor <master-group-name> <ip> <port> <quorum>
```

For the sake of clarity, let's check line by line what the configuration options mean:

The first line is used to tell Redis to monitor a master called *mymaster*, that is at address 127.0.0.1 and port 6379, with a quorum of 2. Everything is pretty obvious but the **quorum** argument:

- The **quorum** is the number of **Sentinels** that need to agree about the fact the **master** is not reachable, in order for really mark the slave as failing, and eventually start a fail over procedure if possible.
- However **the quorum is only used to detect the failure**. In order to actually perform a failover, one of the Sentinels need to be **elected leader** for the failover and be authorized to proceed. This only happens with the vote of the **majority of the Sentinel processes**.

So for example if you have 5 Sentinel processes, and the quorum for a given master set to the value of 2, this is what happens:

- If two Sentinels agree at the same time about the master being unreachable, one of the two will try to start a failover.
- If there are at least a total of three Sentinels **reachable**, the failover will be authorized and will actually start.

In practical terms this means during failures **Sentinel never starts a failover if the majority of Sentinel processes are unable to talk** (aka no failover in the minority partition).

实际上，这意味着在故障期间，如果大多数Sentinel进程无法通话（在少数分区中也没有故障转移），Sentinel永远不会启动故障转移。



## Other Sentinel options

The other options are almost always in the form:

```
sentinel <option_name> <master_name> <option_value>
```

And are used for the following purposes:

- `down-after-milliseconds` is the time in milliseconds an instance should not be reachable (either does not reply to our PINGs or it is replying with an error) for a Sentinel starting to think it is down.

- `parallel-syncs` sets the number of slaves that can be reconfigured to use the new master after a failover at the same time. The lower the number, the more time it will take for the failover process to complete, however if the slaves are configured to serve old data, you may not want all the slaves to re-synchronize with the master at the same time. While the replication process is mostly non blocking for a slave, there is a moment when it stops to load the bulk data from the master. You may want to make sure only one slave at a time is not reachable by setting this option to the value of 1.

  parallel-syncs设置可在同一故障转移后重新配置为使用新主服务器的从服务器数。 数字越小，故障转移过程完成所需的时间就越多，但是如果从属服务器配置为提供旧数据，则可能不希望所有从属服务器同时与主服务器重新同步。 虽然复制过程对于从属设备大部分是非阻塞的，但是有一段时间它停止从主设备加载批量数据。 您可能希望通过将此选项设置为值1来确保一次只能访问一个从站。

Additional options are described in the rest of this document and documented in the example `sentinel.conf` file shipped with the Redis distribution.

All the configuration parameters can be modified at runtime using the `SENTINEL SET` command. See the **Reconfiguring Sentinel at runtime** section for more information.



## Example Sentinel deployments

Now that you know the basic information about Sentinel, you may wonder where you should place your **Sentinel processes**, how much Sentinel processes you need and so forth. This section shows a few example deployments.

We use ASCII art in order to show you configuration examples in a *graphical* format, this is what the different symbols means:

```
+--------------------+
| This is a computer |
| or VM that fails   |
| independently. We  |
| call it a "box"    |
+--------------------+
```

We write inside the boxes what they are running:

```
+-------------------+
| Redis master M1   |
| Redis Sentinel S1 |
+-------------------+
```

Different boxes are connected by lines, to show that they are able to talk:

```
+-------------+               +-------------+
| Sentinel S1 |---------------| Sentinel S2 |
+-------------+               +-------------+
```

Network partitions are shown as interrupted lines using slashes:

```
+-------------+                +-------------+
| Sentinel S1 |------ // ------| Sentinel S2 |
+-------------+                +-------------+
```

Also note that:

- Masters are called M1, M2, M3, ..., Mn.
- Slaves are called R1, R2, R3, ..., Rn (R stands for *replica*).
- Sentinels are called S1, S2, S3, ..., Sn.
- Clients are called C1, C2, C3, ..., Cn.
- When an instance changes role because of Sentinel actions, we put it inside square brackets, so [M1] means an instance that is now a master because of Sentinel intervention.



## Example 1: just two Sentinels, DON'T DO THIS

```
+----+         +----+
| M1 |---------| R1 |
| S1 |         | S2 |
+----+         +----+

Configuration: quorum = 1
```

- In this setup, if the master M1 fails, R1 will be promoted since the two Sentinels can reach agreement about the failure (obviously with quorum set to 1) and can also authorize（批准） a failover because the majority is two. So apparently it could superficially work, however check the next points to see why this setup is broken.
- If the box where M1 is running stops working, also S1 stops working. The Sentinel running in the other box S2 will not be able to authorize a failover, so the system will become not available.



Note that a **majority** is needed in order to order different failovers, and later propagate the latest configuration to all the **Sentinels**. Also note that the ability to failover in a single side of the above setup, without any agreement, would be very dangerous:

```
+----+           +------+
| M1 |----//-----| [M1] |
| S1 |           | S2   |
+----+           +------+
```



In the above configuration we created two masters (assuming S2 could failover without authorization) in a perfectly symmetrical way. Clients may write indefinitely to both sides, and there is no way to understand when the partition heals what configuration is the right one, in order to prevent a *permanent split brain condition*.

在上面的配置中，我们以完全对称的方式创建了两个主服务器（假设S2可以在未经授权的情况下进行故障转移）。 客户端可以无限期地向双方写入，并且无法理解分区何时恢复正确的配置，以防止永久性的裂脑情况。

So please **deploy at least three Sentinels in three different boxes** always.



## Example 2: basic setup with three boxes

This is a very simple setup, that has the advantage to be simple to tune for additional safety. It is based on three boxes, each box running both a Redis process and a Sentinel process.

```
       +----+
       | M1 |
       | S1 |
       +----+
          |
+----+    |    +----+
| R2 |----+----| R3 |
| S2 |         | S3 |
+----+         +----+

Configuration: quorum = 2
```

If the master M1 fails, S2 and S3 will agree about the failure and will be able to authorize a failover, making clients able to continue.

In every Sentinel setup, being Redis asynchronously replicated, there is always the risk of losing some write because a given acknowledged write may not be able to reach the slave which is promoted to master. However in the above setup there is an higher risk due to clients partitioned away with an old master, like in the following picture:

```
         +----+
         | M1 |
         | S1 | <- C1 (writes will be lost)
         +----+
            |
            /
            /
+------+    |    +----+
| [M2] |----+----| R3 |
| S2   |         | S3 |
+------+         +----+
```





# A quick tutorial





## Asking Sentinel about the state of a master





## Obtaining the address of the current master





## Testing the failover







# Sentinel API



## Sentinel commands





## Reconfiguring Sentinel at Runtime





## Adding or removing Sentinels



##  Removing the old master or unreachable slaves



## Pub/Sub Messages





## Handling of -BUSY state



## Slaves priority







##  Sentinel and Redis authentication



## Configuring Sentinel instances with authentication



# More advanced concepts



In the following sections we'll cover a few details about how Sentinel work, without to resorting to implementation details and algorithms that will be covered in the final part of this document.





## SDOWN and ODOWN failure state



## Sentinels and Slaves auto discovery



## Sentinel reconfiguration of instances outside the failover procedure



## Slave selection and priority



# Algorithms and internals

In the following sections we will explore the details of Sentinel behavior. It is not strictly needed for users to be aware of all the details, but a deep understanding of Sentinel may help to deploy and operate Sentinel in a more effective way.

## Quorum