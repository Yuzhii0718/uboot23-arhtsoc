// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#ifndef _AIR_ETH_XSGMII_CONFIG_
#define _AIR_ETH_XSGMII_CONFIG_

#include "xfi_pcs_20210924_reg.h"
#include "rate_adaption_csr_XGMII_reg.h"
#include "rate_adaption_csr_XGMII_hal_reg.h"
#include "xfi_pcs_20210924_hal_reg.h"
#include "multi_sgmii_csr_TX_reg.h"
#include "hsgmii_csr_pcs_reg.h"
#include "hsgmii_csr_pcs_hal_reg.h"
#include "hsgmii_an_csr_reg.h"
#include "hsgmii_an_csr_hal_reg.h"
#include "pcie_ana_pxp_reg.h"
#include "pcie_ana_pxp_hal_reg.h"
#include "../sgmii_globaldef.h"
#include "air_xsgmii.h"
#include "en7581_pcie0_pma_reg.h"
#include "en7581_pcie1_pma_reg.h"
#include "en7581_pcie0_pma_hal_reg.h"
#include "multi_sgmii_csr_RX_reg.h"
#include "multi_sgmii_csr_RX_hal_reg.h"
#include "multi_phy_ana_hal_reg.h"
#include "multi_phy_ana_reg.h"
#include "multi_sgmii_csr_RX_RO_reg.h"



#define USXGMII_INT_EN              			0x5f0
#define USXGMII_INT_STA_CLR         			0x5f4
#define USXGMII_INT_STA             			0x5f8
	
#define USXGMII_PCS1_BASE_OFFSET    			(RgAddr) 0x0900
#define USXGMII_PCS2_BASE_OFFSET    			(RgAddr) 0x0A00
#define SGMII_AN_BASE_OFFSET      				(RgAddr) 0x0000
#define MULTI_PHY_ANA_BASE_OFFSET   			(RgAddr) 0x0000
#define PXP_BASE_OFFSET   						(RgAddr) 0x0000
#define PMA_BASE_OFFSET   						(RgAddr) 0x0000
#define L0_PMA_BASE_OFFSET   					(RgAddr) 0xB000
#define L1_PMA_BASE_OFFSET   					(RgAddr) 0xC000

#define USXGMII_RA_BASE_OFFSET      		 	(RgAddr) 0x0000
#define USXGMII_MULTI_SGMII_BASE_OFFSET      	(RgAddr) 0x0000

#define xsgmii_table air_pcie2l_xsgmii_of_id
#define COMPATIBLE "airoha,air-pcie2l_usxgmii"
#define XSGMII_DRV air_pcie2l_xsgmii_driver
#define DESCRIPTION "pcie2l xsgmii driver"
#define NAME "air_pcie2l_xsgmii"
#define VER "AN7581.PCIE2L.1.1.0"
#define DRV PCIE2L_XSGMII_DRV
//#define Enable_IRQ

/***Additional feature***/
//Support FPGA
#ifndef CONFIG_USE_MT7520_ASIC
	#define FPGA_PLATFORM
#endif	
//Support common phy
#define RESOUCE_FROM_COMMON_MODULE
#ifdef RESOUCE_FROM_COMMON_MODULE
	#include "../../combo_phy/en7581/ecnt_serdes_common_phy.h"
#endif

//Support scu mod & rst operation
#define SCU_MOD_RST_OP
#ifdef SCU_MOD_RST_OP
	extern u32 GET_NP_SCU_SERDES_WIFI0_SEL(void);
	extern void SET_NP_SCU_SERDES_WIFI0_SEL(u32 val);
	extern u32 GET_NP_SCU_SERDES_WIFI1_SEL(void);
	extern void SET_NP_SCU_SERDES_WIFI1_SEL(u32 val);
	extern u32 GET_PCIC(void);
	extern void SET_PCIC(u32 val);	
	extern u32 GET_SCU_RST_RG(void);
	extern void SET_SCU_RST_RG(u32 val);
	static u32 (*fptr_SCU_GetModSel)(void)= GET_NP_SCU_SERDES_WIFI0_SEL;
	static void (*fptr_SCU_SetModSel)(u32)= SET_NP_SCU_SERDES_WIFI0_SEL;
	static u32 (*fptr_SCU_GetModRst)(void)= GET_SCU_RST_RG;
	static void (*fptr_SCU_SetModRst)(u32)= SET_SCU_RST_RG;
	#define SCU_GetModSel (*fptr_SCU_GetModSel)
	#define SCU_SetModSel (*fptr_SCU_SetModSel)
	#define SCU_GetModRst (*fptr_SCU_GetModRst)
	#define SCU_SetModRst (*fptr_SCU_SetModRst)
	#define SCU_PHY_RST_POS 13
	#define SCU_MAC_RST_POS 16
	#define SCU_RST_BIT 0b11
	//#define SCU_MAC_RST_POS 7
#endif
//Debug print
extern u8 dbg_print;
#endif
