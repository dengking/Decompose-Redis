注意，IO thread是redis 6的新特性

```
server.c:initServerConfig(){
    server.io_threads_num = CONFIG_DEFAULT_IO_THREADS_NUM;
    server.io_threads_do_reads = CONFIG_DEFAULT_IO_THREADS_DO_READS;
}
```


```
networking.c:
/* ==========================================================================
 * Threaded I/O
 * ========================================================================== */
 
 


```

从代码来看，redis的IO默认情况下还是single thread的；