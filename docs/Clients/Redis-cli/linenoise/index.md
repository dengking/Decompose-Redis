# linenoise

redis-cli的实现依赖于linenoise，除此之外，使用linenoise我们可以构建自己的server的command line interface。

## [linenoise](https://github.com/antirez/linenoise)

A small self-contained alternative to readline and libedit.



### [The API](https://github.com/antirez/linenoise#the-api)

The library returns a buffer with the line composed by the user, or NULL on end of file or when there is an out of memory condition.

> NOTE: 这段话怎么理解？

