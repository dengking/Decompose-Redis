# Redis Signals Handling

下面是相关code的分析:



## `initServer`

该函数主要做如下事情

### 安装signal handler

```c
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();
```



## system call对`EINTR`的处理

在redis source code中检索`EINTR`，得到的结果如下：
