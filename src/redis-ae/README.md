# redis-ae

This is the simplified version of [event library](https://redis.io/topics/internals-rediseventlib) used by [redis](https://redis.io/), which describes the skeleton of the library and the core data structure.





Redis server is an [event driven program](https://en.wikipedia.org/wiki/Event-driven_programming), the server need to deal with the following events:

- file event: Redis server connects with clients or other servers through [network socket](https://en.wikipedia.org/wiki/Network_socket), file event is abstraction of the socket operation.
- time event: time event is the abstraction of operations that need to be done at appointed time.



[Event loop](https://en.wikipedia.org/wiki/Event_loop)

[Reactor pattern](https://en.wikipedia.org/wiki/Reactor_pattern)

## file event

[Reactor pattern](https://en.wikipedia.org/wiki/Reactor_pattern)

[IO Multiplexing](https://wiki.c2.com/?IoMultiplexing)

## time event

