# mian

1、需要勾画出Redis的主流程



```C
int main()
{
	parse argv;
	load config file;
	daemonize;
	initServer; // 非常重要的一步，建立event 和 event handler的映射
	aeMain(server.el); // event loop、main loop
  aeDeleteEventLoop(server.el); // 退出event loop、main loop
}
```

