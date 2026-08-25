/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 
#include "skbuff.h"
#include <asm/tc3162.h>
#include<stdio.h>
#include<string.h>
#define K0_TO_K1(x)		x//( ((unsigned long)x) | 0xa0000000)

#define SKBUFF_SIZE		512

#ifndef CONFIG_SYS_CACHELINE_SIZE
#define CONFIG_SYS_CACHELINE_SIZE	64
#endif	/* CONFIG_SYS_CACHELINE_SIZE */

sk_buff *skbuff;
int skb_cur = 0;
unsigned char SKB_BUFF[SKBUFF_SIZE][sizeof(sk_buff)] __attribute__ ((__aligned__(CONFIG_SYS_CACHELINE_SIZE)));


void skb_init_addr(void);
void skb_headerinit(sk_buff *skb, unsigned int used);

int skb_init(void) 
{
	sk_buff *skb;
	int i;

	skb_init_addr();
	for (i = 0; i < SKBUFF_SIZE; i++) {
		skb = &skbuff[i];
		skb_headerinit(skb, 0);
	}

	return 0;
}

void skb_init_addr(void){
	if ((sizeof(sk_buff) != SKBUFF_ENTRY_SIZE)) {
		printf("%s() :Error! please check SKBUFF_ENTRY_SIZE sizeof(sk_buff) = 0x%x\n", __func__, sizeof(sk_buff));
	}
	skbuff = (sk_buff *)&(SKB_BUFF[0][0]);
	
}

void skb_headerinit(sk_buff *skb, unsigned int used)
{
	memset(skb, 0, sizeof(sk_buff));

	skb->data = K0_TO_K1(skb->buf);
	skb->len = 0;
	skb->truesize = 2000;
	skb->used = used;
}

sk_buff *alloc_skb(unsigned int size)
{
	sk_buff *skb;
	
	do {
		skb = &skbuff[skb_cur];
		skb_cur = (skb_cur + 1) % SKBUFF_SIZE;
	} while (skb->used == 1);
	
	skb_headerinit(skb, 1);
	return skb;
}

void free_skb(sk_buff *skb)
{
	skb_headerinit(skb, 0);
}

unsigned char *skb_put(sk_buff *skb, unsigned int len)
{
	skb->len += len;
	return skb->data;
}

unsigned char *skb_pull(sk_buff *skb, unsigned int len)
{
	skb->data += len;
	skb->len -= len;
	return skb->data;
}

unsigned char *skb_push(sk_buff *skb, unsigned int len)
{
	skb->data -= len;
	skb->len += len;
	return skb->data;
}

void skb_reserve(sk_buff *skb, unsigned int len)
{
	skb->data += len;
}
