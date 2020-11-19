//
// Created by leoll2 on 11/19/20.
// Copyright (c) 2020 Leonardo Lai. All rights reserved.
//
// The following code derives in part from netmap pkt-gen.c
//

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_mbuf.h>

#include "udpdk_dump.h"


/* Print the content of the packet in hex and ASCII */
void udpdk_dump_payload(const char *payload, int len)
{
	char buf[128];
	int i, j, i0;
	const unsigned char *p = (const unsigned char *)payload;

    printf("Dumping payload [len = %d]:\n", len);

	/* hexdump routine */
	for (i = 0; i < len; ) {
		memset(buf, ' ', sizeof(buf));
		sprintf(buf, "%5d: ", i);
		i0 = i;
		for (j = 0; j < 16 && i < len; i++, j++)
			sprintf(buf + 7 + j*3, "%02x ", (uint8_t)(p[i]));
		i = i0;
		for (j = 0; j < 16 && i < len; i++, j++)
			sprintf(buf + 7 + j + 48, "%c",
				isprint(p[i]) ? p[i] : '.');
		printf("%s\n", buf);
	}
}

void udpdk_dump_mbuf(struct rte_mbuf *m)
{
    udpdk_dump_payload(rte_pktmbuf_mtod(m, char *), rte_pktmbuf_data_len(m));
}
