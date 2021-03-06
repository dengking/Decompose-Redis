# Redis event library: ae



## [what does "ae" mean, short for?](https://redis-db.narkive.com/zj43Vnl5/what-does-ae-mean-short-for)

看了其中的回答，我觉得可能性更大的是**async events**



## 功能/需求

1、需要监控、轮训的事件

2、当事件发生时，需要执行的callback

3、能够cross plateform(Unix-like OS)

4、file、timer

## Implementation概述

1、需要依赖于OS提供的IO multiplex/polling system call来同时对多个file descriptor进行监控，这在`Low-level-IO-multiplex`章节进行描述。

2、基于low level IO multiplex中提供的abstract interface，ae基于自身需求建立起了比较灵活的、强大的、cross plateform的event library，并提供了high level interface，Redis的其他部分就是使用的high level interface，这在 `High-level` 章节进行了描述。



## File descriptors

1、在前面的"`struct aeFileEvent`"章节中，进行了说明



## redis-ae-application

1、voidcn [把redis源码的linux网络库提取出来，自己封装成通用库使用（★firecat推荐★）](http://www.voidcn.com/article/p-agorceqr-brv.html)