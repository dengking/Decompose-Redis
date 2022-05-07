# Redis persistence

docs [Redis persistence](https://redis.io/docs/manual/persistence/)

实现：

1、zhihu [Redis 持久化 RDB 原理](https://zhuanlan.zhihu.com/p/345233479)



## RDB VS AOF

RDB not very durable

AOF full durability



## docs [Redis persistence](https://redis.io/docs/manual/persistence/)

> NOTE:
>
> redis的两种persistence方式都充分运用了Linux fork的copy-on-write特性

### Snapshotting

#### How it works

Whenever Redis needs to dump the dataset to disk, this is what happens:

- Redis [forks](http://linux.die.net/man/2/fork). We now have a child and a parent process.
- The child starts to write the dataset to a temporary RDB file.
- When the child is done writing the new RDB file, it replaces the old one.

This method allows Redis to benefit from **copy-on-write** semantics.

### Append-only file

Since Redis 7.0.0, Redis uses a multi part AOF mechanism. That is, the original single AOF file is split into base file (at most one) and incremental files (there may be more than one). The base file represents an initial (RDB or AOF format) snapshot of the data present when the AOF is [rewritten](https://redis.io/docs/manual/persistence/#log-rewriting). The incremental files contains incremental changes since the last base AOF file was created. All these files are put in a separate directory and are tracked by a manifest file.

> NOTE:
>
> | original        | now                                                          |      |
> | --------------- | ------------------------------------------------------------ | ---- |
> | single AOF file | base file (at most one) + incremental files (there may be more than one |      |
> |                 |                                                              |      |
> |                 |                                                              |      |
>
> 

#### Log rewriting

The AOF gets bigger and bigger as write operations are performed. For example, if you are incrementing a counter 100 times, you'll end up with a single key in your dataset containing the final value, but 100 entries in your AOF. 99 of those entries are not needed to rebuild the current state.

> NOTE:
>
> 这一段其实描述了一个问题: "if you are incrementing a counter 100 times, you'll end up with a single key in your dataset containing the final value, but 100 entries in your AOF. 99 of those entries are not needed to rebuild the current state"
>
> 显然更优秀的方案是不需要如此之多的，那redis是如何实现的呢？redis把此称之为 "log rewriting"

The rewrite is completely safe. While Redis continues appending to the old file, a completely new one is produced with the minimal set of operations needed to create the current data set, and once this second file is ready Redis switches the two and starts appending to the new one.

> NOTE:
>
> 这一段介绍了redis是如何实现"Log rewriting"的大体思路

Since Redis 7.0.0, when an AOF rewrite is scheduled, The Redis parent process opens a new incremental AOF file to continue writing. The child process executes the rewrite logic and generates a **new base AOF**. Redis will use a **temporary manifest file** to track the **newly generated base file** and **incremental file**. When they are ready, Redis will perform an atomic replacement operation to make this **temporary manifest file** take effect. In order to avoid the problem of creating many incremental files in case of repeated failures and retries of an AOF rewrite, Redis introduces an **AOF rewrite limiting mechanism** to ensure that failed AOF rewrites are retried at a slower and slower rate.

> NOTE:
>
> 一、翻译如下:
>
> "从 Redis 7.0.0 开始，在调度 AOF 重写时，Redis 父进程会打开一个新的增量 AOF 文件继续写入。子进程执行重写逻辑并生成新的基础 AOF。 Redis 将使用一个临时清单文件来跟踪新生成的基础文件和增量文件。当它们准备好后，Redis 会执行原子替换操作，使这个临时清单文件生效。为了避免在 AOF 重写重复失败和重试的情况下创建大量增量文件的问题，Redis 引入了 AOF 重写限制机制，以确保失败的 AOF 重写以越来越慢的速度重试。"
>
> 二、 "**manifest file**"的含义是"清单文件"，参见：
>
> wikipedi [Manifest file](https://en.wikipedia.org/wiki/Manifest_file)
>
> 三、"In order to avoid the problem of creating many incremental files in case of repeated failures and retries of an AOF rewrite, Redis introduces an AOF rewrite limiting mechanism to ensure that failed AOF rewrites are retried at a slower and slower rate."
>
> 其实说白了就是 [Exponential backoff](https://en.wikipedia.org/wiki/Exponential_backoff) 、[Implement retries with exponential backoff](https://docs.microsoft.com/en-us/dotnet/architecture/microservices/implement-resilient-applications/implement-retries-exponential-backoff) 
>
> 三、需要注意的是: 
>
> **temporary manifest file** =  **newly generated base file** (child process) +  **incremental file**(Redis parent process)
>
> 在下面的 "How it works" 以更加详细的方式描述了实现细节。

#### How durable is the append only file?[ ](https://redis.io/docs/manual/persistence/#how-durable-is-the-append-only-file)

The suggested (and default) policy is to `fsync` every second. It is both fast and relatively safe. The `always` policy is very slow in practice, but it supports group commit, so if there are multiple parallel writes Redis will try to perform a single `fsync` operation.

> NOTE:
>
> 一、"group commit"要如何理解呢？意思是: 如果有多个parallel write，redis将尝试执行一个" `fsync` operation"
>
> 二、典型的tradeoff

#### What should I do if my AOF gets truncated?

It is possible the server crashed while writing the AOF file, or the volume(磁盘) where the AOF file is stored was full at the time of writing. When this happens the AOF still contains consistent data representing a given point-in-time version of the dataset (that may be old up to one second with the default AOF fsync policy), but the last command in the AOF could be truncated. 

> NOTE:
>
> 一、"可能是服务器在写入 AOF 文件时崩溃，或者存储 AOF 文件的卷在写入时已满。发生这种情况时，AOF 仍然包含表示数据集的给定时间点版本的一致数据（使用默认的 AOF fsync 策略可能会旧到一秒），但 AOF 中的最后一个命令可能会被截断。"

The latest major versions of Redis will be able to load the AOF anyway, just discarding the last non well formed command in the file. In this case the server will emit a log like the following:



#### How it works

Log rewriting uses the same copy-on-write trick already in use for snapshotting. 

**Redis >= 7.0**

1、Redis [forks](http://linux.die.net/man/2/fork), so now we have a child and a parent process.

2、The child starts writing the new base AOF in a temporary file.

> NOTE:
>
> 由child process来实现log rewrite。另外需要注意的是：child process写的是"new base AOF"

3、The parent opens a new **increments AOF file** to continue writing updates. If the rewriting fails, the old base and increment files (if there are any) plus this newly opened increment file represent the complete updated dataset, so we are safe.

4、When the child is done rewriting the base file, the parent gets a signal, and uses the newly opened increment file and child generated base file to build a temp manifest, and persist it.

> NOTE:
>
> **temporary manifest file** =  **newly generated base file** (child process) +  **incremental file**(Redis parent process)

5、Profit! Now Redis does an atomic exchange of the manifest files so that the result of this AOF rewrite takes effect. Redis also cleans up the old base file and any unused increment files.

### Disaster recovery

Disaster recovery in the context of Redis is basically the same story as backups, plus the ability to transfer those backups in many different external data centers. This way data is secured even in the case of some catastrophic event affecting the main data center where Redis is running and producing its snapshots.

> NOTE:
>
> 上面的总结非常好:
>
> disaster recovery = backup + ability to transfer those backups in many different external data centers

