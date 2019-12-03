/*
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SERVER_H_
#define SERVER_H_

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "ae.h"/* Event driven programming library */
#include "anet.h" /* Networking the easy way */
#include "adlist.h"  /* Linked lists */
#include "rax.h"     /* Radix tree */
#include "sds.h"     /* Dynamic safe strings */

/* Error codes */
#define C_OK                    0
#define C_ERR                   -1

#define NET_IP_STR_LEN 46 /* INET6_ADDRSTRLEN is 46, but we need to be sure */

/* Static server configuration */
#define CONFIG_DEFAULT_SERVER_PORT        6379  /* TCP port. */
#define CONFIG_DEFAULT_TCP_BACKLOG       511    /* TCP listen backlog. */
#define CONFIG_DEFAULT_UNIX_SOCKET_PERM 0

#define CONFIG_MIN_RESERVED_FDS 32
#define CONFIG_DEFAULT_MAX_CLIENTS 10000

/* Protocol and I/O related defines */
#define PROTO_MAX_QUERYBUF_LEN  (1024*1024*1024) /* 1GB max query buffer. */
#define PROTO_IOBUF_LEN         (1024*16)  /* Generic I/O buffer size */
#define PROTO_REPLY_CHUNK_BYTES (16*1024) /* 16k output buffer */
#define PROTO_INLINE_MAX_SIZE   (1024*64) /* Max size of inline reads */
#define PROTO_MBULK_BIG_ARG     (1024*32)
#define LONG_STR_SIZE      21          /* Bytes needed for long -> str + '\0' */
#define REDIS_AUTOSYNC_BYTES (1024*1024*32) /* fdatasync every 32MB */

/* Client block type (btype field in client structure)
 * if CLIENT_BLOCKED flag is set. */
#define BLOCKED_NONE 0    /* Not blocked, no CLIENT_BLOCKED flag set. */
#define BLOCKED_LIST 1    /* BLPOP & co. */
#define BLOCKED_WAIT 2    /* WAIT for synchronous replication. */
#define BLOCKED_MODULE 3  /* Blocked by a loadable module. */
#define BLOCKED_STREAM 4  /* XREAD. */
#define BLOCKED_ZSET 5    /* BZPOP et al. */
#define BLOCKED_NUM 6     /* Number of blocked states. */

/* When configuring the server eventloop, we setup it so that the total number
 * of file descriptors we can handle are server.maxclients + RESERVED_FDS +
 * a few more to stay safe. Since RESERVED_FDS defaults to 32, we add 96
 * in order to make sure of not over provisioning more than 128 fds. */
#define CONFIG_FDSET_INCR (CONFIG_MIN_RESERVED_FDS+96)
#define CONFIG_BINDADDR_MAX 16

#define LRU_BITS 24
typedef struct redisObject {
	unsigned type :4;
	unsigned encoding :4;
	unsigned lru :LRU_BITS; /* LRU time (relative to global lru_clock) or
	 * LFU data (least significant 8 bits frequency
	 * and most significant 16 bits access time). */
	int refcount;
	void *ptr;
} robj;

/* This structure represents a Redis user. This is useful for ACLs, the
 * user is associated to the connection after the connection is authenticated.
 * If there is no associated user, the connection uses the default user. */
#define USER_COMMAND_BITS_COUNT 1024    /* The total number of command bits
                                           in the user structure. The last valid
                                           command ID we can set in the user
                                           is USER_COMMAND_BITS_COUNT-1. */
#define USER_FLAG_ENABLED (1<<0)        /* The user is active. */
#define USER_FLAG_DISABLED (1<<1)       /* The user is disabled. */
#define USER_FLAG_ALLKEYS (1<<2)        /* The user can mention any key. */
#define USER_FLAG_ALLCOMMANDS (1<<3)    /* The user can run all commands. */
#define USER_FLAG_NOPASS      (1<<4)    /* The user requires no password, any
                                           provided password will work. For the
                                           default user, this also means that
                                           no AUTH is needed, and every
                                           connection is immediately
                                           authenticated. */

/* Anti-warning macro... */
#define UNUSED(V) ((void) V)

typedef struct user {
	sds name; /* The username as an SDS string. */
	uint64_t flags; /* See USER_FLAG_* */

	list *passwords; /* A list of SDS valid passwords for this user. */
} user;

/* With multiplexing we need to take per-client state.
 * Clients are taken in a linked list. */
typedef struct client {
	uint64_t id; /* Client incremental unique ID. */
	int fd; /* Client socket. */
	int resp; /* RESP protocol version. Can be 2 or 3. */
	robj *name; /* As set by CLIENT SETNAME. */
	sds querybuf; /* Buffer we use to accumulate client queries. */
	size_t qb_pos; /* The position we have read in querybuf. */
	size_t querybuf_peak; /* Recent (100ms or more) peak of querybuf size. */

	user *user; /* User associated with this connection. If the
	 user is set to NULL the connection can do
	 anything (admin). */
	int reqtype; /* Request protocol type: PROTO_REQ_* */

	list *reply; /* List of reply objects to send to the client. */
	unsigned long long reply_bytes; /* Tot bytes of objects in reply list. */
	size_t sentlen; /* Amount of bytes already sent in the current
	 buffer or object being sent. */

	time_t ctime; /* Client creation time. */
	time_t lastinteraction; /* Time of the last interaction, used for timeout */
	time_t obuf_soft_limit_reached_time;
	int flags; /* Client flags: CLIENT_* macros. */

	int authenticated; /* Needed when the default user requires auth. */

	int btype; /* Type of blocking op if CLIENT_BLOCKED. */

	listNode *client_list_node; /* list node in client list */

	/* Response buffer */
	int bufpos;
	char buf[PROTO_REPLY_CHUNK_BYTES];
} client;

struct redisServer {
	/* General */
	pid_t pid; /* Main process pid. */
	aeEventLoop *el;

	/* Limits */
	unsigned int maxclients; /* Max number of simultaneous clients */

	/* Networking */
	int port; /* TCP listening port */
	int tcp_backlog; /* TCP listen() backlog */
	char *bindaddr[CONFIG_BINDADDR_MAX]; /* Addresses we should bind to */
	int bindaddr_count; /* Number of addresses in server.bindaddr[] */
	char *unixsocket; /* UNIX socket path */
	mode_t unixsocketperm; /* UNIX socket permission */
	int ipfd[CONFIG_BINDADDR_MAX]; /* TCP socket file descriptors */
	int ipfd_count; /* Used slots in ipfd[] */
	int sofd; /* Unix socket file descriptor */
	char neterr[ANET_ERR_LEN]; /* Error buffer for anet.c */

	list *clients; /* List of active clients */
	list *clients_to_close; /* Clients to close asynchronously */
	list *clients_pending_write; /* There is to write or install handler. */
	list *clients_pending_read; /* Client has pending read socket buffers. */
	_Atomic uint64_t next_client_id; /* Next client unique ID. Incremental. */

	/* Blocked clients */
	unsigned int blocked_clients; /* # of clients executing a blocking cmd.*/
	unsigned int blocked_clients_by_type[BLOCKED_NUM];
	list *unblocked_clients; /* list of clients to unblock before next loop */

	client *current_client; /* Current client, only used on crash report */
	rax *clients_index; /* Active clients dictionary by client ID. */

	/* Configuration */
	int tcpkeepalive;               /* Set SO_KEEPALIVE if non-zero. */
};

#endif /* SERVER_H_ */
