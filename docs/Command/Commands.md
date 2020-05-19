# Commands

client通过command来操作redis server，本文对redis的command的实现进行分析。



## prototype of redis command function

redis中所有用于处理command的函数的prototype如下：
```
void Command(client *c);
```
因为在处理command之前，`readQueryFromClient`函数已经将用户发送过来的command信息写入到了client的`argv`成员变量中。

### redis cluster command

`void clusterCommand(client *c)`

`void dumpCommand(client *c)`

`void restoreCommand(client *c)`


### redis replication

`void syncCommand(client *c)`

`void replconfCommand(client *c)`

### redis config

`void configCommand(client *c) `