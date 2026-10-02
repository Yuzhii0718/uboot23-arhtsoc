// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <linux/types.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/tc3162.h>
#include <stdio.h>
#include <string.h>
#include <asm/io.h>
#include <malloc.h>
#include <dm.h>
#include <miiphy.h>
#include <phy.h>
#include <linux/delay.h>
#include "eth.h"
#include "arht_eth.h"
#include "multiupgrade.h"
#include "net.h"

#ifdef __BIG_ENDIAN
#define FE_BYTE_SWAP
#endif
#define CONFIG_RX_2B_OFFSET 1

#include<airoha/arhtglobal.h>

#define UDP_ 0x11
#define isMT752XG	(((read_reg_word(0x1fb000f8)&0x3)==0x3) && isMT751020)

static unsigned char mac_addr[6] = {0x00, 0xaa, 0xbb, 0x01, 0x23, 0x45};

int qdmaGetIntMask(uint base, QDMA_InterruptIdx_T intIdx);
extern void flush_dcache_range(unsigned long start, unsigned long end);
extern void macPhyReset(void);
extern int multiupgrade_process(sk_buff *skb, char *mac);
extern void mtEMiiRegWrite(uint32 port_num, uint32 dev_num, uint32 reg_num, uint32 reg_data);
static QDMA_Private_T DummygpQdmaPriv;
static QDMA_Private_T DummygpQdmaPriv_wan;
QDMA_Private_T *gpQdmaPriv = &DummygpQdmaPriv;
QDMA_Private_T *gpQdmaPriv_wan = &DummygpQdmaPriv_wan;

static uint DummydscpInfoAddr[DESC_INFO_SIZE];
static uint DummydscpInfoAddr_wan[DESC_WAN_INFO_SIZE];
static macAdapter_t DummyAdapter;
static unsigned char phy8811Addrs[SERDES_INTF_MAX] = {0, 0, 0, 0, 0};
static unsigned char fw_port_to_phy8811Addrs_index_mapping[SERDES_INTF_MAX] = {1, 4, 2, 3, 0};
macAdapter_t *mac_p = &DummyAdapter;
uint8 use_ext_switch;


#ifndef CONFIG_SYS_CACHELINE_SIZE
#define CONFIG_SYS_CACHELINE_SIZE	64
#endif	/* CONFIG_SYS_CACHELINE_SIZE */

#define OP_SAVE							(1)
#define OP_RESTORE						(2)
static macMemPool_t enetMemPool __attribute__ ((__aligned__(CONFIG_SYS_CACHELINE_SIZE)));
static macMemPool_wan_t enetMemPool_wan __attribute__ ((__aligned__(CONFIG_SYS_CACHELINE_SIZE)));
static int arht_eth_initd = 0;


extern void pause(int ms);
extern void serdes_phy_init(void);
extern int eth_phy_init(macAdapter_t *mac_p);
extern void inic_network_packet_handler(uint8_t *data, int len);
extern uint32_t crc32(uint32_t, const unsigned char *, unsigned int);
static int   arht_eth_init(struct udevice* dev);
static int   arht_eth_send(struct udevice* dev, void *packet, int length);
static int   arht_eth_recv(struct udevice* dev);
static int arht_force_mdio0_en(int op);


void detect_switch(void);
int qdma_bm_dump_dscp(void);

int if_tftp_start = 0 ;
#define load_addr 81800000
ulong fw_upgrade_select = 0;
#ifdef CONFIG_CMD_INIC
#define ETH_TYPE_INIC	0xbeef
#endif

struct  ether_header {
        u_char  ether_dhost[6];
        u_char  ether_shost[6];
        u_short ethertype;
};

static int isINICImage( void *packet,int length)
{
#ifdef CONFIG_CMD_INIC
	const struct ether_header *eth = (const struct ether_header *)packet;
	uint16 ethertype = ntohs(eth->ethertype);

	switch(ethertype)
	{
		case ETH_TYPE_INIC:
		{
			return 1;
		}
	}
#endif
	return 0 ;
}

static int arht_check_pon_serdes_dsl(void)
{
	 unsigned char* serdes_intf[SERDES_INTF_MAX];
	 int len_eth = 0;
	 serdes_intf[SERDES_PON] = env_get("serdes_pon");
	 len_eth = strlen(serdes_intf[SERDES_PON]);

	if(len_eth < 3)
	{
		printf("arht_check_pon_serdes_dsl error eth_len=%d\n",len_eth);
		return 0;
	}
	if(serdes_intf[SERDES_PON][len_eth-3] == SERDES_PHY_TYPE_DSL)
	{
	 	return 1;
	}
	else
		return 0;
}

void dump_skb(sk_buff *skb)
{
	unsigned 	char tmp[80];
	unsigned char *p = skb->data;
	unsigned char *t = tmp;
	int i, n = 0;

	qdma_bm_dump_dscp();
    printf("skb data is below\n");
	if(skb == NULL){
		printf("skb is null");
		return;
	}
	if(skb->data == NULL){
		printf("skb data is null");
		return;
	}
	for (i = 0; i < skb->len; i++) {
		t += sprintf(t, "%02x ", *p++ & 0xff);
		if ((i & 0x0f) == 0x0f) {
			printf("%04x: %s\n", n, tmp);
			n += 16;
			t = tmp;
		}
	}
	if (i & 0x0f)
		printf("%04x: %s\n", n, tmp);
	printf("skb->len %d\n", skb->len);
}

__attribute__((unused))static int arht_force_mdio0_en(int op)
{
	//uint32_t gpio = 0;
	uint32_t reg = 0;
	//static uint32_t iomux_bit;
	static uint32_t REG_IOMUX_1;
	static uint32_t REG_IOMUX_2;
	static uint32_t REG_FORCE_GPIO;
	static uint32_t REG_FLASH_MODE;
	static uint32_t REG_LED_DIS;
	static uint32_t REG_GPIO_CTRL;
	static uint32_t REG_GPIO_DATA;
	static uint32_t REG_GPIO_OE;

	/* check if mdio0 force en is needed*/
	if ((read_reg_word(0x1FB0005C) == 0x1) || (read_reg_word(0x1FA2036C) == 0x0))
	{
		if (op == OP_SAVE)
		{
			/* preserve the regs status */
			REG_IOMUX_1 = read_reg_word(0x1fa20214);
			REG_IOMUX_2 = read_reg_word(0x1fa20218);
			REG_FORCE_GPIO = read_reg_word(0x1fa20228);
			REG_FLASH_MODE = read_reg_word(0x1fbf0234);
			REG_LED_DIS = read_reg_word(0x1fbf021c);
			REG_GPIO_CTRL = read_reg_word(0x1fbf0200);
			REG_GPIO_DATA = read_reg_word(0x1fbf0204);
			REG_GPIO_OE = read_reg_word(0x1fbf0214);

			if ((REG_IOMUX_1 & (0x1 << 14)) == 0)
			{
				reg = REG_GPIO_CTRL | (0x1<<4);
				write_reg_word(0x1fbf0200, reg);

				reg = REG_GPIO_DATA | (0x1<<2);
				write_reg_word(0x1fbf0204, reg);	

				reg = REG_GPIO_OE | (0x1<<2);
				write_reg_word(0x1fbf0214, reg);

				reg = REG_LED_DIS & ~((0x3)<<4);
				write_reg_word(0x1fbf021c, reg);

				reg = REG_FLASH_MODE & ~((0x1)<<2);
				write_reg_word(0x1fbf0234, reg);

				//if((REG_FORCE_GPIO & ((0x1) << 1) != 0) || (REG_IOMUX_1 & ((0x1) << 13) != 0) || (REG_IOMUX_2 & ((0x1) << 9) != 0))
				if(((REG_FORCE_GPIO & ((0x1) << 1)) != 0) || ((REG_IOMUX_1 & ((0x1) << 13)) != 0) || ((REG_IOMUX_2 & ((0x1) << 9)) != 0))
				{
					reg = REG_FORCE_GPIO | ((0x1) << 2);
				}
				else
				{
					reg = REG_FORCE_GPIO | ((0x3) << 1);
				}
				write_reg_word(0x1fa20228, reg);

				write_reg_word(0x1fa20214, (REG_IOMUX_1 | ((0x1) << 14)) & ~((0x1) << 13));
				write_reg_word(0x1fa20218, ((REG_IOMUX_2 & (~(0x1) << 8)) | ((0x1) << 9)));


				//write_reg_word(0x1fa20218, (REG_IOMUX_2 & ~((0x1) << 8) | ((0x1) << 9)));
			}

		}
		else if (op == OP_RESTORE)
		{
			/* recover the gpio/iomux status*/
			if ((REG_IOMUX_1 & (0x1 << 14)) == 0)
			{
				write_reg_word(0x1fbf0200, REG_GPIO_CTRL);
				write_reg_word(0x1fbf0204, REG_GPIO_DATA);
				write_reg_word(0x1fbf0214, REG_GPIO_OE);
				write_reg_word(0x1fbf021c, REG_LED_DIS);
				write_reg_word(0x1fbf0234, REG_FLASH_MODE);
				write_reg_word(0x1fa20228, REG_FORCE_GPIO);
				write_reg_word(0x1fa20214, REG_IOMUX_1);
				write_reg_word(0x1fa20218, REG_IOMUX_2);
				__udelay(100);
			}

		}
	}
	else
	{
		;
	}
	return 0;
}

void miiStationWrite(uint32 enetPhyAddr, uint32 phyReg, uint32 miiData)
{
	uint32 reg;
	uint32 cnt=10000;
	uint32 mdio_reg = GSW_CFG_PIAC;
	
#if CONFIG_TARGET_AN7583
	if (enetPhyAddr <9 || enetPhyAddr >12)
		mdio_reg = CR_NP_SCU_MDIO0IAC;
#endif

	do {
		reg=read_reg_word (mdio_reg);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_ST_START << MDIO_ST_SHIFT) | (MDIO_CMD_WRITE<<MDIO_CMD_SHIFT) | 
		(enetPhyAddr << MDIO_PHY_ADDR_SHIFT) | (phyReg << MDIO_REG_ADDR_SHIFT) | 
		(miiData & MDIO_RW_DATA);
	write_reg_word (mdio_reg, reg);

	cnt = 10000;
	do {
		reg=read_reg_word (mdio_reg);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));
}

uint32 miiStationRead(uint32 enetPhyAddr, uint32 phyReg)
{
	uint32 reg;
	uint32 cnt=10000;
	uint32 mdio_reg = GSW_CFG_PIAC;

#if CONFIG_TARGET_AN7583
	if (enetPhyAddr <9 || enetPhyAddr >12)
		mdio_reg = CR_NP_SCU_MDIO0IAC;
#endif

#ifndef CONFIG_TARGET_AN7583
	arht_force_mdio0_en(OP_SAVE);
#endif
	do {
		reg=read_reg_word (mdio_reg);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_ST_START << MDIO_ST_SHIFT) | (MDIO_CMD_READ<<MDIO_CMD_SHIFT) | 
		(enetPhyAddr << MDIO_PHY_ADDR_SHIFT) | (phyReg << MDIO_REG_ADDR_SHIFT);
	write_reg_word (mdio_reg, reg);

	cnt = 10000;
	do {
		reg=read_reg_word (mdio_reg);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));
	reg = reg & MDIO_RW_DATA;
	
#ifndef CONFIG_TARGET_AN7583
	arht_force_mdio0_en(OP_RESTORE);
#endif
	return reg;
}

uint16_t miiStationRead45(uint32_t phy_addr, uint32_t dev_addr, uint32_t phy_reg)
{
	uint32_t reg;
	uint32_t cnt = 10000;
	uint16_t cl45_value = 0;
	//unsigned long flags;
	uint32 mdio_reg = GSW_CFG_PIAC;

	
#if CONFIG_TARGET_AN7583
	if (phy_addr <9 || phy_addr >12)
		mdio_reg = CR_NP_SCU_MDIO0IAC;
#endif
	
#ifndef CONFIG_TARGET_AN7583
	arht_force_mdio0_en(OP_SAVE);
#endif

	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_CL45_ST_START << MDIO_ST_SHIFT) |
		(MDIO_CL45_CMD_ADDR << MDIO_CMD_SHIFT) |
		(phy_addr << MDIO_PHY_ADDR_SHIFT) |
		(dev_addr << MDIO_REG_ADDR_SHIFT) |
		(phy_reg & MDIO_RW_DATA);
	write_reg_word(mdio_reg, reg);

	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_CL45_ST_START << MDIO_ST_SHIFT) |
		(MDIO_CL45_CMD_POSTREAD_INCADDR << MDIO_CMD_SHIFT) |
		(phy_addr << MDIO_PHY_ADDR_SHIFT) |
		(dev_addr << MDIO_REG_ADDR_SHIFT);
	write_reg_word(mdio_reg, reg);

	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	cl45_value = reg;
#ifndef CONFIG_TARGET_AN7583
	arht_force_mdio0_en(OP_RESTORE);
#endif
	return cl45_value;
}

int miiStationWrite45(uint32_t phy_addr, uint32_t dev_addr, uint32_t phy_reg, uint32_t phy_data)
{
	uint32_t reg;
	uint32_t cnt = 10000;
	//unsigned long flags;
	uint32 mdio_reg = GSW_CFG_PIAC;
		
#if CONFIG_TARGET_AN7583
	if (phy_addr <9 || phy_addr >12)
		mdio_reg = CR_NP_SCU_MDIO0IAC;
#endif
		
	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_CL45_ST_START << MDIO_ST_SHIFT) |
		(MDIO_CL45_CMD_ADDR << MDIO_CMD_SHIFT) |
		(phy_addr << MDIO_PHY_ADDR_SHIFT) |
		(dev_addr << MDIO_REG_ADDR_SHIFT) |
		(phy_reg & MDIO_RW_DATA);
	write_reg_word(mdio_reg, reg);

	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_CL45_ST_START << MDIO_ST_SHIFT) |
		(MDIO_CL45_CMD_WRITE << MDIO_CMD_SHIFT) |
		(phy_addr << MDIO_PHY_ADDR_SHIFT) |
		(dev_addr << MDIO_REG_ADDR_SHIFT) |
		(phy_data & MDIO_RW_DATA);
	write_reg_word(mdio_reg, reg);

	cnt = 10000;
	do {
		reg = read_reg_word(mdio_reg);
		cnt--;
	} while((reg & PHY_ACS_ST) && (cnt != 0));

	return 0;
}

uint32 gswPbusRead(uint32 pbus_addr)
{
	uint32 pbus_data;

	uint32 phyaddr;
	uint32 reg;
	uint32 value;

	phyaddr = 31;
	reg = 31;
	value = (pbus_addr >> 6);
  	miiStationWrite(phyaddr, reg, value);

	reg = (pbus_addr>>2) & 0x000f;
	value = miiStationRead(phyaddr, reg);
	pbus_data = value;

	reg = 16;
	value = miiStationRead(phyaddr, reg);

	pbus_data = (pbus_data) | (value<<16);

	return pbus_data;
} 

int gswPbusWrite(uint32 pbus_addr, uint32 pbus_data)
{
	uint32 phyaddr;
	uint32 reg;
	uint32 value;

	phyaddr = 31;

	reg = 31;
	value = (pbus_addr >> 6);
	miiStationWrite(phyaddr, reg, value);

	reg = (pbus_addr>>2) & 0x000f;
	value = pbus_data & 0xffff;
	miiStationWrite(phyaddr, reg, value);
	
	reg = 16;
	value = (pbus_data>>16) & 0xffff;
	miiStationWrite(phyaddr, reg, value);

  	return 0;
} 

uint32 gswPmiRead(uint32 phy_addr, uint32 phy_reg)
{
	uint32 pbus_addr;
	uint32 pbus_data;
	uint32 phy_data;
	uint32 phy_acs_st;

	pbus_addr = 0x701c;

	phy_addr = phy_addr & 0x1f;
	phy_reg  = phy_reg & 0x1f;

	pbus_data = 0x80090000; // read
	pbus_data = pbus_data | (phy_addr<<20);
	pbus_data = pbus_data | (phy_reg<<25);

	gswPbusWrite(pbus_addr,pbus_data);

	phy_acs_st = 1;
	while (phy_acs_st) {
		pbus_data = gswPbusRead(pbus_addr);
		phy_acs_st = (pbus_data>>31) & 0x1;
	}

	phy_data = pbus_data & 0xffff;
	return phy_data;
} 


uint32 gswPmiWrite(uint32 phy_addr, uint32 phy_reg, uint32 phy_data)
{
	uint32 pbus_addr;
	uint32 pbus_data;

	pbus_addr = 0x701c;

	phy_addr = phy_addr & 0x1f;
	phy_reg  = phy_reg & 0x1f;
	phy_data = phy_data & 0xffff;

	pbus_data = 0x80050000; // write
	pbus_data = pbus_data | (phy_addr<<20);
	pbus_data = pbus_data | (phy_reg<<25);
	pbus_data = pbus_data | (phy_data);

	gswPbusWrite(pbus_addr,pbus_data);

	return 0;
}


void macResetSwMAC(void)
{
	uint32 reg;

	/* reset ethernet phy, ethernet switch, frame engine */
	reg = read_reg_word(CR_RSTCTRL2);
	reg |= (QDMA1_RST | ESW_RST | FE_RST);
	write_reg_word(CR_RSTCTRL2, reg);

	mdelay(1);

	/* de-assert reset ethernet phy, ethernet switch, frame engine */
	reg = read_reg_word(CR_RSTCTRL2);
	reg &= ~(QDMA1_RST | ESW_RST | FE_RST);
	write_reg_word(CR_RSTCTRL2, reg);

	/* add delay time to prevent switch reg I/O hang */
    mdelay(1);

	/* EN7580: read flash info to register, for prom init */


}

void resetSwMAC3262(void)
{
	unsigned iomux = 0, iomux_ori = 0;
	write_reg_word(GSW_PMCR(6), read_reg_word(GSW_PMCR(6)) & ~(FORCE_LNK_PN));	
	/* disable & store Ether PHY LED */
	iomux_ori = read_reg_word(IOMUX_CONTROL1);
	iomux = iomux_ori & ~(0xFF << 3);
	write_reg_word(IOMUX_CONTROL1, iomux);
	
	macResetSwMAC();
    
    if(use_ext_switch){
        gswPbusWrite(0xA4,0x08160816);
        gswPbusWrite(0x1C,0x68166816);
        gswPbusWrite(0x20,0x08160816);
        gswPbusWrite(0x24,0x08160816);
    }else{
        write_reg_word(GSW_BASE + 0xA4,0x08160816);
        write_reg_word(GSW_BASE + 0x1C,0x68166816);
        write_reg_word(GSW_BASE + 0x20,0x08160816);
        write_reg_word(GSW_BASE + 0x24,0x08160816);
    }

	macPhyReset();
	/* restore Ether PHY LED */
	write_reg_word(IOMUX_CONTROL1, iomux_ori);
}


void setEtherRateLimit (int disableRateLimit)
{
	if(!isFPGA)
	{
		if (disableRateLimit == 1)
		{
			write_reg_word(GSW_ERLCR(6),0x4E28488);//set p6 egress traffic limit (cir=40mbps,cbs=8kbyte)
		}
		else
		{
			write_reg_word(GSW_ERLCR(6),0x9C8488);//set p6 egress traffic limit (cir=5mbps,cbs=8kbyte)
		}
	}

	return;
}

void macGetMacAddr(macAdapter_t *mac_p, unsigned char *macAddr)
{
	int i;
	for (i = 0; i < 6; i++) {
		mac_p->macAddr[i] = macAddr[i];
    }
}


void macPhyRestartAN(void)
{
    uint8 i;
    uint16 value;
    for ( i = 0; i < 32; i++ ) {
        value = miiStationRead(i, PHY_CONTROL_REG);
        value |= PHY_RESTART_AN;
	    miiStationWrite(i, PHY_CONTROL_REG, value);
    }
	return;
}

int tc_mii_ext_station_fill_addr_ext(uint32 portAddr, uint32 devAddr, uint32 regAddr)
{
	uint32 pbus_addr;
	uint32 pbus_data;
	uint32 phy_acs_st;
	uint32 max_wait_cnt = 10000;

	pbus_addr = 0x701C;
	pbus_data = 0x80000000; // write
	pbus_data = pbus_data | ((portAddr & 0x1F) << 20);
	pbus_data = pbus_data | ((devAddr & 0x1F) << 25);
	pbus_data = pbus_data | (regAddr & 0xFFFF);
	gswPbusWrite(pbus_addr, pbus_data);

	phy_acs_st = 1;
	while (phy_acs_st) {
		pbus_data = gswPbusRead(pbus_addr);
		phy_acs_st = (pbus_data >> 31) & 0x1;
		if (--max_wait_cnt == 0)
		{
			break;
		}
	}

	
	return (0);
}/*end tc_mii_ext_station_fill_addr_ext*/

void tc_mii_ext_station_write_ext(uint32 portAddr, uint32 devAddr, uint32 regAddr, uint32 miiData)
{
	uint32 pbus_addr;
	uint32 pbus_data;
	uint32 phy_acs_st;
	uint32 max_wait_cnt = 10000;
	portAddr &= 0x1F;
	devAddr &= 0x1F;
	regAddr &= 0xFFFF;

	tc_mii_ext_station_fill_addr_ext(portAddr, devAddr, regAddr);

	pbus_addr = 0x701C;
	pbus_data = 0x80040000; // write
	pbus_data = pbus_data | (portAddr << 20);
	pbus_data = pbus_data | (devAddr << 25);
	pbus_data = pbus_data | (miiData & 0xFFFF);
	gswPbusWrite(pbus_addr, pbus_data);

	// 2. check phy_acs_st
	phy_acs_st = 1;
	while (phy_acs_st) {
		pbus_data = gswPbusRead(pbus_addr);
		phy_acs_st = (pbus_data >> 31) & 0x1;
		if (--max_wait_cnt == 0)
		{
			//prom_printf("\n\n\n!!! %s hang : wait busy bit timeout for %lX/%lX/%lX !!!\n\n\n", __FUNCTION__, portAddr, devAddr, regAddr);
			break;
		}
	}


}/*end tc_mii_ext_station_write_ext*/

void macCfgExtSwitch(void)
{	
	uint32 reg;
	
	reg = gswPbusRead (0x1fe0);
	reg &= (~(1<<31));
	gswPbusWrite (0x1fe0, reg);
	gswPbusWrite(0x7808, 0);
	gswPbusWrite(0x7804, 0x01017e8f);
	gswPbusWrite(0x7808, 1);
	write_reg_word(GSW_BASE+ 0x7808,0);
	write_reg_word(GSW_BASE+ 0x7804,0x01017e8f);
	write_reg_word(GSW_BASE+ 0x7808,1);
	
	mtEMiiRegWrite(0,0x1f,0x404,0x1000);
	mtEMiiRegWrite(0,0x1f,0x409,0x57);
	mtEMiiRegWrite(0,0x1f,0x40a,0x57);
	mtEMiiRegWrite(12,0x1f,0x404,0x1000);
	mtEMiiRegWrite(12,0x1f,0x409,0x57);
	mtEMiiRegWrite(12,0x1f,0x40a,0x57);

	mtEMiiRegWrite(0,0x1f,0x403,0x1800);
	pause(5); //rise delay time
	mtEMiiRegWrite(0,0x1f,0x403,0x1c00);
	mtEMiiRegWrite(0,0x1f,0x401,0xc020);
	mtEMiiRegWrite(0,0x1f,0x406,0xa030);
    mtEMiiRegWrite(0,0x1f,0x406,0xa038);
	mtEMiiRegWrite(0,0x1f,0x410,0x3);
	mtEMiiRegWrite(12,0x1f,0x403,0x1800);
	pause(5); // rise delay time
	mtEMiiRegWrite(12,0x1f,0x403,0x1c00);
	mtEMiiRegWrite(12,0x1f,0x401,0xc020);
	mtEMiiRegWrite(12,0x1f,0x406,0xa030);
    mtEMiiRegWrite(12,0x1f,0x406,0xa038);
	mtEMiiRegWrite(12,0x1f,0x410,0x3);
	pause(50);

	/* TXEN Disable and Link Down */
	reg = (IPG_CFG_64BITS<<IPG_CFG_PN_SHIFT) | MAC_MODE_PN | FORCE_MODE_PN | 
		MAC_RX_EN_PN | BKOFF_EN_PN | BACKPR_EN_PN | 
		(PN_SPEED_1000M<<FORCE_SPD_PN_SHIFT) | FORCE_DPX_PN ;

	write_reg_word(GSW_PMCR(5), reg);
	gswPbusWrite(0x3600, reg);
	pause(5); // rise delay time

	reg = gswPbusRead (0x7a40);
	gswPbusWrite(0x7a40, (reg | (1<<28)));
	pause(5); // rise delay time
	gswPbusWrite(0x7a40, (reg & (~(1<<28))));

	reg =  read_reg_word(GSW_BASE + 0x7a40);
	write_reg_word(GSW_BASE + 0x7a40, (reg | (1<<28)));
	pause(5); //rise delay time
	write_reg_word(GSW_BASE + 0x7a40, (reg & (~(1<<28))));	
	
	reg = gswPbusRead (0x7a00);
	gswPbusWrite(0x7a00, (reg | (1<<31)));

	reg =  read_reg_word(GSW_BASE + 0x7a00);
	write_reg_word(GSW_BASE + 0x7a00, (reg | (1<<31)));
	
	gswPbusWrite(0x7a54, 0xAA);
	gswPbusWrite(0x7a5c, 0xAA);
	gswPbusWrite(0x7a64, 0xAA);
	gswPbusWrite(0x7a6c, 0xAA);
	gswPbusWrite(0x7a74, 0xAA);
	gswPbusWrite(0x7a7c, 0x77);

	write_reg_word(GSW_BASE + 0x7a54, 0xAA);
	write_reg_word(GSW_BASE + 0x7a5c, 0xAA);
	write_reg_word(GSW_BASE + 0x7a64, 0xAA);
	write_reg_word(GSW_BASE + 0x7a6c, 0xAA);
	write_reg_word(GSW_BASE + 0x7a74, 0xAA);
	write_reg_word(GSW_BASE + 0x7a7c, 0x77);

 	gswPbusWrite(0x7830, 1);
  	write_reg_word(GSW_BASE+ 0x7830,0x1);
	pause(5); //rise delay time
	
	reg = gswPbusRead (0x7a00);
	gswPbusWrite(0x7a00, (reg & (~(1<<31))));
	reg =  read_reg_word(GSW_BASE + 0x7a00);
	write_reg_word(GSW_BASE + 0x7a00, (reg & (~(1<<31))));

	/* TXEB Disable and Link up */
	reg = (IPG_CFG_64BITS<<IPG_CFG_PN_SHIFT) | MAC_MODE_PN | FORCE_MODE_PN | 
		 MAC_RX_EN_PN | BKOFF_EN_PN | BACKPR_EN_PN | 
		(PN_SPEED_1000M<<FORCE_SPD_PN_SHIFT) | FORCE_DPX_PN | FORCE_LNK_PN;

	write_reg_word(GSW_PMCR(5), reg);
	gswPbusWrite(0x3600, reg);
        
	write_reg_word(GSW_PSC(5), 0xfff10);
	write_reg_word(GSW_PSC(6), 0xfff10);

	/* set port 5 as 1000Mbps, FC on */
	reg = (IPG_CFG_64BITS<<IPG_CFG_PN_SHIFT) | MAC_MODE_PN | FORCE_MODE_PN | 
		MAC_TX_EN_PN | MAC_RX_EN_PN | BKOFF_EN_PN | BACKPR_EN_PN | 
		(PN_SPEED_1000M<<FORCE_SPD_PN_SHIFT) | FORCE_DPX_PN | FORCE_LNK_PN;
	
	write_reg_word(GSW_PMCR(5), reg);
	gswPbusWrite(0x3600, reg);

	return;
}

void macSetGSW(macAdapter_t *mac_p)
{
	uint32 reg;
    int phy_add_start=0,phy_add_end=0,phy_add;
	/* set port 6 as 1Gbps, FC on */
	reg = (IPG_CFG_SHORT<<IPG_CFG_PN_SHIFT) | MAC_MODE_PN | FORCE_MODE_PN | 
		MAC_TX_EN_PN | MAC_RX_EN_PN | BKOFF_EN_PN | BACKPR_EN_PN | 
		ENABLE_RX_FC_PN | ENABLE_TX_FC_PN | (PN_SPEED_1000M<<FORCE_SPD_PN_SHIFT) | 
		FORCE_DPX_PN | FORCE_LNK_PN;
	write_reg_word(GSW_PMCR(6), reg);

	/* set cpu port as port 6 */
	reg = (0x40<<MFC_BC_FFP_SHIFT) | (0x40<<MFC_UNM_FFP_SHIFT) | (0x40<<MFC_UNU_FFP_SHIFT) |
			MFC_CPU_EN	| (6<<MFC_CPU_PORT_SHIFT);
    if(use_ext_switch)
        gswPbusWrite(0x10, reg);
    else
        write_reg_word(GSW_MFC, reg);

	/* check if FPGA */
	if (isFPGA) {
		/*decrease mdc/mdio clock*/
		reg = read_reg_word(GSW_CFG_PPSC);
		reg &= ~((1<<6) | (1<<7));
		write_reg_word(GSW_CFG_PPSC, reg);

		/* auto polling enable, 2 PHY ports, start PHY addr=1 and end PHY addr=2 */
		reg = read_reg_word(GSW_CFG_PPSC);
        reg &= ~(0x7F << 24);	/* for FPGA external PHY, always use auto polling*/
        reg &= ~(PHY_END_ADDR | PHY_ST_ADDR);
        if ((miiStationRead(1, 2) == 0x4d) && (miiStationRead(1, 3) == 0xd072)){
            if(read_reg_word(GSW_PMSR(0)) == 0xdeadbeef){
                if(isEN7580){
                    reg |= 0x1f << 24;
                    phy_add_start = 0;
                    phy_add_end = 4;
                }else{               
                    reg |= 0x7 << 24;	/* for FPGA external PHY, always use auto polling*/
                    phy_add_start =0;
                    phy_add_end =2;
                }
            }else{
                if(isEN7580){
                    reg |= 0xf << 24;
                    phy_add_start = 1;
                    phy_add_end = 4;        
                }else{
                    reg |= 0x3 << 24;	/* for FPGA external PHY, always use auto polling*/
                    phy_add_start =1;
                    phy_add_end =2;
                }
            }
        }else if ((miiStationRead(2, 2) == 0x4d) && (miiStationRead(2, 3) == 0xd072)){
            if (read_reg_word(GSW_PMSR(0)) == 0xdeadbeef){
                reg |= 0x7 << 24;	/* for FPGA external PHY, always use auto polling*/
                phy_add_start =1;
                phy_add_end =3;
            }else{
                reg |= 0x3 << 24;	/* for FPGA external PHY, always use auto polling*/
                phy_add_start =2;
                phy_add_end =3;
            }
        }else if((miiStationRead(4, 2) == 0xf) && (miiStationRead(4, 3) == 0xc6c2)){
            /* auto polling enable, 2 PHY ports, start PHY addr=2 and end PHY addr=7 */
			reg = read_reg_word(GSW_CFG_PPSC);
			reg |= ((1<<24) | (1<<25)| (1<<26)| (1<<27)| (1<<28)| (1<<29));
			reg &= ~(PHY_END_ADDR | PHY_ST_ADDR);
            phy_add_start =2;
            phy_add_end =7;
		} 
		reg |= (phy_add_end << PHY_END_ADDR_SHIFT) | (phy_add_start << PHY_ST_ADDR_SHIFT);
		write_reg_word(GSW_CFG_PPSC, reg);
        for(phy_add = phy_add_start;phy_add<=phy_add_end;phy_add++){
            reg = miiStationRead(phy_add, 0x9);
            reg &= ~((1<<9) | 1<<8);
            miiStationWrite(phy_add, 0x9, reg);
        }
        macPhyRestartAN();
	}
	else {
		/* Sideband signal error for Port 3, which need the auto polling */
		write_reg_word(GSW_BASE+0x7018, 0x7f7f8c08);
	}
}

void led_gpio_enable(void)
{
    uint32 value=0;
    value = read_reg_word(IOMUX_CONTROL1);
    value |= LAN_LED;
    write_reg_word(IOMUX_CONTROL1,value);
}


void macSetMACCR(macAdapter_t *map_p)
{
	uint32 reg;

	reg = (12<<GDM_JMB_LEN_SHIFT)  |
		(GDM_P_CPU<<GDM_UFRC_P_SHIFT) | (GDM_P_CPU<<GDM_BFRC_P_SHIFT) | 
		(GDM_P_CPU<<GDM_MFRC_P_SHIFT) | (GDM_P_CPU<<GDM_OFRC_P_SHIFT);
	write_reg_word(GDMA1_FWD_CFG, reg);
	reg = (13<<GDM_JMB_LEN_SHIFT)  |
		(5<<GDM_UFRC_P_SHIFT) | (5<<GDM_BFRC_P_SHIFT) | 
		(5<<GDM_MFRC_P_SHIFT) | (5<<GDM_OFRC_P_SHIFT);
	write_reg_word(GDMA2_FWD_CFG, reg);
    reg = GDM_PAD_EN  | GDM_STRPCRC |
		(GDM_P_CPU<<GDM_UFRC_P_SHIFT) | (GDM_P_CPU<<GDM_BFRC_P_SHIFT) | 
		(GDM_P_CPU<<GDM_MFRC_P_SHIFT) | (GDM_P_CPU<<GDM_OFRC_P_SHIFT);
	write_reg_word(GDMA3_FWD_CFG, reg);

    reg = GDM_PAD_EN  | GDM_STRPCRC |
		(GDM_P_CPU<<GDM_UFRC_P_SHIFT) | (GDM_P_CPU<<GDM_BFRC_P_SHIFT) | 
		(GDM_P_CPU<<GDM_MFRC_P_SHIFT) | (GDM_P_CPU<<GDM_OFRC_P_SHIFT);
	write_reg_word(GDMA4_FWD_CFG, reg);

	/* check if FPGA */
	if (isFPGA) {
		/* set 1us clock for FPGA */
		reg = read_reg_word(CR_CLK_CFG);
		reg &= ~(0x3f000000);

		reg |= (0x31<<24);

		write_reg_word(CR_CLK_CFG, reg);
	}
}

void macSetMacReg(macAdapter_t *mac_p)
{
	write_reg_word(GDMA1_MAC_ADRL, mac_p->macAddr[2]<<24 | mac_p->macAddr[3]<<16 | \
                               mac_p->macAddr[4]<<8  | mac_p->macAddr[5]<<0);
	write_reg_word(GDMA1_MAC_ADRH, mac_p->macAddr[0]<<8  | mac_p->macAddr[1]<<0);

	/* fill in switch's MAC address */
	write_reg_word(GSW_SMACCR0, mac_p->macAddr[2]<<24 | mac_p->macAddr[3]<<16 | \
                               mac_p->macAddr[4]<<8  | mac_p->macAddr[5]<<0);
	write_reg_word(GSW_SMACCR1, mac_p->macAddr[0]<<8  | mac_p->macAddr[1]<<0);
}

int macDrvRegInit(macAdapter_t *mac_p)
{
    macSetMACCR(mac_p);
    macSetMacReg(mac_p);      

   	macSetGSW(mac_p);

    return 0;
}



/******************************************************************************
******************************************************************************/
static int qdma_bm_push_tx_dscp(struct QDMA_DscpInfo_S *diPtr, int ringIdxM,uint8 mode) 
{
	if(diPtr->next != NULL) {
		QDMA_ERR("The TX DSCP is not return from tx used pool\n") ;
		return -1 ;
	}

	diPtr->skb = NULL ;
	if(mode == QDMA_LAN)
	{
		if(!gpQdmaPriv->txHeadPtr) {
			gpQdmaPriv->txHeadPtr = diPtr ;
			gpQdmaPriv->txTailPtr = diPtr ;
		} else {
			gpQdmaPriv->txTailPtr->next = diPtr ;
			gpQdmaPriv->txTailPtr = gpQdmaPriv->txTailPtr->next ;
		}
	}else{
		if(!gpQdmaPriv_wan->txHeadPtr) {
			gpQdmaPriv_wan->txHeadPtr = diPtr ;
			gpQdmaPriv_wan->txTailPtr = diPtr ;
		} else {
			gpQdmaPriv_wan->txTailPtr->next = diPtr ;
			gpQdmaPriv_wan->txTailPtr = gpQdmaPriv_wan->txTailPtr->next ;
		}
	}
	return 0 ;
}


/******************************************************************************
******************************************************************************/
static struct QDMA_DscpInfo_S *qdma_bm_pop_tx_dscp(uint8 mode)
{
	struct QDMA_DscpInfo_S *diPtr ;
	
	if(mode == QDMA_LAN){
		diPtr = gpQdmaPriv->txHeadPtr ;
		if(gpQdmaPriv->txHeadPtr == gpQdmaPriv->txTailPtr) {
			gpQdmaPriv->txHeadPtr = NULL ;
			gpQdmaPriv->txTailPtr = NULL ;
		} else {
			gpQdmaPriv->txHeadPtr = gpQdmaPriv->txHeadPtr->next ;
		}
		
		if(diPtr) {
		diPtr->next = NULL ;
		}
	}	
	else{
		diPtr = gpQdmaPriv_wan->txHeadPtr ;
		if(gpQdmaPriv_wan->txHeadPtr == gpQdmaPriv_wan->txTailPtr) {
			gpQdmaPriv_wan->txHeadPtr = NULL ;
			gpQdmaPriv_wan->txTailPtr = NULL ;
		} else {
			gpQdmaPriv_wan->txHeadPtr = gpQdmaPriv_wan->txHeadPtr->next ;
		}
		if(diPtr) {
		diPtr->next = NULL ;
		}
	}

	
	return diPtr ;
}

/******************************************************************************
 Packet Receive
******************************************************************************/
/******************************************************************************
******************************************************************************/
static void qdma_bm_add_rx_dscp(struct QDMA_DscpInfo_S *diPtr, uint8 mode) 
{
	if(mode == QDMA_LAN)
	{
		if(!gpQdmaPriv->rxStartPtr) {
			gpQdmaPriv->rxStartPtr = diPtr ;
			diPtr->next = gpQdmaPriv->rxStartPtr ;
		} else {
			diPtr->next = gpQdmaPriv->rxStartPtr->next;
			gpQdmaPriv->rxStartPtr->next = diPtr;
			gpQdmaPriv->rxStartPtr = diPtr;
		}
	}else{
		if(!gpQdmaPriv_wan->rxStartPtr) {
			gpQdmaPriv_wan->rxStartPtr = diPtr ;
			diPtr->next = gpQdmaPriv_wan->rxStartPtr ;
		} else {
			diPtr->next = gpQdmaPriv_wan->rxStartPtr->next;
			gpQdmaPriv_wan->rxStartPtr->next = diPtr;
			gpQdmaPriv_wan->rxStartPtr = diPtr;
		}
	}
}


static struct QDMA_DscpInfo_S *qdma_bm_get_unused_rx_dscp(uint8 mode)
{
	struct QDMA_DscpInfo_S *diPtr = NULL ;

	if(mode == QDMA_LAN){
		if(gpQdmaPriv->rxStartPtr) {
			if(!gpQdmaPriv->rxEndPtr) {
				diPtr = gpQdmaPriv->rxStartPtr ;
				gpQdmaPriv->rxEndPtr = diPtr ;
			} else if(gpQdmaPriv->rxEndPtr->next != gpQdmaPriv->rxStartPtr) {
				diPtr = gpQdmaPriv->rxEndPtr->next ;
				gpQdmaPriv->rxEndPtr = diPtr ; 
			}
		} 
	}else{
		if(gpQdmaPriv_wan->rxStartPtr) {
			if(!gpQdmaPriv_wan->rxEndPtr) {
				diPtr = gpQdmaPriv_wan->rxStartPtr ;
				gpQdmaPriv_wan->rxEndPtr = diPtr ;
			} else if(gpQdmaPriv_wan->rxEndPtr->next != gpQdmaPriv_wan->rxStartPtr) {
				diPtr = gpQdmaPriv_wan->rxEndPtr->next ;
				gpQdmaPriv_wan->rxEndPtr = diPtr ; 
			}
		} 
	}

	return diPtr ;
}


int qdma_bm_transmit_done(int amount, int qdma_wan_use) 
{
	QDMA_DMA_DSCP_T txDscp ;
	int ret = 0 ;
	struct QDMA_DscpInfo_S *diPtr ;
	uint base = CONFIG_QDMA_BASE_ADDR;
	uint entryLen, headIdx, irqValue=0, irqDepth=CONFIG_IRQ_DEPTH ;
	uint *irqPtr ;
	int i=0, j=0, idx=0, ringIdx=0 ;
	uint RETRY=3 ;
	uint irqStatus ;
	void *msgPtr;

	if(qdma_wan_use)
		base = CONFIG_QDMA2_BASE_ADDR;
	else
		base = CONFIG_QDMA_BASE_ADDR;
	irqStatus = qdmaGetIrqStatus(base) ;
	headIdx = (irqStatus & IRQ_STATUS_HEAD_IDX_MASK) >> IRQ_STATUS_HEAD_IDX_SHIFT ;
	entryLen = (irqStatus & IRQ_STATUS_ENTRY_LEN_MASK) >> IRQ_STATUS_ENTRY_LEN_SHIFT ;
	if(entryLen == 0) {
		QDMA_MSG(DBG_WARN, "qdma_bm_transmit_done-111111\n") ;
		goto out2 ;
	}

	entryLen = (amount && amount<entryLen) ? amount : entryLen ;
	
	if(qdma_wan_use == QDMA_LAN){
		for(i=0 ; i<entryLen ; i++) {
			irqPtr = (uint *)((ulong)gpQdmaPriv->irqQueueAddr) + ((headIdx+i)%irqDepth) ;
			
			RETRY = 3 ;
			while(RETRY--) {
				irqValue = *irqPtr ;
				if(irqValue == CONFIG_IRQ_DEF_VALUE) {
					QDMA_ERR("There is no data available in IRQ queue. irq value:%.8x, irq ptr:%.8lx TIMEs:%d\n", (uint)irqValue, (long)irqPtr, RETRY) ;
					if(RETRY <= 0) {
						gpQdmaPriv->counters.IrqQueueAsynchronous++ ;
						ret = -1 ;
						goto out1 ;
					}
				} else {
					*irqPtr = CONFIG_IRQ_DEF_VALUE ;
					break ;
				}
			}
			
			idx = (irqValue & IRQ_CFG_IDX_MASK) ;
			ringIdx = (irqValue & IRQ_CFG_RINGIDX_MASK) >> IRQ_CFG_RINGIDX_SHIFT;
			if(idx<0 || idx>=gpQdmaPriv->txDscpNum) {
				QDMA_ERR("The TX DSCP index %d is invalid, total gpQdmaPriv->txDscpNum=%d.\n", idx, gpQdmaPriv->txDscpNum) ;
				gpQdmaPriv->counters.txIrqQueueIdxErrs++ ;
				ret = -1;
				continue ;
			}
				
			diPtr = (struct QDMA_DscpInfo_S *)((long)gpQdmaPriv->txBaseAddr) + idx;
			if(diPtr->dscpIdx!=idx || diPtr->next!=NULL) {
				QDMA_ERR("The content of the TX DSCP_INFO(%.8lx) is incorrect. ENTRY_LEN:%d, HEAD_IDX:%d, IRQ_VALUE:%.8x.\n", (long)diPtr, entryLen, headIdx, irqValue) ;
				//QDMA_ERR("The content of the TX DSCP_INFO(%p) is incorrect. ENTRY_LEN:%d, HEAD_IDX:%d, IRQ_VALUE:%.8x.\n", (void*)diPtr, entryLen, headIdx, irqValue) ;

				gpQdmaPriv->counters.txDscpIncorrect++ ;
				ret = -1;
				continue ;
			}
			
			msgPtr = (void *)txDscp.msg ;
			if(msgPtr)
				memset(msgPtr, 0, QDMA_TX_DSCP_MSG_LENS);
			free_skb(diPtr->skb);
			
			qdma_bm_push_tx_dscp(diPtr, 0,QDMA_LAN) ;

		}
	
	}else{ //QDMA_WAN Recycle
		for(i=0 ; i<entryLen ; i++) {
			irqPtr = (uint *)((ulong)gpQdmaPriv_wan->irqQueueAddr) + ((headIdx+i)%irqDepth) ;
			
			RETRY = 3 ;
			while(RETRY--) {
				irqValue = *irqPtr ;
				if(irqValue == CONFIG_IRQ_DEF_VALUE) {
					QDMA_ERR("There is no data available in IRQ queue. irq value:%.8x, irq ptr:%.8lx TIMEs:%d\n", (uint)irqValue, (long)irqPtr, RETRY) ;
					if(RETRY <= 0) {
						gpQdmaPriv_wan->counters.IrqQueueAsynchronous++ ;
						ret = -1 ;
						goto out1 ;
					}
				} else {
					*irqPtr = CONFIG_IRQ_DEF_VALUE ;
					break ;
				}
			}
			
			idx = (irqValue & IRQ_CFG_IDX_MASK) ;
			ringIdx = (irqValue & IRQ_CFG_RINGIDX_MASK) >> IRQ_CFG_RINGIDX_SHIFT;
			if(idx<0 || idx>=gpQdmaPriv_wan->txDscpNum) {
				QDMA_ERR("The TX DSCP index %d is invalid, total gpQdmaPriv_wan->txDscpNum=%d.\n", idx, gpQdmaPriv_wan->txDscpNum) ;
				gpQdmaPriv_wan->counters.txIrqQueueIdxErrs++ ;
				ret = -1;
				continue ;
			}
				
			diPtr = (struct QDMA_DscpInfo_S *)((long)gpQdmaPriv_wan->txBaseAddr) + idx;
			if(diPtr->dscpIdx!=idx || diPtr->next!=NULL) {
				QDMA_ERR("The content of the TX DSCP_INFO(%.8lx) is incorrect. ENTRY_LEN:%d, HEAD_IDX:%d, IRQ_VALUE:%.8x.\n", (long)diPtr, entryLen, headIdx, irqValue) ;
				//QDMA_ERR("The content of the TX DSCP_INFO(%p) is incorrect. ENTRY_LEN:%d, HEAD_IDX:%d, IRQ_VALUE:%.8x.\n", (void*)diPtr, entryLen, headIdx, irqValue) ;

				gpQdmaPriv_wan->counters.txDscpIncorrect++ ;
				ret = -1;
				continue ;
			}
			
			msgPtr = (void *)txDscp.msg ;
			if(msgPtr)
				memset(msgPtr, 0, QDMA_TX_DSCP_MSG_LENS);
			free_skb(diPtr->skb);
			
			qdma_bm_push_tx_dscp(diPtr, 0,QDMA_WAN) ;

		}
		
	}
	QDMA_MSG(DBG_WARN, "qdma_bm_transmit_done-000000\n") ;

out1:
	for(j=0 ; j<(i>>7) ; j++) {
		qdmaSetIrqClearLen(base, 0x80) ;
	}
	qdmaSetIrqClearLen(base, (i&0x7F)) ;

out2:
	return ret ;
}

int qdma_bm_transmit_packet(sk_buff *skb, int ringIdx, uint msg0, uint msg1)
{
	struct QDMA_DscpInfo_S *pNewDscpInfo ;
	QDMA_DMA_DSCP_T *pTxDscp ;
	uint base = CONFIG_QDMA_BASE_ADDR;
	int ret = 0 , qdma_wan = 0;
	unsigned long   addr, len;

	
	

	if(skb==NULL){
		printf("The input arguments are wrong, skb is NULL\n"); 
        return -1;
	}

	if(skb->len<=0 || skb->len>CONFIG_MAX_PKT_LENS) 
	{
		printf("The input arguments are wrong, skb:%p, skbLen:%d.\n", (void*)skb, skb->len) ; 
        return -1;
	}

	/* recycle TX DSCP when send packets in tx polling mode */
	
	if(qdmaGetIrqEntryLen(base) >= QDMA_TX_THRESHOLD) { 
		qdma_bm_transmit_done(0,QDMA_LAN) ;
	}

	/* Get unused TX DSCP from TX unused DSCP link list */	
	pNewDscpInfo = qdma_bm_pop_tx_dscp(QDMA_LAN) ;
	if(pNewDscpInfo == NULL) {
		gpQdmaPriv->counters.noTxDscps++ ;
		printf("pNewDscpInfo is NULL\n") ; 
		return -1 ;
	}
	pTxDscp = gpQdmaPriv->txUsingPtr->dscpPtr ;
	pTxDscp->msg[0] = msg0;
	pTxDscp->msg[1] = msg1;
	pTxDscp->next_idx = pNewDscpInfo->dscpIdx ;
	pTxDscp->pkt_addr = (uintptr_t)K1_TO_PHY(skb->data) ;
	pTxDscp->ctrl.pkt_len = skb->len /* pktLen */; 

	pTxDscp->ctrl.nls = 0 ;

	pTxDscp->ctrl.drop_pkt = 0 ;

	pTxDscp->ctrl.done = 0 ;

	gpQdmaPriv->txUsingPtr->skb = skb ;
	gpQdmaPriv->txUsingPtr = pNewDscpInfo ;


	addr = (unsigned long)(pTxDscp->pkt_addr) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = ((unsigned long)(pTxDscp->ctrl.pkt_len) + 2 * CONFIG_SYS_CACHELINE_SIZE ) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len);
  
	addr = (unsigned long )&mac_p->macMemPool_p->descrPool[0] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE  + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );
  
	addr = (unsigned long )&mac_p->macMemPool_p->descrPool[DESC_TOTAL_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = ((CONFIG_IRQ_DEPTH<<2)+ 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );

	qdmaSetTxCpuIdx(base, ringIdx, pNewDscpInfo->dscpIdx) ;

	gpQdmaPriv->counters.txCounts++ ;
    return ret ;
}
int qdma_wan_bm_transmit_packet(sk_buff *skb, int ringIdx, uint msg0, uint msg1)
{
	struct QDMA_DscpInfo_S *pNewDscpInfo ;
	QDMA_DMA_DSCP_T *pTxDscp ;
	uint base = CONFIG_QDMA2_BASE_ADDR;
	int ret = 0;
	unsigned long   addr, len;


	if(skb==NULL){
		printf("The input arguments are wrong, skb is NULL\n"); 
        return -1;
	}

	if(skb->len<=0 || skb->len>CONFIG_MAX_PKT_LENS) 
	{
		printf("The input arguments are wrong, skb:%p, skbLen:%d.\n", (void*)skb, skb->len) ; 
        return -1;
	}

	/* recycle TX DSCP when send packets in tx polling mode */
	
	if(qdmaGetIrqEntryLen(base) >= QDMA_TX_THRESHOLD) { 
		qdma_bm_transmit_done(0,QDMA_WAN) ;
	}

	/* Get unused TX DSCP from TX unused DSCP link list */	
	pNewDscpInfo = qdma_bm_pop_tx_dscp(QDMA_WAN) ;
	if(pNewDscpInfo == NULL) {
		gpQdmaPriv_wan->counters.noTxDscps++ ;
		printf("pNewDscpInfo is NULL\n") ; 
		return -1 ;
	}
	pTxDscp = gpQdmaPriv_wan->txUsingPtr->dscpPtr ;
	pTxDscp->msg[0] = msg0;
	pTxDscp->msg[1] = msg1;
	pTxDscp->next_idx = pNewDscpInfo->dscpIdx ;
	pTxDscp->pkt_addr = (uintptr_t)K1_TO_PHY(skb->data) ;
	pTxDscp->ctrl.pkt_len = skb->len /* pktLen */; 

	pTxDscp->ctrl.nls = 0 ;

	pTxDscp->ctrl.drop_pkt = 0 ;

	pTxDscp->ctrl.done = 0 ;

	gpQdmaPriv_wan->txUsingPtr->skb = skb ;
	gpQdmaPriv_wan->txUsingPtr = pNewDscpInfo ;


	addr = (unsigned long)(pTxDscp->pkt_addr) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = ((unsigned long)(pTxDscp->ctrl.pkt_len) + 2 * CONFIG_SYS_CACHELINE_SIZE ) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len);
  
	addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[0] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (CONFIG_WAN_TX0_DSCP_NUM*CONFIG_WAN_TX0_DSCP_SIZE  + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );
  
	addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[DESC_WAN_TOTAL_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = ((CONFIG_IRQ_DEPTH<<2)+ 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );

	qdmaSetTxCpuIdx(base, ringIdx, pNewDscpInfo->dscpIdx) ;

	gpQdmaPriv_wan->counters.txCounts++ ;
    return ret ;
}


int tc3162_eth_send(sk_buff *skb)
{

	uint32 length = skb->len;
	ethTxMsg_t ethTxMsg;
	int error;

	if (skb->data == NULL) {
		printf("Tx a empty mbuf\n"); 	
		return 1;
	}

	if (length < 60) {
		length = 60;
	}

  /* GDMA1 */
    memset(&ethTxMsg, 0, sizeof(ethTxMsg_t));
    ethTxMsg.raw.fport = GDM_P_GDMA1;
    ethTxMsg.raw.channel = 0;
    ethTxMsg.raw.queue =0;

    ethTxMsg.raw.nboq = 0;
    ethTxMsg.raw.mtr_g = 0x7f; /*not use any meter ratelimit*/


	error = qdma_bm_transmit_packet(skb, 0,ethTxMsg.msg[0],ethTxMsg.msg[1]) ;
    
    if(error){
		free_skb(skb);
		return 0;
     }


	return 0;
}


int qdmaEnableInt(uint base, uint bit, QDMA_InterruptIdx_T intIdx)
{
	uint t=0 ;

	if( (intIdx < QDMA_INT1_ENABLE1) || (intIdx > QDMA_INT4_ENABLE2) )
	{
		printf("qdmaEnableInt: INT Index Error.\n");
		return -EINVAL;
	}

	if( (intIdx%2) == 1 )
	{
		t = IO_GREG(QDMA_CSR_INT_ENABLE1(base,((intIdx+1)>>1))) ;
		IO_SREG(QDMA_CSR_INT_ENABLE1(base,((intIdx+1)>>1)), (t|bit));
	}
	else
	{
		t = IO_GREG(QDMA_CSR_INT_ENABLE2(base,(intIdx>>1))) ;
		IO_SREG(QDMA_CSR_INT_ENABLE2(base,(intIdx>>1)), (t|bit));
	}

	return 0 ;
}

/******************************************************************************
******************************************************************************/
int qdmaDisableInt(uint base, uint bit, QDMA_InterruptIdx_T intIdx)
{
	uint t=0 ;

	if( (intIdx < QDMA_INT1_ENABLE1) || (intIdx > QDMA_INT4_ENABLE2) )
	{
		printf("qdmaDisableInt: INT Index Error.\n");
		return -EINVAL;
	}

	if( (intIdx%2) == 1 )
	{
		t = IO_GREG(QDMA_CSR_INT_ENABLE1(base,((intIdx+1)>>1))) ;
		IO_SREG(QDMA_CSR_INT_ENABLE1(base,((intIdx+1)>>1)), (t&(~bit)));
	}
	else
	{
		t = IO_GREG(QDMA_CSR_INT_ENABLE2(base,(intIdx>>1))) ;
		IO_SREG(QDMA_CSR_INT_ENABLE2(base,(intIdx>>1)), (t&(~bit)));
	}

	return 0 ;
}

/******************************************************************************
******************************************************************************/
int qdmaSetIntMask(uint base, uint value, QDMA_InterruptIdx_T intIdx)
{

	if( (intIdx < QDMA_INT1_ENABLE1) || (intIdx > QDMA_INT4_ENABLE2) )
	{
		printf("qdmaSetIntMask: INT Index Error\n");
		return -EINVAL;
	}

	if( (intIdx%2) == 1 )
	{
		IO_SREG(QDMA_CSR_INT_ENABLE1(base,((intIdx+1)>>1)), value) ;
	}
	else
	{
		IO_SREG(QDMA_CSR_INT_ENABLE2(base,(intIdx>>1)), value) ;
	}

	return 0 ;
}

/******************************************************************************
******************************************************************************/
int qdmaGetIntMask(uint base, QDMA_InterruptIdx_T intIdx)
{
	ulong value=0 ;

	if( (intIdx < QDMA_INT1_ENABLE1) || (intIdx > QDMA_INT4_ENABLE2) )
	{
		printf("qdmaGetIntMask: INT Index Error\n");
		return -EINVAL;
	}

	if( (intIdx%2) == 1 )
	{
		value = IO_GREG(QDMA_CSR_INT_ENABLE1(base, ((intIdx+1)>>1))) ;
	}
	else
	{
		value = IO_GREG(QDMA_CSR_INT_ENABLE2(base, (intIdx>>1))) ;
	}

	return value ;
}

int qdma_bm_dump_dscp( void ){

	if(arht_eth_initd == 0)
		return 0;
	int idx = 0; 
	struct QDMA_DscpInfo_S *diPtr=NULL ;
	idx = 1 ;
	diPtr = gpQdmaPriv->rxStartPtr ;
	printf("\nRx%d DSCP Ring: RxStartIdx:%d, RxEndIdx:%d\n", 0, gpQdmaPriv->rxStartPtr->dscpIdx, gpQdmaPriv->rxEndPtr->dscpIdx) ;
	do {
		if(diPtr) {
			diPtr = diPtr->next ;
			idx++ ;
		}
	} while(diPtr!=NULL && diPtr!=gpQdmaPriv->rxStartPtr) ;

	return 0;
}


/******************************************************************************
 Packet Transmit
******************************************************************************/


int qdma_bm_hook_receive_buffer(sk_buff *skb, int ringIdx)
{
	struct QDMA_DscpInfo_S *pNewDscpInfo ;
	QDMA_DMA_DSCP_T *pRxDscp ;
	dma_addr_t dmaPktAddr ;
	uint base = CONFIG_QDMA_BASE_ADDR;
	int ret = 0 ;
	unsigned long	addr, len;

	
	pNewDscpInfo = qdma_bm_get_unused_rx_dscp(QDMA_LAN) ;
	if(pNewDscpInfo == NULL) {
		QDMA_ERR("There is not any free RX DSCP.\n") ; 
		gpQdmaPriv->counters.noRxDscps++ ;
		return -1 ;
	}
	
#ifdef CONFIG_RX_2B_OFFSET
	QDMA_MSG(DBG_MSG, "Adjust the skb->tail location for net IP alignment\n") ;
	if(((uintptr_t)skb->data & 7) != 0) {
		//prom_printf("address not align 8\n");
	}
	skb_reserve(skb, 2) ;
	dmaPktAddr = (uintptr_t)K1_TO_PHY((skb->data - 2));
#else
	dmaPktAddr = (uintptr_t)K1_TO_PHY((skb->data));
#endif /* CONFIG_RX_2B_OFFSET */
	
	
	pRxDscp = gpQdmaPriv->rxUsingPtr->dscpPtr ;
	pRxDscp->msg[0] = 0;
	pRxDscp->msg[1] = 0;
	pRxDscp->msg[2] = 0;
	pRxDscp->msg[3] = 0;
	pRxDscp->pkt_addr = dmaPktAddr ;
	pRxDscp->next_idx = pNewDscpInfo->dscpIdx ;
	pRxDscp->ctrl.pkt_len = 1518 ;
	pRxDscp->ctrl.done = 0 ;
	

	QDMA_MSG(DBG_MSG, "Hook RX DSCP to RXDMA. RX_CPU_IDX:%.8x, RX_NULL_IDX:%.8x\n", gpQdmaPriv->rxUsingPtr->dscpIdx, pNewDscpInfo->dscpIdx) ;
	QDMA_MSG(DBG_MSG, "RXDSCP(%x): DONE:%d, PKT:%.8x, PKTLEN:%d, NEXT_IDX:%d\n", 
													pRxDscp,
													(uint)pRxDscp->ctrl.done, 
													(uint)pRxDscp->pkt_addr,
													(uint)pRxDscp->ctrl.pkt_len,
													(uint)pRxDscp->next_idx) ;
													
	gpQdmaPriv->rxUsingPtr->skb = skb ;
	gpQdmaPriv->rxUsingPtr = pNewDscpInfo ;
	gpQdmaPriv->counters.rxCounts++ ; 


	addr = (unsigned long )&mac_p->macMemPool_p->descrPool[CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (CONFIG_RX0_DSCP_NUM*CONFIG_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );

	addr = (unsigned long )pRxDscp->pkt_addr & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (1518 + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len);

	/* Setting DMA Rx Descriptor Register */
	qdmaSetRxCpuIdx(base, ringIdx, pNewDscpInfo->dscpIdx) ;
	
	return ret ;
}
int qdma_wan_bm_hook_receive_buffer(sk_buff *skb, int ringIdx)
{
	struct QDMA_DscpInfo_S *pNewDscpInfo ;
	QDMA_DMA_DSCP_T *pRxDscp ;
	dma_addr_t dmaPktAddr ;
	uint base = CONFIG_QDMA2_BASE_ADDR;
	int ret = 0 ;
	unsigned long	addr, len;

	
	pNewDscpInfo = qdma_bm_get_unused_rx_dscp(QDMA_WAN) ;
	if(pNewDscpInfo == NULL) {
		QDMA_ERR("There is not any free RX DSCP.\n") ; 
		gpQdmaPriv->counters.noRxDscps++ ;
		return -1 ;
	}
	
#ifdef CONFIG_RX_2B_OFFSET
	QDMA_MSG(DBG_MSG, "Adjust the skb->tail location for net IP alignment\n") ;
	if(((uintptr_t)skb->data & 7) != 0) {
		//prom_printf("address not align 8\n");
	}
	skb_reserve(skb, 2) ;
	dmaPktAddr = (uintptr_t)K1_TO_PHY((skb->data - 2));
#else
	dmaPktAddr = (uintptr_t)K1_TO_PHY((skb->data));
#endif /* CONFIG_RX_2B_OFFSET */
	
	
	pRxDscp = gpQdmaPriv_wan->rxUsingPtr->dscpPtr ;
	pRxDscp->msg[0] = 0;
	pRxDscp->msg[1] = 0;
	pRxDscp->msg[2] = 0;
	pRxDscp->msg[3] = 0;
	pRxDscp->pkt_addr = dmaPktAddr ;
	pRxDscp->next_idx = pNewDscpInfo->dscpIdx ;
	pRxDscp->ctrl.pkt_len = 1518 ;
	pRxDscp->ctrl.done = 0 ;
	

	QDMA_MSG(DBG_MSG, "Hook RX DSCP to RXDMA. RX_CPU_IDX:%.8x, RX_NULL_IDX:%.8x\n", gpQdmaPriv_wan->rxUsingPtr->dscpIdx, pNewDscpInfo->dscpIdx) ;
	QDMA_MSG(DBG_MSG, "RXDSCP(%x): DONE:%d, PKT:%.8x, PKTLEN:%d, NEXT_IDX:%d\n", 
													pRxDscp,
													(uint)pRxDscp->ctrl.done, 
													(uint)pRxDscp->pkt_addr,
													(uint)pRxDscp->ctrl.pkt_len,
													(uint)pRxDscp->next_idx) ;
													
	gpQdmaPriv_wan->rxUsingPtr->skb = skb ;
	gpQdmaPriv_wan->rxUsingPtr = pNewDscpInfo ;
	gpQdmaPriv_wan->counters.rxCounts++ ; 


	addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[CONFIG_WAN_TX0_DSCP_NUM*CONFIG_WAN_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (CONFIG_WAN_RX0_DSCP_NUM*CONFIG_WAN_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len );

	addr = (unsigned long )pRxDscp->pkt_addr & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	len = (1518 + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
	flush_dcache_range(addr, addr + len);

	
	qdmaSetRxCpuIdx(base, ringIdx, pNewDscpInfo->dscpIdx) ;
	
	return ret ;
}


int qdma_prepare_rx_buffer(void)
{
	sk_buff *skb = NULL ;

	skb = alloc_skb(2000);
	if(skb == NULL) {
		return -1;
	}

	if(qdma_bm_hook_receive_buffer(skb, 0) != 0) {
		free_skb(skb) ;
		return -1;
	}
	
	return 0 ;
}
int qdma_wan_prepare_rx_buffer(void)
{
	sk_buff *skb = NULL ;

	skb = alloc_skb(2000);
	if(skb == NULL) {
		return -1;
	}

	if(qdma_wan_bm_hook_receive_buffer(skb, 0) != 0) {
		free_skb(skb) ;
		return -1;
	}
	
	return 0 ;
}

int qdma_has_free_rxdscp(uint8 mode)
{
	if(mode == QDMA_LAN)
		return (gpQdmaPriv->rxEndPtr->next != gpQdmaPriv->rxStartPtr) ;
	else
		return (gpQdmaPriv_wan->rxEndPtr->next != gpQdmaPriv_wan->rxStartPtr) ;
}

int ip_rcv_packet(sk_buff *skb)
{  
	struct iphdr *ip_hdr = (struct iphdr *)(skb->data);
	//struct ip_udp_hdr *ip_udp_hdr = (struct ip_udp_hdr *)(skb->data);
	int templen = skb->len;
	char *mac = (char *)(ip_hdr) - sizeof(struct ethhdr);

	/* record ip header pointer */
	skb->ip_hdr = (unsigned char *)(uintptr_t)ip_hdr;

	if (ip_hdr->protocol == UDP_){
		
		skb->len = ntohs(ip_hdr->tot_len);
		skb_pull(skb, sizeof(struct iphdr));

		if (!multiupgrade_process(skb, mac))
			return 0;
		skb_push(skb, sizeof(struct iphdr));
		skb->len = templen;
		
	}
	

	return 1;

}


int qdma_bm_receive_packets(uint maxPkts, int ringIdx) 
{	
	QDMA_DMA_DSCP_T rxDscp ;
	struct QDMA_DscpInfo_S dscpInfo ;
	uint cnt = maxPkts ;
	uint pktCount = 0 ;
	sk_buff *newSkb = NULL ;
	struct ethhdr *eth_hdr = NULL; 
	unsigned long	addr, len;

	
#if 1
	do {
		addr = (unsigned long )&mac_p->macMemPool_p->descrPool[CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_RX0_DSCP_NUM*CONFIG_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		invalidate_dcache_range(addr, addr + len );
		
		if(!gpQdmaPriv->rxStartPtr || gpQdmaPriv->rxStartPtr==gpQdmaPriv->rxEndPtr || !gpQdmaPriv->rxStartPtr->dscpPtr->ctrl.done)
		{
			goto ret ;
		}

		memcpy(&rxDscp, gpQdmaPriv->rxStartPtr->dscpPtr, sizeof(QDMA_DMA_DSCP_T)) ;
		memcpy(&dscpInfo, gpQdmaPriv->rxStartPtr, sizeof(struct QDMA_DscpInfo_S)) ;
		
		gpQdmaPriv->rxStartPtr = gpQdmaPriv->rxStartPtr->next ;
		
		pktCount++ ;
		/* check DSCP cotent */
		if(!rxDscp.pkt_addr || !rxDscp.ctrl.pkt_len)
		{
			QDMA_ERR("The content of the RX DSCP is incorrect.\n") ;
			gpQdmaPriv->counters.rxDscpIncorrect++ ; 
			break ;
		}
		newSkb = alloc_skb(2000);
		if(newSkb == NULL) {
			newSkb = dscpInfo.skb;
			QDMA_ERR("\nalloc fail\n");
			goto next;
		}
		dscpInfo.skb->len = rxDscp.ctrl.pkt_len;
		addr = (unsigned long)dscpInfo.skb->data & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (rxDscp.ctrl.pkt_len + 2 * CONFIG_SYS_CACHELINE_SIZE ) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		invalidate_dcache_range(addr, addr + len);

		if(startmulticast)
		{
			if (dscpInfo.skb->len <= 54 || strncmp( dscpInfo.skb->data + 46 ,"TCBulkFW" , 8))
			{
				free_skb(dscpInfo.skb);
				goto next; 
			}

			eth_hdr = (struct ethhdr *)(dscpInfo.skb->data);
			if (ntohs(eth_hdr->h_proto) == ETH_P_IP)
			{	
				skb_pull(dscpInfo.skb, sizeof(struct ethhdr));
				if (ip_rcv_packet(dscpInfo.skb))
				{
					skb_push(dscpInfo.skb, sizeof(struct ethhdr));
					net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
				}
				else 
				{
					net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
				}
			}
			else 
			{
				net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
			}
		}
		else
		{
			eth_hdr = (struct ethhdr *)(dscpInfo.skb->data);

#ifdef CONFIG_CMD_INIC
			if(ntohs(eth_hdr->h_proto) == ETH_TYPE_INIC)
			{	
				inic_network_packet_handler(dscpInfo.skb->data, dscpInfo.skb->len);
			}
			else
#endif
			{
			    net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
			}
		}
		free_skb(dscpInfo.skb);
next:
		if (qdma_has_free_rxdscp(QDMA_LAN))
		{
			qdma_bm_hook_receive_buffer(newSkb, 0);
		} else {
			QDMA_ERR("\nRX Error: no available QDMA RX descritor\n");
			gpQdmaPriv->counters.noRxDscps++ ;
			free_skb(newSkb);
		}
		

	} while((!maxPkts) || (--cnt)) ;

	return pktCount ;

ret:

	return pktCount ;
#else
	int i = 0;
	for(i = 0; i < gpQdmaPriv->rxDscpNum; i ++){
		memcpy(&rxDscp, gpQdmaPriv->rxStartPtr->dscpPtr, sizeof(QDMA_DMA_DSCP_T)) ;
		memcpy(&dscpInfo, gpQdmaPriv->rxStartPtr, sizeof(struct QDMA_DscpInfo_S)) ;
		gpQdmaPriv->rxStartPtr = gpQdmaPriv->rxStartPtr->next;
		if(gpQdmaPriv->rxStartPtr == NULL)
		{
			printf("gpQdmaPriv->rxStartPtr is null");

		return 0;
		}

		if(dscpInfo.skb == NULL)
			printf("skb is null");
		else{
			dscpInfo.skb->len = rxDscp.ctrl.pkt_len;

		}
		printf("idx is %d dscp idx is %daddr is\n", i, dscpInfo.dscpIdx);


	}
	return 0;

#endif
	if_tftp_start = 0;
	net_boot_file_size = 0;
	net_set_state(NETLOOP_CONTINUE);
	net_set_udp_handler(NULL);
	net_set_arp_handler(NULL);
	net_set_timeout_handler(0, NULL);

	return 0;

}
int qdma_wan_bm_receive_packets(uint maxPkts, int ringIdx) 
{	
	QDMA_DMA_DSCP_T rxDscp ;
	struct QDMA_DscpInfo_S dscpInfo ;
	uint cnt = maxPkts ;
	uint pktCount = 0 ;
	sk_buff *newSkb = NULL ;
	struct ethhdr *eth_hdr = NULL; 
	unsigned long	addr, len;

	
#if 1
	do {
		addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[CONFIG_WAN_TX0_DSCP_NUM*CONFIG_WAN_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_WAN_RX0_DSCP_NUM*CONFIG_WAN_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		invalidate_dcache_range(addr, addr + len );
		
		if(!gpQdmaPriv_wan->rxStartPtr || gpQdmaPriv_wan->rxStartPtr==gpQdmaPriv_wan->rxEndPtr || !gpQdmaPriv_wan->rxStartPtr->dscpPtr->ctrl.done)
		{
			goto ret ;
		}

		memcpy(&rxDscp, gpQdmaPriv_wan->rxStartPtr->dscpPtr, sizeof(QDMA_DMA_DSCP_T)) ;
		memcpy(&dscpInfo, gpQdmaPriv_wan->rxStartPtr, sizeof(struct QDMA_DscpInfo_S)) ;
		
		gpQdmaPriv_wan->rxStartPtr = gpQdmaPriv_wan->rxStartPtr->next ;
		
		pktCount++ ;
		/* check DSCP cotent */
		if(!rxDscp.pkt_addr || !rxDscp.ctrl.pkt_len)
		{
			QDMA_ERR("The content of the RX DSCP is incorrect.\n") ;
			gpQdmaPriv_wan->counters.rxDscpIncorrect++ ; 
			break ;
		}
		newSkb = alloc_skb(2000);
		if(newSkb == NULL) {
			newSkb = dscpInfo.skb;
			QDMA_ERR("\nalloc fail\n");
			goto next;
		}
		dscpInfo.skb->len = rxDscp.ctrl.pkt_len;
		addr = (unsigned long)dscpInfo.skb->data & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (rxDscp.ctrl.pkt_len + 2 * CONFIG_SYS_CACHELINE_SIZE ) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		invalidate_dcache_range(addr, addr + len);

		if(startmulticast)
		{
			if (dscpInfo.skb->len <= 54 || strncmp( dscpInfo.skb->data + 46 ,"TCBulkFW" , 8))
			{
				free_skb(dscpInfo.skb);
				goto next; 
			}

			eth_hdr = (struct ethhdr *)(dscpInfo.skb->data);
			if (ntohs(eth_hdr->h_proto) == ETH_P_IP)
			{	
				skb_pull(dscpInfo.skb, sizeof(struct ethhdr));
				if (ip_rcv_packet(dscpInfo.skb))
				{
					skb_push(dscpInfo.skb, sizeof(struct ethhdr));
					net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
				}
				else 
				{
					net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
				}
			}
			else 
			{
				net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
			}
		}
		else
		{
			eth_hdr = (struct ethhdr *)(dscpInfo.skb->data);

#ifdef CONFIG_CMD_INIC
			if(ntohs(eth_hdr->h_proto) == ETH_TYPE_INIC)
			{	
				inic_network_packet_handler(dscpInfo.skb->data, dscpInfo.skb->len);
			}
			else
#endif
			{
			    net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
			}
		}
		free_skb(dscpInfo.skb);
next:
		if (qdma_has_free_rxdscp(QDMA_WAN))
		{
			qdma_wan_bm_hook_receive_buffer(newSkb, 0);
		} else {
			QDMA_ERR("\nRX Error: no available QDMA WAN RX descritor\n");
			gpQdmaPriv_wan->counters.noRxDscps++ ;
			free_skb(newSkb);
		}
		

	} while((!maxPkts) || (--cnt)) ;

	return pktCount ;

ret:

	return pktCount ;
#else
	int i = 0;
	for(i = 0; i < gpQdmaPriv_wan->rxDscpNum; i ++){
		memcpy(&rxDscp, gpQdmaPriv_wan->rxStartPtr->dscpPtr, sizeof(QDMA_DMA_DSCP_T)) ;
		memcpy(&dscpInfo, gpQdmaPriv_wan->rxStartPtr, sizeof(struct QDMA_DscpInfo_S)) ;
		gpQdmaPriv_wan->rxStartPtr = gpQdmaPriv_wan->rxStartPtr->next;
		if(gpQdmaPriv_wan->rxStartPtr == NULL)
		{
			printf("gpQdmaPriv_wan->rxStartPtr is null");

		return 0;
		}

		if(dscpInfo.skb == NULL)
			printf("skb is null");
		else{
			dscpInfo.skb->len = rxDscp.ctrl.pkt_len;

		}
		printf("idx is %d dscp idx is %daddr is\n", i, dscpInfo.dscpIdx);


	}
	return 0;

#endif
	if_tftp_start = 0;
	net_boot_file_size = 0;
	net_set_state(NETLOOP_CONTINUE);
	net_set_udp_handler(NULL);
	net_set_arp_handler(NULL);
	net_set_timeout_handler(0, NULL);

	return 0;

}
static int qdma_bm_dscp_init(uint8_t mode)
{
	struct QDMA_DscpInfo_S *diPtr ;
	dma_addr_t dscpDmaAddr, irqDmaAddr, hwFwdDmaAddr ;
	dma_addr_t hwFwdBuffAddr ;
	uint dscpBaseAddr, hwTotalDscpSize, hwTotalPktSize ;
	uint i, base;
	uint txDscpNum ;
	uint rxDscpNum ;
	uint hwDscpNum ;
	uint irqDepth ;
	uint hwFwdPktLen ;
	int flag=0, cnt;

	uint lmgrInitCfg = 0 ;

	unsigned long   addr, len;
	if(mode == QDMA_LAN)
	{
		base = gpQdmaPriv->csrBaseAddr ;
		txDscpNum = gpQdmaPriv->txDscpNum ;
		rxDscpNum = gpQdmaPriv->rxDscpNum ;
		hwDscpNum = gpQdmaPriv->hwFwdDscpNum ;
		irqDepth = gpQdmaPriv->irqDepth ;
		hwFwdPktLen = gpQdmaPriv->hwPktSize ;	
	}else{
		base = gpQdmaPriv_wan->csrBaseAddr ;
		txDscpNum = gpQdmaPriv_wan->txDscpNum ;
		rxDscpNum = gpQdmaPriv_wan->rxDscpNum ;
		hwDscpNum = gpQdmaPriv_wan->hwFwdDscpNum ;
		irqDepth = gpQdmaPriv_wan->irqDepth ;
		hwFwdPktLen = gpQdmaPriv_wan->hwPktSize ;	
	}
	
	
	
	/******************************************
	* Allocate descriptor DMA memory          *
	*******************************************/
	//dscpBaseAddr = K0_TO_K1((uint)&mac_p->macMemPool_p->descrPool[0]) ;
	if(mode == QDMA_LAN)
		dscpBaseAddr = K0_TO_K1((uintptr_t)&mac_p->macMemPool_p->descrPool[0]);
	else
		dscpBaseAddr = K0_TO_K1((uintptr_t)&mac_p->macMemPool_wan_p->descrPool[0]);

	dscpDmaAddr = K1_TO_PHY(dscpBaseAddr);
	if(mode == QDMA_LAN){
		addr = (unsigned long )&mac_p->macMemPool_p->descrPool[0] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		flush_dcache_range(addr, addr + len );
		
		qdmaSetTxDscpBase(base, RING_IDX_0, dscpDmaAddr) ;

		addr = (unsigned long )&mac_p->macMemPool_p->descrPool[CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_RX0_DSCP_NUM*CONFIG_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		flush_dcache_range(addr, addr + len );
		qdmaSetRxDscpBase(base, RING_IDX_0, (dscpDmaAddr + sizeof(QDMA_DMA_DSCP_T)*(txDscpNum))) ;	

		qdmaSetRxRingSize(base, RING_IDX_0, rxDscpNum);
		qdmaSetRxRingThrh(base, RING_IDX_0, 0);
	}else{
		addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[0] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_WAN_TX0_DSCP_NUM*CONFIG_WAN_TX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		flush_dcache_range(addr, addr + len );
	
		qdmaSetTxDscpBase(base, RING_IDX_0, dscpDmaAddr) ;

		addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[CONFIG_TX0_DSCP_NUM*CONFIG_TX0_DSCP_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = (CONFIG_WAN_RX0_DSCP_NUM*CONFIG_WAN_RX0_DSCP_SIZE + 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		flush_dcache_range(addr, addr + len );
		qdmaSetRxDscpBase(base, RING_IDX_0, (dscpDmaAddr + sizeof(QDMA_DMA_DSCP_T)*(txDscpNum))) ;	

		qdmaSetRxRingSize(base, RING_IDX_0, rxDscpNum);
		qdmaSetRxRingThrh(base, RING_IDX_0, 0);
	}

	/******************************************
	* Allocate memory for IRQ queue           *
	******************************************/
	if(irqDepth) {
		
		if(mode ==QDMA_LAN)
		{
			gpQdmaPriv->irqQueueAddr = K0_TO_K1((dscpBaseAddr + sizeof(QDMA_DMA_DSCP_T)*(txDscpNum + rxDscpNum))) ;
			irqDmaAddr = K1_TO_PHY(gpQdmaPriv->irqQueueAddr);

			memset((void *)(uintptr_t)gpQdmaPriv->irqQueueAddr, CONFIG_IRQ_DEF_VALUE, irqDepth<<2) ;

			addr = (unsigned long )&mac_p->macMemPool_p->descrPool[DESC_TOTAL_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
			len = ((CONFIG_IRQ_DEPTH<<2)+ 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
			flush_dcache_range(addr, addr + len );
			
			/* Setting the IRQ queue information to QDMA register */
			qdmaSetIrqBase(base, irqDmaAddr) ;
			qdmaSetIrqDepth(base, irqDepth) ;
		}else{
			gpQdmaPriv_wan->irqQueueAddr = K0_TO_K1((dscpBaseAddr + sizeof(QDMA_DMA_DSCP_T)*(txDscpNum + rxDscpNum))) ;
			irqDmaAddr = K1_TO_PHY(gpQdmaPriv_wan->irqQueueAddr);

			memset((void *)(uintptr_t)gpQdmaPriv_wan->irqQueueAddr, CONFIG_IRQ_DEF_VALUE, irqDepth<<2) ;

			addr = (unsigned long )&mac_p->macMemPool_wan_p->descrPool[DESC_WAN_TOTAL_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
			len = ((CONFIG_IRQ_DEPTH<<2)+ 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
			flush_dcache_range(addr, addr + len );
			
			/* Setting the IRQ queue information to QDMA register */
			qdmaSetIrqBase(base, irqDmaAddr) ;
			qdmaSetIrqDepth(base, irqDepth) ;
			
		}
	}
	//brown here
	/***************************************************
	* Allocate memory for TX/RX DSCP Information node  *
	****************************************************/
	if(mode ==QDMA_LAN)
	{
		gpQdmaPriv->dscpInfoAddr = (ulong)&DummydscpInfoAddr;

		memset((void *)(long)gpQdmaPriv->dscpInfoAddr, 0, DESC_INFO_SIZE);

		gpQdmaPriv->txBaseAddr = gpQdmaPriv->dscpInfoAddr;
		gpQdmaPriv->rxBaseAddr = gpQdmaPriv->dscpInfoAddr + sizeof(struct QDMA_DscpInfo_S)*txDscpNum;

		//Create unused tx descriptor link list and using rx descriptor ring
		for(i=0 ; i<(txDscpNum + rxDscpNum) ; i++)
		{
			diPtr = (struct QDMA_DscpInfo_S *)((long)gpQdmaPriv->dscpInfoAddr) + i;
			diPtr->dscpPtr = (QDMA_DMA_DSCP_T *)((long)dscpBaseAddr) + i;
			
			if(i < txDscpNum) {
				diPtr->dscpIdx = i ;
				diPtr->next = NULL ;
				qdma_bm_push_tx_dscp(diPtr, 0,QDMA_LAN) ;
			} else  {
				diPtr->dscpIdx = i - txDscpNum ;
				diPtr->next = NULL ;
				qdma_bm_add_rx_dscp(diPtr,QDMA_LAN) ;
			}
		}	
	}else{
		gpQdmaPriv_wan->dscpInfoAddr = (ulong)&DummydscpInfoAddr_wan;

		memset((void *)(long)gpQdmaPriv_wan->dscpInfoAddr, 0, DESC_WAN_INFO_SIZE);

		gpQdmaPriv_wan->txBaseAddr = gpQdmaPriv_wan->dscpInfoAddr;
		gpQdmaPriv_wan->rxBaseAddr = gpQdmaPriv_wan->dscpInfoAddr + sizeof(struct QDMA_DscpInfo_S)*txDscpNum;

		//Create unused tx descriptor link list and using rx descriptor ring
		for(i=0 ; i<(txDscpNum + rxDscpNum) ; i++)
		{
			diPtr = (struct QDMA_DscpInfo_S *)((long)gpQdmaPriv_wan->dscpInfoAddr) + i;
			diPtr->dscpPtr = (QDMA_DMA_DSCP_T *)((long)dscpBaseAddr) + i;
			
			if(i < txDscpNum) {
				diPtr->dscpIdx = i ;
				diPtr->next = NULL ;
				qdma_bm_push_tx_dscp(diPtr, 0,QDMA_WAN) ;
			} else  {
				diPtr->dscpIdx = i - txDscpNum ;
				diPtr->next = NULL ;
				qdma_bm_add_rx_dscp(diPtr,QDMA_WAN) ;
			}
		}	
		
	}
	/***************************************************
	* Initialization first DSCP for Tx0 DMA              *
	****************************************************/
	if(mode ==QDMA_LAN){
		diPtr = qdma_bm_pop_tx_dscp(QDMA_LAN) ;
		if(!diPtr) {
			QDMA_ERR("There is not any free TX DSCP.\n") ; 
			return -ENOSR ;
		}
		gpQdmaPriv->txUsingPtr = diPtr ;
		qdmaSetTxCpuIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		qdmaSetTxDmaIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		
		/***************************************************
		* Initialization first DSCP for Rx0 DMA              *
		****************************************************/
		diPtr = qdma_bm_get_unused_rx_dscp(QDMA_LAN) ;
		if(diPtr == NULL) {
			QDMA_ERR("There is not any free RX DSCP.\n") ;
			return -ENOSR ;
		} 
		gpQdmaPriv->rxUsingPtr = diPtr ;
		qdmaSetRxCpuIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		qdmaSetRxDmaIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		

		/***************************************************
		* Initialization packets for Rx DMA              *
		****************************************************/

		do {
			if(qdma_prepare_rx_buffer() != 0)
			{
				break ;
			}
		} while(qdma_has_free_rxdscp(QDMA_LAN)) ;	
		
		
		/***************************************************
		* Initialization DSCP for hardware forwarding      *
		****************************************************/
		if(hwDscpNum) {

			hwTotalDscpSize = sizeof(QDMA_HWFWD_DMA_DSCP_T) * hwDscpNum ;

			hwTotalPktSize = hwFwdPktLen * hwDscpNum ;

			gpQdmaPriv->hwFwdPayloadSize = hwFwdPktLen;

			gpQdmaPriv->hwFwdBaseAddr = K0_TO_K1(gpQdmaPriv->irqQueueAddr + (CONFIG_IRQ_DEPTH<<2) );
			hwFwdDmaAddr = K1_TO_PHY(gpQdmaPriv->hwFwdBaseAddr);

			gpQdmaPriv->hwFwdBuffAddr = K0_TO_K1(gpQdmaPriv->hwFwdBaseAddr + hwTotalDscpSize) ;
			hwFwdBuffAddr = K1_TO_PHY(gpQdmaPriv->hwFwdBuffAddr);

			qdmaSetHwDscpBase(base, hwFwdDmaAddr) ;
			qdmaSetHwBuffBase(base, hwFwdBuffAddr) ;

			qdmaSetHwPayloadSize(base, HWFWD_PAYLOAD_SIZE_2K);
			qdmaSetHwLowThrshld(base, HWFWD_LOW_THRESHOLD);

			lmgrInitCfg = qdmaGetHwInitCfg(base) ;
			lmgrInitCfg = (lmgrInitCfg | LMGR_INIT_START | hwDscpNum) ; /*set DSCP in DRAM*/
			qdmaSetHwInitCfg(base, lmgrInitCfg) ;


			flag = 0 ;

			cnt = 1000;
			while((cnt--) > 0) {		
				if(qdmaGetHWInitStart(base) == 0) {
					flag=1;
					break;
				}
				
			}

			if(flag == 0) {
				QDMA_ERR("hw_fwd init fail!\n") ;
				return -1;
			}
		}
	}else{
		diPtr = qdma_bm_pop_tx_dscp(QDMA_WAN) ;
		if(!diPtr) {
			QDMA_ERR("There is not any free TX DSCP.\n") ; 
			return -ENOSR ;
		}
		gpQdmaPriv_wan->txUsingPtr = diPtr ;
		qdmaSetTxCpuIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		qdmaSetTxDmaIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		
		/***************************************************
		* Initialization first DSCP for Rx0 DMA              *
		****************************************************/
		diPtr = qdma_bm_get_unused_rx_dscp(QDMA_WAN) ;
		if(diPtr == NULL) {
			QDMA_ERR("There is not any free RX DSCP.\n") ;
			return -ENOSR ;
		} 
		gpQdmaPriv_wan->rxUsingPtr = diPtr ;
		qdmaSetRxCpuIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		qdmaSetRxDmaIdx(base, RING_IDX_0, diPtr->dscpIdx) ;
		

		/***************************************************
		* Initialization packets for Rx DMA              *
		****************************************************/

		do {
			if(qdma_wan_prepare_rx_buffer() != 0)
			{
				break ;
			}
		} while(qdma_has_free_rxdscp(QDMA_WAN)) ;	
		
		/***************************************************
		* Initialization DSCP for hardware forwarding      *
		****************************************************/
		if(hwDscpNum) {

			hwTotalDscpSize = sizeof(QDMA_HWFWD_DMA_DSCP_T) * hwDscpNum ;

			hwTotalPktSize = hwFwdPktLen * hwDscpNum ;

			gpQdmaPriv_wan->hwFwdPayloadSize = hwFwdPktLen;

			gpQdmaPriv_wan->hwFwdBaseAddr = K0_TO_K1(gpQdmaPriv_wan->irqQueueAddr + (CONFIG_IRQ_DEPTH<<2) );
			hwFwdDmaAddr = K1_TO_PHY(gpQdmaPriv_wan->hwFwdBaseAddr);

			gpQdmaPriv_wan->hwFwdBuffAddr = K0_TO_K1(gpQdmaPriv_wan->hwFwdBaseAddr + hwTotalDscpSize) ;
			hwFwdBuffAddr = K1_TO_PHY(gpQdmaPriv_wan->hwFwdBuffAddr);

			qdmaSetHwDscpBase(base, hwFwdDmaAddr) ;
			qdmaSetHwBuffBase(base, hwFwdBuffAddr) ;

			qdmaSetHwPayloadSize(base, HWFWD_PAYLOAD_SIZE_2K);
			qdmaSetHwLowThrshld(base, HWFWD_LOW_THRESHOLD);

			lmgrInitCfg = qdmaGetHwInitCfg(base) ;
			lmgrInitCfg = (lmgrInitCfg | LMGR_INIT_START | hwDscpNum) ; /*set DSCP in DRAM*/
			qdmaSetHwInitCfg(base, lmgrInitCfg) ;


			flag = 0 ;

			cnt = 1000;
			while((cnt--) > 0) {		
				if(qdmaGetHWInitStart(base) == 0) {
					flag=1;
					break;
				}
				
			}

			if(flag == 0) {
				QDMA_ERR("hw_fwd init fail!\n") ;
				return -1;
			}
		}
	}
	
	return 0 ;
}


/******************************************************************************
******************************************************************************/
int qdma_dev_init(uint8 mode) 
{
	
	uint base = 0;
	uint i=0, glbCfg=0;
	uint int_Enable1=0, int_Enable2=0 ;
	
	if(mode == QDMA_LAN)
		base = gpQdmaPriv->csrBaseAddr;
	else
		base = gpQdmaPriv_wan->csrBaseAddr;
	
	qdmaClearIntStatus1(base, 0xFFFFFFFF) ;
	qdmaClearIntStatus2(base, 0xFFFFFFFF) ;

	/********************************************
	* enable/disable the qdma interrupt         *
	*********************************************/
	/*only enable rx-0 INT*/
	int_Enable1 = 0 ;
	int_Enable2 = INT_MASK_RX_DONE ;
	qdmaSetIntMask(base, int_Enable1, QDMA_INT1_ENABLE1) ;
	qdmaSetIntMask(base, int_Enable2, QDMA_INT1_ENABLE2) ;

	
	/********************************************
	* Setting the global register               *
	*********************************************/
#ifndef TCSUPPORT_LITTLE_ENDIAN
	glbCfg |= (GLB_CFG_DSCP_BYTE_SWAP | GLB_CFG_PAYLOAD_BYTE_SWAP | GLB_CFG_MSG_WORD_SWAP | ((VAL_BST_32_DWARD<<GLB_CFG_BST_SE_SHIFT)&GLB_CFG_BST_SE_MASK)) ;
#else
	glbCfg |= (GLB_CFG_PAYLOAD_BYTE_SWAP | GLB_CFG_MSG_WORD_SWAP | ((VAL_BST_32_DWARD<<GLB_CFG_BST_SE_SHIFT)&GLB_CFG_BST_SE_MASK)) ;
#endif

	glbCfg |= ((long)PREFER_TX1_FWD_TX0<<GLB_CFG_DMA_PREFERENCE_SHIFT)&GLB_CFG_DMA_PREFERENCE_MASK;

	if(gpQdmaPriv->irqDepth) {
		glbCfg |= GLB_CFG_IRQ_EN ;
	}
	
#ifdef CONFIG_RX_2B_OFFSET
	glbCfg |= GLB_CFG_RX_2B_OFFSET ;
#endif /* CONFIG_RX_2B_OFFSET */

#ifdef CONFIG_TX_WB_DONE
	glbCfg |= GLB_CFG_TX_WB_DONE ;
#endif /* GLB_CFG_TX_WB_DONE */
	
	qdmaSetGlbCfg(base, glbCfg) ;

	for(i=0; i<RX_RING_NUM; i++) {
		write_reg_word(QDMA_CSR_RX_DELAY_INT_CFG(base, i), 0);
	}

	return 0 ;
}

int  qdma_lan_init(void){
	int ret;
	/* Initial device private data */

	memset(gpQdmaPriv, 0, sizeof(QDMA_Private_T));
	gpQdmaPriv->dbgLevel = DBG_ERR ;
	/* Initial for design and verification function */
	gpQdmaPriv->txDscpNum = CONFIG_TX0_DSCP_NUM;
	gpQdmaPriv->rxDscpNum = CONFIG_RX0_DSCP_NUM;
	gpQdmaPriv->hwFwdDscpNum = CONFIG_HWFWD_DSCP_NUM;
	gpQdmaPriv->irqDepth = CONFIG_IRQ_DEPTH;
	gpQdmaPriv->hwPktSize = CONFIG_MAX_PKT_LENS;
	gpQdmaPriv->csrBaseAddr = CONFIG_QDMA_BASE_ADDR;
	if((ret = qdma_bm_dscp_init(QDMA_LAN)) != 0) 
	{
		QDMA_ERR("QDMA DSCP initialization failed.\n") ;
		return ret ;
	}
	/***************************************************
	* QDMA device initialization                       *
	****************************************************/
	if((ret = qdma_dev_init(QDMA_LAN)) != 0) {
		QDMA_ERR("QDMA hardware device initialization failed.\n") ;
		return ret ;
	}
	qdmaEnableTxDma(gpQdmaPriv->csrBaseAddr) ;
	qdmaEnableRxDma(gpQdmaPriv->csrBaseAddr) ;
	return 0 ;
} 

int  qdma_wan_init(void){
	int ret;
	/* Initial device private data */

	memset(gpQdmaPriv_wan, 0, sizeof(QDMA_Private_T));
	gpQdmaPriv_wan->dbgLevel = DBG_ERR ;
	/* Initial for design and verification function */
	gpQdmaPriv_wan->txDscpNum = CONFIG_WAN_TX0_DSCP_NUM;
	gpQdmaPriv_wan->rxDscpNum = CONFIG_WAN_RX0_DSCP_NUM;
	gpQdmaPriv_wan->hwFwdDscpNum = CONFIG_HWFWD_DSCP_NUM;
	gpQdmaPriv_wan->irqDepth = CONFIG_IRQ_DEPTH;
	gpQdmaPriv_wan->hwPktSize = CONFIG_MAX_PKT_LENS;
	gpQdmaPriv_wan->csrBaseAddr = CONFIG_QDMA2_BASE_ADDR;
	if((ret = qdma_bm_dscp_init(QDMA_WAN)) != 0) 
	{
		QDMA_ERR("QDMA DSCP initialization failed.\n") ;
		return ret ;
	}
	/***************************************************
	* QDMA device initialization                       *
	****************************************************/
	if((ret = qdma_dev_init(QDMA_WAN)) != 0) {
		QDMA_ERR("QDMA hardware device initialization failed.\n") ;
		return ret ;
	}
	qdmaEnableTxDma(gpQdmaPriv_wan->csrBaseAddr) ;
	qdmaEnableRxDma(gpQdmaPriv_wan->csrBaseAddr) ;
	return 0 ;
} 

void detect_switch()
{
    uint32 switch_chip_id = 0;

    switch_chip_id = read_reg_word(GSW_CFG_CREV);
    if((switch_chip_id & 0xFFFF0000) == 0x75300000){
        use_ext_switch = 0;
    }
    switch_chip_id = gswPbusRead(EXT_GSW_CFG_CREV);
    if((switch_chip_id & 0xFFFF0000) == 0x75300000){
        use_ext_switch = 1;
    }
}

int tc3162_eth_init(unsigned char *mac_addr)
{
	return 0;
}

int tc3162_eth_exit(void)
{
	return 0;
}

#if defined(TCSUPPORT_CPU_AN7583)
void xfi_mac_init(void)
{
	unsigned char* serdes_intf[SERDES_INTF_MAX];
    int i = 0;
	ulong upgrade_serdes = 0;
	int len_eth = 0;
    
    serdes_intf[SERDES_ETH] = env_get("serdes_ethernet");
    serdes_intf[SERDES_USB] = env_get("serdes_usb1");
    serdes_intf[SERDES_PCIE1] = env_get("serdes_wifi1");
    serdes_intf[SERDES_PCIE2] = env_get("serdes_wifi2");

	upgrade_serdes = env_get_ulong("fw_port", 0, FW_PORT_DEFAULT);
	len_eth = strlen(serdes_intf[SERDES_ETH]);

	if((upgrade_serdes == 0) || (upgrade_serdes >= SERDES_INTF_MAX+1))
		return;

	if(len_eth < 2)
		return;

    for(i = 0;i < SERDES_INTF_MAX;i++)
    {
        if((serdes_intf[i][len_eth-2] == SERDES_ETHERLAN) && (upgrade_serdes == (i+1)))
        {
            switch (upgrade_serdes){
				case 1:
					write_reg_word(0x1fa09000, 0x77fe2c00);  //Enable xfi_mac_eth rx mbi/mpi, for 2.5g/10g fw upgrade
					break;
				case 2:
					write_reg_word(0x1fa05000, 0x77fe2c00);  //Enable xfi_mac_usb rx mbi/mpi
					break;
				case 3:
				case 4:
					write_reg_word(0x1fa04000, 0x77fe2c00);  //Enable xfi_mac_pcie rx mbi/mpi
					break;
				default:
					printf("Invalid fw_port\n");
            }
            return;
        }
    }
	return;
}
#endif

#if defined(TCSUPPORT_CPU_AN7583)
struct serdes_fwd_info fwd_info[SERDES_INTF_MAX] = 
{
    {GDM_P_GDMA3,13,0},
    {GDM_P_GDMA4,12,1},
    {GDM_P_GDMA4,10,0},
    {GDM_P_GDMA4,11,0},
    {0,0,0},        
};
#else
struct serdes_fwd_info fwd_info[SERDES_INTF_MAX] = 
{
    {GDM_P_GDMA4,13,0},
    {GDM_P_GDMA4,12,1},
    {GDM_P_GDMA3,10,4},
    {GDM_P_GDMA3,11,5},
    {0,0,0},        
};
#endif

static int arht_eth_check_serdes(unsigned int* p_fport,unsigned int* p_chn,unsigned int* p_nbq)
{	
	/* use upgrade_serdes_sel to choose upgrade serdes */
	
    unsigned char* serdes_intf[SERDES_INTF_MAX];
    int i = 0;
	ulong upgrade_serdes = 0;
	int len_eth = 0;
    
    serdes_intf[SERDES_ETH] = env_get("serdes_ethernet");
    serdes_intf[SERDES_USB] = env_get("serdes_usb1");
    serdes_intf[SERDES_PCIE1] = env_get("serdes_wifi1");
    serdes_intf[SERDES_PCIE2] = env_get("serdes_wifi2");

	upgrade_serdes = env_get_ulong("fw_port", 0, FW_PORT_DEFAULT);
	len_eth = strlen(serdes_intf[SERDES_ETH]);

	//use switch or pon serdes to upgrade fw
	if((upgrade_serdes == 0) || (upgrade_serdes >= SERDES_INTF_MAX+1)||(fw_upgrade_select != 0))
		return 0;


	if(len_eth < 2)
	{
		printf("arht_eth_check_serdes error eth_len=%d\n",len_eth);
		return 0;
	}

    for(i = 0;i < SERDES_INTF_MAX;i++)
    {
        if((serdes_intf[i][len_eth-2] == SERDES_ETHERLAN) && (upgrade_serdes == (i+1)))
        {
            *p_fport = fwd_info[i].fport;
            *p_chn = fwd_info[i].chn;
			*p_nbq = fwd_info[i].nbq;
            return 1;
        }
    }

    return 0;
}
static void ecnt_eth_check_serdes_pon(void)
{
	fw_upgrade_select = env_get_ulong("fw_upgrade_select", 0, FW_UPGRADE);
	return ;
}

#ifdef CONFIG_PHY_AN8831X
#define AN8831_FW_LOAD_ADDR (short unsigned int *)0x81db5000
#define AN8831_FW_MAX_SIZE 0x4b000
int an8811_fw_check(void)
{
	unsigned short *an8831_fw_load_addr = AN8831_FW_LOAD_ADDR;
	unsigned int fw_size = 0, fw_crc = 0;
	unsigned short *fw_data = NULL;
	
	fw_size = (unsigned int)htons(an8831_fw_load_addr[0]) << 16 | htons(an8831_fw_load_addr[1]);
    if (fw_size == 0) {
        printf("No firmware exist.\r\n" );
        return -1;
    }

    if (fw_size > AN8831_FW_MAX_SIZE) {
        printf("AN8831X warning firmware bin size too large.\r\n" );
        return -1;
    }
	
	fw_crc = (unsigned int)htons(an8831_fw_load_addr[2]) << 16 | htons(an8831_fw_load_addr[3]);
	printf("AN8831X image len 0x%x, crc32 0x%x.\r\n", fw_size, fw_crc);
	fw_data = an8831_fw_load_addr + 4;
	
	if(fw_crc != crc32(0, (unsigned char *)fw_data, fw_size))
	{
        printf("AN8831X failed to compare crc 32.\r\n" );
        return -1;      
    }
	
	printf("AN8831X firmare check success at 0x%lx\n",AN8831_FW_LOAD_ADDR);
	
	return 0;
}
#endif

static int arht_eth_send(struct udevice* dev, void *packet, int length)
{
	sk_buff *skb_temp;
    unsigned int fport;
    unsigned int chn;
	unsigned int nbq;
    
	if (packet == NULL)
	{
		return 1;
	}

	skb_temp = alloc_skb(length);
	if (skb_temp == NULL)
	{
		return 1;
	}
	skb_temp->data = packet;
	skb_temp->len = length;

	uint32 len = skb_temp->len;
	ethTxMsg_t ethTxMsg;
	int error;

	if (len < 60) {
		len = 60;
	}

	  /* GDMA1 */
	memset(&ethTxMsg, 0, sizeof(ethTxMsg_t));
	ethTxMsg.raw.queue =0;
	ethTxMsg.raw.nboq = 0;
	ethTxMsg.raw.mtr_g = 0x7f; /*not use any meter ratelimit*/

	if(isINICImage(packet,length) || fw_upgrade_select)
	{	
		ethTxMsg.raw.fport = GDM_P_GDMA2;
		error = qdma_wan_bm_transmit_packet(skb_temp, 0,ethTxMsg.msg[0],ethTxMsg.msg[1]) ;
		if(error){
			free_skb(skb_temp);
			return 0;
		 }
		 return 0;
	}
	else {
#if defined(INCLUDE_BOOT_ENABLE_2G5_LAN)
		/* do nothing */
#else /* !INCLUDE_BOOT_ENABLE_2G5_LAN */
		if(arht_eth_check_serdes(&fport, &chn, &nbq))
		{
			ethTxMsg.raw.fport = fport;
			ethTxMsg.raw.channel = chn;
			ethTxMsg.raw.nboq = nbq;
		}
		else
#endif /* INCLUDE_BOOT_ENABLE_2G5_LAN */
		{
			ethTxMsg.raw.fport = GDM_P_GDMA1;
			ethTxMsg.raw.channel = 0;	
		}
		error = qdma_bm_transmit_packet(skb_temp, 0,ethTxMsg.msg[0],ethTxMsg.msg[1]) ;

		if(error){
			free_skb(skb_temp);
			return 0;
		 }

#if defined(INCLUDE_BOOT_ENABLE_2G5_LAN)
		if(arht_eth_check_serdes(&fport, &chn, &nbq))
		{
			/* for 2.5G LAN */
			sk_buff *skb_lan_temp = NULL;

			skb_lan_temp = alloc_skb(length);
			if (NULL == skb_lan_temp)
			{
				return 0;
			}
			skb_lan_temp->data = packet;
			skb_lan_temp->len = length;

			memset(&ethTxMsg, 0, sizeof(ethTxMsg_t));

			ethTxMsg.raw.queue =0;
			ethTxMsg.raw.nboq = 0;
			ethTxMsg.raw.mtr_g = 0x7f; /*not use any meter ratelimit*/

			ethTxMsg.raw.fport = fport;
			ethTxMsg.raw.channel = chn;
			ethTxMsg.raw.nboq = nbq;

			error = qdma_bm_transmit_packet(skb_lan_temp, 0, ethTxMsg.msg[0], ethTxMsg.msg[1]) ;

			if (error)
			{
				free_skb(skb_lan_temp);
				//printf("qdma tranmit fail\n");
				return 0;
			}
		}
#endif /* INCLUDE_BOOT_ENABLE_2G5_LAN */

		return 0;
	}
	
}


static int arht_eth_recv(struct udevice* dev)
{
	uint intStatus1=0, intStatus2=0, intStatus3=0, intStatus4=0;
	uint base = CONFIG_QDMA_BASE_ADDR;
	uint base_wan = CONFIG_QDMA2_BASE_ADDR;
    int pktlen=0;
		intStatus1 = qdmaGetIntStatus1(base) & qdmaGetIntMask(base, QDMA_INT1_ENABLE1) ;
		intStatus2 = qdmaGetIntStatus2(base) & qdmaGetIntMask(base, QDMA_INT1_ENABLE2) ;
		intStatus3 = qdmaGetIntStatus1(base_wan) & qdmaGetIntMask(base_wan, QDMA_INT1_ENABLE1) ;
		intStatus4 = qdmaGetIntStatus2(base_wan) & qdmaGetIntMask(base_wan, QDMA_INT1_ENABLE2) ;

		if((intStatus2 & INT_STATUS_RX_DONE)) {
			qdmaClearIntStatus2(base, (intStatus2 & INT_STATUS_RX_DONE)) ;
	    #if defined(DRAM_PROTECT_TEST)
			nmi_mem_protect_test();
	    #endif
		
			pktlen = qdma_bm_receive_packets(16,0);
		}
		else if((intStatus4 & INT_STATUS_RX_DONE)){
			qdmaClearIntStatus2(base_wan, (intStatus4 & INT_STATUS_RX_DONE)) ;
	    #if defined(DRAM_PROTECT_TEST)
			nmi_mem_protect_test();
	    #endif
		
			pktlen = qdma_wan_bm_receive_packets(16,0);

		}
		if(intStatus1 & INT_STATUS_QDMA_FAULT) {
			QDMA_MSG(DBG_MSG, "err qdma INT_STATUS=%08lx\n", intStatus1);
		}
		if(intStatus3 & INT_STATUS_QDMA_FAULT) {
			QDMA_MSG(DBG_MSG, "err qdma wan INT_STATUS=%08lx\n", intStatus3);
		}
		return pktlen;

}

void arht_eth_stop(struct udevice* dev)
{

}

/**
 * @brief Get 8811 phy address based on env 8811phy_addr and fw_port
 *
 * @return an8811 phy address (default: 15 on error)
 */
static unsigned char get_an8811_phy_addr(void)
{
	unsigned char *env_ptr = NULL, an8811_phy_addr = AN8811H_PHYADDR;
	int index = 0;
	unsigned long fw_port = 0;
	int val;

	env_ptr = env_get("8811phy_addr");
	if(!env_ptr){
		return an8811_phy_addr;
	}
	phy8811Addrs[0] = simple_strtoul(env_ptr + 0, NULL, 16);
	phy8811Addrs[1] = simple_strtoul(env_ptr + 2, NULL, 16);
	phy8811Addrs[2] = simple_strtoul(env_ptr + 4, NULL, 16);
	phy8811Addrs[3] = simple_strtoul(env_ptr + 6, NULL, 16);
	phy8811Addrs[4] = simple_strtoul(env_ptr + 8, NULL, 16);
	
	fw_port = env_get_ulong("fw_port", NULL, FW_PORT_DEFAULT);
	if((fw_port == 0) || (fw_port >= SERDES_INTF_MAX+1)){
		return an8811_phy_addr;
	}
	index = fw_port_to_phy8811Addrs_index_mapping[fw_port-1];
	/**
	 * * Note: The 8811phy_addr accepts PHY addresses ranging from 8 to 15,
	 * *       as specified in mi.conf. However, in our RFB setup, 
	 * *       PHY addresses 9 to 12 are reserved for the 7530 switch.
	 * *
	 * * If the error message "could not get phy for address xx" appears during
	 * * bootloader initialization, it implies that the 7530 has claimed the PHY address.
	 * */
	if((phy8811Addrs[index] >= 8) && (phy8811Addrs[index] <=0xF)){
		an8811_phy_addr = phy8811Addrs[index];
	}

	return an8811_phy_addr;
}

static int arht_eth_init(struct udevice* dev)
{	
	unsigned iomux = 0, iomux_ori = 0;
	unsigned char an8811h_phyaddr = 0;
	struct mii_dev *arht_mii_dev = NULL;
	struct phy_device *arht_phy_device = NULL;
	int val, val_poll;
	if(arht_eth_initd == 0 ){
		int i;
		uint32 reg_val = 0;
		//struct eth_pdata *pdata = dev_get_plat(dev);
		an8811h_phyaddr = get_an8811_phy_addr();
		ecnt_eth_check_serdes_pon();   /* determine whether to init qdma_lan or qdma_wan*/
		/* disable & store Ether PHY LED */ 
		iomux_ori = read_reg_word(IOMUX_CONTROL1);
		iomux = iomux_ori & ~(0xFF << 3);
		write_reg_word(IOMUX_CONTROL1, iomux);
		macResetSwMAC(); 
		setEtherRateLimit (1);
		detect_switch();
		if(use_ext_switch){
			gswPbusWrite(0x7000, 0x3); /* reset external switch*/
			macCfgExtSwitch();
		}

		VPint(SHARE_FEMEM_SEL) = 0;

		mac_p->macMemPool_p = &enetMemPool;
		eth_phy_init(mac_p);
#ifndef CONFIG_OPEN_IMAGE // if restore the register 0x1FA20214 as 0x2a8,it will set GPIO 1-4 control by hw
		/* restore Ether PHY LED iomux */
		write_reg_word(IOMUX_CONTROL1, iomux_ori);
#endif/*CONFIG_OPEN_IMAGE*/
		pause(100);
		skb_init();
		
		qdma_lan_init();
#ifdef INCLUDE_BOOT_AGINETCONFIG
		macGetMacAddr(mac_p, net_ethaddr);
#else /* !INCLUDE_BOOT_AGINETCONFIG */
		macGetMacAddr(mac_p, mac_addr);
#endif/* INCLUDE_BOOT_AGINETCONFIG */
		macDrvRegInit(mac_p);
		pause(100);
		
		if(fw_upgrade_select || arht_check_pon_serdes_dsl())
		{
			mac_p->macMemPool_wan_p = &enetMemPool_wan;
			qdma_wan_init();
		}

		for(i=0;i<7;i++){
			reg_val = VPint(GSW_MAC_BASE+i*0x100);
			reg_val &=0xffffffcf;/*Disable force TX/RX FC*/
			VPint(GSW_MAC_BASE+i*0x100)=reg_val;
		}	


		if(use_ext_switch){
			gswPbusWrite(0xA4,0x08160816);
			gswPbusWrite(0x1C,0x68166816);
			gswPbusWrite(0x20,0x08160816);
			gswPbusWrite(0x24,0x08160816);
		}else{
			write_reg_word(GSW_BASE + 0xA4,0x08160816);
			write_reg_word(GSW_BASE + 0x1C,0x68166816);
			write_reg_word(GSW_BASE + 0x20,0x08160816);
			write_reg_word(GSW_BASE + 0x24,0x08160816);
		}

		write_reg_word(GSW_MFC, 0xffffff00);
		write_reg_word(GSW_APC, 0x08100810);

		gswPbusWrite(0x7810,0x00010022);

		//init serdes phy
		if(!isFPGA)
			serdes_phy_init();

		arht_mii_dev = miiphy_get_dev_by_name(dev->name);
		if(arht_mii_dev)
		{
#ifdef CONFIG_PHY_AIROHA_EN8811H
			arht_phy_device = phy_connect(arht_mii_dev, an8811h_phyaddr, dev, 0) ;
			if(arht_phy_device)
				phy_config(arht_phy_device);
#endif
#ifdef CONFIG_PHY_AN8831X
			if(0 == an8811_fw_check())
			{
						
				if(isEN7581)
				{
				  //clear mdc polling [30:24]
			  	    val =read_reg_word(0x1fb5f018);
				    val_poll =val;
				    val = val & 0x80ffffff ;
				    write_reg_word(0x1fb5f018, val);
                    //reet 8831				
			  	    val =read_reg_word(0x1fbf0204);
				    val = val & 0xefffffff ;  
				    write_reg_word(0x1fbf0204, val);
				    __udelay(200);
				    val = val | 0x80000000 ;
				    write_reg_word(0x1fbf0204, val);
				   					
					IO_CBITS(0x1fa2020c,0xf << 8);
					IO_SBITS(0x1fa2020c,0x3 << 8);
				    //write_reg_word(0x1fa2020c, 0x00000300);
					//val =read_reg_word(0x1fa2020c);
					//printf("0x1fa2020c %x \n",val);

				}
				if(isAN7583)
				{
					IO_CBITS(0x1fa2020c,0xf << 15);
					IO_SBITS(0x1fa2020c,0x1 << 15);
				}

				arht_phy_device = phy_connect(arht_mii_dev, AN8831X_PHYADDR, dev, 0);
				if(arht_phy_device)
					phy_config(arht_phy_device);
				
			    if(isEN7581)
				{
					//restore mdc polling [30:24]
			        write_reg_word(0x1fb5f018, val_poll);
				}
				
			}
#endif
		}
	}

#ifdef TCSUPPORT_CPU_AN7583
	write_reg_word(0x1fb5fc14, 0xc00017a5);
	__udelay(2000); //delay 1ms
	write_reg_word(0x1fb5fc24, 0x00001040);
	write_reg_word(0x1fb5fc20, 0xc0800000);
	write_reg_word(0x1fb5fc24, 0x00001040);
	write_reg_word(0x1fb5fc20, 0xc1800000);
	write_reg_word(0x1fb5fc24, 0x00001040);
	write_reg_word(0x1fb5fc20, 0xc2800000);
	write_reg_word(0x1fb5fc24, 0x00001040);
	write_reg_word(0x1fb5fc20, 0xc3800000);
	//led
	int phyaddr;
	for(phyaddr=9;phyaddr<13;phyaddr++){
	  mtEMiiRegWrite(phyaddr, 0x1f, 0x21, 0x800a);
	  mtEMiiRegWrite(phyaddr, 0x1f, 0x24, 0xc007);
	  mtEMiiRegWrite(phyaddr, 0x1f, 0x25, 0x003f);
	  mtEMiiRegWrite(phyaddr, 0x1f, 0x26, 0xc007);
	  mtEMiiRegWrite(phyaddr, 0x1f, 0x27, 0x0037);
	}
	xfi_mac_init();
#endif
	arht_eth_initd = 1;

	return (0);
}

static int arht_miiphyread(
	const char *devname,
	uchar addr,
	uchar reg,
	ushort *val
)
{
	*val = miiStationRead(addr, reg);

	return 0;
}

static int arht_miiphywrite(
	const char *devname,
	uchar addr,
	uchar reg,
	ushort val
)
{
	miiStationWrite(addr, reg, val);

	return 0;
}

static int arht_eth_of_to_plat(struct udevice *dev)
{
	return 0;
}

static int arht_eth_probe(struct udevice *dev)
{
#ifdef CONFIG_CMD_MII
		miiphy_register(dev->name, arht_miiphyread, arht_miiphywrite);
#endif
    return 0;
}

static int arht_eth_remove(struct udevice *dev)
{
    return 0;
}

static const struct arht_soc_data an7552_data = {
	.ana_rgc3 = 0x2028,
	.txd_size = sizeof(struct arht_tx_dma),
	.rxd_size = sizeof(struct arht_tx_dma),
};

static const struct arht_soc_data en7523_data = {
	.ana_rgc3 = 0x2028,
	.txd_size = sizeof(struct arht_tx_dma),
	.rxd_size = sizeof(struct arht_tx_dma),
};

static const struct arht_soc_data an7581_data = {
	.ana_rgc3 = 0x2028,
	.txd_size = sizeof(struct arht_tx_dma),
	.rxd_size = sizeof(struct arht_tx_dma),
};

static const struct arht_soc_data an7583_data = {
	.ana_rgc3 = 0x2028,
	.txd_size = sizeof(struct arht_tx_dma),
	.rxd_size = sizeof(struct arht_tx_dma),
};

static const struct udevice_id arht_eth_ids[] = {
	{ .compatible = "Airoha,en7523-eth", .data = (ulong)&en7523_data },
  	{ .compatible = "airoha,an7552-eth", .data = (ulong)&an7552_data },
	{ .compatible = "airoha,an7581-eth", .data = (ulong)&an7581_data },
	{ .compatible = "airoha,an7583-eth", .data = (ulong)&an7583_data },

};

static const struct eth_ops arht_eth_ops = {
	.start = arht_eth_init,
	.stop = arht_eth_stop,
	.send = arht_eth_send,
	.recv = (int (*)(struct udevice *, int,  uchar **))arht_eth_recv,
};

U_BOOT_DRIVER(arht_eth) = {
	.name = "arht-eth",
	.id = UCLASS_ETH,
	.of_match = arht_eth_ids,
	.of_to_plat = arht_eth_of_to_plat,
	.plat_auto	= sizeof(struct eth_pdata),
	.probe = arht_eth_probe,
	.remove = arht_eth_remove,
	.ops = &arht_eth_ops,
};
