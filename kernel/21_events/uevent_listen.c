// SPDX-License-Identifier: GPL-2.0
/*
 * uevent_listen.c — 订阅内核 uevent 的 netlink 组播，等到一条带 VIRT_EVENT= 的消息。
 *
 * udev 收 uevent 用的也是 NETLINK_KOBJECT_UEVENT 的 1 号组。
 * 一条消息是 "动作@路径" 加上若干 "键=值"，中间用 '\0' 隔开。
 */
#define _GNU_SOURCE
#include <linux/netlink.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int timeout_ms = argc > 1 ? atoi(argv[1]) : 5000;
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
		.nl_groups = 1,
	};
	struct pollfd pfd;
	char buf[4096];
	int sock, found = 0;

	sock = socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT);
	if (sock < 0 || bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		perror("netlink");
		return 1;
	}
	printf("listening on NETLINK_KOBJECT_UEVENT group 1\n");
	fflush(stdout);

	pfd.fd = sock;
	pfd.events = POLLIN;
	while (!found && poll(&pfd, 1, timeout_ms) > 0) {
		ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
		ssize_t off = 0;

		if (n <= 0)
			break;
		buf[n] = '\0';
		if (!memmem(buf, n, "VIRT_EVENT=", 11))
			continue;
		while (off < n) {
			printf("  %s\n", buf + off);
			off += strlen(buf + off) + 1;
		}
		found = 1;
	}
	close(sock);
	printf("%s\n", found ? "uevent received" : "uevent timeout");
	return found ? 0 : 1;
}
