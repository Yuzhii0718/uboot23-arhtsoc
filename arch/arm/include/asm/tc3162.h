/*
** $Id$
*/
/************************************************************************
 *
 *	Copyright (C) 2003 Trendchip Technologies, Corp.
 *	All Rights Reserved.
 *
 * Trendchip Confidential; Need to Know only.
 * Protected as an unpublished work.
 *
 * The computer program listings, specifications and documentation
 * herein are the property of Trendchip Technologies, Co. and shall
 * not be reproduced, copied, disclosed, or used in whole or in part
 * for any reason without the prior express written permission of
 * Trendchip Technologeis, Co.
 *
 *************************************************************************/
/*
** $Log: tc3162.h,v $
** Revision 1.9  2011/10/12 08:16:41  lino
** add RT63365 ASIC support
**
** Revision 1.8  2011/07/07 07:57:35  shnwind
** RT63260 & RT63260 auto_bench support
**
** Revision 1.7  2011/06/03 02:44:11  lino
** add RT65168 support
**
** Revision 1.6  2011/02/26 04:20:20  shnwind
** update new sd ram setting for 3162u
**
** Revision 1.5  2010/09/25 10:54:19  here
** [Bug fix]Modify the SDRAM Driving Strength Setting at TC3162U platform.
**
** Revision 1.4  2010/06/21 04:17:34  lino
** add tc3182 support, new toolchain doesn't generate floating point functions
**
** Revision 1.3  2010/06/14 02:28:49  lino
** add tc3182 fpga support
**
** Revision 1.2  2010/06/05 12:26:32  lino
** add tc3182 support
**
** Revision 1.1.1.1  2010/04/09 09:31:11  feiyan
** New TC Linux Make Flow Trunk
**
** Revision 1.2  2010/01/07 12:24:27  lino
** add PLL control register define for TC3162U
**
** Revision 1.1.1.1  2009/12/17 01:47:28  josephxu
** 20091217, from Hinchu ,with VoIP
**
** Revision 1.2  2007/10/23 08:43:38  ian
** Add support for L4/P4 and SPI Flash
**
** Revision 1.1.1.1  2007/04/12 09:42:40  ian
** TCLinuxTurnkey2007
**
** Revision 1.1.1.1  2005/11/02 05:44:57  lino
** no message
**
** Revision 1.3  2004/09/01 13:15:47  lino
** fixed when pc shutdown, system will reboot
**
** Revision 1.2  2004/08/27 12:16:37  lino
** change SYS_HCLK to 96Mhz
**
** Revision 1.1  2004/07/02 08:03:04  lino
** tc3160 and tc3162 code merge
**
*/

#ifndef _TC3162_H_
#define _TC3162_H_




#include <config.h>
#include <asm/ecnt_scu.h>
#include <asm/types.h>

#define prom_printf				printf
#define mb						dmb

typedef signed long int int32;          /* 32-bit signed integer        */
typedef __s32 sint31;         /* 32-bit signed integer        */
typedef __u32 uint32;       /* 32-bit unsigned integer      */

typedef __s16 sint15;            /* 16-bit signed integer        */
typedef __s16 int16;                         /* 16-bit signed integer        */
typedef __u16 uint16;          /* 16-bit unsigned integer      */

typedef __s8 sint7;              /* 8-bit signed integer         */
typedef __u8 uint8;            /* 8-bit unsigned integer       */

#ifndef VPint
#define VPint			*(volatile __u32 *)
#endif
#ifndef VPshort
#define VPshort			*(volatile __u16 *)
#endif
#ifndef VPchar
#define VPchar			*(volatile __u8 *)
#endif

#define read_reg_word(reg) 				VPint(reg)


static inline uint32 regRead32(uint32 reg)		\
{						  	\
	return VPint(reg);			  	\
}		
static inline void regWrite32(uint32 reg, uint32 vlaue)	\
{                                                	\
        VPint(reg) = vlaue;                      	\
}

static inline unsigned long int regReadPhy32(uint32 reg)	\
{       						\
	uint32 tmp;					\
	tmp = VPint((reg & 0xf) + 0x1faf01e0);   	\
	tmp = VPint(reg);				\
        return tmp;             	           	\
}

static inline uint32 GET_HIR(void)
{
	return ((VPint(0x1fb00064) >> 16) & 0xFFFF);
}

static inline uint32 GET_NP_SCU_EMMC(void)
{
	return ((VPint(0x1fb000b8) & BOOT_TRAP_CONF_DEC_EMMC_MASK) >> BOOT_TRAP_CONF_DEC_EMMC_OFFSET);
}

#ifdef TCSUPPORT_UBOOT_64BIT
/**
 * struct arm_smccc_res - Result from SMC/HVC call
 * @a0-a3 result values from registers 0 to 3
 */
#include <linux/arm-smccc.h>
#endif

#define isEN7526c	(((VPint(0x1fb00064)&0xffff0000))==0x00080000)
#define isEN751221 	((((VPint(0x1fb00064)&0xffff0000))==0x00070000) || isEN7526c)
#define isEN7528	(((VPint(0x1fb00064)&0xffff0000))==0x000B0000)
#define isEN751627	((((VPint(0x1fb00064)&0xffff0000))==0x00090000) || isEN7528)
#define isEN7523 	(((VPint(0x1fb00064)&0xffff0000))==0x000C0000)
#define isEN7580 	((((VPint(0x1fb00064)&0xffff0000))==0x000A0000) || isEN7523)
#define isEN7581 	(((VPint(0x1fb00064)&0xffff0000))==0x000E0000)
#define isAN7583	(((VPint(0x1fb00064)&0xffff0000))==0x00100000)

#define isTC3162U ((((unsigned char)(regRead32(0x1fb0008c)>>12)&0xff)==0x10)&&(((regRead32(0x1fb00064)&0xffffffff))==0x00000000)?1:0)
#define isRT63260 ((((unsigned char)(regRead32(0x1fb0008c)>>12)&0xff)==0x20)&&(((regRead32(0x1fb00064)&0xffffffff))==0x00000000)?1:0)



// #define isFPGA				GET_IS_FPGA
#define isFPGA  0


#define EFUSE_VERIFY_DATA0 (0x1fbF8214)
#define EFUSE_VERIFY_DATA1 (0x1fbF8218)
#define EFUSE_PKG_INGORE_BITE0_MASK          (0x3C)
#define EFUSE_PKG_SEL_MASK      (0x3)
#define EFUSE_PKG_MASK_7516          (0xC0000)
#define EFUSE_REMARK_BIT_7516        (1<<0)
#define EFUSE_PKG_REMARK_SHITF_7516 2
#define EFUSE_PKG_MASK          (0x3F)
#define EFUSE_REMARK_BIT        (1<<6)
#define EFUSE_PKG_REMARK_SHITF 7

#define EFUSE_EN7527H	(0x0)
#define EFUSE_EN7527G	(0x0)
#define EFUSE_EN7516G	(0x80000)
#define EFUSE_EN7526F   (0x0)
#define EFUSE_EN7521F   (0x10)
#define EFUSE_EN7521S   (0x20)
#define EFUSE_EN7512    (0x4)
#define EFUSE_EN7526D   (0x1)
#define EFUSE_EN7526FT   (0x11)
#define EFUSE_EN7513    (0x5)
#define EFUSE_EN7526G   (0x2)
#define EFUSE_EN7521G   (0x12)
#define EFUSE_EN7513G   (0x6)
#define EFUSE_EN7586    (0xA)
#define EFUSE_EN7586    (0xA)

/* EN7528 */
#define isEN7528HU	(GET_PACKAGE_ID == 0x0)
#define isEN7528DU	(GET_PACKAGE_ID == 0x1)
#define isEN7561DU	(GET_PACKAGE_ID == 0x2)
#define isEN7561DN	(GET_PACKAGE_ID == 0x3)

/* EN7580 */
#define isEN7580GT	(GET_PACKAGE_ID == 0x0)
#define isEN7580ST	(GET_PACKAGE_ID == 0x1)
#define isEN7580GAT	(GET_PACKAGE_ID == 0x2)
#if defined(TCSUPPORT_CPU_EN7581)

/* AN7581 */
#define isAN7581GT		(isEN7581 && (GET_PACKAGE_ID == 0x0)) //(AN7581GT-AN7581_BASE)
#define isAN7566GT		(isEN7581 && (GET_PACKAGE_ID == 0x1))//(AN7566GT-AN7581_BASE)
#define isAN7581PT		(isEN7581 && (GET_PACKAGE_ID == 0x2)) //(AN7581PT-AN7581_BASE)
#define isAN7581ST		(isEN7581 && (GET_PACKAGE_ID == 0x3))
#define isAN7551PT		(isEN7581 && (GET_PACKAGE_ID == 0x4))      /* 2'b 0_0100 */
#define isAN7581CT		(isEN7581 && (GET_PACKAGE_ID == 0x5))
#define isAN7581DT		(isEN7581 && (GET_PACKAGE_ID == 0x6))
#define isAN7581FG		(isEN7581 && (GET_PACKAGE_ID == 0x7))
#define isAN7581FP		(isEN7581 && (GET_PACKAGE_ID == 0x8))      /* 2'b 0_1000 */
#define isAN7581FD		(isEN7581 && (GET_PACKAGE_ID == 0x9))
#define isAN7551GT		(isEN7581 && (GET_PACKAGE_ID == 0xa))
#define isAN7566PT		(isEN7581 && (GET_PACKAGE_ID == 0xb))
#define isAN7581IT		(isEN7581 && (GET_PACKAGE_ID == 0xc))
/* AN7583 */
#define isAN7583GT		(isAN7583 && (GET_PACKAGE_ID == 0x0))
#define isAN7583GIT		(isAN7583 && (GET_PACKAGE_ID == 0x1))
#define isAN7583CT		(isAN7583 && (GET_PACKAGE_ID == 0x2))
#define isAN7583DT		(isAN7583 && (GET_PACKAGE_ID == 0x3))
#define isAN7583ST		(isAN7583 && (GET_PACKAGE_ID == 0x5))
#define isAN9510GT		(isAN7583 && (GET_PACKAGE_ID == 0x6))
#define isAN7553GT		(isAN7583 && (GET_PACKAGE_ID == 0x8))
#define isAN7553CT		(isAN7583 && (GET_PACKAGE_ID == 0x9))
#define isAN7567GT		(isAN7583 && (GET_PACKAGE_ID == 0xa))
#define isAN7567CT		(isAN7583 && (GET_PACKAGE_ID == 0xb))
#define isAN7583ET		(isAN7583 && (GET_PACKAGE_ID == 0xc))
#define isAN7583EIT		(isAN7583 && (GET_PACKAGE_ID == 0xd))
#define isAN7583FG		(isAN7583 && (GET_PACKAGE_ID == 0x10))
#define isAN7583FP		(isAN7583 && (GET_PACKAGE_ID == 0x12))
#define isAN7583FD		(isAN7583 && (GET_PACKAGE_ID == 0x13))
#define isAN7583FS		(isAN7583 && (GET_PACKAGE_ID == 0x15))
#define isAN7583FF		(isAN7583 && (GET_PACKAGE_ID == 0x16))
#endif

#define isLQFP	 (isEN751221 && ( (regRead32(EFUSE_VERIFY_DATA0)&EFUSE_REMARK_BIT)? \
							(((regRead32(EFUSE_VERIFY_DATA0)>>EFUSE_PKG_REMARK_SHITF)& EFUSE_PKG_SEL_MASK)== 0): \
							((regRead32(EFUSE_VERIFY_DATA0)&EFUSE_PKG_SEL_MASK)==0)))
#define EFUSE_DDR3_BIT (1<<23)
#define EFUSE_DDR3_REMARK_BIT (1<<24)
#define EFUSE_IS_DDR3 ( (regRead32(EFUSE_VERIFY_DATA0)&EFUSE_REMARK_BIT)? \
                        ((regRead32(EFUSE_VERIFY_DATA0)& EFUSE_DDR3_REMARK_BIT)): \
                        ((regRead32(EFUSE_VERIFY_DATA0)& EFUSE_DDR3_BIT)))

#define EFUSE_Fix32MB_BIT (1<<25)
#define EFUSE_Fix32MB_REMARK_BIT (1<<26)
#define EFUSE_Fix32MB ( (regRead32(EFUSE_VERIFY_DATA0)&EFUSE_REMARK_BIT)? \
                        ((regRead32(EFUSE_VERIFY_DATA0)& EFUSE_Fix32MB_REMARK_BIT)): \
                        ((regRead32(EFUSE_VERIFY_DATA0)& EFUSE_Fix32MB_BIT)))


#define REG_SAVE_INFO			(0x1fb00284)
#define GET_REG_SAVE_INFO_POINT	((volatile SYS_GLOBAL_PARM_T *)REG_SAVE_INFO)

#define GET_IS_DDR4						(GET_REG_SAVE_INFO_POINT->raw.isDDR4)
#define SET_IS_DDR4(x)					(GET_IS_DDR4 = (x & 0x1))
#define GET_DRAM_SIZE					((GET_REG_SAVE_INFO_POINT->raw.dram_size) << 4)
#define SET_DRAM_SIZE(x)				((GET_REG_SAVE_INFO_POINT->raw.dram_size) = ((x & 0x3FFF) >> 4))
#define GET_SYS_CLK						(GET_REG_SAVE_INFO_POINT->raw.sys_clk)
#define SET_SYS_CLK(x)					(GET_SYS_CLK = (x & 0x3FF))
#define GET_IS_FPGA						(GET_REG_SAVE_INFO_POINT->raw.isFpga)
#define SET_IS_FPGA(x)					(GET_IS_FPGA = (x & 0x1))
#define GET_IS_SPI_CONTROLLER_ECC		(GET_REG_SAVE_INFO_POINT->raw.isCtrlEcc)
#define SET_IS_SPI_CONTROLLER_ECC(x)	(GET_IS_SPI_CONTROLLER_ECC = (x & 0x1))
#define GET_PACKAGE_ID					(GET_REG_SAVE_INFO_POINT->raw.packageID | (GET_REG_SAVE_INFO_POINT->raw.packageID_ext << 4))
#define SET_PACKAGE_ID(x)				((GET_REG_SAVE_INFO_POINT->raw.packageID = (x & 0xF)); (GET_REG_SAVE_INFO_POINT->raw.packageID_ext) = ((x >> 4) & 0x1))
#define GET_IS_SECURE_MODE				(GET_REG_SAVE_INFO_POINT->raw.isSecureModeEn)
#define SET_IS_SECURE_MODE(x)			(GET_IS_SECURE_MODE = (x & 0x1))
#define GET_IS_SECURE_HWTRAP			(GET_REG_SAVE_INFO_POINT->raw.isSecureHwTrapEn)
#define SET_IS_SECURE_HWTRAP(x)			(GET_IS_SECURE_HWTRAP = (x & 0x1))

#define NOT_NEED_DDR_CALIBRATION 0
#define NEED_DDR_CALIBRATION 1


#define	SYS_HCLK		(GET_SYS_CLK)

#define	SAR_CLK	(SYS_HCLK)/(4.0)		//more accurate if 4.0 not 4

#define ENABLE          1
#define DISABLE         0

#define tc_inb(offset) 			(*(volatile unsigned char *)(offset))
#define tc_inw(offset) 			(*(volatile unsigned short *)(offset))
#define tc_inl(offset) 			(*(volatile unsigned long *)(offset))

#define tc_outb(offset,val)    	(*(volatile unsigned char *)(offset) = val)
#define tc_outw(offset,val)    	(*(volatile unsigned short *)(offset) = val)
#define tc_outl(offset,val)    	(*(volatile unsigned long *)(offset) = val)

/*****************************
 * DMC Module Registers *
 *****************************/

#define CR_DMC_BASE     	0x1fb20000
#define CR_DMC_SRT      	(0x00 | CR_DMC_BASE)
#define CR_DMC_STC      	(0x01 | CR_DMC_BASE)
#define CR_DMC_SAMT      	(0x02 | CR_DMC_BASE)
#define CR_DMC_SCR      	(0x03 | CR_DMC_BASE)

#define CR_DMC_DDR_SR_CNT  	(0x1c | CR_DMC_BASE)
/* RT63165 specific */
#define CR_DMC_DDR_CFG0    	(0x40 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG1    	(0x44 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG2    	(0x48 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG3    	(0x4c | CR_DMC_BASE)
#define CR_DMC_DDR_CFG4    	(0x50 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG8    	(0x60 | CR_DMC_BASE)
#define CR_DMC_DDR_CFG9    	(0x64 | CR_DMC_BASE)

#define CR_DMC_CTL0      	(0x70 | CR_DMC_BASE)
#define CR_DMC_CTL1      	(0x74 | CR_DMC_BASE)
#define CR_DMC_CTL2      	(0x78 | CR_DMC_BASE)
#define CR_DMC_CTL3     	(0x7c | CR_DMC_BASE)
#define CR_DMC_CTL4     	(0x80 | CR_DMC_BASE)

#define CR_DMC_DCSR     	(0xb0 | CR_DMC_BASE)

#define CR_DMC_ISPCFGR     	(0xc0 | CR_DMC_BASE)
#define CR_DMC_DSPCFGR     	(0xc4 | CR_DMC_BASE)
/*****************************
 * GDMA Module Registers *
 *****************************/

#define CR_GDMA_BASE     	0x1fb30000
#define CR_GDMA_DCSA      	(0x00 | CR_GDMA_BASE)
#define CR_GDMA_DCDA      	(0x04 | CR_GDMA_BASE)
#define CR_GDMA_DCBT      	(0x08 | CR_GDMA_BASE)
#define CR_GDMA_DCBL      	(0x0a | CR_GDMA_BASE)
#define CR_GDMA_DCC      	(0x0c | CR_GDMA_BASE)
#define CR_GDMA_DCS      	(0x0e | CR_GDMA_BASE)
#define CR_GDMA_DCKSUM     	(0x10 | CR_GDMA_BASE)

/* RT63365 */
#define CR_GDMA_SA(ch)     	(CR_GDMA_BASE + 0x00 + (ch)*0x10)
#define CR_GDMA_DA(ch)     	(CR_GDMA_BASE + 0x04 + (ch)*0x10)
#define CR_GDMA_CT0(ch)    	(CR_GDMA_BASE + 0x08 + (ch)*0x10)
#define CR_GDMA_CT1(ch)    	(CR_GDMA_BASE + 0x0c + (ch)*0x10)
#define CR_GDMA_UNMASKINT  	(CR_GDMA_BASE + 0x200)
#define CR_GDMA_DONEINT  	(CR_GDMA_BASE + 0x204)
#define CR_GDMA_GCT  		(CR_GDMA_BASE + 0x220)
#define CR_GDMA_REQSTS 		(CR_GDMA_BASE + 0x2a0)
#define CR_GDMA_ACKSTS 		(CR_GDMA_BASE + 0x2a4)
#define CR_GDMA_FINSTS 		(CR_GDMA_BASE + 0x2a8)

/* GDMA1 count define */

#define FE_BASE     			(0x1FB50000)
#define GDMA1_COUNT_BASE 	    (FE_BASE + 0x600)
#define GDMA1_TX_GET_CNT        (GDMA1_COUNT_BASE + 0x00)
#define GDMA1_TX_OK_CNT         (GDMA1_COUNT_BASE + 0x04)
#define GDMA1_TX_DROP_CNT 	    (GDMA1_COUNT_BASE + 0x08)
#define GDMA1_TX_OK_BYTE_CNT    (GDMA1_COUNT_BASE + 0x0c)

#define GDMA1_RX_OK_CNT         (GDMA1_COUNT_BASE + 0x48)
#define GDMA1_RX_FC_DROP_CNT    (GDMA1_COUNT_BASE + 0x4c)
#define GDMA1_RX_RC_DROP_CNT    (GDMA1_COUNT_BASE + 0x50)
#define GDMA1_RX_OVER_DROP_CNT  (GDMA1_COUNT_BASE + 0x54)
#define GDMA1_RX_ERROR_DROP_CNT (GDMA1_COUNT_BASE + 0x58)
#define GDMA1_RX_BYTECNT        (GDMA1_COUNT_BASE + 0x5c)

/* CDMA1 count define */
#define GDMA1_BASE     		(FE_BASE + 0x0500)
#define CDMA1_TX_OK_CNT             (GDMA1_BASE + 0x80)
#define CDMA1_RXCPU_OK_CNT          (GDMA1_BASE + 0x90)
#define CDMA1_RXHWF_OK_CNT          (GDMA1_BASE + 0x94)
#define CDMA1_RXCPU_KA_CNT          (GDMA1_BASE + 0x98)
#define CDMA1_RXCPU_DROP_CNT        (GDMA1_BASE + 0xa0)
#define CDMA1_RXHWF_DROP_CNT        (GDMA1_BASE + 0xa4)
#define CDMA1_RXCPU0_OK_CNT         (GDMA1_BASE + 0xa8)
#define CDMA1_RXCPU1_OK_CNT         (GDMA1_BASE + 0xac)
#define CDMA1_RXHWF_FAST_ALL_CNT    (GDMA1_BASE + 0xb0)
#define CDMA1_RXCPU0_DROP_CNT       (GDMA1_BASE + 0xb4)
#define CDMA1_RXCPU1_DROP_CNT       (GDMA1_BASE + 0xb8)
#define CDMA1_RXHWF_FAST_DROP_CNT   (GDMA1_BASE + 0xbc)



#define GDMA2_BASE     		(FE_BASE + 0x1500)

#define CDMA2_TX_OK_CNT             (GDMA2_BASE + 0x80)
#define CDMA2_RXCPU_OK_CNT          (GDMA2_BASE + 0x90)
#define CDMA2_RXHWF_OK_CNT          (GDMA2_BASE + 0x94)
#define CDMA2_RXCPU_KA_CNT          (GDMA2_BASE + 0x98)
#define CDMA2_RXCPU_DROP_CNT        (GDMA2_BASE + 0xa0)
#define CDMA2_RXHWF_DROP_CNT        (GDMA2_BASE + 0xa4)
#define CDMA2_RXCPU0_OK_CNT         (GDMA2_BASE + 0xa8)
#define CDMA2_RXCPU1_OK_CNT         (GDMA2_BASE + 0xac)
#define CDMA2_RXHWF_FAST_ALL_CNT    (GDMA2_BASE + 0xb0)
#define CDMA2_RXCPU0_DROP_CNT       (GDMA2_BASE + 0xb4)
#define CDMA2_RXCPU1_DROP_CNT       (GDMA2_BASE + 0xb8)
#define CDMA2_RXHWF_FAST_DROP_CNT   (GDMA2_BASE + 0xbc)	

#define PSE_DROP_CNT(i)     (FE_BASE + 0x178 + (i<<2))


#define PSE_FQFC_CFG_STA(i) (FE_BASE + 0x10C + (i<<2))
#define PSE_FQFC_CFG_STA0   (FE_BASE + 0x10C)
#define PSE_FQFC_CFG_STA1   (FE_BASE + 0x110)
#define PSE_FQFC_CFG_STA2   (FE_BASE + 0x114)
#define PSE_FQFC_CFG_STA3   (FE_BASE + 0x118)
#define PSE_FQFC_CFG_STA4   (FE_BASE + 0x11C)
#define PSE_FQFC_CFG_STA5   (FE_BASE + 0x120)

#define PSE_PORT_OQ_STA(i)  (FE_BASE + 0x124 + (i<<2))
#define PSE_PORT_OQ_STA0    (FE_BASE + 0x124)
#define PSE_PORT_OQ_STA1    (FE_BASE + 0x128)
#define PSE_PORT_OQ_STA2    (FE_BASE + 0x12C)
#define PSE_PORT_OQ_STA3    (FE_BASE + 0x130)
#define PSE_PORT_OQ_STA4    (FE_BASE + 0x134)
#define PSE_PORT_OQ_STA5    (FE_BASE + 0x138)

#define PSE_PORT_IQ_STA(i)  (FE_BASE + 0x13C + (i<<2))
#define PSE_PORT_IQ_STA0    (FE_BASE + 0x13C)
#define PSE_PORT_IQ_STA1    (FE_BASE + 0x140)
#define PSE_PORT_IQ_STA2    (FE_BASE + 0x144)
#define PSE_PORT_IQ_STA3    (FE_BASE + 0x148)


#define GDMA2_COUNT_BASE 	(FE_BASE + 0x1600)
#define GDMA2_TX_GETCNT     (GDMA2_COUNT_BASE + 0x00)
#define GDMA2_TX_OKCNT     	(GDMA2_COUNT_BASE + 0x4)
#define GDMA2_TX_DROPCNT   	(GDMA2_COUNT_BASE + 0x8)
#define GDMA2_TX_OKBYTE_CNT   	(GDMA2_COUNT_BASE + 0xc)

#define GDMA2_RX_OKCNT     	(GDMA2_COUNT_BASE + 0x48)
#define GDMA2_RX_FCDROPCNT     	(GDMA2_COUNT_BASE + 0x4c)
#define GDMA2_RX_RCDROPCNT     	(GDMA2_COUNT_BASE + 0x50)

#define GDMA2_RX_OVDROPCNT    (GDMA2_COUNT_BASE + 0x54)
#define GDMA2_RX_ERRDROPCNT    (GDMA2_COUNT_BASE + 0x58)
#define GDMA2_RX_OKBYTECNT    (GDMA2_COUNT_BASE + 0x5c)


/*****************************
 * SPI Module Registers *
 *****************************/

#define CR_SPI_BASE     	0x1fbC0000
#define CR_SPI_CTL      	(0x00 | CR_SPI_BASE)
#define CR_SPI_OPCODE      	(0x04 | CR_SPI_BASE)
#define CR_SPI_DATA      	(0x08 | CR_SPI_BASE)
#define CR_SPI_DATA1      	(0x0C | CR_SPI_BASE)
#define CR_SPI_DATA2      	(0x10 | CR_SPI_BASE)
#define CR_SPI_DATA3      	(0x14 | CR_SPI_BASE)
#define CR_SPI_DATA4      	(0x18 | CR_SPI_BASE)
#define CR_SPI_DATA5      	(0x1C | CR_SPI_BASE)
#define CR_SPI_DATA6      	(0x20 | CR_SPI_BASE)
#define CR_SPI_DATA7      	(0x24 | CR_SPI_BASE)
#define CR_SPI_MODE      	(0x28 | CR_SPI_BASE)
#define CR_SPI_BUFCTL      	(0x2C | CR_SPI_BASE)
#define CR_SPI_QUECTL      	(0x30 | CR_SPI_BASE)
#define CR_SPI_ST	      	(0x34 | CR_SPI_BASE)
#define CR_SPI_CR	      	(0x38 | CR_SPI_BASE)

/*****************************
 * Ethernet Module Registers *
 *****************************/

#define CR_MAC_BASE     	0x1fb50000
#define CR_MAC_ISR      	(0x00 | CR_MAC_BASE)// --- Interrupt Status Register ---
#define CR_MAC_IMR      	(0x04 | CR_MAC_BASE)// --- Interrupt Mask Register ---
#define CR_MAC_MADR  	   	(0x08 | CR_MAC_BASE)// --- MAC Address Register [47:32] ---
#define CR_MAC_LADR     	(0x0c | CR_MAC_BASE)// --- MAC Address Register [31:0] ---
#ifdef TC3162U
#define CR_MAC_EEE    		(0x10 | CR_MAC_BASE)// --- MAC EEE Register
#endif
#if defined(TC3162L2) || defined(TC3262)
// None
#else
  #define CR_MAC_MAHT0         (0x10 | CR_MAC_BASE)// --- MAC Hash Table Address Register [31:0] ---
  #define CR_MAC_MAHT1         (0x14 | CR_MAC_BASE)// --- MAC Hash Table Address Register [31:0] ---
#endif
#define CR_MAC_TXPD     	(0x18 | CR_MAC_BASE)// --- Transmit Poll Demand Register ---
#define CR_MAC_RXPD     	(0x1c | CR_MAC_BASE)// --- Receive Poll Demand Register ---
#define CR_MAC_TXR_BADR 	(0x20 | CR_MAC_BASE)// --- Transmit Ring Base Address Register ---
#define CR_MAC_RXR_BADR 	(0x24 | CR_MAC_BASE)// --- Receive Ring Base Address Register ---
#define CR_MAC_ITC      	(0x28 | CR_MAC_BASE)// --- Interrupt Timer Control Register ---
#if defined(TC3162L2) || defined(TC3262)
  #define CR_MAC_TXR_SIZE  	   (0x2c | CR_MAC_BASE)// --- Transmit Ring Size Register ---
  #define CR_MAC_RXR_SIZE      (0x30 | CR_MAC_BASE)// --- Receive Ring Size Register ---
  #define CR_MAC_RXR_SWIDX     (0x34 | CR_MAC_BASE)// --- Receive Ring Software Index Register ---
#else
#define CR_MAC_APTC     	(0x2c | CR_MAC_BASE)// --- Automatic Polling Timer Control Register ---
#define CR_MAC_DBLAC    	(0x30 | CR_MAC_BASE)// --- DMA Burst Length and Arbitration Control Register ---
#endif
#if defined(TC3162L2) || defined(TC3262)
  #define CR_MAC_TXDESP_SIZE   (0x38 | CR_MAC_BASE)// --- Transmit Descriptor Size Register ---
  #define CR_MAC_RXDESP_SIZE   (0x3c | CR_MAC_BASE)// --- Receive Descriptor Size Register ---
#else
  #define CR_MAC_TXDESCP_ADR   (0x38 | CR_MAC_BASE)// --- Current Transmit Descriptor Address Register ---
  #define CR_MAC_RXDESCP_ADR   (0x3c | CR_MAC_BASE)// --- Current Receive Descriptor Address Register ---
#endif
#if defined(TC3162L2) || defined(TC3262)
  #define CR_MAC_PRIORITY_CFG  (0x50 | CR_MAC_BASE)// --- Priority Configuration Register ---
  #define CR_MAC_VLAN_CFG      (0x54 | CR_MAC_BASE)// --- VLAN Configuration Register ---
  #define CR_MAC_TOS0_CFG      (0x58 | CR_MAC_BASE)// --- TOS 0 Configuration Register ---
  #define CR_MAC_TOS1_CFG      (0x5c | CR_MAC_BASE)// --- TOS 1 Configuration Register ---
  #define CR_MAC_TOS2_CFG      (0x60 | CR_MAC_BASE)// --- TOS 2 Configuration Register ---
  #define CR_MAC_TOS3_CFG      (0x64 | CR_MAC_BASE)// --- TOS 3 Configuration Register ---
  #define CR_MAC_TCP_CFG       (0x68 | CR_MAC_BASE)// --- TCP Configuration Register ---
  #define CR_MAC_SWTAG_CFG     (0x6c | CR_MAC_BASE)// --- Software Tagging Configuration Register ---
  #define CR_MAC_PMBL_CYC_NUM  (0x70 | CR_MAC_BASE)// --- Preamble Cycle Number Register ---
  #define CR_MAC_FCTL_CYC_NUM  (0x74 | CR_MAC_BASE)// --- Flow Control Cycle Number Register ---
  #define CR_MAC_JAM_CYC_NUM   (0x78 | CR_MAC_BASE)// --- JAM Cycle Number Register ---
  #define CR_MAC_DEFER_VAL     (0x7c | CR_MAC_BASE)// --- Defer Value Register ---
  #define CR_MAC_RANDOM_POLY   (0x80 | CR_MAC_BASE)// --- Random Polynomial Register ---
#else
// None
#endif
#define CR_MAC_MACCR    	(0x88 | CR_MAC_BASE)// --- MAC Control Register ---
#define CR_MAC_MACSR    	(0x8c | CR_MAC_BASE)// --- MAC Status Register ---
#define CR_MAC_PHYCR    	(0x90 | CR_MAC_BASE)// --- PHY Control Register ---
#define CR_MAC_PHYWDATA 	(0x94 | CR_MAC_BASE)// --- PHY Write Data Register ---
#define CR_MAC_FCR      	(0x98 | CR_MAC_BASE)// --- Flow Control Register ---
#define CR_MAC_BPR      	(0x9c | CR_MAC_BASE)// --- Back Pressure Register ---
#if defined(TC3162L2) || defined(TC3262)
#define CR_MAC_DESP_IDX        (0xc4 | CR_MAC_BASE)// --- Current Tx/Rx Descriptor Index ---
#endif
#define CR_MAC_WOLCR    	(0xa0 | CR_MAC_BASE)// --- Wake-On-LAN Control Register ---
#define CR_MAC_WOLSR    	(0xa4 | CR_MAC_BASE)// --- Wake-On-LAN Status Register ---
#define CR_MAC_WFCRC    	(0xa8 | CR_MAC_BASE)// --- Wake-up Frame CRC Register ---
#define CR_MAC_WFBM1    	(0xb0 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 1st Double Word Register ---
#define CR_MAC_WFBM2    	(0xb4 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 2nd Double Word Register ---
#define CR_MAC_WFBM3    	(0xb8 | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 3rd Double Word Register ---
#define CR_MAC_WFBM4    	(0xbc | CR_MAC_BASE)// --- Wake-up Frame Byte Mask 4th Double Word Register ---
#define CR_MAC_DMA_FSM  	(0xc8 | CR_MAC_BASE)// --- DMA State Machine
#define CR_MAC_TM       	(0xcc | CR_MAC_BASE)// --- Test Mode Register ---
#define CR_MAC_XMPG_CNT 	(0xdc | CR_MAC_BASE)// --- XM and PG Counter Register ---
#if defined(TC3162L2) || defined(TC3262)
#define CR_MAC_RUNT_TLCC_CNT   (0xe0 | CR_MAC_BASE)// --- Receive Runt and Transmit Late Collision Packet Counter Register ---
#define CR_MAC_RCRC_RLONG_CNT  (0xe4 | CR_MAC_BASE)// --- Receive CRC Error and Long Packet Counter Register ---
#define CR_MAC_RLOSS_RCOL_CNT  (0xe8 | CR_MAC_BASE)// --- Receive Packet Loss and Receive Collision Counter Register ---
#else
#define CR_MAC_RUNT_LCOL_CNT 	(0xe0 | CR_MAC_BASE)// --- Runt and Late Collision Packet Counter Register ---
#define CR_MAC_CRC_LONG_CNT   	(0xe4 | CR_MAC_BASE)// --- CRC and Long Packet Counter Register ---
#define CR_MAC_LOSS_COL_CNT   	(0xe8 | CR_MAC_BASE)// --- Receive Packet Loss and Receive Collision Counter Register ---
#endif
#define CR_MAC_BROADCAST_CNT  	(0xec | CR_MAC_BASE)// --- Receive Broadcast Counter Register ---
#define CR_MAC_MULTICAST_CNT  	(0xf0 | CR_MAC_BASE)// --- Receive Multicast Counter Register ---
#define CR_MAC_RX_CNT   	(0xf4 | CR_MAC_BASE)// --- Receive Good Packet Counter Register ---
#define CR_MAC_TX_CNT   	(0xf8 | CR_MAC_BASE)// --- Transmit Good Packet Counter Register ---

/*************************
 *GSW Registers *
 *************************/
#define GSW_BASE     		0x1FB58000
#define GSW_ARL_BASE     	(GSW_BASE + 0x0000)
#define GSW_BMU_BASE     	(GSW_BASE + 0x1000)
#define GSW_PORT_BASE     	(GSW_BASE + 0x2000)
#define GSW_MAC_BASE     	(GSW_BASE + 0x3000)
#define GSW_MIB_BASE     	(GSW_BASE + 0x4000)
#define GSW_CFG_BASE     	(GSW_BASE + 0x7000)


#define GSW_TX_DROC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x00)
#define GSW_TX_CRC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x04)
#define GSW_TX_UNIC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x08)
#define GSW_TX_MULC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x0c)
#define GSW_TX_BROC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x10)
#define GSW_TX_COLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x14)
#define GSW_TX_SCOLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x18)
#define GSW_TX_MCOLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x1c)
#define GSW_TX_DEFC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x20)
#define GSW_TX_LCOLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x24)
#define GSW_TX_ECOLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x28)
#define GSW_TX_PAUC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x2c)
#define GSW_TX_OCL(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x48)
#define GSW_TX_OCH(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x4c)

#define GSW_RX_DROC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x60)
#define GSW_RX_FILC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x64)
#define GSW_RX_UNIC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x68)
#define GSW_RX_MULC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x6c)
#define GSW_RX_BROC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x70)
#define GSW_RX_ALIGE(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x74)
#define GSW_RX_CRC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x78)
#define GSW_RX_RUNT(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x7c)
#define GSW_RX_FRGE(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x80)
#define GSW_RX_LONG(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x84)
#define GSW_RX_JABE(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x88)
#define GSW_RX_PAUC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0x8c)
#define GSW_RX_OCL(n)    		(GSW_MIB_BASE + (n)*0x100 + 0xa8)
#define GSW_RX_OCH(n)    		(GSW_MIB_BASE + (n)*0x100 + 0xac)
#define GSW_RX_INGC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0xb4)
#define GSW_RX_ARLC(n)    		(GSW_MIB_BASE + (n)*0x100 + 0xb8)


/*************************
 * UART Module Registers *
 *************************/
#define	CR_UART_BASE    	0x1fbF0000
#define	CR_UART_RBR     	(0x00+CR_UART_BASE+0x0)
#define	CR_UART_THR     	(0x00+CR_UART_BASE+0x0)
#define	CR_UART_IER     	(0x04+CR_UART_BASE+0x0)
#define	CR_UART_IIR     	(0x08+CR_UART_BASE+0x0)
#define	CR_UART_FCR     	(0x08+CR_UART_BASE+0x0)
#define	CR_UART_LCR     	(0x0c+CR_UART_BASE+0x0)
#define	CR_UART_MCR     	(0x10+CR_UART_BASE+0x0)
#define	CR_UART_LSR     	(0x14+CR_UART_BASE+0x0)
#define	CR_UART_MSR     	(0x18+CR_UART_BASE+0x0)
#define	CR_UART_SCR     	(0x1c+CR_UART_BASE+0x0)
#define	CR_UART_BRDL    	(0x00+CR_UART_BASE+0x0)
#define	CR_UART_BRDH    	(0x04+CR_UART_BASE+0x0)
#define	CR_UART_WORDA		(0x20+CR_UART_BASE+0x00)
#define	CR_UART_HWORDA		(0x28+CR_UART_BASE+0x00)
#define	CR_UART_MISCC		(0x24+CR_UART_BASE+0x0)
#define	CR_UART_XYD     	(0x2c+CR_UART_BASE)
	
#define	UART_BRD_ACCESS		0x80
#define	UART_XYD_Y          65000
#define	UART_UCLK_115200    0
#define	UART_UCLK_57600     1
#define	UART_UCLK_38400     2
#define	UART_UCLK_28800		3
#define	UART_UCLK_19200		4
#define	UART_UCLK_14400		5
#define	UART_UCLK_9600		6
#define	UART_UCLK_4800		7
#define	UART_UCLK_2400		8
#define	UART_UCLK_1200		9
#define	UART_UCLK_600		10
#define	UART_UCLK_300		11
#define	UART_UCLK_110		12
#define	UART_BRDL			0x03
#define	UART_BRDH			0x00
#define	UART_BRDL_20M		0x01
#define	UART_BRDH_20M		0x00
#define	UART_LCR			0x03
#define	UART_FCR			0x0f
#define	UART_WATERMARK		(0x0<<6)
#define	UART_MCR			0x0
#define	UART_MISCC			0x0
#define	UART_IER			0x01
	
#define	IER_RECEIVED_DATA_INTERRUPT_ENABLE	0x01
#define	IER_THRE_INTERRUPT_ENABLE			0x02
#define	IER_LINE_STATUS_INTERRUPT_ENABLE	0x04
	
#define	IIR_INDICATOR						VPchar(CR_UART_IIR)
#define	IIR_RECEIVED_LINE_STATUS			0x06
#define	IIR_RECEIVED_DATA_AVAILABLE			0x04
#define IIR_RECEIVER_IDLE_TRIGGER			0x0C
#define	IIR_TRANSMITTED_REGISTER_EMPTY		0x02	
#define	LSR_INDICATOR						VPchar(CR_UART_LSR)
#define	LSR_RECEIVED_DATA_READY				0x01
#define	LSR_OVERRUN							0x02
#define	LSR_PARITY_ERROR					0x04
#define	LSR_FRAME_ERROR						0x08
#define	LSR_BREAK							0x10
#define	LSR_THRE							0x20
#define	LSR_THE								0x40
#define	LSR_RFIFO_FLAG						0x80
	
#define uartTxIntOn()		VPchar(CR_UART_IER) |= IER_THRE_INTERRUPT_ENABLE
#define uartTxIntOff()		VPchar(CR_UART_IER) &= ~IER_THRE_INTERRUPT_ENABLE
#define uartRxIntOn()		VPchar(CR_UART_IER) |= IER_RECEIVED_DATA_INTERRUPT_ENABLE
#define	uartRxIntOff()		VPchar(CR_UART_IER) &= ~IER_RECEIVED_DATA_INTERRUPT_ENABLE

/*************************
 * UART2 Module Registers *
 *************************/
#ifdef TCSUPPORT_UART2
#define	CR_UART2_BASE    	0x1fbF0300
#define	CR_UART2_RBR     	(0x00+CR_UART2_BASE)
#define	CR_UART2_THR     	(0x00+CR_UART2_BASE)
#define	CR_UART2_IER     	(0x04+CR_UART2_BASE)
#define	CR_UART2_IIR     	(0x08+CR_UART2_BASE)
#define	CR_UART2_FCR     	(0x08+CR_UART2_BASE)
#define	CR_UART2_LCR     	(0x0c+CR_UART2_BASE)
#define	CR_UART2_MCR     	(0x10+CR_UART2_BASE)
#define	CR_UART2_LSR     	(0x14+CR_UART2_BASE)
#define	CR_UART2_MSR     	(0x18+CR_UART2_BASE)
#define	CR_UART2_SCR     	(0x1c+CR_UART2_BASE)
#define	CR_UART2_BRDL    	(0x00+CR_UART2_BASE)
#define	CR_UART2_BRDH    	(0x04+CR_UART2_BASE)
#define	CR_UART2_WORDA		(0x20+CR_UART2_BASE)
#define	CR_UART2_HWORDA	    (0x28+CR_UART2_BASE)
#define	CR_UART2_MISCC		(0x24+CR_UART2_BASE)
#define	CR_UART2_XYD     	(0x2c+CR_UART2_BASE)

#define	IIR_INDICATOR2		VPchar(CR_UART2_IIR)
#define	LSR_INDICATOR2		VPchar(CR_UART2_LSR)
#endif

/**********************************
 * Interrupt Controller Registers *
 **********************************/
#define CR_INTC_BASE    0x1fb40000                
			// --- Interrupt Type Register ---		  
#define CR_INTC_ITR     (CR_INTC_BASE+0x0000)
			// --- Interrupt Mask Register ---		  
#define CR_INTC_IMR     (CR_INTC_BASE+0x0004)
			// --- Interrupt Pending Register ---	  
#define CR_INTC_IPR     (CR_INTC_BASE+0x0008)
			// --- Interrupt Set Register ---		  
#define CR_INTC_ISR    	(CR_INTC_BASE+0x000c)     
			// --- Interrupt Priority Register 0 ---  
#define CR_INTC_IPR0    (CR_INTC_BASE+0x0010)     
			// --- Interrupt Priority Register 1 ---  
#define CR_INTC_IPR1    (CR_INTC_BASE+0x0014)     
			// --- Interrupt Priority Register 2 ---  
#define CR_INTC_IPR2    (CR_INTC_BASE+0x0018)     
			// --- Interrupt Priority Register 3 ---  
#define CR_INTC_IPR3    (CR_INTC_BASE+0x001c)     
			// --- Interrupt Priority Register 4 ---  
#define CR_INTC_IPR4    (CR_INTC_BASE+0x0020)     
			// --- Interrupt Priority Register 5 ---  
#define CR_INTC_IPR5    (CR_INTC_BASE+0x0024)    
			// --- Interrupt Priority Register 6 ---  
#define CR_INTC_IPR6    (CR_INTC_BASE+0x0028)     
			// --- Interrupt Priority Register 7 ---  
#define CR_INTC_IPR7    (CR_INTC_BASE+0x002c)     
			// --- Interrupt Vector egister --- 	  
#define CR_INTC_IVR     (CR_INTC_BASE+0x0030)     
													  
#ifndef TCSUPPORT_CPU_EN7523													  
enum											  
interrupt_source								  
	{											  
		UART_INT,		//0 	IPL10			  
		RTC_ALARM_INT,	//1 	IPL29			  
		RTC_TICK_INT,	//2 	IPL31			  
		RESERVED0,		//3 	IPL30			  
		TIMER0_INT, 	//4 	IPL1			  
		TIMER1_INT, 	//5 	IPL5			  
		TIMER2_INT, 	//6 	IPL6			  
		TIMER3_INT, 	//7 	IPL7			  
		TIMER4_INT, 	//8 	IPL8			  
		TIMER5_INT, 	//9 	IPL9			  
		GPIO_INT,		//10	IPL11			  
		RESERVED1,		//11	IPL20			  
		RESERVED2,		//12	IPL21			  
		RESERVED3,		//13	IPL22			  
		APB_DMA0_INT,	//14	IPL12			  
		APB_DMA1_INT,	//15	IPL13			  
		RESERVED4,		//16	IPL23			  
		RESERVED5,		//17	IPL24			  
		DYINGGASP_INT,	//18	IPL25			  
		DMT_INT,		//19	IPL26			  
		ARBITER_ERR_INT,//20	IPL0			  
		MAC_INT,		//21	IPL3			  
		SAR_INT,		//22	IPL2			  
		USB_INT,		//23	IPL14			  
		PCI_A_INT,		//24			  
		PCI_B_INT,		//25			  
//		  RESERVED8,	  //24	  IPL27 		  
//		  RESERVED9,	  //25	  IPL28 		  
		XSLV0_INT,		//26	IPL15			  
		XSLV1_INT,		//27	IPL16			  
		XSLV2_INT,		//28	IPL17			  
		XAPB0_INT,		//29	IPL18			  
		XAPB1_INT,		//30	IPL19			  
		SWR_INT 		//31	IPL4			  
	};						
#else
enum											  
interrupt_source								  
	{											  
		UART_INT=50,		//2+32()+16			  
		DRAMC_INTP,
		TIMER0_INT,
		TIMER1_INT,
		TIMER2_INT, // 54
		PTP_THERM_INT,
		PTP_FSM_INT,
		TIMER3_INT,
		GPIO_INT,
		PCM1_INT,
		RESERVED0, // 60
		RESERVED1,
		GDMA_INT,
		GSW_INT,
		UART2_INT,
		USBHOST_INT,
		DYING_GASP_INT,
		TRNG_INT,
		SIM_IRQ,
		MAC_INT, //69
		
	};
#endif

enum											  
interrupt_priority								  
{												  
		IPL0,	IPL1,	IPL2,	IPL3,	IPL4,	  
		IPL5,	IPL6,	IPL7,	IPL8,	IPL9,	  
		IPL10,	IPL11,	IPL12,	IPL13,	IPL14,	  
		IPL15,	IPL16,	IPL17,	IPL18,	IPL19,	  
		IPL20,	IPL21,	IPL22,	IPL23,	IPL24,	  
		IPL25,	IPL26,	IPL27,	IPL28,	IPL29,	  
		IPL30,	IPL31							  
};												  

#ifdef TCSUPPORT_MIPS_1004K
enum
interrupt_vector {
    IV0,     IV1,     IV2,     IV3,     IV4,
    IV5,     IV6,     IV7,     IV8,     IV9,
    IV10,    IV11,    IV12,    IV13,    IV14,
    IV15,    IV16,    IV17,    IV18,    IV19,
    IV20,    IV21,    IV22,    IV23,    IV24,
    IV25,    IV26,    IV27,    IV28,    IV29,
    IV30,    IV31,    IV32,    IV33,    IV34,
    IV35,    IV36,    IV37,    IV38,    IV39,
    IV40,    IV41,    IV42,    IV43,    IV44,
    IV45,    IV46,    IV47,    IV48,    IV49,
    IV50,    IV51,    IV52,    IV53,    IV54,
    IV55,    IV56,    IV57,    IV58,    IV59,
    IV60,    IV61,    IV62,    IV63
};
#endif
/**************************
 * Timer Module Registers *
 **************************/
#define CR_TIMER_BASE  		0x1fbF0100
#define CR_TIMER_CTL    	(CR_TIMER_BASE + 0x00)
#define CR_TIMER0_LDV   	(CR_TIMER_BASE + 0x04)
#define CR_TIMER0_VLR    	(CR_TIMER_BASE + 0x08)
#define CR_TIMER1_LDV       (CR_TIMER_BASE + 0x0C)
#define CR_TIMER1_VLR       (CR_TIMER_BASE + 0x10)
#define CR_TIMER2_LDV       (CR_TIMER_BASE + 0x14)
#define CR_TIMER2_VLR       (CR_TIMER_BASE + 0x18)
#define CR_TIMER3_LDV       (CR_TIMER_BASE + 0x1C)
#define CR_TIMER3_VLR       (CR_TIMER_BASE + 0x20)
#define CR_TIMER4_LDV       (CR_TIMER_BASE + 0x24)
#define CR_TIMER4_VLR       (CR_TIMER_BASE + 0x28)
#define CR_TIMER5_LDV       (CR_TIMER_BASE + 0x2C)
#define CR_TIMER5_VLR       (CR_TIMER_BASE + 0x30)
	
#define TIMER_ENABLE         1
#define TIMER_DISABLE        0
#define TIMER_TOGGLEMODE     1
#define TIMER_INTERVALMODE   0
#define TIMER_TICKENABLE     1
#define TIMER_TICKDISABLE    0
#define TIMER_WDENABLE       1
#define TIMER_WDDISABLE      0
#define TIMER_HALTENABLE     1
#define TIMER_HALTDISABLE    0
		  
#define TIMERTICKS_1MS       1  
#define TIMERTICKS_10MS      10  // set timer ticks as 10 ms
#define TIMERTICKS_100MS     100
#define TIMERTICKS_1S        1000 
#define TIMERTICKS_10S       10000
	
#define timerCtlSet(timer_no, timer_enable, timer_mode,timer_halt)	timer_Configure(timer_no, timer_enable, timer_mode, timer_halt)
#define timerWdSet(tick_enable, watchdog_enable) timer_WatchDogConfigure(tick_enable,watchdog_enable)
#define timerLdvSet(timer_no,val) *(volatile uint32 *)(CR_TIMER0_LDV+timer_no*0x08) = (val)
#define timerVlrGet(timer_no,val) (val)=*(volatile uint32 *)(CR_TIMER0_VLR+timer_no*0x08)


/*************************
 * GPIO Module Registers *
 *************************/
#define CR_GPIO_BASE       	0x1fbF0200
#define CR_GPIO_CTRL	    (CR_GPIO_BASE + 0x00)
#define CR_GPIO_DATA	    (CR_GPIO_BASE + 0x04)
#define CR_GPIO_INTS      	(CR_GPIO_BASE + 0x08)
#define CR_GPIO_EDET	    (CR_GPIO_BASE + 0x0C)
#define CR_GPIO_LDET       	(CR_GPIO_BASE + 0x10)
#define CR_GPIO_ODRAIN      (CR_GPIO_BASE + 0x14)

#define CR_GPIO_CTRL1		(CR_GPIO_BASE + 0x20)
#define CR_GPIO_CTRL2	    (CR_GPIO_BASE + 0x60)
#define CR_GPIO_CTRL3	    (CR_GPIO_BASE + 0x64)
#define CR_GPIO_FLAMOD_EXT	(CR_GPIO_BASE + 0x68)
#define CR_GPIO_DATA1		(CR_GPIO_BASE + 0x70)
#define CR_GPIO_ODRAIN1     (CR_GPIO_BASE + 0x78)


#define GPIO_IN				0x0
#define GPIO_OUT			0x1
#define GPIO_ALT_IN			0x2
#define GPIO_ALT_OUT		0x3

#define GPIO_E_DIS			0x0
#define GPIO_E_RISE			0x1
#define GPIO_E_FALL			0x2
#define GPIO_E_BOTH			0x3

#define GPIO_L_DIS			0x0
#define GPIO_L_HIGH			0x1
#define GPIO_L_LOW			0x2
#define GPIO_L_BOTH			0x3

/*****************************
 * Arbiter/Decoder Registers *
 *****************************/
#define CR_AHB_BASE       	0x1fb00000
#define CR_AHB_AACS	    	(CR_AHB_BASE + 0x00)
#define CR_AHB_ABEM      	(CR_AHB_BASE + 0x08)
#define CR_AHB_ABEA		    (CR_AHB_BASE + 0x0C)
#define CR_AHB_DMB0       	(CR_AHB_BASE + 0x10)
#define CR_AHB_DMB1       	(CR_AHB_BASE + 0x14)
#define CR_AHB_DMB2       	(CR_AHB_BASE + 0x18)
#define CR_AHB_DMB3       	(CR_AHB_BASE + 0x1C)
#define CR_AHB_SMB0       	(CR_AHB_BASE + 0x20)
#define CR_AHB_SMB1       	(CR_AHB_BASE + 0x24)
#define CR_AHB_SMB2       	(CR_AHB_BASE + 0x28)
#define CR_AHB_SMB3       	(CR_AHB_BASE + 0x2C)
#define CR_AHB_SMB4       	(CR_AHB_BASE + 0x30)
#define CR_AHB_SMB5       	(CR_AHB_BASE + 0x34)
#if defined(TC3262)
#define CR_AHB_CPUF       	(CR_AHB_BASE + 0x58)
#endif
#define CR_AHB_PMCR       	(CR_AHB_BASE + 0x80)
#define CR_AHB_DMTCR       	(CR_AHB_BASE + 0x84)
#if defined(TCSUPPORT_CPU_EN7528)
#define BOOT_TRAP_CONF      (0x1fb000B8)
#endif
#if defined(TCSUPPORT_CPU_EN7580)
#define CR_AHB_HWCONF       (isFPGA ? 0x1fa2FF30 : 0x1fa20254)
#elif defined(TCSUPPORT_CPU_EN7516)||defined(TCSUPPORT_CPU_EN7527)
#define CR_AHB_HWCONF       (isFPGA ? 0x1fa2FF28 : 0x1fa20174)
#else
#define CR_AHB_HWCONF       (CR_AHB_BASE + 0x8C)
#endif
#define CR_AHB_SSR       	(CR_AHB_BASE + 0x90)
#define CR_AHB_SSTR			(CR_AHB_BASE + 0x9C)
#if defined(TC3162L2) || defined(TC3262)
#define CR_IMEM       	(CR_AHB_BASE + 0x9C)
#define CR_DMEM       	(CR_AHB_BASE + 0xA0)
#endif
#define CR_AHB_PLL       	(CR_AHB_BASE + 0xB0)
#define CR_AHB_ABMR       	(CR_AHB_BASE + 0xB8)
#define CR_PSMCR       		(CR_AHB_BASE + 0xCC)
#define CR_PSMDR       		(CR_AHB_BASE + 0xD0)
#define CR_PSMMR       		(CR_AHB_BASE + 0xD0)

/* RT63165 */
#define CR_SRAM       		(CR_AHB_BASE + 0xF4)
#define CR_AHB_HWCONF2      (CR_AHB_BASE + 0xF8)

/* RT63365 */
#define CR_CLK_CFG     		(CR_AHB_BASE + 0x82c)
#define CR_RSTCTRL2    		(CR_AHB_BASE + 0x834)
#define SHARE_FEMEM_SEL 	(CR_AHB_BASE + 0x958)

#define CR_SDRAM_DRI_STRENG_1	(CR_AHB_PLL + 0x4) /*SDRAM Driving Strength Setting*/
#define CR_SDRAM_DRI_STRENG_2	(CR_AHB_PLL + 0x8) /*SDRAM Driving Strength Setting 2*/
#define CR_SDRAM_DRI_STRENG_3	(CR_AHB_PLL + 0xc) /*SDRAM Driving Strength Setting 3*/
/*************************************************
 * SRAM/FLASH/ROM Controller Operation Registers *
 *************************************************/
#define CR_SMC_BASE       	0x1fb10000
#define CR_SMC_BCR0	    	(CR_SMC_BASE + 0x00)
#define CR_SMC_BCR1	    	(CR_SMC_BASE + 0x04)
#define CR_SMC_BCR2	    	(CR_SMC_BASE + 0x08)
#define CR_SMC_BCR3	    	(CR_SMC_BASE + 0x0C)
#define CR_SMC_BCR4	    	(CR_SMC_BASE + 0x10)
#define CR_SMC_BCR5	    	(CR_SMC_BASE + 0x14)

/*****************************
 * Clock Generator Registers *
 *****************************/

/****************************
 * USB Module Registers *
 ****************************/

#define CR_USB_BASE     0x1fb70000

        // --- System Control Register ---
#define CR_USB_SYS_CTRL_REG           (0x00 | CR_USB_BASE)

        // --- Device Control Register ---
#define CR_USB_DEV_CTRL_REG           (0x04 | CR_USB_BASE)

        // --- Interrupt Status Register ---
#define CR_USB_INTR_STATUS_REG        (0x08 | CR_USB_BASE)

        // --- Interrupt Mask Register ---
#define CR_USB_INTR_MASK_REG          (0x0c | CR_USB_BASE)

        // --- Control Endpoint I/O Mode Control Register ---
#define CR_USB_CTRL_ENDP_IO_CTRL_REG  (0x10 | CR_USB_BASE)

        // --- Control Endpoint I/O Mode OUT Transfer Data Register #00 ---
#define CR_USB_CTRL_ENDP_IO_OUT_REG0  (0x18 | CR_USB_BASE)

        // --- Control Endpoint I/O Mode OUT Transfer Data Register #01 ---
#define CR_USB_CTRL_ENDP_IO_OUT_REG1  (0x1c | CR_USB_BASE)

        // --- Control Endpoint I/O Mode IN Transfer Data Register #00 ---
#define CR_USB_CTRL_ENDP_IO_IN_REG0   (0x20 | CR_USB_BASE)

        // --- Control Endpoint I/O Mode IN Transfer Data Register #01 ---
#define CR_USB_CTRL_ENDP_IO_IN_REG1   (0x24 | CR_USB_BASE)

        // --- Interrupt IN Endpoint Control Register ---
#define CR_USB_INTR_IN_ENDP_CTRL_REG  (0x30 | CR_USB_BASE)

        // --- Interrupt IN Endpoint IN Transfer Data Register #00 ---
#define CR_USB_INTR_IN_ENDP_IN_REG0   (0x38 | CR_USB_BASE)

        // --- Interrupt IN Endpoint IN Transfer Data Register #01 ---
#define CR_USB_INTR_IN_ENDP_IN_REG1   (0x3c | CR_USB_BASE)

        // --- Bulk/ISO OUT Descriptor Pointer Register ---
#define CR_USB_BULKISO_OUT_DESCP_BASE_REG   (0x40 | CR_USB_BASE)

        // --- Bulk/ISO IN Descriptor Pointer Register ---
#define CR_USB_BULKISO_IN_DESCP_BASE_REG    (0x44 | CR_USB_BASE)

        // --- Bulk/ISO IN/OUT Endpoint Number Register ---
#define CR_USB_BULKISO_INOUT_ENDP_NUM_REG   (0x48 | CR_USB_BASE)

        // --- Bulk/ISO Endpoint DMA Control Register ---
#define CR_USB_BULKISO_ENDP_DMA_CTRL_REG    (0x4c | CR_USB_BASE)

        // --- Bulk/ISO Endpoint DMA Configuration Register ---
#define CR_USB_BULKISO_ENDP_DMA_CONF_REG    (0x50 | CR_USB_BASE)

        // --- ISO Endpoint Transfer Delimiter Register #00 ---
#define CR_USB_ISO_ENDP_DELIMITER_REG0      (0x58 | CR_USB_BASE)

        // --- ISO Endpoint Transfer Delimiter Register #01 ---
#define CR_USB_ISO_ENDP_DELIMITER_REG1      (0x5c | CR_USB_BASE)

        // --- Vendor ID Register ---
#define CR_USB_VENDOR_ID_REG                (0x68 | CR_USB_BASE)

        // --- Product ID Register ---
#define CR_USB_PRODUCT_ID_REG               (0x6c | CR_USB_BASE)

/****************************
 * ATM SAR Module Registers *
 ****************************/
#define TSCONTROL_BASE			0x1fb00000
#define TSARM_REGISTER_BASE		(TSCONTROL_BASE + 0x00060000)

/* ----- General configuration registers  ----- */

/* ----- Reset And Identify register  ----- */
#define TSARM_RAI				VPint(TSARM_REGISTER_BASE + 0x0000)
/* ----- General Configuration register  ----- */
#define TSARM_GFR				VPint(TSARM_REGISTER_BASE + 0x0004)
/* ----- Traffic Scheduler Timer Base Counter register  ----- */
#define TSARM_TSTBR				VPint(TSARM_REGISTER_BASE + 0x0008)
/* ----- Receive Maximum Packet Length register  ----- */
#define TSARM_RMPLR				VPint(TSARM_REGISTER_BASE + 0x000c)
#if defined(TC3162L2) || defined(TC3262)
//Transmit Priority 0/1 Data Buffer Control and Status Register
#define TSARM_TXDBCSR_P01		VPint(TSARM_REGISTER_BASE + 0x0010)
#else
/* ----- TX Data Buffer Control and Status register  ----- */
#define TSARM_TXDBCSR			VPint(TSARM_REGISTER_BASE + 0x0010)
#endif
/* ----- TX OAM Buffer Control and Status register  ----- */
#define TSARM_TXMBCSR			VPint(TSARM_REGISTER_BASE + 0x0014)
/* ----- RX Data Buffer Control and Status register  ----- */
#define TSARM_RXDBCSR			VPint(TSARM_REGISTER_BASE + 0x0018)
/* ----- RX OAM Buffer Control and Status register  ----- */
#define TSARM_RXMBCSR			VPint(TSARM_REGISTER_BASE + 0x001c)
/* ----- Last IRQ Status register  ----- */
#define TSARM_LIRQ				VPint(TSARM_REGISTER_BASE + 0x0020)
/* ----- IRQ Queue Base Address register  ----- */
#define TSARM_IRQBA				VPint(TSARM_REGISTER_BASE + 0x0024)
/* ----- IRQ Queue Entry Length register  ----- */
#define TSARM_IRQLEN			VPint(TSARM_REGISTER_BASE + 0x0028)
/* ----- IRQ Head Indication register  ----- */
#define TSARM_IRQH				VPint(TSARM_REGISTER_BASE + 0x002c)
/* ----- Clear IRQ Entry register  ----- */
#define TSARM_IRQC				VPint(TSARM_REGISTER_BASE + 0x0030)
#if defined(TC3162L2) || defined(TC3262)
//Traffic Scheduler Line Rate Counter Register
#define TSARM_TXSLRC			VPint(TSARM_REGISTER_BASE + 0x0034)
//Transmit Priority 2/3 Data Buffer Control and Status Register
#define TSARM_TXDBCSR_P23		VPint(TSARM_REGISTER_BASE + 0x0038)
#endif

/* ----- VC IRQ Mask register  ----- */
#define TSARM_IRQM_BASE			(TSARM_REGISTER_BASE + 0x0040)
#define TSARM_IRQM(vc)			VPint(TSARM_IRQM_BASE + (vc * 4))
#define TSARM_IRQMCC			VPint(TSARM_IRQM_BASE + 0x0040)
#if defined(TC3162L2) || defined(TC3262)
#define TSARM_IRQ_QUE_THRE		VPint(TSARM_REGISTER_BASE + 0x0084)		//IRQ Queue Threshold Register
#define TSARM_IRQ_TIMEOUT_CTRL 	VPint(TSARM_REGISTER_BASE + 0x0088)		//IRQ Timeout Control Register
#endif

/* ----- VC Configuration register  ----- */
#define TSARM_VCCR_BASE			(TSARM_REGISTER_BASE + 0x0100)
#define TSARM_VCCR(vc)			VPint(TSARM_VCCR_BASE + (vc * 4))
#define TSARM_CCCR				VPint(TSARM_VCCR_BASE + 0x0040)

/* ----- Transmit Buffer Descriptor register  ----- */
#define TSARM_TXDCBDA_BASE		(TSARM_REGISTER_BASE + 0x0200)
#define TSARM_TXDCBDA(vc)		VPint(TSARM_TXDCBDA_BASE + (vc * 4))
#define TSARM_TXMCBDA_BASE		(TSARM_REGISTER_BASE + 0x0240)
#define TSARM_TXMCBDA(vc)		VPint(TSARM_TXMCBDA_BASE + (vc * 4))

#if defined(TC3162L2) || defined(TC3262)
#define TSARM_CC_TX_BD_BASE				VPint(TSARM_REGISTER_BASE + 0x0228)		//Control Channel Transmit BD Base Address 0x228
#define TSARM_CC_TX_BD_MNG_BASE			VPint(TSARM_REGISTER_BASE + 0x0268)		//Control Channel Transmit BD Management Base
#define TSARM_VC_TX_BD_PRIORITY01_BASE		(TSARM_REGISTER_BASE + 0x0280)
#define TSARM_VC_TX_BD_PRIORITY01(vc)		VPint(TSARM_VC_TX_BD_PRIORITY01_BASE + vc * 4)		//VC0 Transmit BD Data Priority 0/1 Base 280
#define TSARM_VC_TX_BD_PRIORITY23_BASE		(TSARM_REGISTER_BASE + 0x02c0)
#define TSARM_VC_TX_BD_PRIORITY23(vc)		VPint(TSARM_VC_TX_BD_PRIORITY23_BASE + vc * 4)		//VC0 Transmit BD Data Priority 0/1 Base 280
#else
#define TSARM_TXCCBDA			VPint(TSARM_REGISTER_BASE + 0x0280)
#endif

/* ----- Receive Buffer Descriptor register  ----- */
#define TSARM_RXDCBDA_BASE		(TSARM_REGISTER_BASE + 0x0300)
#define TSARM_RXDCBDA(vc)		VPint(TSARM_RXDCBDA_BASE + (vc * 4))
#define TSARM_RXMCBDA_BASE		(TSARM_REGISTER_BASE + 0x0340)
#define TSARM_RXMCBDA(vc)		VPint(TSARM_RXMCBDA_BASE + (vc * 4))

#if defined(TC3162L2) || defined(TC3262)
#define TSARM_CC_RX_BD_BASE			VPint(TSARM_REGISTER_BASE + 0x328)		//Control Channel Receive BD Base Address	0x328
#define TSARM_CC_RX_BD_MNG_BASE		VPint(TSARM_REGISTER_BASE + 0x368)		//Control Channel Receive BD Management Base	0x368
#define TSARM_VC_RX_DATA_BASE				(TSARM_REGISTER_BASE + 0x380)
#define TSARM_VC_RX_DATA(vc)			VPint(TSARM_VC_RX_DATA_BASE + vc * 4)	//VC0 Receive BD Data Base	0x380
#else
#define TSARM_RXCCBDA			VPint(TSARM_REGISTER_BASE + 0x0380)
#endif

/* ----- Traffic Scheduler register  ----- */
#define TSARM_PCR_BASE			(TSARM_REGISTER_BASE + 0x0400)
#define TSARM_PCR(vc)			VPint(TSARM_PCR_BASE + (vc * 4))
#define TSARM_SCR_BASE			(TSARM_REGISTER_BASE + 0x0440)
#define TSARM_SCR(vc)			VPint(TSARM_SCR_BASE + (vc * 4))
#define TSARM_MBSTP_BASE		(TSARM_REGISTER_BASE + 0x0480)
#define TSARM_MBSTP(vc)			VPint(TSARM_MBSTP_BASE + (vc * 4))

#if defined(TC3162L2) || defined(TC3262)
#define TSARM_MAX_FRAME_SIZE_BASE	(TSARM_REGISTER_BASE + 0x04c0)
#define TSARM_MAX_FRAME_SIZE(vc)		VPint(TSARM_MAX_FRAME_SIZE_BASE + (vc * 4))
#else
/* ----- Receive Timeout register  ----- */
#define TSARM_RTOCNT_BASE		(TSARM_REGISTER_BASE + 0x0500)
#define TSARM_RTOCNT(vc)		VPint(TSARM_RTOCNT_BASE + (vc * 4))
#endif

/* ----- TX Statistic Counter register  ----- */
#define TSARM_TDCNT_BASE		(TSARM_REGISTER_BASE + 0x0600)
#define TSARM_TDCNT(vc)			VPint(TSARM_TDCNT_BASE + (vc * 4))
#define TSARM_TDCNTCC			VPint(TSARM_TDCNT_BASE + 0x0040)

/* ----- RX Statistic Counter register  ----- */
#define TSARM_RDCNT_BASE		(TSARM_REGISTER_BASE + 0x0700)
#define TSARM_RDCNT(vc)			VPint(TSARM_RDCNT_BASE + (vc * 4))
#define TSARM_RDCNTCC			VPint(TSARM_RDCNT_BASE + 0x0040)
#define TSARM_MISCNT			VPint(TSARM_RDCNT_BASE + 0x0044)

#if defined(TC3162L2) || defined(TC3262)
#define TSARM_MPOA_GCR				VPint(TSARM_REGISTER_BASE + 0x0800)			//MPOA global control register
#define TSARM_VC_MPOA_CTRL_BASE			(TSARM_REGISTER_BASE + 0x0810)			//VC0 ~9  MPOA Control register
#define TSARM_VC_MPOA_CTRL(vc)			VPint(TSARM_VC_MPOA_CTRL_BASE + vc * 4)	
#define TSARM_MPOA_HFIV11				VPint(TSARM_REGISTER_BASE + 0x0850)			//MPOA header Field1 Insertion Value1
#define TSARM_MPOA_HFIV12				VPint(TSARM_REGISTER_BASE + 0x0854)			//MPOA header Field1 Insertion Value2
#define TSARM_MPOA_HFIV13				VPint(TSARM_REGISTER_BASE + 0x0858)			//MPOA header Field2 Insertion Value1
#define TSARM_MPOA_HFIV21				VPint(TSARM_REGISTER_BASE + 0x0860)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV22				VPint(TSARM_REGISTER_BASE + 0x0864)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV23				VPint(TSARM_REGISTER_BASE + 0x0868)			//MPOA header Field2 Insertion Value2
#define TSARM_MPOA_HFIV31				VPint(TSARM_REGISTER_BASE + 0x0870)			//MPOA header Field3 Insertion Value1
#define TSARM_MPOA_HFIV32				VPint(TSARM_REGISTER_BASE + 0x0874)			//MPOA header Field3 Insertion Value2
#define TSARM_MPOA_HFIV33				VPint(TSARM_REGISTER_BASE + 0x0878)			//MPOA header Field3 Insertion Value3
#define TSARM_MPOA_HFIV41				VPint(TSARM_REGISTER_BASE + 0x0880)			//MPOA header Field4 Insertion Value1
#define TSARM_MPOA_HFIV42				VPint(TSARM_REGISTER_BASE + 0x0884)			//MPOA header Field4 Insertion Value2
#define TSARM_MPOA_HFIV43				VPint(TSARM_REGISTER_BASE + 0x0888)			//MPOA header Field4 Insertion Value2
#endif

/**************************
 * USB Module Registers *
 **************************/

#define LA_DEBUG_TRIGGER(addr,val) VPint(0xbfc00000+addr) = val

/**************************
 * EFUSE Registers *
 **************************/
#ifdef TCSUPPORT_CPU_EN7528
#define CR_EFUSE_BASE1				(0x1fbF8200)
#define CR_EFUSE_BASE2				(0x1faA0000)
#define CR_EFUSE_STATUS				(0x0C)
#define CR_EFUSE_DATA0				(0x14)
#define CR_EFUSE_DATA1				(0x18)
#define CR_EFUSE_CLK_RCNT			(0x30)
#endif

/******************************/

static inline void disable_all_interrupts(void)
{
    #ifdef TCSUPPORT_MIPS_1004K
    VPint(GIC_SH_RMASK31_00)=0xffffffff;
    VPint(GIC_SH_RMASK63_32)=0xffffffff;
    #else
    VPint(CR_INTC_IMR)=0x0;
    #endif

	return;
}

#endif /* _TC3162_H_ */
