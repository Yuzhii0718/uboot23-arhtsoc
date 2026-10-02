#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "inic_common.h"
#include <ecnt_image.h>
#include <linux/byteorder/generic.h>
#include <net.h>
#include <linux/delay.h>

extern uint32_t *get_inic_addr(void);
extern uint32_t get_inic_len(void);
extern uint32_t *get_tclinux_7517_addr(void);
extern uint32_t get_tclinux_7517_len(void);
extern void end_of_sending_tclinux(void);
extern bool check_tclinux_sending_state(void);
extern int pt_pkt_send(unsigned short ethertype, uint8_t* buf,int len);
extern int pt_pkt_recv(unsigned short ethertype, uint8_t* payload);
extern unsigned long simple_strtoul(const char * cp, char ** endp, unsigned int base);
extern int mdio_msg_send(unsigned int phy_addr, unsigned int addr, unsigned int data);
extern int mdio_msg_rcv(unsigned int phy_addr, unsigned int addr);

struct trx_header {
	unsigned int magic;			/* "HDR0" */
	unsigned int header_len;    /*Length of trx header*/
	unsigned int len;			/* Length of file including header */
	unsigned int crc32;			/* 32-bit CRC from flag_version to end of file */
	unsigned char version[32];  /*firmware version number*/
	unsigned char customerversion[32];  /*firmware version number*/
//	unsigned int flag_version;	/* 0:15 flags, 16:31 version */
#if 0
	unsigned int reserved[44];	/* Reserved field of header */
#else
	unsigned int kernel_len;	//kernel length
	unsigned int rootfs_len;	//rootfs length
	unsigned int romfile_len;	//romfile length
	#if 0
	unsigned int reserved[42];  /* Reserved field of header */
	#else
	unsigned char Model[32];
	unsigned int decompAddr;//kernel decompress address
	unsigned int saflag;
	unsigned int saflen;
	unsigned int linux_7z_pad;	/* for 4byte alignment */
	unsigned int reserved[29];  /* Reserved field of header */
	#endif
#if defined(TCSUPPORT_SECURE_BOOT_V2)
	SECURE_HEADER_V2 sHeader;
#elif defined(TCSUPPORT_SECURE_BOOT_V1) || defined(TCSUPPORT_SECURE_BOOT_FLASH_OTP)
	SECURE_HEADER_V1 sHeader;
#endif
#endif	
};

#define UNHANDLE_FAIL -1
#define SOCKET_FAIL -2
#define PT_SUCCESS 0
#define CRC_FAIL -3
#define TIMEOUT_FAIL -4

#define FE_SRAM_BASE 0x1fa30000
#define STATUS_REG	0x1fb00f08
#define MSG_CHANNEL_TX	0x1fb00f00
#define MSG_CHANNEL_RX	0x1fb00f04
#define CRC_DISABLE		0x1fb00f0c
#define IMAGE_LEN 0x1fb00f10
#define IMAGE_ADDR 0x1fb00f14
#define CONF_SIZE 0x1fb00f18
#define TCLINUX_START 0x1fb00f1c


#define MSG_READY 0x57414445
#define MSG_READY_INIC 0x5244494E
#define MSG_IMAGE_DONE 0x444f4E45
#define MSG_IMAGE_DONE_INIC 0x494E444E
#define MSG_IMAGE_DONE_INIC_SEG 0x494E5345
#define MSG_FINISH 0x66697368

#define MSG_SGMII2RGMII_MODE 0x53475247
#define MSG_CRC_FAIL 0x4352464c
#define MSG_CRC_SUCCESS 0x46574F4B
#define MSG_NO_CRC	0x4E4F4352
#define CONF_MAGIC 0x434F4E46


#define BP2_TIMEOUT 20
/* HSM timer */
// extern int hsm_timeout;
// extern void hsm_timer(int sig);


/* ROM STATUS */

#define UART_INIT_DONE (0x1<<0)
#define CRC_CHECK_SUCCESS (0x1 <<1)
#define CRC_CHECK_FAIL	(0x1<<2)
#define MSG_HANDLER_DONE (0x1 <<3)
#define CPU_STANDBY	(0x1<<4)
#define NO_CRC_CHECK_BOOT	(0x1<<5)


#define BOOTROM_MAGIC_NO	0x31623162			/* magic no */
#define	BOOTROM_SEGSIZE		1400			/* data segment size */
#define MAX_PKT_SIZE		1518
#define NET_IP_ALIGN 2
#define SEG_SIZE		0x7800
	
/*
 * Command opcode
 */
#define	BOOTROM_NOTIFY      0x01				/* notify */
#define	BOOTROM_NOTIFY_ACK  0x02				/* notify ack */
#define	BOOTROM_RRQ			0x03				/* read request */
#define	BOOTROM_WRQ			0x04				/* write request */
#define	BOOTROM_DATA		0x05				/* data packet */
#define	BOOTROM_ACK			0x06				/* ack */
#define	BOOTROM_NAK			0x07				/* nak */
#define	BOOTROM_START_BOOT	0x08				/* start boot */
#define	BOOTROM_BOOT_OK		0x09				/* boot ok notify */
#define	BOOTROM_ERROR		0x0a				/* error code */
#define	BOOTROM_RESTART		0x0b				/* restart */
#define	BOOTROM_MEMRL		0x0c				/* read memory long value */
#define	BOOTROM_MEMWL		0x0d				/* write memory long value */
#define	BOOTROM_MIIR		0x0e				/* read mii value */
#define	BOOTROM_MIIW		0x0f				/* write mii value */

/*
 * Restart opcode
 */
#define	BOOTROM_RESTART_COLD	0x01				/* cold restart */
#define	BOOTROM_RESTART_WARM	0x02				/* warm restart */
#define	BOOTROM_RESTART_RRQ		0x03				/* restart to read request */


#define UPLOADING_FILE 1
#define NOT_UPLOADING_FILE 0

/* iNIC communication structure */

int last_len;
int inic_file_fd = -1;
char isUploadingFile = NOT_UPLOADING_FILE;
/*char upload_file_name[80];*/

/*uint8_t iNIC_host_mac_addr[] = {0x00, 0xaa, 0xbb, 0x01, 0x23, 0x34};*/
/*uint8_t iNIC_client_mac_addr[] = {0x00, 0x11, 0x11, 0x11, 0x11, 0x11};*/
/*uint8_t broadcast_mac_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};*/
uint16_t checksum;
uint8_t send_buf[MAX_PKT_SIZE];
uint32_t *tclinux_7517_ptr = 0;
uint32_t tclinux_7517_len = 0;
uint32_t bin_ptr_offset = 0;
extern unsigned char mac_addr[6];
uint32_t ram_size = 0;

/* IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence) */
#define ETH_ALEN	6		        /* Octets in one ethernet addr	 */
#define ETH_HLEN	14		        /* Total octets in header.	 */
#define ETH_ZLEN	60		        /* Min. octets in frame sans FCS */
#define ETH_DATA_LEN	1500		/* Max. octets in payload	 */
#define ETH_FRAME_LEN	1514		/* Max. octets in frame sans FCS */
/*******************************************************************
 * S T R U C T U R E S
 *******************************************************************/


unsigned short rom_in_csum(unsigned short *ptr, int nbytes)
{
	register int			sum;		/* assumes long == 32 bits */
	unsigned short			oddbyte;
	register unsigned short	answer; 	/* assumes u_short == 16 bits */

	/*
	 * Our algorithm is simple, using a 32-bit accumulator (sum),
	 * we add sequential 16-bit words to it, and at the end, fold back
	 * all the carry bits from the top 16 bits into the lower 16 bits.
	 */

	sum = 0;

	while (nbytes > 1)	{
		sum += *ptr++;
		nbytes -= 2;
	}
	/* mop up an odd byte, if necessary */
	if (nbytes == 1) {
		oddbyte = 0;		/* make sure top half is zero */
		*((unsigned char *) &oddbyte) = *(unsigned char *)ptr;   /* one byte only */
		sum += oddbyte;
	}

	/*
	 * Add back carry outs from top 16 bits to low 16 bits.
	 */

	sum  = (sum >> 16) + (sum & 0xffff);	/* add high-16 to low-16 */
	sum += (sum >> 16); 		/* add carry */
	answer = ~sum;		/* ones-complement, then truncate to 16 bits */
	return(answer);
}


void rom_send_packet(uint16_t cmd_opcode, uint16_t length, uint16_t block_no, uint8_t *data,
			uint16_t file_checksum, uint16_t restart_code, uint32_t mem_addr, 
			uint32_t mem_value)
{
	int total_len, eth_len, rom_len;
	struct romhdr *romh;

	if (sizeof(*romh)+ length + ETH_HLEN> MAX_PKT_SIZE)
	{
		printf("pkt exceed MAX_PKT_SIZE\n");
		return;
	}
	rom_len = eth_len = length + sizeof(*romh);
	total_len = eth_len + NET_IP_ALIGN;

	romh = (struct romhdr *) (send_buf + ETH_HLEN);

	memset(romh, 0, sizeof(*romh));
	romh->magic_no = htonl(BOOTROM_MAGIC_NO);
	romh->cmd_opcode = htons(cmd_opcode);
	romh->cmd_id = htons(0x0);
	romh->length = htons(length);
	romh->checksum = htons(0);
	romh->block_no = htons(block_no);
	romh->file_checksum = file_checksum;
	romh->restart_code = htons(restart_code);
	romh->mem_addr = htonl(mem_addr);
	romh->mem_value = htonl(mem_value);
	memcpy((uint8_t *)romh+sizeof(*romh),data,length);

	romh->checksum = rom_in_csum((unsigned short *) romh, rom_len);

	pt_pkt_send(ETH_TYPE_INIC,(uint8_t *)romh, total_len);
}


/*_____________________________________________________________________________
**      function name: rom_rcv_notify
**      descriptions:
**            To handle NOTIFY packet send from iNIC client.
**
**      parameters:
**            skb: socket buffer.
**
**      return:
**             none.
**
**____________________________________________________________________________
*/
static void rom_rcv_notify(uint8_t* payload, struct romhdr *romh)
{
	isUploadingFile = UPLOADING_FILE;
	rom_send_packet(BOOTROM_NOTIFY_ACK, 6, 0, mac_addr, 
					0, 0, 0, 0); 
	printf("[hsm] rom_rcv_notify %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);
}

static uint16_t rom_file_csum(char *f)
{
	unsigned short *ptr = (unsigned short *)tclinux_7517_ptr;
	int nbytes;
	int			sum;		/* assumes long == 32 bits */
	unsigned short			oddbyte;
	unsigned short	answer; 	/* assumes u_short == 16 bits */

	/*
	 * Our algorithm is simple, using a 32-bit accumulator (sum),
	 * we add sequential 16-bit words to it, and at the end, fold back
	 * all the carry bits from the top 16 bits into the lower 16 bits.
	 */

	sum = 0;
	nbytes = tclinux_7517_len;

	while (nbytes > 1)	{
		sum += *ptr++;
		nbytes -= 2;
	}

	if(nbytes == 1)
	{
		oddbyte = 0;		/* make sure top half is zero */
		*((unsigned char *) &oddbyte) = *(unsigned char *)ptr;   /* one byte only */
		sum += oddbyte;
	}

	sum  = (sum >> 16) + (sum & 0xffff);	/* add high-16 to low-16 */
	sum += (sum >> 16); 		/* add carry */
	answer = ~sum;		/* ones-complement, then truncate to 16 bits */


	return(answer);
}

/*_____________________________________________________________________________
**      function name: rom_rcv_notify
**      descriptions:
**            To handle RRQ packet send from iNIC client.
**
**      parameters:
**            skb: socket buffer.
**
**      return:
**             none.
**
**____________________________________________________________________________
*/
static void rom_rcv_rrq(uint8_t* payload, struct romhdr *romh)
{
	char buffer[BOOTROM_SEGSIZE];
	char file[80];
	int byte = 0;

	if(check_tclinux_sending_state()!=true)
		return;

	printf("[hsm] rom_rcv_rrq 7517 tclinux.bin load...\n");

	checksum = rom_file_csum(file);

	memcpy(buffer, (uint8_t*)(tclinux_7517_ptr), BOOTROM_SEGSIZE);

	bin_ptr_offset += BOOTROM_SEGSIZE;
	byte += BOOTROM_SEGSIZE;

	rom_send_packet(BOOTROM_DATA, byte, 1, buffer, checksum, 0, 0, 0);
}

/*_____________________________________________________________________________
**      function name: rom_rcv_ack
**      descriptions:
**            To handle ACK packet send from iNIC client.
**
**      parameters:
**            skb: socket buffer.
**
**      return:
**             none.
**
**____________________________________________________________________________
*/

static void rom_rcv_ack(uint8_t* payload, struct romhdr *romh)
{
	int byte = 0;

	if ( check_tclinux_sending_state() != true ) {
		return;
	}

	int start = (romh->block_no) * BOOTROM_SEGSIZE;
	int end = start + BOOTROM_SEGSIZE;
	if ( tclinux_7517_len <= end ) {
		end = tclinux_7517_len;
	}
	byte = end - start;

	if((byte > 0) || ((byte == 0) && (last_len == BOOTROM_SEGSIZE)) ) {
		rom_send_packet(BOOTROM_DATA, byte, romh->block_no + 1,
			((uint8_t*)tclinux_7517_ptr + start), checksum , 0 , 0 , 0 );
	} else {
		/* finish send packet */
		end_of_sending_tclinux();
	}

	/* To record send len */
	last_len = byte;

	return;
}


/*_____________________________________________________________________________
**      function name: rom_rcv_ack
**      descriptions:
**            To handle packet send from iNIC client.
**
**      parameters:
**            work: 
**
**      return:
**             none.
**
**____________________________________________________________________________
*/
void inic_network_packet_handler(uint8_t *data, int len)
{
	struct romhdr *romh = (struct romhdr *)(data+14);//14 = source mac + destination mac + ether type 

	pt_pkt_recv(ETH_TYPE_INIC, data);

	romh->magic_no = ntohl(romh->magic_no);
	romh->cmd_opcode = ntohs(romh->cmd_opcode);
	romh->cmd_id = ntohs(romh->cmd_id);
	romh->length = ntohs(romh->length);
	romh->block_no = ntohs(romh->block_no);

	switch (romh->cmd_opcode) {
		case BOOTROM_NOTIFY:
			rom_rcv_notify(NULL, romh);
			break;

		case BOOTROM_RRQ:
			rom_rcv_rrq(NULL, romh);
			break;

		case BOOTROM_ACK:
			rom_rcv_ack(NULL, romh);
			break;
		case BOOTROM_BOOT_OK:
		case BOOTROM_DATA:
		case BOOTROM_NOTIFY_ACK:
		case BOOTROM_WRQ:
		case BOOTROM_NAK:
		case BOOTROM_START_BOOT:
		case BOOTROM_ERROR:
		case BOOTROM_RESTART:
		case BOOTROM_MEMRL:
		case BOOTROM_MEMWL:
		case BOOTROM_MIIR:
		case BOOTROM_MIIW:
			break;
		default:
			printf("unknow opcode %d\n",romh->cmd_opcode);
				break;
	}
}




void parse_mac_addr_from_env(void ) {

        char *addr = env_get("ethaddr");

        mac_addr[0] = simple_strtoul(addr + 0 , NULL, 16);
        mac_addr[1] = simple_strtoul(addr + 3 , NULL, 16);
        mac_addr[2] = simple_strtoul(addr + 6 , NULL, 16);
        mac_addr[3] = simple_strtoul(addr + 9 , NULL, 16);
        mac_addr[4] = simple_strtoul(addr + 12, NULL, 16);
        mac_addr[5] = simple_strtoul(addr + 15, NULL, 16);

        return ;
}


int mdio_msg_handler_bp2 (unsigned int phy_addr, unsigned int start_addr)
{
	uint32_t data = 0;
	uint32_t file_len = 0;
	uint32_t seg_len = 0;
	uint32_t rcv_result = 0;
	uint32_t temp_addr = 0x1fa40000;
	uint32_t conf_magic = CONF_MAGIC;
	int ret = UNHANDLE_FAIL;
	int hsm_timeout = 0;
	uint32_t *inic_ptr = NULL;
	uint32_t inic_len = 0;
	uint32_t conf_len = 10;
	uint32_t inic_len_mod = 0;
	uint32_t tempConf[4]; /* (FW:0~3 + CONF:10) bytes <= 4 words*/
	uint8_t *ptempConf = (uint8_t *)tempConf;

	// new mac address arch
	parse_mac_addr_from_env();
	mac_addr[0] = 0x02;
	mac_addr[1] = 0xE0;

	inic_ptr = get_inic_addr();
	inic_len = get_inic_len();

	printf("inic ptr = 0x%8p, len = %u\n", inic_ptr, inic_len);

	inic_len_mod = inic_len % 4;

	if(inic_len_mod)
	{
		inic_len -= inic_len_mod;
		conf_len += inic_len_mod;

		tempConf[0] = data;
		ptempConf += inic_len_mod;
	}

	memcpy(ptempConf, &conf_magic, 4);
	ptempConf += 4;
	memcpy(ptempConf, mac_addr, 6);
	/*ptempConf += 6;*/

	ptempConf = (uint8_t *)tempConf;

	while(1)
	{
		while(1)
		{
			rcv_result = mdio_msg_rcv(phy_addr, MSG_CHANNEL_TX);

			if (rcv_result == MSG_READY_INIC)
			{
				printf("[hsm] mdio_msg_handler_bp2 MSG_READY_INIC\n");
				break;
			}
			else
			{
				hsm_timeout++;
				printf("[hsm] mdio_msg_handler_bp2 not ready\n");
				if(hsm_timeout > 30)
					return TIMEOUT_FAIL;

				continue;
			}
		}

		while((file_len < inic_len) && (seg_len < SEG_SIZE))
		{
			data = *inic_ptr;
			inic_ptr++;
			mdio_msg_send(phy_addr, temp_addr, data);
			temp_addr += 4;
			file_len  += 4;
			seg_len   += 4;
		}
		while((file_len < (inic_len + conf_len)) && (seg_len < SEG_SIZE))
		{
			mdio_msg_send(phy_addr, temp_addr, *((uint32_t*)ptempConf));
			ptempConf += 4;
			temp_addr += 4;
			file_len  += 4;
			seg_len   += 4;

			if(file_len > (inic_len + conf_len))
				file_len = (inic_len + conf_len);
		}

		mdio_msg_send(phy_addr, IMAGE_ADDR, start_addr);
		mdio_msg_send(phy_addr, MSG_CHANNEL_TX, 0);
		mdio_msg_send(phy_addr, IMAGE_LEN, file_len);

		if (file_len == (inic_len + conf_len))
		{
			printf("write iNIC.bin Done! seg=0x%X file=0x%X\n", seg_len, file_len);
			mdio_msg_send(phy_addr, MSG_CHANNEL_RX, MSG_IMAGE_DONE_INIC);
			break;
		}
		else if (seg_len==SEG_SIZE)
		{
			printf("write iNIC.bin SEG! seg=0x%X file=0x%X\n", seg_len, file_len);
			mdio_msg_send(phy_addr, MSG_CHANNEL_RX, MSG_IMAGE_DONE_INIC_SEG);

			temp_addr = 0x1fa40000;
			seg_len = 0;
		}
	}

	hsm_timeout = 0;
	while(1)
	{
		mdelay(100);
		rcv_result = mdio_msg_rcv(phy_addr, MSG_CHANNEL_TX);
		if ((rcv_result == MSG_CRC_SUCCESS) ||(rcv_result == MSG_NO_CRC))
		{
			ret = PT_SUCCESS;
			printf("client boot into iNIC.bin!\n");
			break;
		}
		else if(rcv_result == MSG_CRC_FAIL)
		{
			ret = CRC_FAIL;
			printf("CRC check failed!\n");
			break;
		}
		else if(++hsm_timeout>30)
		{
			printf("host bp2 message handler timeout !\n");
			ret = RECEIVE_TIMEOUT;
			goto exit_handle;
		}
		else
			continue;
	}
	
	/* notify client that host has received result*/
	mdio_msg_send(phy_addr, MSG_CHANNEL_RX, MSG_FINISH);

	/* Disable EN7517 MDIO read path */
	mdio_msg_send(phy_addr, 0x1fa20160, 0x800);

exit_handle:
	return ret;
}

int set_ram_size(unsigned int phy_addr)
{
	int ret = 0;
	uint32_t root_offset = 0;

	struct trx_header *trx = (struct trx_header *)tclinux_7517_ptr;
   
    root_offset = trx->linux_7z_pad + trx->header_len;
    mdio_msg_send(phy_addr, TCLINUX_START, INIC_RAM_BASE-root_offset);

    ret = PT_SUCCESS;
    return ret;
}


int iNIC_server(unsigned int phy_addr)
{
	int ret = -1;
	
	printf("start iNIC server!\n");

	tclinux_7517_ptr = get_tclinux_7517_addr();
	tclinux_7517_len = get_tclinux_7517_len();

	ret = set_ram_size(phy_addr);
	if (ret != PT_SUCCESS)
	{
		printf("set_ram_size error !\n");
		return ret;
	}

	printf("iNIC_server: ptr 0x%8p, len %u\n", tclinux_7517_ptr, tclinux_7517_len);

	return ret;
}

int host_bp2(unsigned int phy_addr, unsigned int start_addr)
{
	int ret = -1;
	ret = mdio_msg_handler_bp2(phy_addr, start_addr);
	if (ret != PT_SUCCESS)
		return ret;
	ret = iNIC_server(phy_addr);
	
	return ret;
}

