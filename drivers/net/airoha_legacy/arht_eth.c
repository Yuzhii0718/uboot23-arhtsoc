/***************************************************************
Copyright Statement:

This software/firmware and related documentation (EcoNet Software) 
are protected under relevant copyright laws. The information contained herein 
is confidential and proprietary to EcoNet (HK) Limited (EcoNet) and/or 
its licensors. Without the prior written permission of EcoNet and/or its licensors, 
any reproduction, modification, use or disclosure of EcoNet Software, and 
information contained herein, in whole or in part, shall be strictly prohibited.

Airoha Technology Corp. ALL RIGHTS RESERVED.

BY OPENING OR USING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY 
ACKNOWLEDGES AND AGREES THAT THE SOFTWARE/FIRMWARE AND ITS 
DOCUMENTATIONS (ECONET SOFTWARE) RECEIVED FROM ECONET 
AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON AN AS IS 
BASIS ONLY. ECONET EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES, 
WHETHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED 
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, 
OR NON-INFRINGEMENT. NOR DOES ECONET PROVIDE ANY WARRANTY 
WHATSOEVER WITH RESPECT TO THE SOFTWARE OF ANY THIRD PARTIES WHICH 
MAY BE USED BY, INCORPORATED IN, OR SUPPLIED WITH THE ECONET SOFTWARE. 
RECEIVER AGREES TO LOOK ONLY TO SUCH THIRD PARTIES FOR ANY AND ALL 
WARRANTY CLAIMS RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES 
THAT IT IS RECEIVERS SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD 
PARTY ALL PROPER LICENSES CONTAINED IN ECONET SOFTWARE.

ECONET SHALL NOT BE RESPONSIBLE FOR ANY ECONET SOFTWARE RELEASES 
MADE TO RECEIVERS SPECIFICATION OR CONFORMING TO A PARTICULAR 
STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND 
ECONET'S ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE ECONET 
SOFTWARE RELEASED HEREUNDER SHALL BE, AT ECONET'S SOLE OPTION, TO 
REVISE OR REPLACE THE ECONET SOFTWARE AT ISSUE OR REFUND ANY SOFTWARE 
LICENSE FEES OR SERVICE CHARGES PAID BY RECEIVER TO ECONET FOR SUCH 
ECONET SOFTWARE.

*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*
***************************************************************/
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
static QDMA_Private_T DummygpQdmaPriv;
QDMA_Private_T *gpQdmaPriv = &DummygpQdmaPriv;
static uint DummydscpInfoAddr[DESC_INFO_SIZE];
static macAdapter_t DummyAdapter;
macAdapter_t *mac_p = &DummyAdapter;
uint8 use_ext_switch;


#ifndef CONFIG_SYS_CACHELINE_SIZE
#define CONFIG_SYS_CACHELINE_SIZE	64
#endif	/* CONFIG_SYS_CACHELINE_SIZE */


static macMemPool_t enetMemPool __attribute__ ((__aligned__(CONFIG_SYS_CACHELINE_SIZE)));

static int arht_eth_initd = 0;


extern void pause(int ms);
extern int eth_phy_init(macAdapter_t *mac_p);
static int   arht_eth_init(struct udevice* dev);
static int   arht_eth_send(struct udevice* dev, void *packet, int length);
static int   arht_eth_recv(struct udevice* dev);
void detect_switch(void);
int qdma_bm_dump_dscp(void);

int if_tftp_start = 0 ;
#define load_addr 81800000
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



void miiStationWrite(uint32 enetPhyAddr, uint32 phyReg, uint32 miiData)
{
	uint32 reg;
	uint32 cnt=10000;

	do {
		reg=read_reg_word (GSW_CFG_PIAC);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_ST_START << MDIO_ST_SHIFT) | (MDIO_CMD_WRITE<<MDIO_CMD_SHIFT) | 
		(enetPhyAddr << MDIO_PHY_ADDR_SHIFT) | (phyReg << MDIO_REG_ADDR_SHIFT) | 
		(miiData & MDIO_RW_DATA);
	write_reg_word (GSW_CFG_PIAC, reg);

	cnt = 10000;
	do {
		reg=read_reg_word (GSW_CFG_PIAC);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));
}

uint32 miiStationRead(uint32 enetPhyAddr, uint32 phyReg)
{
	uint32 reg;
	uint32 cnt=10000;

	do {
		reg=read_reg_word (GSW_CFG_PIAC);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));

	reg = PHY_ACS_ST | (MDIO_ST_START << MDIO_ST_SHIFT) | (MDIO_CMD_READ<<MDIO_CMD_SHIFT) | 
		(enetPhyAddr << MDIO_PHY_ADDR_SHIFT) | (phyReg << MDIO_REG_ADDR_SHIFT);
	write_reg_word (GSW_CFG_PIAC, reg);

	cnt = 10000;
	do {
		reg=read_reg_word (GSW_CFG_PIAC);
		cnt--;
	} while ((reg & PHY_ACS_ST) && (cnt != 0));
	reg = reg & MDIO_RW_DATA;

	return reg;
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
static int qdma_bm_push_tx_dscp(struct QDMA_DscpInfo_S *diPtr, int ringIdx) 
{
	if(diPtr->next != NULL) {
		QDMA_ERR("The TX DSCP is not return from tx used pool\n") ;
		return -1 ;
	}

	diPtr->skb = NULL ;
	if(!gpQdmaPriv->txHeadPtr) {
		gpQdmaPriv->txHeadPtr = diPtr ;
		gpQdmaPriv->txTailPtr = diPtr ;
	} else {
		gpQdmaPriv->txTailPtr->next = diPtr ;
		gpQdmaPriv->txTailPtr = gpQdmaPriv->txTailPtr->next ;
	}
	
	return 0 ;
}


/******************************************************************************
******************************************************************************/
static struct QDMA_DscpInfo_S *qdma_bm_pop_tx_dscp(void)
{
	struct QDMA_DscpInfo_S *diPtr ;
	
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
	
	return diPtr ;
}

/******************************************************************************
 Packet Receive
******************************************************************************/
/******************************************************************************
******************************************************************************/
static void qdma_bm_add_rx_dscp(struct QDMA_DscpInfo_S *diPtr) 
{
	if(!gpQdmaPriv->rxStartPtr) {
		gpQdmaPriv->rxStartPtr = diPtr ;
		diPtr->next = gpQdmaPriv->rxStartPtr ;
	} else {
		diPtr->next = gpQdmaPriv->rxStartPtr->next;
		gpQdmaPriv->rxStartPtr->next = diPtr;
		gpQdmaPriv->rxStartPtr = diPtr;
	}
}


static struct QDMA_DscpInfo_S *qdma_bm_get_unused_rx_dscp(void)
{
	struct QDMA_DscpInfo_S *diPtr = NULL ;

	
	if(gpQdmaPriv->rxStartPtr) {
		if(!gpQdmaPriv->rxEndPtr) {
			diPtr = gpQdmaPriv->rxStartPtr ;
			gpQdmaPriv->rxEndPtr = diPtr ;
		} else if(gpQdmaPriv->rxEndPtr->next != gpQdmaPriv->rxStartPtr) {
			diPtr = gpQdmaPriv->rxEndPtr->next ;
			gpQdmaPriv->rxEndPtr = diPtr ; 
		}
	} 

	return diPtr ;
}


int qdma_bm_transmit_done(int amount) 
{
	QDMA_DMA_DSCP_T txDscp ;
	int ret = 0 ;
	struct QDMA_DscpInfo_S *diPtr ;
	uint base = gpQdmaPriv->csrBaseAddr ;
	uint entryLen, headIdx, irqValue=0, irqDepth=CONFIG_IRQ_DEPTH ;
	uint *irqPtr ;
	int i=0, j=0, idx=0, ringIdx=0 ;
	uint RETRY=3 ;
	uint irqStatus ;
	void *msgPtr;

	irqStatus = qdmaGetIrqStatus(base) ;
	headIdx = (irqStatus & IRQ_STATUS_HEAD_IDX_MASK) >> IRQ_STATUS_HEAD_IDX_SHIFT ;
	entryLen = (irqStatus & IRQ_STATUS_ENTRY_LEN_MASK) >> IRQ_STATUS_ENTRY_LEN_SHIFT ;
	if(entryLen == 0) {
		QDMA_MSG(DBG_WARN, "qdma_bm_transmit_done-111111\n") ;
		goto out2 ;
	}

	entryLen = (amount && amount<entryLen) ? amount : entryLen ;
	for(i=0 ; i<entryLen ; i++) {
		irqPtr = (uint *)gpQdmaPriv->irqQueueAddr + ((headIdx+i)%irqDepth) ;
		
		RETRY = 3 ;
		while(RETRY--) {
			irqValue = *irqPtr ;
			if(irqValue == CONFIG_IRQ_DEF_VALUE) {
				QDMA_ERR("There is no data available in IRQ queue. irq value:%.8x, irq ptr:%.8x TIMEs:%d\n", (uint)irqValue, (uint)irqPtr, RETRY) ;
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
			
		diPtr = (struct QDMA_DscpInfo_S *)gpQdmaPriv->txBaseAddr + idx ;
		if(diPtr->dscpIdx!=idx || diPtr->next!=NULL) {
			QDMA_ERR("The content of the TX DSCP_INFO(%.8x) is incorrect. ENTRY_LEN:%d, HEAD_IDX:%d, IRQ_VALUE:%.8x.\n", (uint)diPtr, entryLen, headIdx, irqValue) ;
			gpQdmaPriv->counters.txDscpIncorrect++ ;
			ret = -1;
			continue ;
		}
		
		msgPtr = (void *)txDscp.msg ;
		if(msgPtr)
			memset(msgPtr, 0, QDMA_TX_DSCP_MSG_LENS);
		free_skb(diPtr->skb);
		
		qdma_bm_push_tx_dscp(diPtr, 0) ;

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
	uint base = gpQdmaPriv->csrBaseAddr ;
	int ret = 0 ;
	unsigned long   addr, len;

	if(skb==NULL){
		printf("The input arguments are wrong, skb is NULL\n"); 
        return -1;
	}

	if(skb->len<=0 || skb->len>CONFIG_MAX_PKT_LENS) 
	{
		printf("The input arguments are wrong, skb:%.8x, skbLen:%d.\n", (uint)skb, skb->len) ; 
        return -1;
	}

	/* recycle TX DSCP when send packets in tx polling mode */
	
	if(qdmaGetIrqEntryLen(base) >= QDMA_TX_THRESHOLD) { 
		qdma_bm_transmit_done(0) ;
	}

	/* Get unused TX DSCP from TX unused DSCP link list */	
	pNewDscpInfo = qdma_bm_pop_tx_dscp() ;
	if(pNewDscpInfo == NULL) {
		gpQdmaPriv->counters.noTxDscps++ ;
		printf("pNewDscpInfo is NULL\n") ; 
		return -1 ;
	}
	pTxDscp = gpQdmaPriv->txUsingPtr->dscpPtr ;
	pTxDscp->msg[0] = msg0;
	pTxDscp->msg[1] = msg1;
	pTxDscp->next_idx = pNewDscpInfo->dscpIdx ;
	pTxDscp->pkt_addr = K1_TO_PHY(skb->data) ;
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
	uint base = gpQdmaPriv->csrBaseAddr ;
	int ret = 0 ;
	unsigned long	addr, len;

	pNewDscpInfo = qdma_bm_get_unused_rx_dscp() ;
	if(pNewDscpInfo == NULL) {
		QDMA_ERR("There is not any free RX DSCP.\n") ; 
		gpQdmaPriv->counters.noRxDscps++ ;
		return -1 ;
	}
	
#ifdef CONFIG_RX_2B_OFFSET
	QDMA_MSG(DBG_MSG, "Adjust the skb->tail location for net IP alignment\n") ;
	if(((uint)skb->data & 7) != 0) {
		//prom_printf("address not align 8\n");
	}
	skb_reserve(skb, 2) ;
	dmaPktAddr = K1_TO_PHY((void *)((uint)skb->data - 2));
#else
	dmaPktAddr = K1_TO_PHY((void *)((uint)skb->data));
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

int qdma_has_free_rxdscp(void)
{
	return (gpQdmaPriv->rxEndPtr->next != gpQdmaPriv->rxStartPtr) ;
}


int ip_rcv_packet(sk_buff *skb)
{  
	struct iphdr *ip_hdr = (struct iphdr *)(skb->data);
	struct ip_udp_hdr *ip_udp_hdr = (struct ip_udp_hdr *)(skb->data);
	int templen = skb->len;
	char *mac = (char *)(ip_hdr) - sizeof(struct ethhdr);

	/* record ip header pointer */
	skb->ip_hdr = ip_hdr;

	if (ip_hdr->protocol == UDP_){
		
		skb->len = ntohs(ip_hdr->tot_len);
		skb_pull(skb, sizeof(struct iphdr));

		if (!multiupgrade_process(skb, mac))
			return 0;
		skb_push(skb, sizeof(struct iphdr));
		skb->len = templen;
		
	}
	
nonHandle:

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
			net_process_received_packet(dscpInfo.skb->data, dscpInfo.skb->len);
		}
		free_skb(dscpInfo.skb);
next:
		if (qdma_has_free_rxdscp())
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
done:
	if_tftp_start = 0;
	net_boot_file_size = 0;
	net_set_state(NETLOOP_CONTINUE);
	net_set_udp_handler(NULL);
	net_set_arp_handler(NULL);
	net_set_timeout_handler(0, NULL);

	return 0;

}

static int qdma_bm_dscp_init(void)
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

	base = gpQdmaPriv->csrBaseAddr ;
	txDscpNum = gpQdmaPriv->txDscpNum ;
	rxDscpNum = gpQdmaPriv->rxDscpNum ;
	hwDscpNum = gpQdmaPriv->hwFwdDscpNum ;
	irqDepth = gpQdmaPriv->irqDepth ;
	hwFwdPktLen = gpQdmaPriv->hwPktSize ;	

	
	/******************************************
	* Allocate descriptor DMA memory          *
	*******************************************/
	dscpBaseAddr = K0_TO_K1((uint)&mac_p->macMemPool_p->descrPool[0]) ;
	dscpDmaAddr = K1_TO_PHY(dscpBaseAddr);

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

	/******************************************
	* Allocate memory for IRQ queue           *
	******************************************/
	if(irqDepth) {

		gpQdmaPriv->irqQueueAddr = K0_TO_K1((dscpBaseAddr + sizeof(QDMA_DMA_DSCP_T)*(txDscpNum + rxDscpNum))) ;
		irqDmaAddr = K1_TO_PHY(gpQdmaPriv->irqQueueAddr);

		memset((void *)gpQdmaPriv->irqQueueAddr, CONFIG_IRQ_DEF_VALUE, irqDepth<<2) ;

		addr = (unsigned long )&mac_p->macMemPool_p->descrPool[DESC_TOTAL_SIZE] & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		len = ((CONFIG_IRQ_DEPTH<<2)+ 2 * CONFIG_SYS_CACHELINE_SIZE) & ~(CONFIG_SYS_CACHELINE_SIZE - 1);
		flush_dcache_range(addr, addr + len );
		
		/* Setting the IRQ queue information to QDMA register */
		qdmaSetIrqBase(base, irqDmaAddr) ;
		qdmaSetIrqDepth(base, irqDepth) ;
	}
	/***************************************************
	* Allocate memory for TX/RX DSCP Information node  *
	****************************************************/
	gpQdmaPriv->dscpInfoAddr = &DummydscpInfoAddr ;

	memset(gpQdmaPriv->dscpInfoAddr, 0, DESC_INFO_SIZE);

	gpQdmaPriv->txBaseAddr = gpQdmaPriv->dscpInfoAddr;
	gpQdmaPriv->rxBaseAddr = gpQdmaPriv->dscpInfoAddr + sizeof(struct QDMA_DscpInfo_S)*txDscpNum;

	//Create unused tx descriptor link list and using rx descriptor ring
	for(i=0 ; i<(txDscpNum + rxDscpNum) ; i++)
	{
		diPtr = (struct QDMA_DscpInfo_S *)gpQdmaPriv->dscpInfoAddr + i ;
		diPtr->dscpPtr = (QDMA_DMA_DSCP_T *)dscpBaseAddr + i ;
		
		if(i < txDscpNum) {
			diPtr->dscpIdx = i ;
			diPtr->next = NULL ;
			qdma_bm_push_tx_dscp(diPtr, 0) ;
		} else  {
			diPtr->dscpIdx = i - txDscpNum ;
			diPtr->next = NULL ;
			qdma_bm_add_rx_dscp(diPtr) ;
		}
	}	

	/***************************************************
	* Initialization first DSCP for Tx0 DMA              *
	****************************************************/
	diPtr = qdma_bm_pop_tx_dscp() ;
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
	diPtr = qdma_bm_get_unused_rx_dscp() ;
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
	} while(qdma_has_free_rxdscp()) ;	

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
	
	return 0 ;
}


/******************************************************************************
******************************************************************************/
int qdma_dev_init(void) 
{
	uint base = gpQdmaPriv->csrBaseAddr ;
	uint i=0, glbCfg=0;
	uint int_Enable1=0, int_Enable2=0 ;
	

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

	glbCfg |= (PREFER_TX1_FWD_TX0<<GLB_CFG_DMA_PREFERENCE_SHIFT)&GLB_CFG_DMA_PREFERENCE_MASK;

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

int  qdma_init(void){
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
	if((ret = qdma_bm_dscp_init()) != 0) 
	{
		QDMA_ERR("QDMA DSCP initialization failed.\n") ;
		return ret ;
	}
	/***************************************************
	* QDMA device initialization                       *
	****************************************************/
	if((ret = qdma_dev_init()) != 0) {
		QDMA_ERR("QDMA hardware device initialization failed.\n") ;
		return ret ;
	}
	qdmaEnableTxDma(gpQdmaPriv->csrBaseAddr) ;
	qdmaEnableRxDma(gpQdmaPriv->csrBaseAddr) ;
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


struct serdes_fwd_info fwd_info[SERDES_INTF_MAX] = 
{
    {GDM_P_GDMA4,12},
    {GDM_P_GDMA4,12},
    {GDM_P_GDMA3,10},
    {GDM_P_GDMA3,11},
    {0,0},        
};

static int arht_eth_check_serdes(unsigned int* p_fport,unsigned int* p_chn)
{	

    return 0;
}

static int arht_eth_send(struct udevice* dev, void *packet, int length)
{
	sk_buff *skb_temp;
    unsigned int fport;
    unsigned int chn;
    
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
    if(arht_eth_check_serdes(&fport,&chn))
    {
	    ethTxMsg.raw.fport = fport;
	    ethTxMsg.raw.channel = chn;
    }
    else
	{
	    ethTxMsg.raw.fport = GDM_P_GDMA1;
	    ethTxMsg.raw.channel = 0;
    }
	ethTxMsg.raw.queue =0;
	ethTxMsg.raw.nboq = 0;
	ethTxMsg.raw.mtr_g = 0x7f; /*not use any meter ratelimit*/

	error = qdma_bm_transmit_packet(skb_temp, 0,ethTxMsg.msg[0],ethTxMsg.msg[1]) ;

		if(error){
			free_skb(skb_temp);
			return 0;
		 }

	return 0;
}


static int arht_eth_recv(struct udevice* dev)
{
	uint intStatus1=0, intStatus2=0;
	uint base = gpQdmaPriv->csrBaseAddr ;
    int pktlen=0;
		intStatus1 = qdmaGetIntStatus1(base) & qdmaGetIntMask(base, QDMA_INT1_ENABLE1) ;
		intStatus2 = qdmaGetIntStatus2(base) & qdmaGetIntMask(base, QDMA_INT1_ENABLE2) ;

		if((intStatus2 & INT_STATUS_RX_DONE)) {
			qdmaClearIntStatus2(base, (intStatus2 & INT_STATUS_RX_DONE)) ;

	    #if defined(DRAM_PROTECT_TEST)
			nmi_mem_protect_test();
	    #endif
		
			pktlen = qdma_bm_receive_packets(16,0);
		}
		if(intStatus1 & INT_STATUS_QDMA_FAULT) {
			QDMA_MSG(DBG_MSG, "err qdma INT_STATUS=%08lx\n", intStatus1);
		}

		return pktlen;

}

void arht_eth_stop(struct udevice* dev)
{

}

static int arht_eth_init(struct udevice* dev)
{	
	unsigned iomux = 0, iomux_ori = 0;

	if(arht_eth_initd == 0 ){
		int i;
		uint32 reg_val = 0;
		struct eth_pdata *pdata = dev_get_plat(dev);

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

		/* restore Ether PHY LED iomux */
		write_reg_word(IOMUX_CONTROL1, iomux_ori);
		pause(100);
		skb_init();
		qdma_init(); //seems OK
#ifdef INCLUDE_BOOT_AGINETCONFIG
		macGetMacAddr(mac_p, net_ethaddr);
#else /* !INCLUDE_BOOT_AGINETCONFIG */
		macGetMacAddr(mac_p, mac_addr);
#endif/* INCLUDE_BOOT_AGINETCONFIG */
		macDrvRegInit(mac_p);
		pause(100);

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

		write_reg_word(GSW_MFC, 0xffffffe0);
		write_reg_word(GSW_APC, 0x08100810);

		gswPbusWrite(0x7810,0x00010022);

	}
	arht_eth_initd = 1;

	return (0);
}

static int arht_eth_of_to_plat(struct udevice *dev)
{
	return 0;
}

static int arht_eth_probe(struct udevice *dev)
{
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

static const struct udevice_id arht_eth_ids[] = {
	{ .compatible = "Airoha,en7523-eth", .data = (ulong)&en7523_data },
  	{ .compatible = "airoha,an7552-eth", .data = (ulong)&an7552_data },
	{ .compatible = "airoha,an7581-eth", .data = (ulong)&an7581_data },
};

static const struct eth_ops arht_eth_ops = {
	.start = arht_eth_init,
	.stop = arht_eth_stop,
	.send = arht_eth_send,
	.recv = arht_eth_recv,
};

U_BOOT_DRIVER(arht_eth) = {
	.name = "arht-eth",
	.id = UCLASS_ETH,
	.of_match = arht_eth_ids,
	.of_to_plat = arht_eth_of_to_plat,
	.plat_auto	= sizeof(struct eth_pdata),
	.probe = arht_eth_init,
	.remove = arht_eth_remove,
	.ops = &arht_eth_ops,
};


