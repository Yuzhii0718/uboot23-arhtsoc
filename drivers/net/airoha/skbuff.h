// SPDX-License-Identifier: GPL-2.0-only 
/*
 * Copyright (c) 2024 AIROHA Inc
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 
#ifndef __SKBUFF_H
#define __SKBUFF_H

#define SKBUFF_ENTRY_SIZE 0x800 /* this value must match sizeof(sk_buff) */
#if __LONG_MAX__ == 2147483647L
#define IS_32BIT
#else
#define IS_64BIT
#endif

typedef struct sk_buff_s {
	unsigned char buf[2000];
	unsigned int truesize;			/* Buffer size 				*/

	unsigned char *data;			/* Data head pointer		*/
	unsigned char *ip_hdr;			/* IP header pointer		*/
	unsigned char *udp_hdr;			/* UDP header pointer		*/
	unsigned int used;
	unsigned int len;				/* Length of actual data	*/
	/* EN7580's rx pkt must be align to 16*n, then add resv1 and resv2 resv3 to make sure pkt length as 2048Byte */
	unsigned int resv1;
	unsigned int resv2;
#ifdef IS_32BIT	
	unsigned int resv3[4];
#endif

} sk_buff;

int skb_init(void);
sk_buff *alloc_skb(unsigned int size);
void free_skb(sk_buff *skb);
unsigned char *skb_put(sk_buff *skb, unsigned int len);
unsigned char *skb_push(sk_buff *skb, unsigned int len);
unsigned char *skb_pull(sk_buff *skb, unsigned int len);
void skb_reserve(sk_buff *skb, unsigned int len);

#endif	/* __SKBUFF_H */
