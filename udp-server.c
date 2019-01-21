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
#include <linux/ipv6.h>
#include <arpa/inet.h>

static int init_server(int port)
{
	int listening_fd;
	struct sockaddr_in6 addr = { 0 };

	listening_fd = socket(AF_INET6, SOCK_DGRAM, 0);
	if (listening_fd < 0) {
		fprintf(stderr, "Error opening socket: %s\n", strerror(errno));
		return -1;
	}

	/* See 'man 7 ipv6' for 'IPV6_RECVPKTINFO' */
	int opts[] = { 1 };
	if (setsockopt
	    (listening_fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &opts,
	     sizeof(int)) < 0) {
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

	/* Listen on all local interfaces and IP addresses. */
	addr.sin6_family = AF_INET6;
	addr.sin6_addr = in6addr_any;
	addr.sin6_port = htons(port);

	if (bind(listening_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
		return -1;
	}

	return listening_fd;
}

static int find_header(struct msghdr *msg, struct in6_pktinfo *pktinfo)
{
	for (struct cmsghdr * cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL;
	     cmsg = CMSG_NXTHDR(msg, cmsg)) {
		/* Ignore other headers */
		if (cmsg->cmsg_level != IPPROTO_IPV6 ||
		    cmsg->cmsg_type != IPV6_PKTINFO) {
			continue;
		}

		struct in6_pktinfo *pi = (struct in6_pktinfo *)CMSG_DATA(cmsg);
		memcpy(pktinfo, pi, sizeof(*pktinfo));
		return 0;
	}

	return -1;
}

static void handle_one_packet(int listening_fd, int port)
{
	struct sockaddr_in6 peer_addr = { 0 };
	struct iovec io;
	char data_buffer[100] = { 0 };
	char oob_buffer[256] = { 0 };
	struct msghdr msg = { 0 };
	int ret;

	peer_addr.sin6_family = AF_INET6;

	io.iov_base = data_buffer;
	io.iov_len = sizeof(data_buffer);

	msg.msg_name = &peer_addr;
	msg.msg_namelen = sizeof(peer_addr);
	msg.msg_iov = &io;
	msg.msg_iovlen = 1;
	msg.msg_control = oob_buffer;
	msg.msg_controllen = sizeof(oob_buffer);

	/* Wait for one UDP packet. Get packet payload and auxiliary data. */
	ret = recvmsg(listening_fd, &msg, 0);
	if (ret < 0) {
		fprintf(stderr, "recmsg() failed: %s\n", strerror(errno));
		return;
	}

	/*
	 * See /usr/include/linux/ipv6.h:
	 *    struct in6_pktinfo {
	 *        struct in6_addr ipi6_addr;
	 *        int             ipi6_ifindex;
	 *    };
	 */
	struct in6_pktinfo pktinfo;

	if (find_header(&msg, &pktinfo) < 0) {
		fprintf(stderr, "dsfs\n");
		return;
	}

	/* Print peer and remote address information. */
	char peer_addr_str[100] = { 0 };
	char local_addr_str[100] = { 0 };

	assert(inet_ntop(AF_INET6, &peer_addr.sin6_addr, peer_addr_str,
			 sizeof(peer_addr_str)) != NULL);
	assert(inet_ntop(AF_INET6, &pktinfo.ipi6_addr, local_addr_str,
			 sizeof(local_addr_str)) != NULL);

	printf("peer %s (%5d)  ->  local %s (%5d)\n",
	       peer_addr_str, ntohs(peer_addr.sin6_port), local_addr_str, port);

	/* Ignore the actual packet payload. Just print the packet length. */
	printf("Server %d received bytes of data from the client.\n", ret);
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

	while (true) {
		handle_one_packet(listening_fd, port);
	}

	return 0;		/* Never reached */
}
