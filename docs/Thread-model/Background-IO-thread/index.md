# Background I/O service for Redis.

 This file implements operations that we need to perform in the background.  Currently there is only a single operation, that is a background `close(2)`  system call. This is needed as when the process is the last owner of a reference to a file closing it means unlinking it, and the deletion of the file is slow, blocking the server.

In the future we'll either continue implementing new things we need or we'll switch to [`libeio`](https://metacpan.org/pod/release/TYPESTER/UV-0.1/deps/libuv/src/unix/eio/eio.pod) . However there are probably long term uses for this file as we may want to put here Redis specific background tasks (for instance it is not impossible that we'll need a non blocking `FLUSHDB`/`FLUSHALL` implementation).

DESIGN
 ------

The design is trivial, we have a structure representing a **job** to perform and a different thread and **job queue** for every job type. Every thread waits for new jobs in its queue, and process every job sequentially.

Jobs of the same type are guaranteed to be processed from the least recently inserted to the most recently inserted (older jobs processed first).

Currently there is no way for the creator of the job to be notified about the **completion of the operation**, this will only be added when/if needed.

