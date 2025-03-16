/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 * DNS & Socket Docs & Refs:
 * https://docs.zephyrproject.org/apidoc/latest/group__bsd__sockets.html
 * https://docs.zephyrproject.org/apidoc/latest/netdb_8h.html (getaddrinfo / freeaddrinfo)
 * https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/net/dns_resolve.h
 * https://docs.zephyrproject.org/apidoc/latest/group__ip__4__6.html (net_addr_ntop)
 */

#include <zephyr/net/socket.h>
#include "zq3.h"


#define TLS_PORTSTR     ("8883")
#define TLS_PORT        (8883)
#define NON_TLS_PORTSTR ("1883")
#define NON_TLS_PORT    (1883)

// Resolve hostname to IPv4 IP (IPv6 not supported)
// This requires CONFIG_POSIX_API=y and CONFIG_NET_SOCKETS_POSIX_NAMES=y.
//
int zq3_dns_resolve(zq3_context *zctx, struct sockaddr_storage *broker) {
	if (zctx == NULL || broker == NULL) {
		return 1;
	}
	struct addrinfo *res = NULL;
	const struct addrinfo hint = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};
	const char * service = zctx->tls ? TLS_PORTSTR : NON_TLS_PORTSTR;
	int err = getaddrinfo(zctx->host, service, &hint, &res);
	if (err) {
		switch(err) {
		case DNS_EAI_SYSTEM:
			printk("DNS_EAI_SYSTEM ERR: Is wifi connected?\n");
			break;
		default:
			// Look for enum with EAI_* in include/zephyr/net/dns_resolve.h
			printk("ERR: DNS fail %d\n", err);
		}
	} else if (!res || !(res->ai_addr) || (res->ai_family != AF_INET)) {
		// This shouldn't happen, but check anyway because null pointer
		// dereference hard faults are no fun.
		printk("ERR: DNS result struct was damaged\n");
	} else {
		// At this point, we can trust that res and res->ai_addr are not NULL
		// and that the result is an IPv4 address.

		// Copy IPv4 address from DNS lookup (src) to broker struct (dst)
		//
		// CAUTION! This uses weird pointer casting because that's how the
		// socket API expects you to handle the possibility that a DNS lookup
		// can resolve to either or both of IPv4 and IPv6 addresses.
		//
		// CAUTION: This ignores the possibility of more than one IP address
		// and just uses the address from the first result.
		//
		struct sockaddr_in *src = (struct sockaddr_in *)res->ai_addr;
		struct sockaddr_in *dst = (struct sockaddr_in *)broker;
		dst->sin_family = AF_INET;
		dst->sin_addr.s_addr = src->sin_addr.s_addr;
		// Set TLS or non-TLS port with network byte order conversion function
		dst->sin_port = htons(zctx->tls ? TLS_PORT : NON_TLS_PORT);

		// Debug print the DNS lookup result
		char ip_str[INET_ADDRSTRLEN];  // max length IPv4 address string
		net_addr_ntop(AF_INET, &dst->sin_addr, ip_str, sizeof(ip_str));
		printk("DNS IPv4 result: %s\n", ip_str);
	}
	// IMPORTANT: always free getaddrinfo() result to avoid memory leak
	freeaddrinfo(res);
	return err;
}
