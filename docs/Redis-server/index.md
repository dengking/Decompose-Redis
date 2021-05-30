

## `initServer`

该函数主要做如下事情

### 安装signal handler

```c
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    setupSignalHandlers();
```

