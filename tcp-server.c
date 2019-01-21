// SPDX-License-Identifier: Unlicense
/*
 * Copyright (C) Stefan Lengfeld 2019
 */

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int init_server(int port)
{
	int listening_fd;
	struct sockaddr_in6 addr = { 0 };

	listening_fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (listening_fd < 0) {
		fprintf(stderr, "Error opening socket: %s\n", strerror(errno));
		return -1;
	}

	/*
	 * Set SO_REUSEADDR on the socket to allow rebinding to the same
	 * port/address at once even when there are still connections in the
	 * TIME_WAIT state. See 'netstat -t'.
	 */
	int opts[] = { 1 };
	if (setsockopt
	    (listening_fd, SOL_SOCKET, SO_REUSEADDR, &opts, sizeof(int)) < 0) {
		fprintf(stderr, "Error opening socket: %s\n", strerror(errno));
		return -1;
	}

	/*
	 * Receive also IPv4 connections with the IPv4-mapped IPV6 addresses.
	 * Set the flag explicitly to avoid relying on the system default
	 * setting in '/proc/sys/net/ipv6/bindv6only'.  See 'man 7 ipv6'.
	 */
	int opts2[] = { 0 };	/* == false */
	if (setsockopt
	    (listening_fd, IPPROTO_IPV6, IPV6_V6ONLY, &opts2,
	     sizeof(int)) < 0) {
		fprintf(stderr, "Error in setsockopt(): %s\n", strerror(errno));
		return -1;
	}

	/* Listen on all local interfaces and IP addresses */
	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(port);

	if (bind(listening_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
		return -1;
	}

	if (listen(listening_fd, 2) < 0) {
		fprintf(stderr, "Error listen on socket: %s\n",
			strerror(errno));
		return -1;
	}

	return listening_fd;
}

static int accept_new_client(int listening_fd)
{
	int client_fd;
	struct sockaddr_in6 peer_addr = { 0 };
	socklen_t peer_addr_size = sizeof(peer_addr);
	struct sockaddr_in6 local_addr = { 0 };
	socklen_t local_addr_size = sizeof(local_addr);
	int ret;

	/*
	 * The second argument of accept(), the content of return parameter
	 * 'peer_addr', is the same value as function getpeername() would
	 * return.
	 */
	client_fd = accept(listening_fd, (struct sockaddr *)&peer_addr,
			   &peer_addr_size);
	assert(sizeof(peer_addr) == peer_addr_size);
	if (client_fd < 0) {
		fprintf(stderr, "Cannot accept new connection\n");
		return -1;
	}

	/* Get local IP address and port of the TCP connection. */
	ret = getsockname(client_fd, (struct sockaddr *)&local_addr,
			  &local_addr_size);
	assert(sizeof(local_addr) == local_addr_size);
	if (ret != 0) {
		fprintf(stderr, "Cannot call getsockname!\n");
		return -1;
	}

	/* Print peer and remote address information. */
	char peer_addr_str[100] = { 0 };
	char local_addr_str[100] = { 0 };

	assert(inet_ntop(AF_INET6, &peer_addr.sin6_addr, peer_addr_str,
			 sizeof(peer_addr_str)) != NULL);
	assert(inet_ntop(AF_INET6, &local_addr.sin6_addr, local_addr_str,
			 sizeof(local_addr_str)) != NULL);

	printf("peer %s (%5d)  ->  local %s (%5d)\n",
	       peer_addr_str, ntohs(peer_addr.sin6_port),
	       local_addr_str, ntohs(local_addr.sin6_port));

	return client_fd;
}

__attribute__ ((__noreturn__))
void mainloop(int listening_fd)
{
	char buf[] = "Hello, world.\n";
	int buf_len = strlen(buf);

	while (true) {
		int client_fd;
		int ret;

		client_fd = accept_new_client(listening_fd);
		if (client_fd < 0) {
			continue;
		}

		/* Send some static data to every client. */
		ret = write(client_fd, buf, buf_len);
		if (ret != buf_len) {
			fprintf(stderr,
				"Failure or not written all bytes to client!\n");
		}

		close(client_fd);
	}
}

int parse_port(const char *str)
{
	long long int parsed_number;
	char *end_str;

	errno = 0;
	parsed_number = strtol(str, &end_str, 10);
	if (errno) {
		fprintf(stderr, "Could not extract port number from string!\n");
		errno = 0;
		return -1;
	}

	if (*end_str != '\0') {
		fprintf(stderr, "Additional characters after port!\n");
		return -1;
	}

	if (parsed_number < 0 || parsed_number > ((1 << 16) - 1)) {
		fprintf(stderr, "Number is not a valid port. Out of range!\n");
		return -1;
	}

	return parsed_number;
}

int main(int argc, char **argv)
{
	int listening_fd;
	int port;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		return 1;
	}

	port = parse_port(argv[1]);
	if (port == -1) {
		return 1;
	}

	if (port == 0) {
		fprintf(stderr, "Magic port number 0 is not allowed!\n");
		return 1;
	}

	listening_fd = init_server(port);
	if (listening_fd < 0) {
		return 1;
	}

	mainloop(listening_fd);
}
