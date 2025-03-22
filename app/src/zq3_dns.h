/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZQ3_DNS_H
#define ZQ3_DNS_H

int zq3_dns_resolve(
	const uint8_t *name,
	bool tls,
	struct sockaddr_storage *addr
);

#endif /* ZQ3_DNS_H */
