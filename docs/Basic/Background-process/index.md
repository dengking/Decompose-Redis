
# redis的background process

通过全局检索`fork`系统调用，以下是结果：


## rdb background saving process

`rdb.c:rdbSaveBackground`会fork一个process，这个process所执行的就是save，`struct redisServer`的`rdb_child_pid`成员变量用于保存这个process的`pid`，在redis的source code中进行了全局的检索，发现以下地方涉及`rdb.c:rdbSaveBackground`的调用：

- `server.c`中会定时触发background save
- `replication.c`中`startBgsaveForReplication`会调用`rdbSaveBackground`

需要注意的是：`rdb.c:rdbSaveBackground`只是启动background saving process，它并不wait这个process的finish；

虽然有多处对`rdb.c:rdbSaveBackground`的调用，但是从redis的设计角度来看，在同一时间它仅仅允许一个background saving process，并不允许同时运行多个background saving process，它的`struct redisServer`的`rdb_child_pid`成员变量是一个标量；

其实这就引出了一个问题：background saving process有多种方式被调用，并且redis的设计还要求唯一性，互斥性，那么它如何得知当前正在允许的background saving process是为何而运行的呢？

其实回答这个问题，可以从后往前来进行推，核实对background saving process的wait处的代码，该处代码位于`server.c:serverCron`中，它最终会调用`rdb.c:backgroundSaveDoneHandler`，在`rdb.c:backgroundSaveDoneHandler`中会调用`replication.c:updateSlavesWaitingBgsave`这个函数用于更新等待background saving process完成的slave。



rdb backgroud saving process的设计还是遵循的event driven programming，当触发了rdb background saving process开始运行后，main process并不blocking，而是进行event loop，在这个event loop中，除了监视file event，time event，还监视background process的状态(通过poll的方式)，如果background process的状态变更了，也会触发响应的handler；	参见《`redis-code-analysis-handler.md`》。




## aof background saving process

`aof.c:rewriteAppendOnlyFileBackground`会fork一个process


## `srcripting.c`

`scripting.c:ldbStartSession`


## `sentinel.c`

`sentinelRunPendingScripts`

## `server.c`

`daemonize`