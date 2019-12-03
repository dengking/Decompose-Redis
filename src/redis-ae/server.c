/*
 * server.c
 *
 *  Created on: 2019Äê6ÔÂ9ÈÕ
 */

#ifndef SERVER_C_
#define SERVER_C_

#include "server.h"

/* Global vars */
struct redisServer server; /* Server global state */

void initServerConfig(void) {
	server.port = CONFIG_DEFAULT_SERVER_PORT;
	server.tcp_backlog = CONFIG_DEFAULT_TCP_BACKLOG;
	server.bindaddr_count = 0;
	server.unixsocket = NULL;
	server.unixsocketperm = CONFIG_DEFAULT_UNIX_SOCKET_PERM;
	server.ipfd_count = 0;
	server.sofd = -1;
	server.maxclients = CONFIG_DEFAULT_MAX_CLIENTS;
}

/* Initialize a set of file descriptors to listen to the specified 'port'
 * binding the addresses specified in the Redis server configuration.
 *
 * The listening file descriptors are stored in the integer array 'fds'
 * and their number is set in '*count'.
 *
 * The addresses to bind are specified in the global server.bindaddr array
 * and their number is server.bindaddr_count. If the server configuration
 * contains no specific addresses to bind, this function will try to
 * bind * (all addresses) for both the IPv4 and IPv6 protocols.
 *
 * On success the function returns C_OK.
 *
 * On error the function returns C_ERR. For the function to be on
 * error, at least one of the server.bindaddr addresses was
 * impossible to bind, or no bind addresses were specified in the server
 * configuration but the function is not able to bind * for at least
 * one of the IPv4 or IPv6 protocols. */
int listenToPort(int port, int *fds, int *count) {
	int j;

	/* Force binding of 0.0.0.0 if no bind address is specified, always
	 * entering the loop if j == 0. */
	if (server.bindaddr_count == 0)
		server.bindaddr[0] = NULL;
	for (j = 0; j < server.bindaddr_count || j == 0; j++) {
		if (server.bindaddr[j] == NULL) {
			int unsupported = 0;
			/* Bind * for both IPv6 and IPv4, we enter here only if
			 * server.bindaddr_count == 0. */
			fds[*count] = anetTcp6Server(server.neterr, port, NULL,
					server.tcp_backlog);
			if (fds[*count] != ANET_ERR) {
				anetNonBlock(NULL, fds[*count]);
				(*count)++;
			} else if (errno == EAFNOSUPPORT) {
				unsupported++;
				serverLog(LL_WARNING, "Not listening to IPv6: unsupported");
			}

			if (*count == 1 || unsupported) {
				/* Bind the IPv4 address as well. */
				fds[*count] = anetTcpServer(server.neterr, port, NULL,
						server.tcp_backlog);
				if (fds[*count] != ANET_ERR) {
					anetNonBlock(NULL, fds[*count]);
					(*count)++;
				} else if (errno == EAFNOSUPPORT) {
					unsupported++;
					serverLog(LL_WARNING, "Not listening to IPv4: unsupported");
				}
			}
			/* Exit the loop if we were able to bind * on IPv4 and IPv6,
			 * otherwise fds[*count] will be ANET_ERR and we'll print an
			 * error and return to the caller with an error. */
			if (*count + unsupported == 2)
				break;
		} else if (strchr(server.bindaddr[j], ':')) {
			/* Bind IPv6 address. */
			fds[*count] = anetTcp6Server(server.neterr, port,
					server.bindaddr[j], server.tcp_backlog);
		} else {
			/* Bind IPv4 address. */
			fds[*count] = anetTcpServer(server.neterr, port, server.bindaddr[j],
					server.tcp_backlog);
		}
		if (fds[*count] == ANET_ERR) {
			serverLog(LL_WARNING,
					"Could not create server TCP listening socket %s:%d: %s",
					server.bindaddr[j] ? server.bindaddr[j] : "*", port,
					server.neterr);
			if (errno == ENOPROTOOPT || errno == EPROTONOSUPPORT
					|| errno == ESOCKTNOSUPPORT || errno == EPFNOSUPPORT
					|| errno == EAFNOSUPPORT || errno == EADDRNOTAVAIL)
				continue;
			return C_ERR;
		}
		anetNonBlock(NULL, fds[*count]);
		(*count)++;
	}
	return C_OK;
}

void initServer(void) {
	server.pid = getpid();
	server.current_client = NULL;
	server.clients = listCreate();
	server.clients_index = raxNew();
	server.clients_to_close = listCreate();
	server.clients_pending_write = listCreate();
	server.clients_pending_read = listCreate();
	server.unblocked_clients = listCreate();

	adjustOpenFilesLimit();
	server.el = aeCreateEventLoop(server.maxclients + CONFIG_FDSET_INCR);
	if (server.el == NULL) {
		serverLog(LL_WARNING,
				"Failed creating the event loop. Error message: '%s'",
				strerror(errno));
		exit(1);
	}
	/* Open the TCP listening socket for the user commands. */
	if (server.port
			!= 0&& listenToPort(server.port, server.ipfd, &server.ipfd_count)
			== C_ERR) {
		exit(1);
	}
	/* Open the listening Unix domain socket. */
	if (server.unixsocket != NULL) {
		unlink(server.unixsocket); /* don't care if this fails */
		server.sofd = anetUnixServer(server.neterr, server.unixsocket,
				server.unixsocketperm, server.tcp_backlog);
		if (server.sofd == ANET_ERR) {
			serverLog(LL_WARNING, "Opening Unix socket: %s", server.neterr);
			exit(1);
		}
		anetNonBlock(NULL, server.sofd);
	}

	/* Abort if there are no listening sockets at all. */
	if (server.ipfd_count == 0 && server.sofd < 0) {
		serverLog(LL_WARNING, "Configured to not listen anywhere, exiting.");
		exit(1);
	}
	/* Create the timer callback, this is our way to process many background
	 * operations incrementally, like clients timeout, eviction of unaccessed
	 * expired keys and so forth. */
	if (aeCreateTimeEvent(server.el, 1, serverCron, NULL, NULL) == AE_ERR) {
		serverPanic("Can't create event loop timers.");
		exit(1);
	}
	/* Create an event handler for accepting new connections in TCP and Unix
	 * domain sockets. */
	for (int j = 0; j < server.ipfd_count; j++) {
		if (aeCreateFileEvent(server.el, server.ipfd[j], AE_READABLE,
				acceptTcpHandler, NULL) == AE_ERR) {
			serverPanic("Unrecoverable error creating server.ipfd file event.");
		}
	}
	if (server.sofd > 0&& aeCreateFileEvent(server.el,server.sofd,AE_READABLE,
			acceptUnixHandler,NULL) == AE_ERR)
		serverPanic("Unrecoverable error creating server.sofd file event.");


	/* 32 bit instances are limited to 4GB of address space, so if there is
	 * no explicit limit in the user provided configuration we set a limit
	 * at 3 GB using maxmemory with 'noeviction' policy'. This avoids
	 * useless crashes of the Redis instance for out of memory. */
	if (server.arch_bits == 32 && server.maxmemory == 0) {
		serverLog(LL_WARNING,
				"Warning: 32 bit instance detected but no memory limit set. Setting 3 GB maxmemory limit with 'noeviction' policy now.");
		server.maxmemory = 3072LL * (1024 * 1024); /* 3 GB */
		server.maxmemory_policy = MAXMEMORY_NO_EVICTION;
	}
}

int main(int argc, char **argv) {
	struct timeval tv;
	int j;

	setlocale(LC_COLLATE, "");
	tzset(); /* Populates 'timezone' global. */
	zmalloc_set_oom_handler(redisOutOfMemoryHandler);

	gettimeofday(&tv, NULL);

	initServerConfig();

	server.supervised = redisIsSupervised(server.supervised_mode);
	int background = server.daemonize && !server.supervised;
	if (background)
		daemonize();

	initServer();

	if (background || server.pidfile)
		createPidFile();
	redisSetProcTitle(argv[0]);
	redisAsciiArt();
	checkTcpBacklogSettings();

	aeSetBeforeSleepProc(server.el, beforeSleep);
	aeSetAfterSleepProc(server.el, afterSleep);

	aeMain(server.el); // main loop
	aeDeleteEventLoop(server.el);
	return 0;
}

#endif /* SERVER_C_ */
