/*
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "inic_common.h"
#include <net.h>

uint8_t dest_mac[6]={0xff,0xff,0xff,0xff,0xff,0xff};

struct	ether_header {
	u_char	ether_dhost[6];
	u_char	ether_shost[6];
	u_short	ether_type;
};

int timeout_val = 90;

#if TCSUPPORT_SISM_HOST
#define DEFAULT_IF	"nas10"
const int inic_stage = 0;
#else
#define DEFAULT_IF	"eth0.1.99"
int inic_stage = 0;
#endif
uint16_t cmd_id = 0;

static uint8_t is_first_time = 0;

#define IFNAMSIZ 16
#define SEND_BUF	1
#define RCV_BUF		2
#define SRC_PKT_LEN	 (sizeof(src_hdr_t)+sizeof(blapi_hdr_t)+sizeof(struct ether_header))

/* IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence) */
#define ETH_ALEN	6		        /* Octets in one ethernet addr	 */
#define ETH_HLEN	14		        /* Total octets in header.	 */
#define ETH_ZLEN	60		        /* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
#define ETH_FRAME_LEN	1514		/* Max. octets in frame sans FCS */



char ifName[IFNAMSIZ];

void htonll(uint64_t val, uint64_t * target)
{
	uint64_t val_64;
	val_64 = (((uint64_t) htonl(val)) <<32)+htonl(val>> 32);
	memcpy(target,&val_64,sizeof(uint64_t));
}

void ntohll(uint64_t val, uint64_t * target)
{
	uint64_t val_64;
	val_64 = (((uint64_t) ntohl(val)) <<32)+ntohl(val>> 32);
	memcpy(target,&val_64,sizeof(uint64_t));
}

int pt_pkt_send(unsigned short ethertype, uint8_t* buf,int len)
{
	int tx_len = 0;
	int buf_len = 0;
	int ret = -1;
	uint8_t *sendbuf;
	extern unsigned char mac_addr[6];

	if(ethertype == ETH_TYPE_PROC)
	{
		len += sizeof(src_hdr_t);
	}
	buf_len = len + sizeof(struct ether_header);

	sendbuf = (uint8_t *)malloc(buf_len);

	if (sendbuf == NULL)
	{
		SISM_DBG_MSG(DBG_ERR,"sendbuf alloc failed\n");
		return -1;
	}

	struct ether_header *eh = (struct ether_header *) sendbuf;

	/* Construct the Ethernet header */
	memset(sendbuf, 0, sizeof(struct ether_header));
	/* Ethernet header */
	eh->ether_shost[0] = 0x00;
	eh->ether_shost[1] = 0xaa;
	eh->ether_shost[2] = 0xbb;
	eh->ether_shost[3] = 0x01;
	eh->ether_shost[4] = 0x23;
	eh->ether_shost[5] = 0x45;

	if(is_first_time == 1)
	{
		eh->ether_dhost[0] = dest_mac[0];
		eh->ether_dhost[1] = dest_mac[1];
		eh->ether_dhost[2] = dest_mac[2];
		eh->ether_dhost[3] = dest_mac[3];
		eh->ether_dhost[4] = dest_mac[4];
		eh->ether_dhost[5] = dest_mac[5];
	}
	else
	{
		eh->ether_dhost[0] = mac_addr[0];
		eh->ether_dhost[1] = mac_addr[1];
		eh->ether_dhost[2] = mac_addr[2];
		eh->ether_dhost[3] = mac_addr[3];
		eh->ether_dhost[4] = mac_addr[4];
		eh->ether_dhost[5] = mac_addr[5];
	}

	/* Ethertype field */
	if (ethertype == ETH_TYPE_SRC)
	{
		tx_len+=sizeof(blapi_hdr_t);
		tx_len += sizeof(src_hdr_t);
	}	
	else
	{
		tx_len+=len;

	}	

    //printf("[hsm] txlen %d len %d buf_len %d\n", tx_len, len, buf_len);
	ethertype = htons(ethertype);
	memcpy(&eh->ether_type, &ethertype, sizeof(ethertype));
	tx_len += sizeof(struct ether_header);
	

	memcpy(sendbuf + sizeof(struct ether_header), buf, tx_len - sizeof(struct ether_header));
	/* Index of the network device */
	// socket_address.sll_ifindex = if_idx.ifr_ifindex;
	// /* Address length*/
	// socket_address.sll_halen = ETH_ALEN;
	// /* Destination MAC */
	// socket_address.sll_addr[0] = dest_mac[0];
	// socket_address.sll_addr[1] = dest_mac[1];
	// socket_address.sll_addr[2] = dest_mac[2];
	// socket_address.sll_addr[3] = dest_mac[3];
	// socket_address.sll_addr[4] = dest_mac[4];
	// socket_address.sll_addr[5] = dest_mac[5];

	SISM_DBG_MSG(DBG_INFO,"tx_len = %x\n",tx_len);

	if (tx_len > buf_len)
	{
		SISM_DBG_MSG(DBG_ERR,"tx_len: %x exceed buf_len: %x, Send failed\n",tx_len, buf_len);
		goto end; 	
	}

	/* Send packet */
	ret = eth_send(sendbuf, buf_len);

end:
	if(sendbuf != NULL)
	{
		free(sendbuf);
	}
	return ret;
}

int pt_pkt_recv(unsigned short ethertype, uint8_t* payload)
{
	int ret = 0;
	//int sockfd = -1;
	//size_t numbytes;

	/* Header structures */
	struct ether_header *eh = (struct ether_header *) payload;
	//SISM_DBG_MSG(DBG_INFO,"listener: Waiting to recvfrom...\n");
	//numbytes = recvfrom(sockfd, rcvbuf, BUF_SIZ+sizeof(struct ether_header), 0, NULL, NULL);
	//SISM_DBG_MSG(DBG_INFO,"listener: got packet %d bytes\n", numbytes);
	/* record destination MAC*/
	if (is_first_time == 0)
	{
		dest_mac[0] = eh->ether_shost[0];
		dest_mac[1] = eh->ether_shost[1];
		dest_mac[2] = eh->ether_shost[2];
		dest_mac[3] = eh->ether_shost[3];
		dest_mac[4] = eh->ether_shost[4];
		dest_mac[5] = eh->ether_shost[5];
		is_first_time = 1;
	}

#if DEBUG
	uint8_t *rcvbuf = payload-sizeof(struct ether_header);
	printf("\tData:");
	for (i=0; i<80; i++) printf("%02x:", rcvbuf[i]);
		printf("\n");
#endif

	return ret;
}

