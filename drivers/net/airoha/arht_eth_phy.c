// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <common.h>
#include <command.h>
/////////
//#include <asm/errno.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/tc3162.h>
#include <asm/io.h>
#include "arht_eth.h"
#include "arht_eth_phy.h"

#include "H/air_eth_xsgmii.h"
#include "H/air_eth_xsgmii_config.h"

#include "H/en7581_xfi_pma_hal_reg.h"
#include "H/en7581_xfi_pma_reg.h"
#include "sgmii_globaldef.h"
#include "H/xfi_ana_pxp_hal_reg.h"
#include "H/xfi_ana_pxp_reg.h"

#include "H/en7581_pcie1_pma_reg.h"
#include "H/pcie_ana_pxp_reg.h"

#define pcs1_base  0x1fa75900
//#define USXGMII_PCS1_BASE_OFFSET    			(u32)(uintptr_t)(RgAddr) 0x0900
#undef PMA_BASE_OFFSET
#ifdef TCSUPPORT_CPU_AN7583
#define pma_base   0x1fa7e000
#define PMA_BASE_OFFSET   						(u32)(uintptr_t)(RgAddr) 0xE000
#else
#define pma_base   0x1fa7b000
#undef PMA_BASE_OFFSET
#define PMA_BASE_OFFSET   						(u32)(uintptr_t)(RgAddr) 0xB000
#endif

#define L1_PMA_BASE_OFFSET   					(u32) 0xC000

u8 dbg_print                    =  0;
#if 0
int XSGMII_Linkdn_Ignone_SD_EN   =  0;
int linkup_sta                   =  0;
int linkdn_sta                   =  0;
#endif
extern uint16_t miiStationRead45(uint32_t phy_addr, uint32_t dev_addr, uint32_t phy_reg);
extern int miiStationWrite45(uint32_t phy_addr, uint32_t dev_addr, uint32_t phy_reg, uint32_t phy_data);

extern u32 xsgmii_api(u8 serdes,u8 xsgmii,u8 mod,u8 rate,u8 an);
extern int irq_register (unsigned int irq_num, void (*fxn)(void), unsigned char level0edge1);
extern void gic_dic_set_enable (unsigned int intrID);
extern void gic_dic_set_icenable (unsigned int intr);
extern void __udelay(unsigned long usec);

static void RG_W_PL(u32 Base,u32 Base_start,u32 Addr,u32 Data){

 write_reg_word( Base + (Addr - Base_start),Data);

}
	
static u32 RG_R_PL(u32 Base,u32 Base_start,u32 Addr){

  return read_reg_word(Base + (Addr - Base_start));	
}  

void XSI_MAC_LOGIC_RESET(void){
	u32 mac_lock;
	//unlock
	mac_lock = read_reg_word(0x1fa09000);
	write_reg_word(0x1fa09000, (mac_lock | 0xf));
	//delay 1ms
	udelay(1000);
	//reset
	write_reg_word(0x1fa09010,0);
	write_reg_word(0x1fa09010,1);
	//lock
	write_reg_word(0x1fa09000, (mac_lock & 0xfffffff0));		
}

static u8 RX_CDR_LFP_L2D_sta(void){
	rg_type_t(HAL_rg_force_da_pxp_cdr_lpf_lck2data) rg_force_da_pxp_cdr_lpf_lck2data;
	u8 sta; 
	rg_force_da_pxp_cdr_lpf_lck2data.dat.value = RG_R_PL(pma_base,(u32)PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_force_da_pxp_cdr_lpf_lck2data);
	sta = rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_sel_da_pxp_cdr_lpf_lck2data ;	
	if(dbg_print) printf("RX_CDR_LFP_L2D_sta %x\n",sta);
	return sta;
}

static void RX_CDR_LFP_L2D(u8 mod,u8 sel){
	rg_type_t(HAL_rg_force_da_pxp_cdr_lpf_lck2data) rg_force_da_pxp_cdr_lpf_lck2data;	
	if(dbg_print) printf("RX_CDR_LFP_L2D mode %x, sel %x\n",mod,sel);
	rg_force_da_pxp_cdr_lpf_lck2data.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_force_da_pxp_cdr_lpf_lck2data);

	//rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_sel_da_pxp_cdr_lpf_lck2data = 1;	
	rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_sel_da_pxp_cdr_lpf_lck2data = mod;
	rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_da_pxp_cdr_lpf_lck2data = sel;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_force_da_pxp_cdr_lpf_lck2data,rg_force_da_pxp_cdr_lpf_lck2data.dat.value);
}


static void RX_CDR_LPF_RSTB(u8 mod,u8 sel){
	rg_type_t(HAL_rg_force_da_pxp_cdr_lpf_lck2data) rg_force_da_pxp_cdr_lpf_lck2data;	
	if(dbg_print) printf("RX_CDR_LPF_RSTB mode %x, sel%x\n",mod,sel);
	rg_force_da_pxp_cdr_lpf_lck2data.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_force_da_pxp_cdr_lpf_lck2data);

	//rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_sel_da_pxp_cdr_lpf_rstb = 1;
	rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_sel_da_pxp_cdr_lpf_rstb = mod;
	rg_force_da_pxp_cdr_lpf_lck2data.hal.rg_force_da_pxp_cdr_lpf_rstb = sel;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_force_da_pxp_cdr_lpf_lck2data,rg_force_da_pxp_cdr_lpf_lck2data.dat.value);
}

static void RX_CDR_RST(void){
	if(dbg_print) printf("RX_CDR_RST\n");
	RX_CDR_LFP_L2D(1,0);
	RX_CDR_LPF_RSTB(1,0);
	udelay(700);
	RX_CDR_LPF_RSTB(1,1);
	udelay(100);
	RX_CDR_LFP_L2D(1,1);

	//switch to auto
	RX_CDR_LPF_RSTB(0,1);
	RX_CDR_LFP_L2D(0,1);
}

static u8 RX_RDY_Sta(void){
	rg_type_t(HAL_RX_CTRL_SEQUENCE_DISB_CTRL_1) RX_CTRL_SEQUENCE_DISB_CTRL_1;	
	RX_CTRL_SEQUENCE_DISB_CTRL_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_CTRL_SEQUENCE_DISB_CTRL_1);
	if(dbg_print) printf("RX_RDY_Sta %x \n",RX_CTRL_SEQUENCE_DISB_CTRL_1.hal.rg_disb_rx_rdy);
	return RX_CTRL_SEQUENCE_DISB_CTRL_1.hal.rg_disb_rx_rdy;
}

static void RX_RDY(u8 mod,u8 sel){
	rg_type_t(HAL_RX_CTRL_SEQUENCE_DISB_CTRL_1) RX_CTRL_SEQUENCE_DISB_CTRL_1;	
	rg_type_t(HAL_RX_CTRL_SEQUENCE_FORCE_CTRL_1) RX_CTRL_SEQUENCE_FORCE_CTRL_1;
	if(dbg_print) printf("RX_RDY %x, sel %x\n",mod,sel);
	RX_CTRL_SEQUENCE_DISB_CTRL_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_CTRL_SEQUENCE_DISB_CTRL_1);
	RX_CTRL_SEQUENCE_DISB_CTRL_1.hal.rg_disb_rx_rdy = mod;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_CTRL_SEQUENCE_DISB_CTRL_1,RX_CTRL_SEQUENCE_DISB_CTRL_1.dat.value);

	RX_CTRL_SEQUENCE_FORCE_CTRL_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_CTRL_SEQUENCE_FORCE_CTRL_1);
	RX_CTRL_SEQUENCE_FORCE_CTRL_1.hal.rg_force_rx_rdy = sel;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_CTRL_SEQUENCE_FORCE_CTRL_1,RX_CTRL_SEQUENCE_FORCE_CTRL_1.dat.value);
}



static void xsgmii_force_data(u8 xsgmii,u8 mod,u8 an, u8 rate){
	rg_type_t(HAL_rg_user_define_sel) rg_user_define_sel;	
	rg_type_t(HAL_rg_user_define_xgmii_control) rg_user_define_xgmii_control;
	rg_type_t(HAL_rg_user_define_xgmii_data_lsb) rg_user_define_xgmii_data_lsb;
	if(dbg_print)printf("USXGMII_force_data xsgmii %x, force %x\n",xsgmii,an);
	switch(xsgmii){
		case USXGMII:
			switch(rate){
				case 0:
					rg_user_define_xgmii_data_lsb.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_xgmii_data_lsb);			
					if(rg_user_define_xgmii_data_lsb.hal.rg_user_define_rxd_lsb != USR_DATA){
						rg_user_define_xgmii_data_lsb.hal.rg_user_define_rxd_lsb = USR_DATA;
						RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_xgmii_data_lsb,rg_user_define_xgmii_data_lsb.dat.value);
					}
					
						rg_user_define_xgmii_control.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_xgmii_control);
					if(rg_user_define_xgmii_control.hal.rg_user_define_rxc != 0xff){
						rg_user_define_xgmii_control.hal.rg_user_define_rxc = 0xff;				
						RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_xgmii_control,rg_user_define_xgmii_control.dat.value);				
					}
					
					rg_user_define_sel.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_sel);
					rg_user_define_sel.hal.rg_user_define_sel = an;
					RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_sel,rg_user_define_sel.dat.value);
					break;
				case 1:					
					rg_user_define_sel.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_sel);
					rg_user_define_sel.hal.rg_user_define_sel = an;
					RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr)_rg_user_define_sel,rg_user_define_sel.dat.value);
					break;
			}
			break;
		case HSGMII:	
		case SGMII:
			break;
	}
}

static u8 RX_SigDet_Flag(void){
	RGDATA_t rg;	
	u8 i,cnt = 0;		
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ADD_DIG_RESERVE_0,0x30000);	
	for (i=0;i<=5;i++){		
		rg.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ADD_DIG_RO_RESERVE_2);
		cnt += rg.bit.b8;
	}
	if(dbg_print)printf("RX_SigDet_Flag, cnt %x\n",cnt);
	
	return cnt >= 4? 1:0;  
}

static u8 RX_SigDet_Flag_D(void){
	rg_type_t(HAL_XPON_INT_EN_3) XPON_INT_EN_3;		
	rg_type_t(HAL_SS_RX_SIGDET_1) SS_RX_SIGDET_1;		
	rg_type_t(HAL_XPON_INT_STA_3) XPON_INT_STA_3;
	rg_type_t(HAL_RX_RESET_1) RX_RESET_1;	

	XPON_INT_EN_3.dat.value = RG_R_PL(pma_base,(u32)PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_EN_3);	
	XPON_INT_EN_3.hal.rg_rx_sigdet_int_en = 0;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_EN_3,XPON_INT_EN_3.dat.value);	

	SS_RX_SIGDET_1.dat.value = RG_R_PL(pma_base,(u32)PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _SS_RX_SIGDET_1);	
	SS_RX_SIGDET_1.hal.rg_sigdet_en = 1;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _SS_RX_SIGDET_1,SS_RX_SIGDET_1.dat.value);

	RX_RESET_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1);	
	RX_RESET_1.hal.rg_sigdet_rst_b = 0;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1,RX_RESET_1.dat.value);
	
	RX_RESET_1.hal.rg_sigdet_rst_b = 1;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1,RX_RESET_1.dat.value);

	XPON_INT_STA_3.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3);	
	XPON_INT_STA_3.hal.rx_sigdet_int = 1;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3,XPON_INT_STA_3.dat.value);	
	udelay(50);	
	XPON_INT_STA_3.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3);	

	SS_RX_SIGDET_1.hal.rg_sigdet_en = 0;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _SS_RX_SIGDET_1,SS_RX_SIGDET_1.dat.value);
	
	if(dbg_print)printf("RX_SigDet_Flag_D %x\n",XPON_INT_STA_3.hal.rx_sigdet_int);
	return XPON_INT_STA_3.hal.rx_sigdet_int;  
}

static void SigDet_Int_Init(u8 en){
	rg_type_t(HAL_XPON_INT_EN_3) XPON_INT_EN_3;		
	rg_type_t(HAL_SS_RX_SIGDET_1) SS_RX_SIGDET_1;		
	rg_type_t(HAL_XPON_INT_STA_3) XPON_INT_STA_3;	
	rg_type_t(HAL_RX_RESET_1) RX_RESET_1;	

	if(dbg_print)printf("SigDet_Int_Init, en %x\n",en);

	XPON_INT_STA_3.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3);	
	XPON_INT_STA_3.hal.rx_sigdet_int = 1;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3,XPON_INT_STA_3.dat.value);	

	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3,0x0);	
	XPON_INT_EN_3.dat.value = RG_R_PL(pma_base,(u32)PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_EN_3);	
	XPON_INT_EN_3.hal.rg_rx_sigdet_int_en = en;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_EN_3,XPON_INT_EN_3.dat.value);	

	SS_RX_SIGDET_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _SS_RX_SIGDET_1);	
	SS_RX_SIGDET_1.hal.rg_sigdet_en = en;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _SS_RX_SIGDET_1,SS_RX_SIGDET_1.dat.value);	

	RX_RESET_1.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1);	
	RX_RESET_1.hal.rg_sigdet_rst_b = 0;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1,RX_RESET_1.dat.value);
	
	RX_RESET_1.hal.rg_sigdet_rst_b = 1;
	RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _RX_RESET_1,RX_RESET_1.dat.value);
}
static u8 SigDet_IntEn_sta(void){
	rg_type_t(HAL_XPON_INT_EN_3) XPON_INT_EN_3;	
	XPON_INT_EN_3.dat.value = RG_R_PL(pma_base,(u32)PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_EN_3);
	return (u8) XPON_INT_EN_3.hal.rg_rx_sigdet_int_en;
}


void usxgmii_isr(void)
{	
	rg_type_t(HAL_xfi_pcs_int_sta_2) rg_xfi_pcs_int_sta_2; 	 
	rg_type_t(HAL_xfi_pcs_int_sta_3) rg_xfi_pcs_int_sta_3; 	 
	rg_type_t(HAL_xfi_pcs_int_sta_4) rg_xfi_pcs_int_sta_4; 	

	//rg_type_t(HAL_rg_xfi_pcs_int_ctrl_2) rg_xfi_pcs_int_ctrl_2;    
	//rg_type_t(HAL_rg_xfi_pcs_int_ctrl_3) rg_xfi_pcs_int_ctrl_3;	
	//rg_type_t(HAL_rg_xfi_pcs_int_ctrl_4) rg_xfi_pcs_int_ctrl_4;

	//rg_type_t(HAL_XPON_INT_EN_3) XPON_INT_EN_3;	
	rg_type_t(HAL_XPON_INT_STA_3) XPON_INT_STA_3;		

	u8 signal = XSGMII_SigDet_A_EN? RX_SigDet_Flag(): RX_SigDet_Flag_D();
	u16 sync = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ro_base_r_10gb_t_pcs_stus1);
	u8 sd_int = 0;
	
	if(dbg_print) printf("eth xsgmii: usxgmii_isr signal %x, sync %x\n",signal,sync);
//SD int
	if(SigDet_IntEn_sta()){
		XPON_INT_STA_3.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3);	
		if(dbg_print)printf("rx_sigdet_int %x\n",XPON_INT_STA_3.hal.rx_sigdet_int);

		if(XPON_INT_STA_3.hal.rx_sigdet_int)
		{			
			sd_int = 1;
			if(dbg_print)printf("---------------rx_sigdet_int isr *** ---------------\n");
			if(RX_CDR_LFP_L2D_sta()== 1) RX_CDR_RST();
			if(RX_RDY_Sta()== 0) RX_RDY(1,0);
			SigDet_Int_Init(0);
			//if(XSGMII_SigDet_Wrapper_EN) MAC_SigDet_Wrapper();
		}
		
		RG_W_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3,XPON_INT_STA_3.dat.value);	
		if(dbg_print){
			XPON_INT_STA_3.dat.value = RG_R_PL(pma_base,PMA_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _XPON_INT_STA_3);	
			printf("_XPON_INT_STA_3 %x\n",XPON_INT_STA_3.dat.value);
		}
		if(dbg_print)printf("---------------rx_sigdet_int isr &&&---------------\n");
		
		if(sd_int){
			sd_int = 0;
			return;
		}
	}	

//Linkup int	
	rg_xfi_pcs_int_sta_2.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2);	 
	rg_xfi_pcs_int_sta_3.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3);	 
	rg_xfi_pcs_int_sta_4.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4);	
	if(dbg_print) printf("Interrupt sta rg_xfi_pcs_int_sta_2= %x,rg_xfi_pcs_int_sta_3= %x,rg_xfi_pcs_int_sta_4= %x\n",rg_xfi_pcs_int_sta_2.dat.value,rg_xfi_pcs_int_sta_3.dat.value,rg_xfi_pcs_int_sta_4.dat.value);

	if(dbg_print)printf("link_up_st_int %x\n",rg_xfi_pcs_int_sta_3.hal.link_up_st_int);
	if(rg_xfi_pcs_int_sta_3.hal.link_up_st_int)	{
		if(dbg_print)printf("---------------link_up_st_int isr *** ---------------\n");
		if(signal) sync = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ro_base_r_10gb_t_pcs_stus1);
		if(dbg_print)printf("link_up_st_int sync = %x\n",sync);
		if((0x100d == sync) && signal) {
			//if(XSGMII_Linkup_Wrapper_EN)MAC_Linkup_Wrapper();
			linkup_sta = 1;
			//if(USX_FORCE_USR_DATA) xsgmii_force_data(0,8,0,0);
			xsgmii_force_data(0,8,0,0);
			//printf("xsgmii_force_data 0 8 0 0\n");
		}	
		else linkup_sta = 0;
		if(dbg_print)printf("---------------link_up_st_int isr &&& ---------------\n");
	}
	else linkup_sta = 0;

//Linkdn int		
	if(dbg_print)printf("link_down_st_int %x\n",rg_xfi_pcs_int_sta_4.hal.link_down_st_int);
	if(rg_xfi_pcs_int_sta_4.hal.link_down_st_int){
		if(dbg_print)printf("---------------link_down_st_int isr *** ---------------\n");
		if(((0x100d != sync) && !signal )||((0x100d != sync) && XSGMII_Linkdn_Ignone_SD_EN)){
			linkdn_sta = 1;
			//if(USX_FORCE_USR_DATA)xsgmii_force_data(0,8,1,0);
			xsgmii_force_data(0,8,1,0);
			RX_RDY(0,0);
			RX_CDR_LFP_L2D(1,0);			
			#if 0
				TMR_INI();
			#else
				SigDet_Int_Init(1);
			#endif
			//if(XSGMII_Linkdn_Wrapper_EN)MAC_Linkdn_Wrapper();
			XSI_MAC_LOGIC_RESET();
			//printf("------XSI_MAC_LOGIC_RESET----------\n");

		}
		else linkdn_sta = 0;
		if(dbg_print)printf("---------------link_down_st_int isr &&& ---------------\n");
	}
	else linkdn_sta = 0;
		
	if(dbg_print)printf("usxgmii linkup_sta=%x,linkdn_sta=%x\n",linkup_sta,linkdn_sta);
	
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2,0x1010101);
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3,0x1010101);
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4,0x1);
	
	if(dbg_print){
		rg_xfi_pcs_int_sta_2.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2);	 
		rg_xfi_pcs_int_sta_3.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3);	 
		rg_xfi_pcs_int_sta_4.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4);	
		printf("After clear, rg_xfi_pcs_int_sta_2= %x,rg_xfi_pcs_int_sta_3= %x,rg_xfi_pcs_int_sta_4= %x\n",rg_xfi_pcs_int_sta_2.dat.value,rg_xfi_pcs_int_sta_3.dat.value,rg_xfi_pcs_int_sta_4.dat.value);
	}
	
}

static void usxgmii_pcs_int_init(u8 en){
	
	rg_type_t(HAL_rg_xfi_pcs_int_ctrl_2) rg_xfi_pcs_int_ctrl_2;    
	rg_type_t(HAL_rg_xfi_pcs_int_ctrl_3) rg_xfi_pcs_int_ctrl_3;	
	rg_type_t(HAL_rg_xfi_pcs_int_ctrl_4) rg_xfi_pcs_int_ctrl_4;

	printf("usxgmii_pcs_int en %x\n",en);
			RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_0,0x00); 				
			RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_1,0x00); 				
			RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_2,0x00); 				
			RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_3,0x00); 	
			RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_4,0x00);

	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2,0x1010101);
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3,0x1010101);
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4,0x1);
	

			rg_xfi_pcs_int_ctrl_2.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_2);	
			rg_xfi_pcs_int_ctrl_3.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_3);	
			rg_xfi_pcs_int_ctrl_4.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_4);				

	rg_xfi_pcs_int_ctrl_2.hal.rg_r_type_e_int_en =0;			
	rg_xfi_pcs_int_ctrl_2.hal.rg_rxpcs_fsm_dec_err_int_en =0;	
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_2,rg_xfi_pcs_int_ctrl_2.dat.value);					
	rg_xfi_pcs_int_ctrl_3.hal.rg_hi_ber_st_int_en=0;	
	rg_xfi_pcs_int_ctrl_3.hal.rg_link_up_st_int_en=en;		
	rg_xfi_pcs_int_ctrl_3.hal.rg_rx_block_lock_st_int_en=0;
	rg_xfi_pcs_int_ctrl_3.hal.rg_fail_sync_xor_st_int_en=0;
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_3,rg_xfi_pcs_int_ctrl_3.dat.value);		
	rg_xfi_pcs_int_ctrl_4.hal.rg_link_down_st_int_en=en; 
	RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _rg_xfi_pcs_int_ctrl_4,rg_xfi_pcs_int_ctrl_4.dat.value);
}

static u32 IO_GPHYA_REG_BITS(u32 reg_name,u32 end_index,u32 start_index)
{
	if((end_index>=start_index)&&(end_index<32))
	{
		if((end_index==31)&&(start_index==0))
		{
			return (read_reg_word(reg_name)) ; 
		}
		else
		{
			return ((read_reg_word(reg_name)>>start_index) & ((1<<(end_index-start_index+1))-1)) ;
		}
	}
	else
	{
		//printk("%s end_index=%d	start_index=%d Error!\r\n",__FUNCTION__,end_index,start_index);
		return 0;
	}
}

static void IO_SPHYA_REG_BITS(u32 reg_name,u32 end_index,u32 start_index,u32 value)
{
	u32 data;
	data=read_reg_word(reg_name);
	
	if((end_index>=start_index)&&(end_index<32))
	{	
		if((end_index==31)&&(start_index==0))
		{
			write_reg_word(reg_name,value);
		}
		else
		{
			write_reg_word(reg_name,((data & ~(((1<<(end_index-start_index+1))-1)<<start_index)) | ((value&((1<<(end_index-start_index+1))-1))<<start_index))) ;
		}
	}
	else
	{
		//printk("%s end_index=%d	start_index=%d Error!\r\n",__FUNCTION__,end_index,start_index);
	}
}

static u8 XFI_ETH_RX_SigDet_Flag(void)
{	
	u8 i,cnt = 0;		

	for (i=0;i<=5;i++){				
		cnt = IO_GPHYA_REG_BITS(0x1fa7e290, 24, 24);  	
		cnt += cnt;
	}
	if(dbg_print)printf("RX_SigDet_Flag, cnt %x\n",cnt);	
	
	return cnt >= 4? 1:0;  
}


static u8 XFI_ETH_RX_SigDet_OUT_Read(void)
{		
	return IO_GPHYA_REG_BITS(0x1fa7e290, 24, 24);  
}


__attribute__((unused))static u8 XFI_ETH_RX_SigDet_Flag_D(void)
{
	uint XPON_INT_STA_3 = 0;

	IO_SPHYA_REG_BITS(0x1fa7e474, 16, 16, 0x0); //rg_rx_sigdet_int_en
	IO_SPHYA_REG_BITS(0x1fa7e16c, 0, 0, 0x0); //rg_sigdet_en
	IO_SPHYA_REG_BITS(0x1fa7e208, 8, 8, 0x0); //rg_sigdet_rst_b
	IO_SPHYA_REG_BITS(0x1fa7e208, 8, 8, 0x1); //rg_sigdet_rst_b
	IO_SPHYA_REG_BITS(0x1fa7e47c, 16, 16, 0x1); //rg_sigdet_int	
	udelay(50);	

	XPON_INT_STA_3 = IO_GPHYA_REG_BITS(0x1fa7e47c, 16, 16);  //rg_sigdet_int
	IO_SPHYA_REG_BITS(0x1fa7e16c, 0, 0, 0x0); //rg_sigdet_en	
	
	if(dbg_print)printf("RX_SigDet_Flag_D %x\n",XPON_INT_STA_3);
	return XPON_INT_STA_3;
}




static void XFI_ETH_SigDet_Int_Init(u8 en)
{
	if(dbg_print)printf("SigDet_Int_Init, en %x\n",en);

	IO_SPHYA_REG_BITS(0x1fa7e47c, 16, 16, 0x1); //rg_sigdet_int	
	IO_SPHYA_REG_BITS(0x1fa7e47c, 31, 0, 0x0); //rg_sigdet_int	
	IO_SPHYA_REG_BITS(0x1fa7e474, 16, 16, en); //rg_rx_sigdet_int_en	
	IO_SPHYA_REG_BITS(0x1fa7e16c, 0, 0, en); //rg_sigdet_en	
	IO_SPHYA_REG_BITS(0x1fa7e208, 8, 8, 0x0); //rg_sigdet_rst_b	
	IO_SPHYA_REG_BITS(0x1fa7e208, 8, 8, 0x1); //rg_sigdet_rst_b	
}



static u8 XFI_ETH_SigDet_IntEn_sta(void)
{
	u8 read_data=0;
	read_data = IO_GPHYA_REG_BITS(0x1fa7e474, 16, 16);  //rg_rx_sigdet_int_en	
	return (u8)read_data;
}



static u32 XFI_ETH_SigDet_Int_sta3_read(void)
{	
	uint read_data=0;
	read_data = IO_GPHYA_REG_BITS(0x1fa7e47c, 31, 0);  	
	return read_data;
}



static void XFI_ETH_SigDet_Int_sta3_write(u32 data)
{	
	IO_SPHYA_REG_BITS(0x1fa7e47c, 31, 0, data); //rg_sigdet_rst_b	
}



static u8 XFI_ETH_RX_CDR_LFP_L2D_sta(void)
 {
	u8 sta; 
	sta = IO_GPHYA_REG_BITS(0x1fa7e818, 8, 8);  //rg_force_sel_da_pxp_cdr_lpf_lck2data	
	if(dbg_print) printf("RX_CDR_LFP_L2D_sta %x\n",sta);
	return sta;//Read 1fa7b818 bit8 
}

static u8 XFI_ETH_RX_RDY_Sta(void)
{
	uint read_data=0;
	read_data = IO_GPHYA_REG_BITS(0x1fa7e10c, 24, 24);  //rg_disb_rx_dly
	if(dbg_print) printf("RX_RDY_Sta %x \n",read_data);	
	return read_data ;	
}


static void XFI_ETH_RX_RDY(u8 mod,u8 sel)
{	
	if(dbg_print) printf("RX_RDY %x, sel %x\n",mod,sel);

	IO_SPHYA_REG_BITS(0x1fa7e10c, 24, 24, mod); //rg_disb_rx_rdy	
	IO_SPHYA_REG_BITS(0x1fa7e114, 24, 24, sel); //rg_force_rx_rdy	
	
}

static void XFI_ETH_RX_CDR_LFP_L2D(u8 mod,u8 sel)
{
	if(dbg_print) printf("RX_CDR_LFP_L2D mode %x, sel %x\n",mod,sel);
	IO_SPHYA_REG_BITS(0x1fa7e818, 8, 8, mod); //rg_force_sel_da_pxp_cdr_lpf_lck2data	
	IO_SPHYA_REG_BITS(0x1fa7e818, 0, 0, sel); //rg_force_da_pxp_cdr_lpf_lck2data	
}



static void XFI_ETH_RX_CDR_LPF_RSTB(u8 mod,u8 sel)
{	
	if(dbg_print) printf("RX_CDR_LPF_RSTB mode %x, sel%x\n",mod,sel);

	IO_SPHYA_REG_BITS(0x1fa7e818, 24, 24, mod); //rg_force_sel_da_pxp_cdr_lpf_rstb
	IO_SPHYA_REG_BITS(0x1fa7e818, 16, 16, sel); //rg_force_da_pxp_cdr_lpf_rstb
}
static void XFI_ETH_RX_CDR_RST(void)
{
    if(dbg_print) printf("RX_CDR_RST\n");

	XFI_ETH_RX_CDR_LFP_L2D(1,0);
	XFI_ETH_RX_CDR_LPF_RSTB(1,0);
	udelay(700);
	XFI_ETH_RX_CDR_LPF_RSTB(1,1);
	udelay(100);
	XFI_ETH_RX_CDR_LFP_L2D(1,1);

	//switch to auto
	XFI_ETH_RX_CDR_LPF_RSTB(0,1);
	XFI_ETH_RX_CDR_LFP_L2D(0,1);
}


void AN7583_usxgmii_isr(void)
{
        rg_type_t(HAL_xfi_pcs_int_sta_2) rg_xfi_pcs_int_sta_2;   
        rg_type_t(HAL_xfi_pcs_int_sta_3) rg_xfi_pcs_int_sta_3;   
        rg_type_t(HAL_xfi_pcs_int_sta_4) rg_xfi_pcs_int_sta_4;  
    
        //rg_type_t(HAL_rg_xfi_pcs_int_ctrl_2) rg_xfi_pcs_int_ctrl_2;    
        //rg_type_t(HAL_rg_xfi_pcs_int_ctrl_3) rg_xfi_pcs_int_ctrl_3; 
        //rg_type_t(HAL_rg_xfi_pcs_int_ctrl_4) rg_xfi_pcs_int_ctrl_4;
    
        //rg_type_t(HAL_XPON_INT_EN_3) XPON_INT_EN_3; 
        rg_type_t(HAL_XPON_INT_STA_3) XPON_INT_STA_3;
        u8 i=0,sig_det_out=0,sig_det_accum=0;
        u8 signal = XFI_ETH_RX_SigDet_Flag();//XSGMII_SigDet_A_EN? XFI_ETH_RX_SigDet_Flag(): XFI_ETH_RX_SigDet_Flag_D();
        u16 sync = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ro_base_r_10gb_t_pcs_stus1);
        u8 sd_int = 0;
    
        if(dbg_print)printf("eth xsgmii: usxgmii_isr signal %x, sync %x\n",signal,sync);
        //Signal Detect Interrupt
        if(XFI_ETH_SigDet_IntEn_sta())
        {
            XPON_INT_STA_3.dat.value = XFI_ETH_SigDet_Int_sta3_read();    
            if(dbg_print)printf("rx_sigdet_int %x\n",XPON_INT_STA_3.hal.rx_sigdet_int);
    
            if(XPON_INT_STA_3.hal.rx_sigdet_int)
            {           
                sd_int = 1;
                if(dbg_print)printf("---------------rx_sigdet_int isr *** ---------------\n");
                if(XFI_ETH_RX_CDR_LFP_L2D_sta()== 1) XFI_ETH_RX_CDR_RST();
                if(XFI_ETH_RX_RDY_Sta()== 0) XFI_ETH_RX_RDY(1,0);
                //Eth_Ser_plug_reset (XFI_PLUG_IN , spd_sel_bk);
                XFI_ETH_SigDet_Int_Init(0);
                //if(XSGMII_SigDet_Wrapper_EN) MAC_SigDet_Wrapper();
            }
            XFI_ETH_SigDet_Int_sta3_write(XPON_INT_STA_3.dat.value);
            if(dbg_print){
                XPON_INT_STA_3.dat.value = XFI_ETH_SigDet_Int_sta3_read();    
                printf("_XPON_INT_STA_3 %x\n",XPON_INT_STA_3.dat.value);
            }
            if(dbg_print)printf("---------------rx_sigdet_int isr &&&---------------\n");
            
            if(sd_int){
                sd_int = 0;
                return;
            }
        }    
    //Linkup int    
        rg_xfi_pcs_int_sta_2.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2);  
        rg_xfi_pcs_int_sta_3.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3);  
        rg_xfi_pcs_int_sta_4.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4); 
        if(dbg_print)printf("Interrupt sta rg_xfi_pcs_int_sta_2= %x,rg_xfi_pcs_int_sta_3= %x,rg_xfi_pcs_int_sta_4= %x\n",rg_xfi_pcs_int_sta_2.dat.value,rg_xfi_pcs_int_sta_3.dat.value,rg_xfi_pcs_int_sta_4.dat.value);
    
        if(dbg_print)printf("link_up_st_int %x\n",rg_xfi_pcs_int_sta_3.hal.link_up_st_int);
        if(rg_xfi_pcs_int_sta_3.hal.link_up_st_int) 
        {
            if(dbg_print)printf("---------------link_up_st_int isr *** ---------------\n");
            
            //signal = XSGMII_SigDet_A_EN? XFI_ETH_RX_SigDet_Flag(): XFI_ETH_RX_SigDet_Flag_D();
    
            sig_det_accum = 0;
            for(i=0;i<100;i++)
            {
                sig_det_out = XFI_ETH_RX_SigDet_OUT_Read();
                sync = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ro_base_r_10gb_t_pcs_stus1);
                sig_det_accum += sig_det_out;
                //if(dbg_print)printf("sig_det_out = %d, sync = %x\n",sig_det_out,sync);
            }
            if(dbg_print)printf("sig_det_accum = %d\n",sig_det_accum);
            if(sig_det_accum >95) signal=1;
            else signal=0;
    
            if(dbg_print)printf("signal = %x\n",signal);
            sync = 0;
            if(signal) sync = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _ro_base_r_10gb_t_pcs_stus1);
            if(dbg_print)printf("link_up_st_int sync = %x\n",sync);
            if((0x100d == sync) && signal) 
            {
                //if(XSGMII_Linkup_Wrapper_EN)MAC_Linkup_Wrapper(); 
                linkup_sta = 1;
                //if(USX_FORCE_USR_DATA)
                    xsgmii_force_data(0,8,0,0);
            }   
            else linkup_sta = 0;
            if(dbg_print)printf("---------------link_up_st_int isr &&& ---------------\n");
        }
        else linkup_sta = 0;
    
    //Linkdn int        
        if(dbg_print)printf("link_down_st_int %x\n",rg_xfi_pcs_int_sta_4.hal.link_down_st_int);
        if(rg_xfi_pcs_int_sta_4.hal.link_down_st_int){
            if(dbg_print)printf("---------------link_down_st_int isr *** ---------------\n");
            //if(((0x100d != sync) && !signal )||((0x100d != sync) && XSGMII_Linkdn_Ignone_SD_EN)||(!linkup_sta && !signal && XSGMII_Linkdn_after_linkup_EN)){
            if(((0x100d != sync) && !signal )||((0x100d != sync) && XSGMII_Linkdn_Ignone_SD_EN)){

                linkdn_sta = 1;
                //if(USX_FORCE_USR_DATA)
                    xsgmii_force_data(0,8,1,0);

                XFI_ETH_RX_RDY(0,0);
                XFI_ETH_RX_CDR_LFP_L2D(1,0);                       
                //Eth_Ser_plug_reset (XFI_PLUG_OUT , spd_sel_bk);
                udelay(2000);
                XFI_ETH_SigDet_Int_Init(1);

                //if(XSGMII_Linkdn_Wrapper_EN)MAC_Linkdn_Wrapper();
                XSI_MAC_LOGIC_RESET();
            }
            else linkdn_sta = 0;
            if(dbg_print)printf("---------------link_down_st_int isr &&& ---------------\n");
        }
        else linkdn_sta = 0;
            
        if(dbg_print)printf("usxgmii linkup_sta=%x,linkdn_sta=%x\n",linkup_sta,linkdn_sta);
    
        //Clear all interrupt state flag
        RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2,0x1010101);
        RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3,0x1010101);
        RG_W_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4,0x1);
        
        if(dbg_print){
            rg_xfi_pcs_int_sta_2.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_2);  
            rg_xfi_pcs_int_sta_3.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_3);  
            rg_xfi_pcs_int_sta_4.dat.value = RG_R_PL(pcs1_base,(ulong)USXGMII_PCS1_BASE_OFFSET,(u32)(uintptr_t)(RgAddr) _xfi_pcs_int_sta_4); 
            printf("After clear, rg_xfi_pcs_int_sta_2= %x,rg_xfi_pcs_int_sta_3= %x,rg_xfi_pcs_int_sta_4= %x\n",rg_xfi_pcs_int_sta_2.dat.value,rg_xfi_pcs_int_sta_3.dat.value,rg_xfi_pcs_int_sta_4.dat.value);
        }

}

void AN7583_XFI_10G_PMA_BringUp(void)
{
    
    // XFI_DIG_reset
    write_reg_word(0x1fa7e460 ,0x0);
    
    // XFI_TXPLL
    write_reg_word(0x1fa7e854 ,0x1000000);
    write_reg_word(0x1fa7e004 ,0x1100a01);
    write_reg_word(0x1fa7e444 ,0x11300fa);
    write_reg_word(0x1fa7e448 ,0x9b0210);
    write_reg_word(0x1fa7e440 ,0x40026);
    write_reg_word(0x1fa7e468 ,0x28000);
    write_reg_word(0x1fa7e028 ,0x303);
    write_reg_word(0x1fa7e024 ,0x10100);
    write_reg_word(0x1fa7e794 ,0x1000000);
    write_reg_word(0x1fa7e798 ,0x33900000);
    write_reg_word(0x1fa7e048 ,0x67200000);
    write_reg_word(0x1fa7e04c ,0x67200000);
    
    // XFI_TX
    write_reg_word(0x1fa7f0c4 ,0x1010401);
    write_reg_word(0x1fa7f000 ,0x10040101);
    write_reg_word(0x1fa7e874 ,0x1010000);
    write_reg_word(0x1fa7e778 ,0x1000100);
    write_reg_word(0x1fa7e780 ,0x1000100);
    write_reg_word(0x1fa7e778 ,0x100010e);
    write_reg_word(0x1fa7e780 ,0x1000104);
    write_reg_word(0x1fa7e77c ,0x1050000);
    write_reg_word(0x1fa7e784 ,0x102);
    write_reg_word(0x1fa7e580 ,0x2);
    
    // XFI_RX
    write_reg_word(0x1fa7e08c ,0x101);
    write_reg_word(0x1fa7e104 ,0x2);
    write_reg_word(0x1fa7e090 ,0x3e80002);
    write_reg_word(0x1fa7e09c ,0x3e80002);
    write_reg_word(0x1fa7e094 ,0x3e80002);
    write_reg_word(0x1fa7e098 ,0x3e80002);
    write_reg_word(0x1fa7e120 ,0x103);
    write_reg_word(0x1fa7e068 ,0x24001f0);
    write_reg_word(0x1fa7e068 ,0x24001c0);
    write_reg_word(0x1fa7e088 ,0x0);
    write_reg_word(0x1fa7e088 ,0x1);
    write_reg_word(0x1fa7f0d4 ,0x4cc31030);
    write_reg_word(0x1fa7f120 ,0x3ff00);
    write_reg_word(0x1fa7f0dc ,0x0);
    write_reg_word(0x1fa7e814 ,0x1000000);
    write_reg_word(0x1fa7e814 ,0x1010000);
    write_reg_word(0x1fa7e88c ,0x100);
    write_reg_word(0x1fa7f0d4 ,0x4cc31030);
    write_reg_word(0x1fa7e88c ,0x101);
    write_reg_word(0x1fa7e360 ,0x100);
    write_reg_word(0x1fa7e294 ,0x1);
    write_reg_word(0x1fa7e300 ,0x1010100);
    write_reg_word(0x1fa7f0f8 ,0x4010808);
    write_reg_word(0x1fa7f0d8 ,0x100020a);
    write_reg_word(0x1fa7f10c ,0x70604);
    write_reg_word(0x1fa7f0cc ,0x1000000);
    write_reg_word(0x1fa7f0d8 ,0x1010242);
    write_reg_word(0x1fa7f0e8 ,0x2000000);
    write_reg_word(0x1fa7e76c ,0x1000000);
    write_reg_word(0x1fa7e374 ,0x2);
    
    // XFI_ANA
    write_reg_word(0x1fa7f084 ,0x101031b);
    write_reg_word(0x1fa7f088 ,0x0);
    write_reg_word(0x1fa7f088 ,0x1);
    write_reg_word(0x1fa7f0a8 ,0x10100);
    write_reg_word(0x1fa7f064 ,0x1040001);
    udelay(2000); //Align AE Recommend to 7581
    write_reg_word(0x1fa7f064 ,0x1040000);
    write_reg_word(0x1fa7f068 ,0x300);
    write_reg_word(0x1fa7f068 ,0x10300);
    write_reg_word(0x1fa7f068 ,0x10000);
    write_reg_word(0x1fa7f068 ,0x10001);
    write_reg_word(0x1fa7f06c ,0x1000003);
    write_reg_word(0x1fa7f080 ,0x82);
    write_reg_word(0x1fa7f080 ,0x0);
    write_reg_word(0x1fa7f07c ,0x0);
    write_reg_word(0x1fa7f084 ,0x1010000);
    write_reg_word(0x1fa7f098 ,0x0);
    write_reg_word(0x1fa7f050 ,0x1f05000c);
    write_reg_word(0x1fa7f050 ,0x1f0a000c);
    write_reg_word(0x1fa7f050 ,0x1f0a0018);
    write_reg_word(0x1fa7f054 ,0x180005);
    write_reg_word(0x1fa7f054 ,0x181605);
    write_reg_word(0x1fa7f06c ,0x1010003);
    write_reg_word(0x1fa7f054 ,0x181602);
    write_reg_word(0x1fa7f074 ,0x2000001);
    write_reg_word(0x1fa7f078 ,0x4040401);
    write_reg_word(0x1fa7f078 ,0x4040701);
    write_reg_word(0x1fa7f058 ,0x30003ff);
    write_reg_word(0x1fa7f058 ,0x30002ff);
    write_reg_word(0x1fa7f05c ,0x101);
    write_reg_word(0x1fa7f058 ,0x30002e4);
    write_reg_word(0x1fa7f054 ,0x181602);
    write_reg_word(0x1fa7f094 ,0x10010);
    write_reg_word(0x1fa7e858 ,0x100);
    write_reg_word(0x1fa7f05c ,0x101);
    write_reg_word(0x1fa7f070 ,0x4000b03);
    write_reg_word(0x1fa7f074 ,0x2000001);
    write_reg_word(0x1fa7f094 ,0x1000f);
    write_reg_word(0x1fa7f070 ,0x4000b03);
    write_reg_word(0x1fa7f074 ,0x2000001);
    write_reg_word(0x1fa7f06c ,0x1010003);
    write_reg_word(0x1fa7f0c0 ,0x4020000);
    write_reg_word(0x1fa7f0c0 ,0x2020000);
    write_reg_word(0x1fa7f118 ,0x1010100);
    write_reg_word(0x1fa7f11c ,0x401);
    write_reg_word(0x1fa7f0e8 ,0x800000);
    write_reg_word(0x1fa7f0e0 ,0x1078000);
    write_reg_word(0x1fa7f100 ,0x100);
    write_reg_word(0x1fa7f110 ,0x0);
    
    // XFI_TXPLL_ON
    write_reg_word(0x1fa7e074 ,0x10000);
    write_reg_word(0x1fa7e078 ,0x404);
    write_reg_word(0x1fa7e07c ,0x40004);
    write_reg_word(0x1fa7e080 ,0xff0000d0);
    write_reg_word(0x1fa7e118 ,0xa000a01);
    write_reg_word(0x1fa7e118 ,0xa01);
    write_reg_word(0x1fa7e118 ,0x1);
    write_reg_word(0x1fa7e11c ,0x1);
    write_reg_word(0x1fa7e160 ,0x1003100);
    write_reg_word(0x1fa7e160 ,0x1002e00);
    write_reg_word(0x1fa7e160 ,0x1012e01);
    write_reg_word(0x1fa7e164 ,0x500);
    write_reg_word(0x1fa7e164 ,0x0);
    write_reg_word(0x1fa7e100 ,0xc80005);
    write_reg_word(0x1fa7e100 ,0xa0005);
    write_reg_word(0x1fa7e144 ,0x0);
    write_reg_word(0x1fa7e06c ,0x1940);
    write_reg_word(0x1fa7e06c ,0x3f40);
    write_reg_word(0x1fa7e06c ,0x13f40);
    write_reg_word(0x1fa7e070 ,0x18);
    write_reg_word(0x1fa7e48c ,0x1000203);
    write_reg_word(0x1fa7e48c ,0x1000202);
    write_reg_word(0x1fa7e170 ,0xa503);
    write_reg_word(0x1fa7e170 ,0xa502);
    write_reg_word(0x1fa7e174 ,0x4010000);
    write_reg_word(0x1fa7e184 ,0x40001ff);
    write_reg_word(0x1fa7e178 ,0x20403);
    write_reg_word(0x1fa7e320 ,0x10101);
    write_reg_word(0x1fa7e200 ,0x101);
    write_reg_word(0x1fa7e200 ,0x1);
    write_reg_word(0x1fa7e148 ,0x109f0901);
    write_reg_word(0x1fa7e148 ,0x10f00a01);
    write_reg_word(0x1fa7e148 ,0x8700a01);
    write_reg_word(0x1fa7e854 ,0x1000000);
    write_reg_word(0x1fa7e854 ,0x1010000);
    write_reg_word(0x1fa7e000 ,0x1000000);
    write_reg_word(0x1fa7e854 ,0x1010100);
    write_reg_word(0x1fa7e854 ,0x1010101);
    write_reg_word(0x1fa7f094 ,0x1000f);
    write_reg_word(0x1fa7f060 ,0x101);
    write_reg_word(0x1fa7f094 ,0xf);
    write_reg_word(0x1fa7f04c ,0x0);
    
    // XFI_TXPLL_ON
    write_reg_word(0x1fa7e260 ,0x1);
    write_reg_word(0x1fa7e410 ,0x100);
    write_reg_word(0x1fa7e410 ,0x101);
    write_reg_word(0x1fa7e260 ,0x101);
    
    // XFI_RX_preset
    write_reg_word(0x1fa7f114 ,0x40200);
    write_reg_word(0x1fa7f114 ,0x20200);
    write_reg_word(0x1fa7f110 ,0x3000000);
    write_reg_word(0x1fa7f10c ,0x70604);
    write_reg_word(0x1fa7e114 ,0x0);
    write_reg_word(0x1fa7e10c ,0x1010001);
    write_reg_word(0x1fa7e818 ,0x100);
    write_reg_word(0x1fa7e768 ,0x1000000);
    write_reg_word(0x1fa7e330 ,0x100);
    write_reg_word(0x1fa7e33c ,0x1010001);
    write_reg_word(0x1fa7e330 ,0x100);
    write_reg_word(0x1fa7e33c ,0x1000001);
    write_reg_word(0x1fa7e338 ,0x1010100);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e10c ,0x1010001);
    write_reg_word(0x1fa7e114 ,0x10000);
    
    // XFI_RX_on
    write_reg_word(0x1fa7e824 ,0x1000000);
    write_reg_word(0x1fa7e824 ,0x1010000);
    write_reg_word(0x1fa7e824 ,0x1010100);
    write_reg_word(0x1fa7e824 ,0x1010101);
    write_reg_word(0x1fa7e81c ,0x0);
    write_reg_word(0x1fa7e81c ,0x100);
    write_reg_word(0x1fa7e81c ,0x101);
    write_reg_word(0x1fa7e894 ,0x100);
    write_reg_word(0x1fa7e894 ,0x101);
    write_reg_word(0x1fa7e84c ,0x1000000);
    write_reg_word(0x1fa7e84c ,0x1010000);
    write_reg_word(0x1fa7e818 ,0x1000100);
    write_reg_word(0x1fa7e818 ,0x1010100);
    write_reg_word(0x1fa7e34c ,0x1000000);
    write_reg_word(0x1fa7e34c ,0x1010000);
    write_reg_word(0x1fa7e34c ,0x1010100);
    write_reg_word(0x1fa7e34c ,0x1010101);
    write_reg_word(0x1fa7e350 ,0x1);
    write_reg_word(0x1fa7e38c ,0x1);
    write_reg_word(0x1fa7f0fc ,0x80505);
    write_reg_word(0x1fa7e108 ,0x1010001);
    write_reg_word(0x1fa7e108 ,0x1000001);
    write_reg_word(0x1fa7e108 ,0x1);
    write_reg_word(0x1fa7e10c ,0x1010000);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e10c ,0x1000000);
    write_reg_word(0x1fa7f100 ,0x100);
    write_reg_word(0x1fa7f108 ,0x0);
    write_reg_word(0x1fa7e460 ,0x2);
    write_reg_word(0x1fa7e460 ,0x22);
    write_reg_word(0x1fa7e818 ,0x1000100);
    write_reg_word(0x1fa7e818 ,0x1010100);
    
    // XFI_RX_L2R
    write_reg_word(0x1fa7e818 ,0x1010100);
    write_reg_word(0x1fa7e818 ,0x1000100);
    write_reg_word(0x1fa7e818 ,0x1010100);
    
    // XFI_RX_OSCal
    write_reg_word(0x1fa7e33c ,0x1000000);
    write_reg_word(0x1fa7e330 ,0x101);
    write_reg_word(0x1fa7e83c ,0x1000000);
    write_reg_word(0x1fa7e83c ,0x1010000);
    write_reg_word(0x1fa7e840 ,0x1000000);
    write_reg_word(0x1fa7e840 ,0x1010000);
    write_reg_word(0x1fa7e840 ,0x1010100);
    write_reg_word(0x1fa7e840 ,0x1010101);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e10c ,0x1000000);
    write_reg_word(0x1fa7e110 ,0x0);
    write_reg_word(0x1fa7e114 ,0x10000);
    write_reg_word(0x1fa7e110 ,0x1);
    
    // XFI_RX_pical
    write_reg_word(0x1fa7e308 ,0x1010101);
    write_reg_word(0x1fa7e15c ,0x400);
    write_reg_word(0x1fa7e118 ,0x8);
    write_reg_word(0x1fa7e204 ,0x1000101);
    write_reg_word(0x1fa7e328 ,0x0);
    write_reg_word(0x1fa7e334 ,0x1010001);
    write_reg_word(0x1fa7e328 ,0x0);
    write_reg_word(0x1fa7e334 ,0x1010000);
    write_reg_word(0x1fa7e31c ,0x1010100);
    write_reg_word(0x1fa7e318 ,0x0);
    write_reg_word(0x1fa7e324 ,0x10101);
    write_reg_word(0x1fa7e110 ,0x1);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e30c ,0x0);
    write_reg_word(0x1fa7e204 ,0x1010101);
    write_reg_word(0x1fa7e328 ,0x100);
    write_reg_word(0x1fa7e328 ,0x101);
    write_reg_word(0x1fa7e318 ,0x100);
    write_reg_word(0x1fa7e110 ,0x101);
    write_reg_word(0x1fa7e110 ,0x1);
    write_reg_word(0x1fa7e318 ,0x0);
    write_reg_word(0x1fa7e30c ,0x1);
    
    // XFI_RX_pdos
    write_reg_word(0x1fa7e894 ,0x1000101);
    write_reg_word(0x1fa7e894 ,0x1010101);
    write_reg_word(0x1fa7e114 ,0x10000);
    write_reg_word(0x1fa7e10c ,0x1000000);
    write_reg_word(0x1fa7e304 ,0x1010101);
    write_reg_word(0x1fa7e308 ,0x1010101);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e338 ,0x1010100);
    write_reg_word(0x1fa7e084 ,0x101);
    write_reg_word(0x1fa7e084 ,0x1);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e338 ,0x10100);
    write_reg_word(0x1fa7e084 ,0x1);
    write_reg_word(0x1fa7e084 ,0x0);
    write_reg_word(0x1fa7e200 ,0x20001);
    write_reg_word(0x1fa7e328 ,0x101);
    write_reg_word(0x1fa7e334 ,0x1000000);
    write_reg_word(0x1fa7e208 ,0x10100);
    write_reg_word(0x1fa7e110 ,0x1);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e110 ,0x0);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e328 ,0x10101);
    write_reg_word(0x1fa7e208 ,0x10101);
    write_reg_word(0x1fa7e110 ,0x10000);
    write_reg_word(0x1fa7e110 ,0x0);
    write_reg_word(0x1fa7e084 ,0x0);
    write_reg_word(0x1fa7e084 ,0x100);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e338 ,0x1010100);
    write_reg_word(0x1fa7e084 ,0x100);
    write_reg_word(0x1fa7e084 ,0x101);
    write_reg_word(0x1fa7e894 ,0x1010101);
    write_reg_word(0x1fa7e894 ,0x1000101);
    
    // XFI_RX_feos
    write_reg_word(0x1fa7e114 ,0x10000);
    write_reg_word(0x1fa7e10c ,0x1000000);
    write_reg_word(0x1fa7e308 ,0x1010101);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e338 ,0x1010100);
    write_reg_word(0x1fa7e144 ,0x30);
    write_reg_word(0x1fa7e32c ,0x0);
    write_reg_word(0x1fa7e338 ,0x1000100);
    write_reg_word(0x1fa7e204 ,0x1010001);
    write_reg_word(0x1fa7e110 ,0x0);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e110 ,0x1);
    write_reg_word(0x1fa7e108 ,0x0);
    write_reg_word(0x1fa7e32c ,0x10000);
    write_reg_word(0x1fa7e204 ,0x1010101);
    write_reg_word(0x1fa7e110 ,0x1000001);
    write_reg_word(0x1fa7e110 ,0x1);
    write_reg_word(0x1fa7e110 ,0x0);
    
    // XFI_RX_sdcal
    write_reg_word(0x1fa7e898 ,0x100);
    write_reg_word(0x1fa7e840 ,0x1010101);
    write_reg_word(0x1fa7e204 ,0x10101);
    write_reg_word(0x1fa7e32c ,0x10000);
    write_reg_word(0x1fa7e10c ,0x1000000);
    write_reg_word(0x1fa7e338 ,0x1000000);
    write_reg_word(0x1fa7e114 ,0x10000);
    write_reg_word(0x1fa7e204 ,0x1010101);
    write_reg_word(0x1fa7e32c ,0x10100);
    write_reg_word(0x1fa7e114 ,0x10001);
    write_reg_word(0x1fa7e114 ,0x10000);
    write_reg_word(0x1fa7e898 ,0x100);
    write_reg_word(0x1fa7e83c ,0x1010000);
    write_reg_word(0x1fa7e83c ,0x1000000);
    write_reg_word(0x1fa7e840 ,0x1010101);
    write_reg_word(0x1fa7e840 ,0x1000101);
    write_reg_word(0x1fa7e840 ,0x1000100);
    
    // XFI_phy_status
    write_reg_word(0x1fa7e114 ,0x10100);
    write_reg_word(0x1fa7e10c ,0x1000000);
    
    // XFI_DIG_reset_release
    write_reg_word(0x1fa7e460 ,0x7f);
    
    //XFI_L2D
    write_reg_word(0x1fa7e818 ,0x1010101);
    write_reg_word(0x1fa7e818 ,0x1000101);
    write_reg_word(0x1fa7e818 ,0x1010101);
    
    // XFI_RX_rxrdy
    write_reg_word(0x1fa7e114 ,0x1010100);
    write_reg_word(0x1fa7e10c ,0x0);
    write_reg_word(0x1fa7e460 ,0x7e);
    write_reg_word(0x1fa7e460 ,0x30fff);


}

void AN7583_PCIE0_10G_PMA_BringUp(void)
{
         
         // PCIE_TXPLL
         write_reg_word(0x1fc7e854 , 0x1010101 );
         write_reg_word(0x1fc7e854 , 0x1000101 );
         write_reg_word(0x1fc7e034 , 0x1 );
         write_reg_word(0x1fc7e02c , 0x500 );
         write_reg_word(0x1fc7f0c8 , 0xf0000000 );
         write_reg_word(0x1fc7f0d8 , 0x101010a );
         write_reg_word(0x1fc7e004 , 0x1100a00 );
         write_reg_word(0x1fc7e004 , 0x1100a01 );
         write_reg_word(0x1fc7e444 , 0x11309c4 );
         write_reg_word(0x1fc7e444 , 0x11300fa );
         write_reg_word(0x1fc7e448 , 0x9b14a0 );
         write_reg_word(0x1fc7e448 , 0x9b0210 );
         write_reg_word(0x1fc7e440 , 0x409bf );
         write_reg_word(0x1fc7e440 , 0x40026 );
         write_reg_word(0x1fc7e468 , 0x28000 );
         write_reg_word(0x1fc7e028 , 0x303 );
         write_reg_word(0x1fc7e024 , 0x10100 );
         write_reg_word(0x1fc7e794 , 0x1000000 );
         write_reg_word(0x1fc7e034 , 0x1 );
         write_reg_word(0x1fc7e798 , 0x33900000 );
         write_reg_word(0x1fc7e048 , 0x67200000 );
         write_reg_word(0x1fc7e04c , 0x67200000 );
         write_reg_word(0x1fc7f0c4 , 0x1010401 );
         
         // PCIE0_TX
         write_reg_word(0x1fc7f0c4 , 0x1010401 );
         write_reg_word(0x1fc7f000 , 0x10040001 );
         write_reg_word(0x1fc7f000 , 0x10040101 );
         write_reg_word(0x1fc7e874 , 0x1000000 );
         write_reg_word(0x1fc7e874 , 0x1010000 );
         write_reg_word(0x1fc7e77c , 0x1000101 );
         write_reg_word(0x1fc7e77c , 0x1050101 );
         write_reg_word(0x1fc7e784 , 0x100 );
         write_reg_word(0x1fc7e784 , 0x102 );
         write_reg_word(0x1fc7e580 , 0x2 );
         
         // PCIE0_RX
         write_reg_word(0x1fc7e08c , 0x101 );
         write_reg_word(0x1fc7e104 , 0x2 );
         write_reg_word(0x1fc7e08c , 0x101 );
         write_reg_word(0x1fc7e090 , 0x3e80002 );
         write_reg_word(0x1fc7e09c , 0x3e80002 );
         write_reg_word(0x1fc7e094 , 0x3e80002 );
         write_reg_word(0x1fc7e098 , 0x3e80002 );
         write_reg_word(0x1fc7e120 , 0x103 );
         write_reg_word(0x1fc7e068 , 0x24001f0 );
         write_reg_word(0x1fc7e068 , 0x24001c0 );
         write_reg_word(0x1fc7e088 , 0x0 );
         write_reg_word(0x1fc7f0d4 , 0xccc31030 );
         write_reg_word(0x1fc7f120 , 0x3ff08 );
         write_reg_word(0x1fc7f0dc , 0x0 );
         write_reg_word(0x1fc7f0dc , 0x0 );
         write_reg_word(0x1fc7f144 , 0x1000000 );
         write_reg_word(0x1fc7f148 , 0x1 );
         write_reg_word(0x1fc7f148 , 0x101 );
         write_reg_word(0x1fc7f148 , 0x10101 );
         write_reg_word(0x1fc7f148 , 0x1010101 );
         write_reg_word(0x1fc7e88c , 0x103 );
         write_reg_word(0x1fc7e88c , 0x101 );
         write_reg_word(0x1fc7f0d4 , 0xccc31030 );
         write_reg_word(0x1fc7e360 , 0xf0100 );
         write_reg_word(0x1fc7e294 , 0x1 );
         write_reg_word(0x1fc7e300 , 0x1010100 );
         write_reg_word(0x1fc7f0f8 , 0x4020808 );
         write_reg_word(0x1fc7f0d8 , 0x101020a );
         write_reg_word(0x1fc7f10c , 0x70604 );
         write_reg_word(0x1fc7f0cc , 0x1000000 );
         write_reg_word(0x1fc7f0d8 , 0x101020a );
         write_reg_word(0x1fc7f0d8 , 0x1010242 );
         write_reg_word(0x1fc7f0e8 , 0x2000000 );
         write_reg_word(0x1fc7f0cc , 0x1000000 );
         write_reg_word(0x1fc7e76c , 0x1000000 );
         write_reg_word(0x1fc7e374 , 0x2 );
         
         // PCIE0_ANA
         write_reg_word(0x1fc7f0a0 , 0x1000d09 );
         write_reg_word(0x1fc7f084 , 0x1010000 );
         write_reg_word(0x1fc7f088 , 0x0 );
         write_reg_word(0x1fc7f088 , 0x1 );
         write_reg_word(0x1fc7f064 , 0x1040001 );
         udelay(2000); //Align AE Recommend to 7581
         write_reg_word(0x1fc7f064 , 0x1040000 );
         write_reg_word(0x1fc7f068 , 0x0 );
         write_reg_word(0x1fc7f06c , 0x1000003 );
         write_reg_word(0x1fc7f080 , 0x0 );
         write_reg_word(0x1fc7f07c , 0x0 );
         write_reg_word(0x1fc7f084 , 0x1010000 );
         write_reg_word(0x1fc7f050 , 0x1f05012d );
         write_reg_word(0x1fc7f050 , 0x1f0a012d );
         write_reg_word(0x1fc7f050 , 0x1f0a002d );
         write_reg_word(0x1fc7f054 , 0x180001 );
         write_reg_word(0x1fc7f054 , 0x181601 );
         write_reg_word(0x1fc7f054 , 0x181602 );
         write_reg_word(0x1fc7f068 , 0x10000 );
         write_reg_word(0x1fc7f06c , 0x1010003 );
         write_reg_word(0x1fc7f068 , 0x10001 );
         write_reg_word(0x1fc7f090 , 0xff0000 );
         write_reg_word(0x1fc7f050 , 0x1f0a0018 );
         write_reg_word(0x1fc7f074 , 0x2000301 );
         write_reg_word(0x1fc7f078 , 0x4040701 );
         write_reg_word(0x1fc7f058 , 0x30004e4 );
         write_reg_word(0x1fc7f058 , 0x30002e4 );
         write_reg_word(0x1fc7f05c , 0x1 );
         write_reg_word(0x1fc7f058 , 0x30002e4 );
         write_reg_word(0x1fc7f054 , 0x181602 );
         write_reg_word(0x1fc7f094 , 0x1000f );
         write_reg_word(0x1fc7e858 , 0x100 );
         write_reg_word(0x1fc7f05c , 0x1 );
         write_reg_word(0x1fc7f05c , 0x101 );
         write_reg_word(0x1fc7f074 , 0x2000301 );
         write_reg_word(0x1fc7f094 , 0x1000f );
         write_reg_word(0x1fc7f070 , 0x4000b03 );
         write_reg_word(0x1fc7f074 , 0x2000001 );
         write_reg_word(0x1fc7f06c , 0x1010003 );
         write_reg_word(0x1fc7f0c0 , 0x4020000 );
         write_reg_word(0x1fc7f0c0 , 0x2020000 );
         write_reg_word(0x1fc7e778 , 0x1000000 );
         write_reg_word(0x1fc7e778 , 0x1000100 );
         write_reg_word(0x1fc7e780 , 0x100 );
         write_reg_word(0x1fc7e780 , 0x1000100 );
         write_reg_word(0x1fc7e778 , 0x1010100 );
         write_reg_word(0x1fc7e780 , 0x100010d );
         write_reg_word(0x1fc7f118 , 0x1010100 );
         write_reg_word(0x1fc7f11c , 0x2000401 );
         write_reg_word(0x1fc7f0e8 , 0x800000 );
         write_reg_word(0x1fc7f0e0 , 0x1078000 );
         write_reg_word(0x1fc7f100 , 0x100 );
         write_reg_word(0x1fc7f110 , 0x1000200 );
         write_reg_word(0x1fc7e088 , 0x1 );
         
         // PCIE0_TXPLL_ON
         write_reg_word(0x1fc7e074 , 0x10000 );
         write_reg_word(0x1fc7e078 , 0x404 );
         write_reg_word(0x1fc7e07c , 0x40004 );
         write_reg_word(0x1fc7e080 , 0xff000000 );
         write_reg_word(0x1fc7e080 , 0xff0000d0 );
         write_reg_word(0x1fc7e118 , 0xa000a01 );
         write_reg_word(0x1fc7e118 , 0xa01 );
         write_reg_word(0x1fc7e118 , 0x1 );
         write_reg_word(0x1fc7e11c , 0x1 );
         write_reg_word(0x1fc7e160 , 0x1003100 );
         write_reg_word(0x1fc7e160 , 0x1002e00 );
         write_reg_word(0x1fc7e160 , 0x1002e01 );
         write_reg_word(0x1fc7e160 , 0x1012e01 );
         write_reg_word(0x1fc7e164 , 0x0 );
         write_reg_word(0x1fc7e100 , 0xc80005 );
         write_reg_word(0x1fc7e100 , 0xa0005 );
         write_reg_word(0x1fc7e144 , 0x0 );
         write_reg_word(0x1fc7e06c , 0x1940 );
         write_reg_word(0x1fc7e06c , 0x3f40 );
         write_reg_word(0x1fc7e06c , 0x13f40 );
         write_reg_word(0x1fc7e070 , 0x18 );
         write_reg_word(0x1fc7e48c , 0x1000203 );
         write_reg_word(0x1fc7e48c , 0x1000202 );
         write_reg_word(0x1fc7e170 , 0xa503 );
         write_reg_word(0x1fc7e174 , 0x4010000 );
         write_reg_word(0x1fc7e184 , 0x40001ff );
         write_reg_word(0x1fc7e178 , 0x20403 );
         write_reg_word(0x1fc7e320 , 0x10101 );
         write_reg_word(0x1fc7e200 , 0x101 );
         write_reg_word(0x1fc7e200 , 0x1 );
         write_reg_word(0x1fc7e148 , 0x109f0901 );
         write_reg_word(0x1fc7e148 , 0x109f0a01 );
         write_reg_word(0x1fc7e148 , 0x10f00a01 );
         write_reg_word(0x1fc7e148 , 0x8700a01 );
         write_reg_word(0x1fc7f048 , 0xf20ff );
         write_reg_word(0x1fc7e828 , 0x1010000 );
         write_reg_word(0x1fc7e854 , 0x1000101 );
         write_reg_word(0x1fc7e854 , 0x1010101 );
         write_reg_word(0x1fc7e000 , 0x1000000 );
         //udelay(6000);
         //printf("Delay 6000\n");
         write_reg_word(0x1fc7e854 , 0x1010101 );
         write_reg_word(0x1fc7f094 , 0x1000f );
         write_reg_word(0x1fc7f060 , 0x100 );
         write_reg_word(0x1fc7f060 , 0x101 );
         write_reg_word(0x1fc7f094 , 0xf );
         write_reg_word(0x1fc7f04c , 0x10001 );
         write_reg_word(0x1fc7e14c , 0x7fff0010 );
         write_reg_word(0x1fc7e14c , 0x7fff7fff );
         //udelay(5000);
         //printf("Delay 5000\n");
         // PCIE0_TXPLL_ON
         write_reg_word(0x1fc7e260 , 0x101 );
         write_reg_word(0x1fc7e410 , 0x100 );
         write_reg_word(0x1fc7e410 , 0x101 );
         write_reg_word(0x1fc7e260 , 0x101 );
         //udelay(100);
         
         // PCIE0_RX_preset
         write_reg_word(0x1fc7e264 , 0x2010000 );
         write_reg_word(0x1fc7e264 , 0x1010000 );
         write_reg_word(0x1fc7f114 , 0x1050200 );
         write_reg_word(0x1fc7f114 , 0x1020200 );
         write_reg_word(0x1fc7f110 , 0x3000200 );
         write_reg_word(0x1fc7f10c , 0x70604 );
         write_reg_word(0x1fc7e114 , 0x0 );
         write_reg_word(0x1fc7e10c , 0x1010000 );
         write_reg_word(0x1fc7e818 , 0x0 );
         write_reg_word(0x1fc7e818 , 0x100 );
         write_reg_word(0x1fc7e768 , 0x1000000 );
         write_reg_word(0x1fc7e330 , 0x100 );
         write_reg_word(0x1fc7e33c , 0x1010001 );
         write_reg_word(0x1fc7e330 , 0x100 );
         write_reg_word(0x1fc7e33c , 0x1000001 );
         write_reg_word(0x1fc7e338 , 0x1010100 );
         write_reg_word(0x1fc7e32c , 0x0 );
         
         // XFI_RX_on
         write_reg_word(0x1fc7e10c , 0x1010000 );
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e034 , 0x101 );
         write_reg_word(0x1fc7e02c , 0x500 );
         write_reg_word(0x1fc7e010 , 0x100 );
         write_reg_word(0x1fc7e824 , 0x1000000 );
         write_reg_word(0x1fc7e824 , 0x1010000 );
         write_reg_word(0x1fc7e824 , 0x1010100 );
         write_reg_word(0x1fc7e824 , 0x1010101 );
         write_reg_word(0x1fc7e81c , 0x101 );
         write_reg_word(0x1fc7e894 , 0x101 );
         write_reg_word(0x1fc7e84c , 0x1010000 );
         write_reg_word(0x1fc7e818 , 0x1000100 );
         write_reg_word(0x1fc7e818 , 0x1010100 );
         write_reg_word(0x1fc7e34c , 0x1000100 );
         write_reg_word(0x1fc7e34c , 0x1010100 );
         write_reg_word(0x1fc7e34c , 0x1010101 );
         write_reg_word(0x1fc7e350 , 0x1 );
         write_reg_word(0x1fc7e38c , 0x1 );
         write_reg_word(0x1fc7f0fc , 0x80505 );
         write_reg_word(0x1fc7e108 , 0x1010001 );
         write_reg_word(0x1fc7e108 , 0x1000001 );
         write_reg_word(0x1fc7e108 , 0x1 );
         write_reg_word(0x1fc7e10c , 0x1010000 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e808 , 0x100 );
         write_reg_word(0x1fc7f100 , 0x100 );
         write_reg_word(0x1fc7f108 , 0x10000 );
         write_reg_word(0x1fc7e460 , 0x17f );
         write_reg_word(0x1fc7e818 , 0x1000100 );
         write_reg_word(0x1fc7e818 , 0x1010100 );
         write_reg_word(0x1fc7e818 , 0x1000100 );
         
         // PCIE0_RX_L2R
         write_reg_word(0x1fc7e818 , 0x1010100 );
         write_reg_word(0x1fc7e33c , 0x1000000 );
         write_reg_word(0x1fc7e330 , 0x101 );
         
         // PCIE0_RX_OSCal
         write_reg_word(0x1fc7e83c , 0x1000000 );
         write_reg_word(0x1fc7e83c , 0x1010000 );
         write_reg_word(0x1fc7e840 , 0x1000000 );
         write_reg_word(0x1fc7e840 , 0x1010000 );
         write_reg_word(0x1fc7e814 , 0x1000000 );
         write_reg_word(0x1fc7e814 , 0x1010000 );
         write_reg_word(0x1fc7e840 , 0x1010100 );
         write_reg_word(0x1fc7e840 , 0x1010101 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e110 , 0x0 );
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e110 , 0x1 );
         
         // PCIE0_RX_pical
         write_reg_word(0x1fc7e308 , 0x1010101 );
         write_reg_word(0x1fc7e15c , 0x400 );
         write_reg_word(0x1fc7e118 , 0x8 );
         write_reg_word(0x1fc7e204 , 0x1000101 );
         write_reg_word(0x1fc7e328 , 0x0 );
         write_reg_word(0x1fc7e334 , 0x1010001 );
         write_reg_word(0x1fc7e328 , 0x0 );
         write_reg_word(0x1fc7e334 , 0x1010000 );
         write_reg_word(0x1fc7e31c , 0x1010100 );
         write_reg_word(0x1fc7e318 , 0x0 );
         write_reg_word(0x1fc7e324 , 0x10101 );
         write_reg_word(0x1fc7e110 , 0x1 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e30c , 0x0 );
         write_reg_word(0x1fc7e204 , 0x1010101 );
         write_reg_word(0x1fc7e328 , 0x100 );
         write_reg_word(0x1fc7e328 , 0x101 );
         write_reg_word(0x1fc7e318 , 0x100 );
         write_reg_word(0x1fc7e110 , 0x101 );
         write_reg_word(0x1fc7e110 , 0x1 );
         write_reg_word(0x1fc7e318 , 0x0 );
         write_reg_word(0x1fc7e30c , 0x1 );
         
         // PCIE0_RX_pdos
         write_reg_word(0x1fc7e894 , 0x1000101 );
         write_reg_word(0x1fc7e894 , 0x1010101 );
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e304 , 0x1010101 );
         write_reg_word(0x1fc7e308 , 0x1010101 );
         write_reg_word(0x1fc7e32c , 0x0 );
         write_reg_word(0x1fc7e338 , 0x1010100 );
         write_reg_word(0x1fc7e084 , 0x101 );
         write_reg_word(0x1fc7e084 , 0x1 );
         write_reg_word(0x1fc7e32c , 0x0 );
         write_reg_word(0x1fc7e338 , 0x10100 );
         write_reg_word(0x1fc7e084 , 0x1 );
         write_reg_word(0x1fc7e084 , 0x0 );
         write_reg_word(0x1fc7e200 , 0x20001 );
         write_reg_word(0x1fc7e328 , 0x101 );
         write_reg_word(0x1fc7e334 , 0x1000000 );
         write_reg_word(0x1fc7e208 , 0x10100 );
         write_reg_word(0x1fc7e110 , 0x1 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e110 , 0x0 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e328 , 0x10101 );
         write_reg_word(0x1fc7e208 , 0x10101 );
         write_reg_word(0x1fc7e110 , 0x10000 );
         write_reg_word(0x1fc7e110 , 0x0 );
         write_reg_word(0x1fc7e084 , 0x0 );
         write_reg_word(0x1fc7e084 , 0x100 );
         write_reg_word(0x1fc7e32c , 0x0 );
         write_reg_word(0x1fc7e338 , 0x1010100 );
         write_reg_word(0x1fc7e084 , 0x100 );
         write_reg_word(0x1fc7e084 , 0x101 );
         write_reg_word(0x1fc7e894 , 0x1010101 );
         write_reg_word(0x1fc7e894 , 0x1000101 );
         
         // PCIE0_RX_feos
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e308 , 0x1010101 );
         write_reg_word(0x1fc7e32c , 0x0 );
         write_reg_word(0x1fc7e338 , 0x1010100 );
         write_reg_word(0x1fc7e144 , 0x30 );
         write_reg_word(0x1fc7e32c , 0x0 );
         write_reg_word(0x1fc7e338 , 0x1000100 );
         write_reg_word(0x1fc7e204 , 0x1010001 );
         write_reg_word(0x1fc7e110 , 0x0 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e110 , 0x1 );
         write_reg_word(0x1fc7e108 , 0x0 );
         write_reg_word(0x1fc7e32c , 0x10000 );
         write_reg_word(0x1fc7e204 , 0x1010101 );
         write_reg_word(0x1fc7e110 , 0x1000001 );
         write_reg_word(0x1fc7e110 , 0x1 );
         write_reg_word(0x1fc7e110 , 0x0 );
         
         // PCIE0_RX_sdcal
         write_reg_word(0x1fc7e898 , 0x100 );
         write_reg_word(0x1fc7e898 , 0x101 );
         write_reg_word(0x1fc7e840 , 0x1010101 );
         write_reg_word(0x1fc7e204 , 0x10101 );
         write_reg_word(0x1fc7e32c , 0x10000 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e338 , 0x1000000 );
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e204 , 0x1010101 );
         write_reg_word(0x1fc7e32c , 0x10100 );
         write_reg_word(0x1fc7e114 , 0x10001 );
         write_reg_word(0x1fc7e114 , 0x10000 );
         write_reg_word(0x1fc7e898 , 0x101 );
         write_reg_word(0x1fc7e898 , 0x100 );
         write_reg_word(0x1fc7e83c , 0x1010000 );
         write_reg_word(0x1fc7e83c , 0x1000000 );
         write_reg_word(0x1fc7e840 , 0x1010101 );
         write_reg_word(0x1fc7e840 , 0x1000101 );
         write_reg_word(0x1fc7e840 , 0x1000100 );
         
         // PCIE0_phy_status
         write_reg_word(0x1fc7e114 , 0x10100 );
         write_reg_word(0x1fc7e10c , 0x1000000 );
         write_reg_word(0x1fc7e460 , 0x17e );
         write_reg_word(0x1fc7e460 , 0x13e );
         write_reg_word(0x1fc7e460 , 0x11e );
         write_reg_word(0x1fc7e460 , 0x10e );
         write_reg_word(0x1fc7e460 , 0x10a );
         write_reg_word(0x1fc7e460 , 0x108 );
         write_reg_word(0x1fc7e460 , 0x100 );
         write_reg_word(0x1fc7e460 , 0x120 );
         write_reg_word(0x1fc7e460 , 0x130 );
         write_reg_word(0x1fc7e460 , 0x138 );
         write_reg_word(0x1fc7e460 , 0x139 );
         write_reg_word(0x1fc7e460 , 0x179 );
         write_reg_word(0x1fc7e460 , 0x17d );
         write_reg_word(0x1fc7e460 , 0x17f );
          
         //PCIE0_L2D
         write_reg_word(0x1fc7e818 , 0x1010100 );
         write_reg_word(0x1fc7e818 , 0x1000100 );
         write_reg_word(0x1fc7e818 , 0x1010100 );
         write_reg_word(0x1fc7e818 , 0x1010101 );
          
         // PCIE0_RX_rxrdy
         write_reg_word(0x1fc7e114 , 0x1010100 );
         write_reg_word(0x1fc7e10c , 0x0 );
         write_reg_word(0x1fc7e460 , 0x17e );
         write_reg_word(0x1fc7e460 , 0x17f );
         write_reg_word(0x1fc7e460 , 0x30fff );
}
void AN7583_PON_XFI_10G_PMA_BringUp(void)
{
       
        // XFI_DIG_reset
        write_reg_word(0x1fa8e460 ,0x0);
        
        // XFI_TXPLL
        write_reg_word(0x1fa8e854 ,0x1000000);
        write_reg_word(0x1fa8e004 ,0x1100a01);
        write_reg_word(0x1fa8e444 ,0x11300fa);
        write_reg_word(0x1fa8e448 ,0x9b0210);
        write_reg_word(0x1fa8e440 ,0x40026);
        write_reg_word(0x1fa8e468 ,0x28000);
        write_reg_word(0x1fa8e028 ,0x303);
        write_reg_word(0x1fa8e024 ,0x10100);
        write_reg_word(0x1fa8e794 ,0x1000000);
        write_reg_word(0x1fa8e798 ,0x33900000);
        write_reg_word(0x1fa8e048 ,0x67200000);
        write_reg_word(0x1fa8e04c ,0x67200000);
        
        // XFI_TX
        write_reg_word(0x1fa8f0c4 ,0x1010401);
        write_reg_word(0x1fa8f000 ,0x10040101);
        write_reg_word(0x1fa8e874 ,0x1010000);
        write_reg_word(0x1fa8e778 ,0x1000100);
        write_reg_word(0x1fa8e780 ,0x1000100);
        write_reg_word(0x1fa8e778 ,0x100010e);
        write_reg_word(0x1fa8e780 ,0x1000104);
        write_reg_word(0x1fa8e77c ,0x1050000);
        write_reg_word(0x1fa8e784 ,0x102);
        write_reg_word(0x1fa8e580 ,0x2);
        
        // XFI_RX
        write_reg_word(0x1fa8e08c ,0x101);
        write_reg_word(0x1fa8e104 ,0x2);
        write_reg_word(0x1fa8e090 ,0x3e80002);
        write_reg_word(0x1fa8e09c ,0x3e80002);
        write_reg_word(0x1fa8e094 ,0x3e80002);
        write_reg_word(0x1fa8e098 ,0x3e80002);
        write_reg_word(0x1fa8e120 ,0x103);
        write_reg_word(0x1fa8e068 ,0x24001f0);
        write_reg_word(0x1fa8e068 ,0x24001c0);
        write_reg_word(0x1fa8e088 ,0x0);
        write_reg_word(0x1fa8e088 ,0x1);
        write_reg_word(0x1fa8f0d4 ,0x4cc31030);
        write_reg_word(0x1fa8f120 ,0x3ff00);
        write_reg_word(0x1fa8f0dc ,0x0);
        write_reg_word(0x1fa8e814 ,0x1000000);
        write_reg_word(0x1fa8e814 ,0x1010000);
        write_reg_word(0x1fa8e88c ,0x100);
        write_reg_word(0x1fa8f0d4 ,0x4cc31030);
        write_reg_word(0x1fa8e88c ,0x101);
        write_reg_word(0x1fa8e360 ,0x100);
        write_reg_word(0x1fa8e294 ,0x1);
        write_reg_word(0x1fa8e300 ,0x1010100);
        write_reg_word(0x1fa8f0f8 ,0x4010808);
        write_reg_word(0x1fa8f0d8 ,0x100020a);
        write_reg_word(0x1fa8f10c ,0x70604);
        write_reg_word(0x1fa8f0cc ,0x1000000);
        write_reg_word(0x1fa8f0d8 ,0x1010242);
        write_reg_word(0x1fa8f0e8 ,0x2000000);
        write_reg_word(0x1fa8e76c ,0x1000000);
        write_reg_word(0x1fa8e374 ,0x2);
        
        // XFI_ANA
        write_reg_word(0x1fa8f084 ,0x101031b);
        write_reg_word(0x1fa8f088 ,0x0);
        write_reg_word(0x1fa8f088 ,0x1);
        write_reg_word(0x1fa8f0a8 ,0x10100);
        write_reg_word(0x1fa8f064 ,0x1040001);
        udelay(2000); //Align AE Recommend to 7581
        write_reg_word(0x1fa8f064 ,0x1040000);
        write_reg_word(0x1fa8f068 ,0x300);
        write_reg_word(0x1fa8f068 ,0x10300);
        write_reg_word(0x1fa8f068 ,0x10000);
        write_reg_word(0x1fa8f068 ,0x10001);
        write_reg_word(0x1fa8f06c ,0x1000003);
        write_reg_word(0x1fa8f080 ,0x82);
        write_reg_word(0x1fa8f080 ,0x0);
        write_reg_word(0x1fa8f07c ,0x0);
        write_reg_word(0x1fa8f084 ,0x1010000);
        write_reg_word(0x1fa8f098 ,0x0);
        write_reg_word(0x1fa8f050 ,0x1f05000c);
        write_reg_word(0x1fa8f050 ,0x1f0a000c);
        write_reg_word(0x1fa8f050 ,0x1f0a0018);
        write_reg_word(0x1fa8f054 ,0x180005);
        write_reg_word(0x1fa8f054 ,0x181605);
        write_reg_word(0x1fa8f06c ,0x1010003);
        write_reg_word(0x1fa8f054 ,0x181602);
        write_reg_word(0x1fa8f074 ,0x2000001);
        write_reg_word(0x1fa8f078 ,0x4040401);
        write_reg_word(0x1fa8f078 ,0x4040701);
        write_reg_word(0x1fa8f058 ,0x30003ff);
        write_reg_word(0x1fa8f058 ,0x30002ff);
        write_reg_word(0x1fa8f05c ,0x101);
        write_reg_word(0x1fa8f058 ,0x30002e4);
        write_reg_word(0x1fa8f054 ,0x181602);
        write_reg_word(0x1fa8f094 ,0x10010);
        write_reg_word(0x1fa8e858 ,0x100);
        write_reg_word(0x1fa8f05c ,0x101);
        write_reg_word(0x1fa8f070 ,0x4000b03);
        write_reg_word(0x1fa8f074 ,0x2000001);
        write_reg_word(0x1fa8f094 ,0x1000f);
        write_reg_word(0x1fa8f070 ,0x4000b03);
        write_reg_word(0x1fa8f074 ,0x2000001);
        write_reg_word(0x1fa8f06c ,0x1010003);
        write_reg_word(0x1fa8f0c0 ,0x4020000);
        write_reg_word(0x1fa8f0c0 ,0x2020000);
        write_reg_word(0x1fa8f118 ,0x1010100);
        write_reg_word(0x1fa8f11c ,0x401);
        write_reg_word(0x1fa8f0e8 ,0x800000);
        write_reg_word(0x1fa8f0e0 ,0x1078000);
        write_reg_word(0x1fa8f100 ,0x100);
        write_reg_word(0x1fa8f110 ,0x0);
        
        // XFI_TXPLL_ON
        write_reg_word(0x1fa8e074 ,0x10000);
        write_reg_word(0x1fa8e078 ,0x404);
        write_reg_word(0x1fa8e07c ,0x40004);
        write_reg_word(0x1fa8e080 ,0xff0000d0);
        write_reg_word(0x1fa8e118 ,0xa000a01);
        write_reg_word(0x1fa8e118 ,0xa01);
        write_reg_word(0x1fa8e118 ,0x1);
        write_reg_word(0x1fa8e11c ,0x1);
        write_reg_word(0x1fa8e160 ,0x1003100);
        write_reg_word(0x1fa8e160 ,0x1002e00);
        write_reg_word(0x1fa8e160 ,0x1012e01);
        write_reg_word(0x1fa8e164 ,0x500);
        write_reg_word(0x1fa8e164 ,0x0);
        write_reg_word(0x1fa8e100 ,0xc80005);
        write_reg_word(0x1fa8e100 ,0xa0005);
        write_reg_word(0x1fa8e144 ,0x0);
        write_reg_word(0x1fa8e06c ,0x1940);
        write_reg_word(0x1fa8e06c ,0x3f40);
        write_reg_word(0x1fa8e06c ,0x13f40);
        write_reg_word(0x1fa8e070 ,0x18);
        write_reg_word(0x1fa8e48c ,0x1000203);
        write_reg_word(0x1fa8e48c ,0x1000202);
        write_reg_word(0x1fa8e170 ,0xa503);
        write_reg_word(0x1fa8e170 ,0xa502);
        write_reg_word(0x1fa8e174 ,0x4010000);
        write_reg_word(0x1fa8e184 ,0x40001ff);
        write_reg_word(0x1fa8e178 ,0x20403);
        write_reg_word(0x1fa8e320 ,0x10101);
        write_reg_word(0x1fa8e200 ,0x101);
        write_reg_word(0x1fa8e200 ,0x1);
        write_reg_word(0x1fa8e148 ,0x109f0901);
        write_reg_word(0x1fa8e148 ,0x10f00a01);
        write_reg_word(0x1fa8e148 ,0x8700a01);
        write_reg_word(0x1fa8e854 ,0x1000000);
        write_reg_word(0x1fa8e854 ,0x1010000);
        write_reg_word(0x1fa8e000 ,0x1000000);
        write_reg_word(0x1fa8e854 ,0x1010100);
        write_reg_word(0x1fa8e854 ,0x1010101);
        write_reg_word(0x1fa8f094 ,0x1000f);
        write_reg_word(0x1fa8f060 ,0x101);
        write_reg_word(0x1fa8f094 ,0xf);
        write_reg_word(0x1fa8f04c ,0x0);
        
        // XFI_TXPLL_ON
        write_reg_word(0x1fa8e260 ,0x1);
        write_reg_word(0x1fa8e410 ,0x100);
        write_reg_word(0x1fa8e410 ,0x101);
        write_reg_word(0x1fa8e260 ,0x101);
        
        // XFI_RX_preset
        write_reg_word(0x1fa8f114 ,0x40200);
        write_reg_word(0x1fa8f114 ,0x20200);
        write_reg_word(0x1fa8f110 ,0x3000000);
        write_reg_word(0x1fa8f10c ,0x70604);
        write_reg_word(0x1fa8e114 ,0x0);
        write_reg_word(0x1fa8e10c ,0x1010001);
        write_reg_word(0x1fa8e818 ,0x100);
        write_reg_word(0x1fa8e768 ,0x1000000);
        write_reg_word(0x1fa8e330 ,0x100);
        write_reg_word(0x1fa8e33c ,0x1010001);
        write_reg_word(0x1fa8e330 ,0x100);
        write_reg_word(0x1fa8e33c ,0x1000001);
        write_reg_word(0x1fa8e338 ,0x1010100);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e10c ,0x1010001);
        write_reg_word(0x1fa8e114 ,0x10000);
        
        // XFI_RX_on
        write_reg_word(0x1fa8e824 ,0x1000000);
        write_reg_word(0x1fa8e824 ,0x1010000);
        write_reg_word(0x1fa8e824 ,0x1010100);
        write_reg_word(0x1fa8e824 ,0x1010101);
        write_reg_word(0x1fa8e81c ,0x0);
        write_reg_word(0x1fa8e81c ,0x100);
        write_reg_word(0x1fa8e81c ,0x101);
        write_reg_word(0x1fa8e894 ,0x100);
        write_reg_word(0x1fa8e894 ,0x101);
        write_reg_word(0x1fa8e84c ,0x1000000);
        write_reg_word(0x1fa8e84c ,0x1010000);
        write_reg_word(0x1fa8e818 ,0x1000100);
        write_reg_word(0x1fa8e818 ,0x1010100);
        write_reg_word(0x1fa8e34c ,0x1000000);
        write_reg_word(0x1fa8e34c ,0x1010000);
        write_reg_word(0x1fa8e34c ,0x1010100);
        write_reg_word(0x1fa8e34c ,0x1010101);
        write_reg_word(0x1fa8e350 ,0x1);
        write_reg_word(0x1fa8e38c ,0x1);
        write_reg_word(0x1fa8f0fc ,0x80505);
        write_reg_word(0x1fa8e108 ,0x1010001);
        write_reg_word(0x1fa8e108 ,0x1000001);
        write_reg_word(0x1fa8e108 ,0x1);
        write_reg_word(0x1fa8e10c ,0x1010000);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e10c ,0x1000000);
        write_reg_word(0x1fa8f100 ,0x100);
        write_reg_word(0x1fa8f108 ,0x0);
        write_reg_word(0x1fa8e460 ,0x2);
        write_reg_word(0x1fa8e460 ,0x22);
        write_reg_word(0x1fa8e818 ,0x1000100);
        write_reg_word(0x1fa8e818 ,0x1010100);
        
        // XFI_RX_L2R
        write_reg_word(0x1fa8e818 ,0x1010100);
        write_reg_word(0x1fa8e818 ,0x1000100);
        write_reg_word(0x1fa8e818 ,0x1010100);
        
        // XFI_RX_OSCal
        write_reg_word(0x1fa8e33c ,0x1000000);
        write_reg_word(0x1fa8e330 ,0x101);
        write_reg_word(0x1fa8e83c ,0x1000000);
        write_reg_word(0x1fa8e83c ,0x1010000);
        write_reg_word(0x1fa8e840 ,0x1000000);
        write_reg_word(0x1fa8e840 ,0x1010000);
        write_reg_word(0x1fa8e840 ,0x1010100);
        write_reg_word(0x1fa8e840 ,0x1010101);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e10c ,0x1000000);
        write_reg_word(0x1fa8e110 ,0x0);
        write_reg_word(0x1fa8e114 ,0x10000);
        write_reg_word(0x1fa8e110 ,0x1);
        
        // XFI_RX_pical
        write_reg_word(0x1fa8e308 ,0x1010101);
        write_reg_word(0x1fa8e15c ,0x400);
        write_reg_word(0x1fa8e118 ,0x8);
        write_reg_word(0x1fa8e204 ,0x1000101);
        write_reg_word(0x1fa8e328 ,0x0);
        write_reg_word(0x1fa8e334 ,0x1010001);
        write_reg_word(0x1fa8e328 ,0x0);
        write_reg_word(0x1fa8e334 ,0x1010000);
        write_reg_word(0x1fa8e31c ,0x1010100);
        write_reg_word(0x1fa8e318 ,0x0);
        write_reg_word(0x1fa8e324 ,0x10101);
        write_reg_word(0x1fa8e110 ,0x1);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e30c ,0x0);
        write_reg_word(0x1fa8e204 ,0x1010101);
        write_reg_word(0x1fa8e328 ,0x100);
        write_reg_word(0x1fa8e328 ,0x101);
        write_reg_word(0x1fa8e318 ,0x100);
        write_reg_word(0x1fa8e110 ,0x101);
        write_reg_word(0x1fa8e110 ,0x1);
        write_reg_word(0x1fa8e318 ,0x0);
        write_reg_word(0x1fa8e30c ,0x1);
        
        // XFI_RX_pdos
        write_reg_word(0x1fa8e894 ,0x1000101);
        write_reg_word(0x1fa8e894 ,0x1010101);
        write_reg_word(0x1fa8e114 ,0x10000);
        write_reg_word(0x1fa8e10c ,0x1000000);
        write_reg_word(0x1fa8e304 ,0x1010101);
        write_reg_word(0x1fa8e308 ,0x1010101);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e338 ,0x1010100);
        write_reg_word(0x1fa8e084 ,0x101);
        write_reg_word(0x1fa8e084 ,0x1);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e338 ,0x10100);
        write_reg_word(0x1fa8e084 ,0x1);
        write_reg_word(0x1fa8e084 ,0x0);
        write_reg_word(0x1fa8e200 ,0x20001);
        write_reg_word(0x1fa8e328 ,0x101);
        write_reg_word(0x1fa8e334 ,0x1000000);
        write_reg_word(0x1fa8e208 ,0x10100);
        write_reg_word(0x1fa8e110 ,0x1);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e110 ,0x0);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e328 ,0x10101);
        write_reg_word(0x1fa8e208 ,0x10101);
        write_reg_word(0x1fa8e110 ,0x10000);
        write_reg_word(0x1fa8e110 ,0x0);
        write_reg_word(0x1fa8e084 ,0x0);
        write_reg_word(0x1fa8e084 ,0x100);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e338 ,0x1010100);
        write_reg_word(0x1fa8e084 ,0x100);
        write_reg_word(0x1fa8e084 ,0x101);
        write_reg_word(0x1fa8e894 ,0x1010101);
        write_reg_word(0x1fa8e894 ,0x1000101);
        
        // XFI_RX_feos
        write_reg_word(0x1fa8e114 ,0x10000);
        write_reg_word(0x1fa8e10c ,0x1000000);
        write_reg_word(0x1fa8e308 ,0x1010101);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e338 ,0x1010100);
        write_reg_word(0x1fa8e144 ,0x30);
        write_reg_word(0x1fa8e32c ,0x0);
        write_reg_word(0x1fa8e338 ,0x1000100);
        write_reg_word(0x1fa8e204 ,0x1010001);
        write_reg_word(0x1fa8e110 ,0x0);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e110 ,0x1);
        write_reg_word(0x1fa8e108 ,0x0);
        write_reg_word(0x1fa8e32c ,0x10000);
        write_reg_word(0x1fa8e204 ,0x1010101);
        write_reg_word(0x1fa8e110 ,0x1000001);
        write_reg_word(0x1fa8e110 ,0x1);
        write_reg_word(0x1fa8e110 ,0x0);
        
        // XFI_RX_sdcal
        write_reg_word(0x1fa8e898 ,0x100);
        write_reg_word(0x1fa8e840 ,0x1010101);
        write_reg_word(0x1fa8e204 ,0x10101);
        write_reg_word(0x1fa8e32c ,0x10000);
        write_reg_word(0x1fa8e10c ,0x1000000);
        write_reg_word(0x1fa8e338 ,0x1000000);
        write_reg_word(0x1fa8e114 ,0x10000);
        write_reg_word(0x1fa8e204 ,0x1010101);
        write_reg_word(0x1fa8e32c ,0x10100);
        write_reg_word(0x1fa8e114 ,0x10001);
        write_reg_word(0x1fa8e114 ,0x10000);
        write_reg_word(0x1fa8e898 ,0x100);
        write_reg_word(0x1fa8e83c ,0x1010000);
        write_reg_word(0x1fa8e83c ,0x1000000);
        write_reg_word(0x1fa8e840 ,0x1010101);
        write_reg_word(0x1fa8e840 ,0x1000101);
        write_reg_word(0x1fa8e840 ,0x1000100);
        
        // XFI_phy_status
        write_reg_word(0x1fa8e114 ,0x10100);
        write_reg_word(0x1fa8e10c ,0x1000000);
        
        // XFI_DIG_reset_release
        write_reg_word(0x1fa8e460 ,0x7f);
        
        //XFI_L2D
        write_reg_word(0x1fa8e818 ,0x1010101);
        write_reg_word(0x1fa8e818 ,0x1000101);
        write_reg_word(0x1fa8e818 ,0x1010101);
        
        // XFI_RX_rxrdy
        write_reg_word(0x1fa8e114 ,0x1010100);
        write_reg_word(0x1fa8e10c ,0x0);
        write_reg_word(0x1fa8e460 ,0x7e);
        write_reg_word(0x1fa8e460 ,0x30fff);
    
   

}

void AN7583_PON_XFI_1G_PMA_BringUp(void)
{
    
    // XFI_DIG_reset_hold
          write_reg_word(0x1fa8e460 ,0x0);
          
          // XFI_TXPLL
          write_reg_word(0x1fa8e854 ,0x1000000);
          write_reg_word(0x1fa8e004 ,0x1100a00);
          write_reg_word(0x1fa8e004 ,0x1100a01);
          write_reg_word(0x1fa8e444 ,0x11309c4);
          write_reg_word(0x1fa8e444 ,0x11300fa);
          write_reg_word(0x1fa8e448 ,0x9b14a0);
          write_reg_word(0x1fa8e448 ,0x9b0210);
          write_reg_word(0x1fa8e440 ,0x409bf);
          write_reg_word(0x1fa8e440 ,0x40026);
          write_reg_word(0x1fa8e468 ,0x28000);
          write_reg_word(0x1fa8e028 ,0x303);
          write_reg_word(0x1fa8e024 ,0x10100);
          write_reg_word(0x1fa8e794 ,0x1000000);
          write_reg_word(0x1fa8e798 ,0x32000000);
          write_reg_word(0x1fa8e048 ,0x64000000);
          write_reg_word(0x1fa8e04c ,0x64000000);
          
          
          // XFI_TX
          write_reg_word(0x1fa8f0c4 ,0x1010400);
          write_reg_word(0x1fa8f0c4 ,0x1010401);
          write_reg_word(0x1fa8f000 ,0x10040001);
          write_reg_word(0x1fa8f000 ,0x10040101);
          write_reg_word(0x1fa8e874 ,0x1000000);
          write_reg_word(0x1fa8e874 ,0x1010000);
          write_reg_word(0x1fa8e778 ,0x1000000);
          write_reg_word(0x1fa8e778 ,0x1000100);
          write_reg_word(0x1fa8e780 ,0x100);
          write_reg_word(0x1fa8e780 ,0x1000100);
          write_reg_word(0x1fa8e778 ,0x1000100);
          write_reg_word(0x1fa8e780 ,0x1000100);
          write_reg_word(0x1fa8e77c ,0x1000000);
          write_reg_word(0x1fa8e77c ,0x1020000);
          write_reg_word(0x1fa8e784 ,0x100);
          write_reg_word(0x1fa8e784 ,0x101);
          write_reg_word(0x1fa8e580 ,0x1);
          
          // XFI_RX
          write_reg_word(0x1fa8e08c ,0x101);
          write_reg_word(0x1fa8e104 ,0x2);
          write_reg_word(0x1fa8e08c ,0x101);
          write_reg_word(0x1fa8e090 ,0x3e80002);
          write_reg_word(0x1fa8e09c ,0x3e80002);
          write_reg_word(0x1fa8e094 ,0x3e80002);
          write_reg_word(0x1fa8e098 ,0x3e80002);
          write_reg_word(0x1fa8e120 ,0x103);
          write_reg_word(0x1fa8e068 ,0x24001f0);
          write_reg_word(0x1fa8e068 ,0x24001c0);
          write_reg_word(0x1fa8e088 ,0x0);
          write_reg_word(0x1fa8e088 ,0x1);
          write_reg_word(0x1fa8f0d4 ,0x4cc31030);
          write_reg_word(0x1fa8f120 ,0x3ff00);
          write_reg_word(0x1fa8f0dc ,0x0);
          write_reg_word(0x1fa8e814 ,0x1000000);
          write_reg_word(0x1fa8e814 ,0x1010000);
          write_reg_word(0x1fa8e88c ,0x100);
          write_reg_word(0x1fa8f0d4 ,0x4cc318b0);
          write_reg_word(0x1fa8e88c ,0x103);
          write_reg_word(0x1fa8e360 ,0x300);
          write_reg_word(0x1fa8e294 ,0x3);
          write_reg_word(0x1fa8e300 ,0x1010100);
          write_reg_word(0x1fa8f0f8 ,0x4010808);
          write_reg_word(0x1fa8f0d8 ,0x100010a);
          write_reg_word(0x1fa8f10c ,0x70604);
          write_reg_word(0x1fa8f0cc ,0x1000000);
          write_reg_word(0x1fa8f0d8 ,0x101010a);
          write_reg_word(0x1fa8f0d8 ,0x1010129);
          write_reg_word(0x1fa8f0e8 ,0x2000003);
          write_reg_word(0x1fa8f0cc ,0x1000000);
          write_reg_word(0x1fa8e76c ,0x1000000);
          write_reg_word(0x1fa8e76c ,0x1030000);
          write_reg_word(0x1fa8e374 ,0x0);
          
          // XFI_ANA
          write_reg_word(0x1fa8f084 ,0x101031b);
          write_reg_word(0x1fa8f088 ,0x0);
          write_reg_word(0x1fa8f088 ,0x1);
          write_reg_word(0x1fa8f0a8 ,0x10000);
          write_reg_word(0x1fa8f0a8 ,0x10100);
          write_reg_word(0x1fa8f064 ,0x1040001);
          write_reg_word(0x1fa8f064 ,0x1040000);
          write_reg_word(0x1fa8f068 ,0x300);
          write_reg_word(0x1fa8f068 ,0x0);
          write_reg_word(0x1fa8f06c ,0x1000003);
          write_reg_word(0x1fa8f080 ,0x82);
          write_reg_word(0x1fa8f080 ,0x0);
          write_reg_word(0x1fa8f07c ,0x0);
          write_reg_word(0x1fa8f084 ,0x1010000);
          write_reg_word(0x1fa8f098 ,0x0);
          write_reg_word(0x1fa8f050 ,0x1f05000c);
          write_reg_word(0x1fa8f050 ,0x1f050031);
          write_reg_word(0x1fa8f054 ,0x180005);
          write_reg_word(0x1fa8f054 ,0x180b05);
          write_reg_word(0x1fa8f06c ,0x1000003);
          write_reg_word(0x1fa8f054 ,0x180b02);
          write_reg_word(0x1fa8f074 ,0x3000001);
          write_reg_word(0x1fa8f078 ,0x4040401);
          write_reg_word(0x1fa8f078 ,0x4040701);
          write_reg_word(0x1fa8f058 ,0x30003ff);
          write_reg_word(0x1fa8f058 ,0x30002ff);
          write_reg_word(0x1fa8f05c ,0x101);
          write_reg_word(0x1fa8f058 ,0x30002e4);
          write_reg_word(0x1fa8f054 ,0x180b02);
          write_reg_word(0x1fa8f094 ,0x10010);
          write_reg_word(0x1fa8e858 ,0x100);
          write_reg_word(0x1fa8f05c ,0x101);
          write_reg_word(0x1fa8f070 ,0x4000b03);
          write_reg_word(0x1fa8f074 ,0x3000001);
          write_reg_word(0x1fa8f094 ,0x1000f);
          write_reg_word(0x1fa8f070 ,0x4000b03);
          write_reg_word(0x1fa8f074 ,0x3000001);
          write_reg_word(0x1fa8f06c ,0x1000003);
          write_reg_word(0x1fa8f0c0 ,0x4020000);
          write_reg_word(0x1fa8f0c0 ,0x2020000);
          write_reg_word(0x1fa8f118 ,0x1000000);
          write_reg_word(0x1fa8f118 ,0x1010000);
          write_reg_word(0x1fa8f118 ,0x1010100);
          write_reg_word(0x1fa8f11c ,0x401);
          write_reg_word(0x1fa8f0e8 ,0x800003);
          write_reg_word(0x1fa8f0e0 ,0x1078000);
          write_reg_word(0x1fa8f100 ,0x100);
          write_reg_word(0x1fa8f110 ,0x0);
          
          // XFI_TXPLL_ON
          write_reg_word(0x1fa8e074 ,0x10000);
          write_reg_word(0x1fa8e078 ,0x404);
          write_reg_word(0x1fa8e07c ,0x40004);
          write_reg_word(0x1fa8e080 ,0xff0000d0);
          write_reg_word(0x1fa8e118 ,0xa000a01);
          write_reg_word(0x1fa8e118 ,0xa01);
          write_reg_word(0x1fa8e118 ,0x1);
          write_reg_word(0x1fa8e11c ,0x1);
          write_reg_word(0x1fa8e160 ,0x1003100);
          write_reg_word(0x1fa8e160 ,0x1002e00);
          write_reg_word(0x1fa8e160 ,0x1002e01);
          write_reg_word(0x1fa8e160 ,0x1012e01);
          write_reg_word(0x1fa8e164 ,0x500);
          write_reg_word(0x1fa8e164 ,0x0);
          write_reg_word(0x1fa8e100 ,0xc80005);
          write_reg_word(0x1fa8e100 ,0xa0005);
          write_reg_word(0x1fa8e144 ,0x0);
          write_reg_word(0x1fa8e06c ,0x1940);
          write_reg_word(0x1fa8e06c ,0x3f40);
          write_reg_word(0x1fa8e06c ,0x13f40);
          write_reg_word(0x1fa8e070 ,0x18);
          write_reg_word(0x1fa8e48c ,0x1000203);
          write_reg_word(0x1fa8e48c ,0x1000202);
          write_reg_word(0x1fa8e170 ,0xa503);
          write_reg_word(0x1fa8e170 ,0xa502);
          write_reg_word(0x1fa8e174 ,0x4010000);
          write_reg_word(0x1fa8e184 ,0x40001ff);
          write_reg_word(0x1fa8e178 ,0x20403);
          write_reg_word(0x1fa8e320 ,0x10101);
          write_reg_word(0x1fa8e200 ,0x101);
          write_reg_word(0x1fa8e200 ,0x1);
          write_reg_word(0x1fa8e148 ,0x109f0a01);
          write_reg_word(0x1fa8e148 ,0x10f00a01);
          write_reg_word(0x1fa8e148 ,0x8700a01);
          write_reg_word(0x1fa8e854 ,0x1000000);
          write_reg_word(0x1fa8e854 ,0x1010000);
          write_reg_word(0x1fa8e000 ,0x1000000);
          write_reg_word(0x1fa8e854 ,0x1010100);
          write_reg_word(0x1fa8e854 ,0x1010101);
          write_reg_word(0x1fa8f094 ,0x1000f);
          write_reg_word(0x1fa8f060 ,0x101);
          write_reg_word(0x1fa8f094 ,0xf);
          write_reg_word(0x1fa8f04c ,0x0);
          
          // XFI_TX_ON
          write_reg_word(0x1fa8e260 ,0x1);
          write_reg_word(0x1fa8e410 ,0x100);
          write_reg_word(0x1fa8e410 ,0x101);
          write_reg_word(0x1fa8e260 ,0x101);
          
          // XFI_RX_preset
          write_reg_word(0x1fa8f114 ,0x40200);
          write_reg_word(0x1fa8f114 ,0x20200);
          write_reg_word(0x1fa8f110 ,0x3000000);
          write_reg_word(0x1fa8f10c ,0x70604);
          write_reg_word(0x1fa8e114 ,0x0);
          write_reg_word(0x1fa8e10c ,0x1010001);
          write_reg_word(0x1fa8e818 ,0x100);
          write_reg_word(0x1fa8e768 ,0x1000000);
          write_reg_word(0x1fa8e330 ,0x100);
          write_reg_word(0x1fa8e33c ,0x1010001);
          write_reg_word(0x1fa8e330 ,0x100);
          write_reg_word(0x1fa8e33c ,0x1000001);
          write_reg_word(0x1fa8e338 ,0x1010100);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e10c ,0x1010001);
          write_reg_word(0x1fa8e114 ,0x10000);
          
          // XFI_RX_on
          write_reg_word(0x1fa8e824 ,0x1000000);
          write_reg_word(0x1fa8e824 ,0x1010000);
          write_reg_word(0x1fa8e824 ,0x1010100);
          write_reg_word(0x1fa8e824 ,0x1010101);
          write_reg_word(0x1fa8e81c ,0x0);
          write_reg_word(0x1fa8e81c ,0x100);
          write_reg_word(0x1fa8e81c ,0x101);
          write_reg_word(0x1fa8e894 ,0x100);
          write_reg_word(0x1fa8e894 ,0x101);
          write_reg_word(0x1fa8e84c ,0x1000000);
          write_reg_word(0x1fa8e84c ,0x1010000);
          write_reg_word(0x1fa8e818 ,0x1000100);
          write_reg_word(0x1fa8e818 ,0x1010100);
          write_reg_word(0x1fa8e34c ,0x1000000);
          write_reg_word(0x1fa8e34c ,0x1010000);
          write_reg_word(0x1fa8e34c ,0x1010100);
          write_reg_word(0x1fa8e34c ,0x1010101);
          write_reg_word(0x1fa8e350 ,0x1);
          write_reg_word(0x1fa8e38c ,0x1);
          write_reg_word(0x1fa8f0fc ,0x80505);
          write_reg_word(0x1fa8e108 ,0x1010001);
          write_reg_word(0x1fa8e108 ,0x1000001);
          write_reg_word(0x1fa8e108 ,0x1);
          write_reg_word(0x1fa8e10c ,0x1010000);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e10c ,0x1000000);
          write_reg_word(0x1fa8f100 ,0x100);
          write_reg_word(0x1fa8f108 ,0x0);
          write_reg_word(0x1fa8e460 ,0x2);
          write_reg_word(0x1fa8e460 ,0x22);
          write_reg_word(0x1fa8e818 ,0x1000100);
          write_reg_word(0x1fa8e818 ,0x1010100);
          
          // XFI_RX_L2R
          write_reg_word(0x1fa8e818 ,0x1010100);
          write_reg_word(0x1fa8e818 ,0x1000100);
          write_reg_word(0x1fa8e818 ,0x1010100);
          
          // XFI_RX_OSCal
          write_reg_word(0x1fa8e33c ,0x1000000);
          write_reg_word(0x1fa8e330 ,0x101);
          write_reg_word(0x1fa8e83c ,0x1000000);
          write_reg_word(0x1fa8e83c ,0x1010000);
          write_reg_word(0x1fa8e840 ,0x1000000);
          write_reg_word(0x1fa8e840 ,0x1010000);
          write_reg_word(0x1fa8e840 ,0x1010100);
          write_reg_word(0x1fa8e840 ,0x1010101);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e10c ,0x1000000);
          write_reg_word(0x1fa8e110 ,0x0);
          write_reg_word(0x1fa8e114 ,0x10000);
          write_reg_word(0x1fa8e110 ,0x1);
          
          // XFI_RX_pical
          write_reg_word(0x1fa8e308 ,0x1010101);
          write_reg_word(0x1fa8e15c ,0x400);
          write_reg_word(0x1fa8e118 ,0x8);
          write_reg_word(0x1fa8e204 ,0x1000101);
          write_reg_word(0x1fa8e328 ,0x0);
          write_reg_word(0x1fa8e334 ,0x1010001);
          write_reg_word(0x1fa8e328 ,0x0);
          write_reg_word(0x1fa8e334 ,0x1010000);
          write_reg_word(0x1fa8e31c ,0x1010100);
          write_reg_word(0x1fa8e318 ,0x0);
          write_reg_word(0x1fa8e324 ,0x10101);
          write_reg_word(0x1fa8e110 ,0x1);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e30c ,0x0);
          write_reg_word(0x1fa8e204 ,0x1010101);
          write_reg_word(0x1fa8e328 ,0x100);
          write_reg_word(0x1fa8e328 ,0x101);
          write_reg_word(0x1fa8e318 ,0x100);
          write_reg_word(0x1fa8e110 ,0x101);
          write_reg_word(0x1fa8e110 ,0x1);
          write_reg_word(0x1fa8e318 ,0x0);
          write_reg_word(0x1fa8e30c ,0x1);
          
          // XFI_RX_pdos
          write_reg_word(0x1fa8e894 ,0x1000101);
          write_reg_word(0x1fa8e894 ,0x1010101);
          write_reg_word(0x1fa8e114 ,0x10000);
          write_reg_word(0x1fa8e10c ,0x1000000);
          write_reg_word(0x1fa8e304 ,0x1010101);
          write_reg_word(0x1fa8e308 ,0x1010101);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e338 ,0x1010100);
          write_reg_word(0x1fa8e084 ,0x101);
          write_reg_word(0x1fa8e084 ,0x1);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e338 ,0x10100);
          write_reg_word(0x1fa8e084 ,0x1);
          write_reg_word(0x1fa8e084 ,0x0);
          write_reg_word(0x1fa8e200 ,0x20001);
          write_reg_word(0x1fa8e328 ,0x101);
          write_reg_word(0x1fa8e334 ,0x1000000);
          write_reg_word(0x1fa8e208 ,0x10100);
          write_reg_word(0x1fa8e110 ,0x1);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e110 ,0x0);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e328 ,0x10101);
          write_reg_word(0x1fa8e208 ,0x10101);
          write_reg_word(0x1fa8e110 ,0x10000);
          write_reg_word(0x1fa8e110 ,0x0);
          write_reg_word(0x1fa8e084 ,0x0);
          write_reg_word(0x1fa8e084 ,0x100);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e338 ,0x1010100);
          write_reg_word(0x1fa8e084 ,0x100);
          write_reg_word(0x1fa8e084 ,0x101);
          write_reg_word(0x1fa8e894 ,0x1010101);
          write_reg_word(0x1fa8e894 ,0x1000101);
          
          // XFI_RX_feos
          write_reg_word(0x1fa8e114 ,0x10000);
          write_reg_word(0x1fa8e10c ,0x1000000);
          write_reg_word(0x1fa8e308 ,0x1010101);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e338 ,0x1010100);
          write_reg_word(0x1fa8e144 ,0x30);
          write_reg_word(0x1fa8e32c ,0x0);
          write_reg_word(0x1fa8e338 ,0x1000100);
          write_reg_word(0x1fa8e204 ,0x1010001);
          write_reg_word(0x1fa8e110 ,0x0);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e110 ,0x1);
          write_reg_word(0x1fa8e108 ,0x0);
          write_reg_word(0x1fa8e32c ,0x10000);
          write_reg_word(0x1fa8e204 ,0x1010101);
          write_reg_word(0x1fa8e110 ,0x1000001);
          write_reg_word(0x1fa8e110 ,0x1);
          write_reg_word(0x1fa8e110 ,0x0);
          
          // XFI_RX_sdcal
          write_reg_word(0x1fa8e898 ,0x100);
          write_reg_word(0x1fa8e840 ,0x1010101);
          write_reg_word(0x1fa8e204 ,0x10101);
          write_reg_word(0x1fa8e32c ,0x10000);
          write_reg_word(0x1fa8e10c ,0x1000000);
          write_reg_word(0x1fa8e338 ,0x1000000);
          write_reg_word(0x1fa8e114 ,0x10000);
          write_reg_word(0x1fa8e204 ,0x1010101);
          write_reg_word(0x1fa8e32c ,0x10100);
          write_reg_word(0x1fa8e114 ,0x10001);
          write_reg_word(0x1fa8e114 ,0x10000);
          write_reg_word(0x1fa8e898 ,0x100);
          write_reg_word(0x1fa8e83c ,0x1010000);
          write_reg_word(0x1fa8e83c ,0x1000000);
          write_reg_word(0x1fa8e840 ,0x1010101);
          write_reg_word(0x1fa8e840 ,0x1000101);
          write_reg_word(0x1fa8e840 ,0x1000100);
          
          // XFI_phy_status
          write_reg_word(0x1fa8e114 ,0x10100);
          write_reg_word(0x1fa8e10c ,0x1000000);
          
          // XFI_DIG_reset_release
          write_reg_word(0x1fa8e460 ,0x7f);
          
          //XFI_L2D
          write_reg_word(0x1fa8e818 ,0x1010101);
          write_reg_word(0x1fa8e818 ,0x1000101);
          write_reg_word(0x1fa8e818 ,0x1010101);
          
          // XFI_RX_rxrdy
          write_reg_word(0x1fa8e114 ,0x1010100);
          write_reg_word(0x1fa8e10c ,0x0);
          write_reg_word(0x1fa8e460 ,0x7e);
          write_reg_word(0x1fa8e460 ,0x30fff);

    


}





void serdes_phy_init()
{
#ifdef TCSUPPORT_CPU_AN7583
	int len_eth = 0;
	unsigned char* serdes_intf[SERDES_INTF_MAX];
    //int i = 0;	

    uint wan_config = 0;//1fb00070    
    uint scu_ssr3 = 0;//1fb00094
	uint scu_sstr = 0;//1fb0009c

	serdes_intf[SERDES_ETH] = serdes_intf_env("serdes_ethernet");
    serdes_intf[SERDES_USB] = serdes_intf_env("serdes_usb1");
    serdes_intf[SERDES_PCIE1] = serdes_intf_env("serdes_wifi1");
    serdes_intf[SERDES_PCIE2] = serdes_intf_env("serdes_wifi2");
	serdes_intf[SERDES_PON] = serdes_intf_env("serdes_pon");
	/////////////////////////////////// AN7583 Serdes 1 Start  ///////////////////////////////////

	len_eth = strlen(serdes_intf[SERDES_PON]);
	if(len_eth < 2)
	{
		printf("error pon_len=%d\n",len_eth);
		return;
	}

    if((serdes_intf[4][2]!= '0') && (serdes_intf[4][2] != '4')) 
    {
        //printf("Serdes1 Uboot Init, PON VER = AN7583_PON.0.09.16\n"); 
        switch(serdes_intf[4][2])
        {
            case SERDES_PON_XFI_MODE        : printf("Serdes1 XFI MODE Init, PON VER = AN7583_PON.0.09.16\n");break;
            case SERDES_PON_USXGMII_MODE    : printf("Serdes1 USXGMII MODE Init, PON VER = AN7583_PON.0.09.16\n");break;
            case SERDES_PON_HSGMII_MODE     : 
                if(serdes_intf[4][0] != SERDES_PON_DSL_PHY)
                    printf("Serdes1 HSGMII MODE Init, PON VER = AN7583_PON.0.09.16\n");
                break;
            case SERDES_PON_SGMII_MODE      : 
                if(serdes_intf[4][0] != SERDES_PON_DSL_PHY)
                    printf("Serdes1 SGMII MODE Init, PON VER = AN7583_PON.0.09.16\n");
                break;
            default :break;
        }
        if(serdes_intf[4][0] == SERDES_PON_DSL_PHY)printf("Serdes1 DSL MODE Init, PON VER = AN7583_PON.0.09.16\n");
    }

	if((serdes_intf[4][1] == SERDES_ETHERWAN) && (serdes_intf[4][2] == SERDES_PON_XFI_MODE))
	{
	    if(UBOOT_DEBUG_LOG)printf("PON XFI Mode Init\n");
	    //SCU Setting
        wan_config = read_reg_word(0x1fb00070);
        wan_config = wan_config & 0xFFFFFF00;
        wan_config = wan_config | 0x12;
        write_reg_word( 0x1fb00070, wan_config);
        
        scu_sstr = read_reg_word(0x1fb0009c);   
		scu_sstr = scu_sstr & 0xFFFFF9FF;
		scu_sstr = scu_sstr | (1<<9);
		write_reg_word( 0x1fb0009c, scu_sstr);

        /////////////////////////////////// PMA Start  ///////////////////////////////////             
        AN7583_PON_XFI_10G_PMA_BringUp();
    
        /////////////////////////////////// PCS Start  ///////////////////////////////////
        if(UBOOT_DEBUG_LOG)printf("PON XFI Mode PCS Init\n");
        write_reg_word(0x1fa85c34,0x01e11111);
        write_reg_word(0x1fa84100,0x10010001);
        
        //b'usxgmii_pcs_int en 0\r\n'
        write_reg_word(0x1fa85bc0,0x0);
        write_reg_word(0x1fa85bc4,0x0);
        write_reg_word(0x1fa85bc8,0x0);
        write_reg_word(0x1fa85bcc,0x0);
        write_reg_word(0x1fa85be0,0x0);
        write_reg_word(0x1fa85bd8,0x0);
        write_reg_word(0x1fa85bdc,0x0);
        write_reg_word(0x1fa85be4,0x0);
        write_reg_word(0x1fa85bc8,0x0);
        write_reg_word(0x1fa85bcc,0x0);
        write_reg_word(0x1fa85be0,0x0);
        
        write_reg_word(0x1fa85bf8,0x6330000);
        write_reg_word(0x1fa85bfc,0x1001);
        write_reg_word(0x1fa86000,0xc000c11);
        write_reg_word(0x1fa8602c,0x104);
        
        //set mac reg init
        write_reg_word(0x1fa08000,0x77FE2800);
        //printf("set mac reg init\n");

        //b'usxgmii_pcs_an_ctrl7\r\n'
        write_reg_word(0x1fa85c20,0x1000);
        if(UBOOT_DEBUG_LOG)printf("PON XFI Mode Init exit\n");


    }
    else if((serdes_intf[4][1] == SERDES_ETHERWAN) && (serdes_intf[4][2] == SERDES_PON_USXGMII_MODE))
    {
	    if(UBOOT_DEBUG_LOG)printf("PON USXGMII Mode Init\n");
	    //SCU Setting
        wan_config = read_reg_word(0x1fb00070);
        wan_config = wan_config & 0xFFFFFF00;
        wan_config = wan_config | 0x12;
        write_reg_word( 0x1fb00070, wan_config);
        
        scu_sstr = read_reg_word(0x1fb0009c);   
		scu_sstr = scu_sstr & 0xFFFFF9FF;
		scu_sstr = scu_sstr | (1<<9);
		write_reg_word( 0x1fb0009c, scu_sstr);

        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        AN7583_PON_XFI_10G_PMA_BringUp();
    
        /////////////////////////////////// PCS Start  ///////////////////////////////////
        if(UBOOT_DEBUG_LOG)printf("PON USXGMII Mode PCS Init\n");
        write_reg_word(0x1fa85c34,0x01e11111);
        write_reg_word(0x1fa84100,0x10010001);    

        //b'PCS E0/E1 solution\r\n'
		write_reg_word(0x1fa85b2c,0x0);
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa85bc0,0x0);
		write_reg_word(0x1fa85bc4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);
		write_reg_word(0x1fa85bd8,0x0);
		write_reg_word(0x1fa85bdc,0x0);
		write_reg_word(0x1fa85be4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);

		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa85bf8,0x6330001);
		//b'xSGMII_AN_AutoSetting\r\n'
		write_reg_word(0x1fa85C20,0x110000);
		write_reg_word(0x1fa86000,0xc11);
		
        //set mac reg init
      	write_reg_word(0x1fa08000,0x71082800);
        if(UBOOT_DEBUG_LOG)printf("PON USXGMII Mode Init exit\n");

    }
    else if((serdes_intf[4][1] == SERDES_ETHERWAN) && (serdes_intf[4][0] == SERDES_PON_DSL_PHY))
    {
	    if(UBOOT_DEBUG_LOG)printf("PON DSL Mode Init\n");
	    //SCU Setting
        wan_config = read_reg_word(0x1fb00070);
        wan_config = wan_config & 0xFFFFFF00;
        wan_config = wan_config | 0x10;
        write_reg_word( 0x1fb00070, wan_config);
        
        scu_sstr = read_reg_word(0x1fb0009c);   
		scu_sstr = scu_sstr & 0xFFFFF9FF;
		scu_sstr = scu_sstr | (2<<9);
		write_reg_word( 0x1fb0009c, scu_sstr);

        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        AN7583_PON_XFI_1G_PMA_BringUp();
    
        /////////////////////////////////// PCS Start  ///////////////////////////////////
        if(UBOOT_DEBUG_LOG)printf("PON 1G AN Force Mode PCS Init\n");
        write_reg_word(0x1fa80034,0x31120029);
        write_reg_word(0x1fa80a24,0x1);
                                        
        write_reg_word(0x1fa80a00,0x4c9cc000);
        write_reg_word(0x1fa84018,0x24);
        write_reg_word(0x1fa86018,0x7070707);
        write_reg_word(0x1fa86020,0xff);
                                        
        write_reg_word(0x1fa84014,0x8301);
        write_reg_word(0x1fa80a20,0x1);
        write_reg_word(0x1fa8414c,0x1);
        
        
        write_reg_word(0x1fa86100,0x0);       //SGMII_RA_XGMII
        write_reg_word(0x1fa80000,0x1140);    //SGMII_AN
        write_reg_word(0x1fa86000,0xc000c11);
                                        
        write_reg_word(0x1fa80000,0x1340);
        udelay(3);
        write_reg_word(0x1fa80000,0x1140);
                                        
        //Solution1-AN FORCE ONLY
        write_reg_word(0x1fa86004,0x10f);
        write_reg_word(0x1fa86004,0x50f010f);

        if(UBOOT_DEBUG_LOG)printf("PON DSL Mode Init exit\n");

    }   
    else if((serdes_intf[4][1] == SERDES_ETHERWAN) && (serdes_intf[4][2] == SERDES_PON_HSGMII_MODE))
    {
	    if(UBOOT_DEBUG_LOG)printf("PON HSGMII Mode Init\n");
	    //SCU Setting
        wan_config = read_reg_word(0x1fb00070);
        wan_config = wan_config & 0xFFFFFF00;
        wan_config = wan_config | 0x11;
        write_reg_word( 0x1fb00070, wan_config);
        
        scu_sstr = read_reg_word(0x1fb0009c);   
		scu_sstr = scu_sstr & 0xFFFFF9FF;
		scu_sstr = scu_sstr | (2<<9);
		write_reg_word( 0x1fb0009c, scu_sstr);

        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        
        write_reg_word(0x1fa8e854 , 0x1000000);
        write_reg_word(0x1fa8e034 , 0x1);
        write_reg_word(0x1fa8e02c , 0x500);
        write_reg_word(0x1fa8f0c8 , 0xf0000000);
        write_reg_word(0x1fa8f0d8 , 0x100000a);
        write_reg_word(0x1fa8e004 , 0x1100a00);
        write_reg_word(0x1fa8e004 , 0x1100a01);
        write_reg_word(0x1fa8e444 , 0x11309c4);
        write_reg_word(0x1fa8e444 , 0x11300fa);
        write_reg_word(0x1fa8e448 , 0x9b14a0);
        write_reg_word(0x1fa8e448 , 0x9b0210);
        write_reg_word(0x1fa8e440 , 0x409bf);
        write_reg_word(0x1fa8e440 , 0x40026);
        write_reg_word(0x1fa8e468 , 0x28000);
        write_reg_word(0x1fa8e028 , 0x303);
        write_reg_word(0x1fa8e024 , 0x10100);
        write_reg_word(0x1fa8e794 , 0x1000000);
        write_reg_word(0x1fa8e034 , 0x101);
        write_reg_word(0x1fa8e798 , 0x3e800000);
        write_reg_word(0x1fa8e048 , 0x7d000000);
        write_reg_word(0x1fa8e04c , 0x7d000000);
        write_reg_word(0x1fa8f0c4 , 0x1010400);
        write_reg_word(0x1fa8f0c4 , 0x1010401);
        write_reg_word(0x1fa8f000 , 0x10040001);
        write_reg_word(0x1fa8f000 , 0x10040101);
        write_reg_word(0x1fa8e874 , 0x1000000);
        write_reg_word(0x1fa8e874 , 0x1010000);
        write_reg_word(0x1fa8e778 , 0x1000000);
        write_reg_word(0x1fa8e778 , 0x1000100);
        write_reg_word(0x1fa8e780 , 0x100);
        write_reg_word(0x1fa8e780 , 0x1000100);
        write_reg_word(0x1fa8e778 , 0x1000100);
        write_reg_word(0x1fa8e778 , 0x1000108);
        write_reg_word(0x1fa8e780 , 0x1000100);
        write_reg_word(0x1fa8e77c , 0x1000000);
        write_reg_word(0x1fa8e77c , 0x1040000);
        write_reg_word(0x1fa8e784 , 0x100);
        write_reg_word(0x1fa8e784 , 0x101);
        write_reg_word(0x1fa8e580 , 0x1);
        write_reg_word(0x1fa8e60c , 0x3);
        write_reg_word(0x1fa8e468 , 0x10028000);
        write_reg_word(0x1fa8e08c , 0x101);
        write_reg_word(0x1fa8e104 , 0x2);
        write_reg_word(0x1fa8e08c , 0x101);
        write_reg_word(0x1fa8e090 , 0x3e80002);
        write_reg_word(0x1fa8e09c , 0x3e80002);
        write_reg_word(0x1fa8e094 , 0x3e80002);
        write_reg_word(0x1fa8e098 , 0x3e80002);
        write_reg_word(0x1fa8e120 , 0x103);
        write_reg_word(0x1fa8e068 , 0x24001f0);
        write_reg_word(0x1fa8e068 , 0x24001c0);
        write_reg_word(0x1fa8e088 , 0x0);
        write_reg_word(0x1fa8f0d4 , 0x4cc31030);
        write_reg_word(0x1fa8f120 , 0x3ff00);
        write_reg_word(0x1fa8f0dc , 0x0);
        write_reg_word(0x1fa8e814 , 0x1000000);
        write_reg_word(0x1fa8e814 , 0x1010000);
        write_reg_word(0x1fa8f0d8 , 0x100010a);
        write_reg_word(0x1fa8f0f8 , 0x4010808);
        write_reg_word(0x1fa8e88c , 0x100);
        write_reg_word(0x1fa8e8b8 , 0x1);
        write_reg_word(0x1fa8e88c , 0x103);
        write_reg_word(0x1fa8f0d4 , 0x4cc318b0);
        write_reg_word(0x1fa8e360 , 0x300);
        write_reg_word(0x1fa8e294 , 0x3);
        write_reg_word(0x1fa8e300 , 0x1010100);
        write_reg_word(0x1fa8f0f8 , 0x4010806);
        write_reg_word(0x1fa8f0d8 , 0x100010a);
        write_reg_word(0x1fa8f10c , 0x70604);
        write_reg_word(0x1fa8f0cc , 0x1000000);
        write_reg_word(0x1fa8f0d8 , 0x101010a);
        write_reg_word(0x1fa8f0d8 , 0x101010b);
        write_reg_word(0x1fa8f0e8 , 0x2000001);
        write_reg_word(0x1fa8f0cc , 0x1000000);
        write_reg_word(0x1fa8e76c , 0x1000000);
        write_reg_word(0x1fa8e76c , 0x1010000);
        write_reg_word(0x1fa8e374 , 0x0);
        write_reg_word(0x1fa8f0a0 , 0x1000009);
        write_reg_word(0x1fa8f084 , 0x101031b);
        write_reg_word(0x1fa8f088 , 0x0);
        write_reg_word(0x1fa8f088 , 0x1);
        write_reg_word(0x1fa8f0a8 , 0x10000);
        write_reg_word(0x1fa8f0a8 , 0x10100);
        write_reg_word(0x1fa8f064 , 0x1040001);
        write_reg_word(0x1fa8f064 , 0x1040000);
        write_reg_word(0x1fa8f068 , 0x300);
        write_reg_word(0x1fa8f06c , 0x1000003);
        write_reg_word(0x1fa8f068 , 0x300);
        write_reg_word(0x1fa8f068 , 0x0);
        write_reg_word(0x1fa8f06c , 0x1000003);
        write_reg_word(0x1fa8f080 , 0x82);
        write_reg_word(0x1fa8f080 , 0x0);
        write_reg_word(0x1fa8f07c , 0x0);
        write_reg_word(0x1fa8f084 , 0x1010000);
        write_reg_word(0x1fa8f098 , 0x1);
        write_reg_word(0x1fa8f050 , 0x1f05001e);
        write_reg_word(0x1fa8f054 , 0xb05);
        write_reg_word(0x1fa8f050 , 0x1f05001e);
        write_reg_word(0x1fa8f054 , 0x180b05);
        write_reg_word(0x1fa8f054 , 0x180b02);
        write_reg_word(0x1fa8f074 , 0x1);
        write_reg_word(0x1fa8f078 , 0x4040401);
        write_reg_word(0x1fa8f078 , 0x4040701);
        write_reg_word(0x1fa8f058 , 0x30003ff);
        write_reg_word(0x1fa8f058 , 0x30002ff);
        write_reg_word(0x1fa8f05c , 0x101);
        write_reg_word(0x1fa8f058 , 0x30002e4);
        write_reg_word(0x1fa8f054 , 0x180b02);
        write_reg_word(0x1fa8f094 , 0x10010);
        write_reg_word(0x1fa8e858 , 0x100);
        write_reg_word(0x1fa8f05c , 0x101);
        write_reg_word(0x1fa8f094 , 0x1000f);
        write_reg_word(0x1fa8f070 , 0x4000903);
        write_reg_word(0x1fa8f070 , 0x4000e03);
        write_reg_word(0x1fa8f074 , 0x10001);
        write_reg_word(0x1fa8f06c , 0x1000003);
        write_reg_word(0x1fa8f0c0 , 0x4020000);
        write_reg_word(0x1fa8f0c0 , 0x2020000);
        write_reg_word(0x1fa8f118 , 0x1000000);
        write_reg_word(0x1fa8f118 , 0x1010000);
        write_reg_word(0x1fa8f118 , 0x1010100);
        write_reg_word(0x1fa8f11c , 0x401);
        write_reg_word(0x1fa8f0e8 , 0x800001);
        write_reg_word(0x1fa8f0e0 , 0x1078000);
        write_reg_word(0x1fa8f100 , 0x100);
        write_reg_word(0x1fa8f110 , 0x200);
        write_reg_word(0x1fa8e088 , 0x1);
        write_reg_word(0x1fa8e074 , 0x10000);
        write_reg_word(0x1fa8e078 , 0x404);
        write_reg_word(0x1fa8e07c , 0x40004);
        write_reg_word(0x1fa8e080 , 0xff000000);
        write_reg_word(0x1fa8e080 , 0xff0000d0);
        write_reg_word(0x1fa8e118 , 0xa000a01);
        write_reg_word(0x1fa8e118 , 0xa01);
        write_reg_word(0x1fa8e118 , 0x1);
        write_reg_word(0x1fa8e11c , 0x1);
        write_reg_word(0x1fa8e160 , 0x1003100);
        write_reg_word(0x1fa8e160 , 0x1002e00);
        write_reg_word(0x1fa8e160 , 0x1002e01);
        write_reg_word(0x1fa8e160 , 0x1012e01);
        write_reg_word(0x1fa8e164 , 0x500);
        write_reg_word(0x1fa8e164 , 0x0);
        write_reg_word(0x1fa8e100 , 0xc80005);
        write_reg_word(0x1fa8e100 , 0xa0005);
        write_reg_word(0x1fa8e144 , 0x0);
        write_reg_word(0x1fa8e06c , 0x1940);
        write_reg_word(0x1fa8e06c , 0x3f40);
        write_reg_word(0x1fa8e06c , 0x13f40);
        write_reg_word(0x1fa8e070 , 0x18);
        write_reg_word(0x1fa8e48c , 0x1000203);
        write_reg_word(0x1fa8e48c , 0x1000202);
        write_reg_word(0x1fa8e170 , 0xa503);
        write_reg_word(0x1fa8e170 , 0xa502);
        write_reg_word(0x1fa8e174 , 0x4010000);
        write_reg_word(0x1fa8e184 , 0x40001ff);
        write_reg_word(0x1fa8e178 , 0x20403);
        write_reg_word(0x1fa8e178 , 0x20403);
        write_reg_word(0x1fa8e320 , 0x10101);
        write_reg_word(0x1fa8e200 , 0x101);
        write_reg_word(0x1fa8e200 , 0x1);
        write_reg_word(0x1fa8e148 , 0x109f0901);
        write_reg_word(0x1fa8e148 , 0x109f0a01);
        write_reg_word(0x1fa8e148 , 0x10f00a01);
        write_reg_word(0x1fa8e148 , 0x8700a01);
        write_reg_word(0x1fa8e854 , 0x1000000);
        write_reg_word(0x1fa8e854 , 0x1010000);
        write_reg_word(0x1fa8e000 , 0x1000000);
        write_reg_word(0x1fa8e854 , 0x1010100);
        write_reg_word(0x1fa8e854 , 0x1010101);
        write_reg_word(0x1fa8f094 , 0x1000f);
        write_reg_word(0x1fa8f060 , 0x101);
        write_reg_word(0x1fa8f094 , 0xf);
        write_reg_word(0x1fa8f04c , 0x0);
        write_reg_word(0x1fa8e260 , 0x1);
        write_reg_word(0x1fa8e410 , 0x100);
        write_reg_word(0x1fa8e410 , 0x101);
        write_reg_word(0x1fa8e260 , 0x101);
        write_reg_word(0x1fa8e264 , 0x2010000);
        write_reg_word(0x1fa8e264 , 0x1010000);
        write_reg_word(0x1fa8f114 , 0x40200);
        write_reg_word(0x1fa8f114 , 0x20200);
        write_reg_word(0x1fa8f110 , 0x3000200);
        write_reg_word(0x1fa8f10c , 0xf0604);
        write_reg_word(0x1fa8f10c , 0xe0604);
        write_reg_word(0x1fa8e114 , 0x0);
        write_reg_word(0x1fa8e10c , 0x1010001);
        write_reg_word(0x1fa8e818 , 0x0);
        write_reg_word(0x1fa8e818 , 0x100);
        write_reg_word(0x1fa8e768 , 0x1000000);
        write_reg_word(0x1fa8e88c , 0x103);
        write_reg_word(0x1fa8e88c , 0x101);
        write_reg_word(0x1fa8e330 , 0x100);
        write_reg_word(0x1fa8e33c , 0x1010001);
        write_reg_word(0x1fa8e330 , 0x100);
        write_reg_word(0x1fa8e33c , 0x1000001);
        write_reg_word(0x1fa8e338 , 0x1010100);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e10c , 0x1010001);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e794 , 0x1000000);
        write_reg_word(0x1fa8e034 , 0x101);
        write_reg_word(0x1fa8e864 , 0x100);
        write_reg_word(0x1fa8e864 , 0x101);
        write_reg_word(0x1fa8e02c , 0x500);
        write_reg_word(0x1fa8e010 , 0x100);
        write_reg_word(0x1fa8e824 , 0x1000000);
        write_reg_word(0x1fa8e824 , 0x1010000);
        write_reg_word(0x1fa8e824 , 0x1010100);
        write_reg_word(0x1fa8e824 , 0x1010101);
        write_reg_word(0x1fa8e81c , 0x0);
        write_reg_word(0x1fa8e81c , 0x100);
        write_reg_word(0x1fa8e81c , 0x101);
        write_reg_word(0x1fa8e894 , 0x100);
        write_reg_word(0x1fa8e894 , 0x101);
        write_reg_word(0x1fa8e84c , 0x1000000);
        write_reg_word(0x1fa8e84c , 0x1010000);
        write_reg_word(0x1fa8e818 , 0x1000100);
        write_reg_word(0x1fa8e818 , 0x1010100);
        write_reg_word(0x1fa8e34c , 0x1000000);
        write_reg_word(0x1fa8e34c , 0x1010000);
        write_reg_word(0x1fa8e34c , 0x1010100);
        write_reg_word(0x1fa8e34c , 0x1010101);
        write_reg_word(0x1fa8e350 , 0x1);
        write_reg_word(0x1fa8e38c , 0x1);
        write_reg_word(0x1fa8f0fc , 0x80505);
        write_reg_word(0x1fa8e108 , 0x1010001);
        write_reg_word(0x1fa8e108 , 0x1000001);
        write_reg_word(0x1fa8e108 , 0x1);
        write_reg_word(0x1fa8e10c , 0x1010000);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8f100 , 0x100);
        write_reg_word(0x1fa8f108 , 0x0);
        write_reg_word(0x1fa8e460 , 0x2);
        write_reg_word(0x1fa8e460 , 0x22);
        write_reg_word(0x1fa8e818 , 0x1000100);
        write_reg_word(0x1fa8e818 , 0x1010100);
        write_reg_word(0x1fa8e818 , 0x1000100);
        write_reg_word(0x1fa8e818 , 0x1010100);
        write_reg_word(0x1fa8e460 , 0x22);
        write_reg_word(0x1fa8e33c , 0x1000000);
        write_reg_word(0x1fa8e330 , 0x101);
        write_reg_word(0x1fa8e83c , 0x1000000);
        write_reg_word(0x1fa8e83c , 0x1010000);
        write_reg_word(0x1fa8e840 , 0x1000000);
        write_reg_word(0x1fa8e840 , 0x1010000);
        write_reg_word(0x1fa8e840 , 0x1010100);
        write_reg_word(0x1fa8e840 , 0x1010101);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8e110 , 0x0);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e308 , 0x1010101);
        write_reg_word(0x1fa8e15c , 0x400);
        write_reg_word(0x1fa8e118 , 0x8);
        write_reg_word(0x1fa8e204 , 0x1000101);
        write_reg_word(0x1fa8e328 , 0x0);
        write_reg_word(0x1fa8e334 , 0x1010001);
        write_reg_word(0x1fa8e328 , 0x0);
        write_reg_word(0x1fa8e334 , 0x1010000);
        write_reg_word(0x1fa8e31c , 0x1010100);
        write_reg_word(0x1fa8e318 , 0x0);
        write_reg_word(0x1fa8e324 , 0x10101);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e30c , 0x0);
        write_reg_word(0x1fa8e204 , 0x1010101);
        write_reg_word(0x1fa8e328 , 0x100);
        write_reg_word(0x1fa8e328 , 0x101);
        write_reg_word(0x1fa8e318 , 0x100);
        write_reg_word(0x1fa8e110 , 0x101);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e318 , 0x0);
        write_reg_word(0x1fa8e30c , 0x1);
        write_reg_word(0x1fa8e894 , 0x1000101);
        write_reg_word(0x1fa8e894 , 0x1010101);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8e304 , 0x1010101);
        write_reg_word(0x1fa8e308 , 0x1010101);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e338 , 0x1010100);
        write_reg_word(0x1fa8e084 , 0x101);
        write_reg_word(0x1fa8e084 , 0x1);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e338 , 0x10100);
        write_reg_word(0x1fa8e084 , 0x1);
        write_reg_word(0x1fa8e084 , 0x0);
        write_reg_word(0x1fa8e200 , 0x20001);
        write_reg_word(0x1fa8e328 , 0x101);
        write_reg_word(0x1fa8e334 , 0x1000000);
        write_reg_word(0x1fa8e208 , 0x10100);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e110 , 0x0);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e328 , 0x10101);
        write_reg_word(0x1fa8e208 , 0x10101);
        write_reg_word(0x1fa8e110 , 0x10000);
        write_reg_word(0x1fa8e110 , 0x0);
        write_reg_word(0x1fa8e084 , 0x0);
        write_reg_word(0x1fa8e084 , 0x100);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e338 , 0x1010100);
        write_reg_word(0x1fa8e084 , 0x100);
        write_reg_word(0x1fa8e084 , 0x101);
        write_reg_word(0x1fa8e894 , 0x1010101);
        write_reg_word(0x1fa8e894 , 0x1000101);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8e308 , 0x1010101);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e338 , 0x1010100);
        write_reg_word(0x1fa8e144 , 0x30);
        write_reg_word(0x1fa8e32c , 0x0);
        write_reg_word(0x1fa8e338 , 0x1000100);
        write_reg_word(0x1fa8e204 , 0x1010001);
        write_reg_word(0x1fa8e110 , 0x0);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e108 , 0x0);
        write_reg_word(0x1fa8e32c , 0x10000);
        write_reg_word(0x1fa8e204 , 0x1010101);
        write_reg_word(0x1fa8e110 , 0x1000001);
        write_reg_word(0x1fa8e110 , 0x1);
        write_reg_word(0x1fa8e110 , 0x0);
        write_reg_word(0x1fa8e898 , 0x100);
        write_reg_word(0x1fa8e898 , 0x101);
        write_reg_word(0x1fa8e840 , 0x1010101);
        write_reg_word(0x1fa8e204 , 0x10101);
        write_reg_word(0x1fa8e32c , 0x10000);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8e338 , 0x1000000);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e204 , 0x1010101);
        write_reg_word(0x1fa8e32c , 0x10100);
        write_reg_word(0x1fa8e114 , 0x10001);
        write_reg_word(0x1fa8e114 , 0x10000);
        write_reg_word(0x1fa8e898 , 0x101);
        write_reg_word(0x1fa8e898 , 0x100);
        write_reg_word(0x1fa8e83c , 0x1010000);
        write_reg_word(0x1fa8e83c , 0x1000000);
        write_reg_word(0x1fa8e840 , 0x1010101);
        write_reg_word(0x1fa8e840 , 0x1000101);
        write_reg_word(0x1fa8e840 , 0x1000100);
        write_reg_word(0x1fa8e114 , 0x10100);
        write_reg_word(0x1fa8e10c , 0x1000000);
        write_reg_word(0x1fa8e460 , 0x7f);
        write_reg_word(0x1fa8e818 , 0x1010100);
        write_reg_word(0x1fa8e818 , 0x1000100);
        write_reg_word(0x1fa8e818 , 0x1010100);
        write_reg_word(0x1fa8e818 , 0x1010101);
        
        
        //EO_Scan_Result
        write_reg_word(0x1fa8e88c , 0x103);
        write_reg_word(0x1fa8e768 , 0x10e0000);
        
        write_reg_word(0x1fa8e114 , 0x1010100);
        write_reg_word(0x1fa8e10c , 0x0);
        write_reg_word(0x1fa8e460 , 0x7e);
        write_reg_word(0x1fa8e460 , 0x30fff);
        
        
    
        /////////////////////////////////// PCS Start  ///////////////////////////////////
        if(UBOOT_DEBUG_LOG)printf("PON HSGMII Mode PCS Init\n");
        write_reg_word(0x1fa80a00,0x4c9cc000);
		write_reg_word(0x1fa84018,0x34);
		//b'hsgmii_init\r\n'
		//b'xsgmii 1 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 1, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa80000,0x140);
		//b'hsgmii\r\n'
		//b'HGMII_2.5G\r\n'
		write_reg_word(0x1fa86000,0xc000c11);
        if(UBOOT_DEBUG_LOG)printf("PON HSGMII Mode Init exit\n");
    } 
    else if((serdes_intf[4][1] == SERDES_ETHERWAN) && (serdes_intf[4][2] == SERDES_PON_SGMII_MODE))
    {
	    if(UBOOT_DEBUG_LOG)printf("PON SGMII Mode Init\n");
	    //SCU Setting
        wan_config = read_reg_word(0x1fb00070);
        wan_config = wan_config & 0xFFFFFF00;
        wan_config = wan_config | 0x10;
        write_reg_word( 0x1fb00070, wan_config);
        
        scu_sstr = read_reg_word(0x1fb0009c);   
		scu_sstr = scu_sstr & 0xFFFFF9FF;
		scu_sstr = scu_sstr | (2<<9);
		write_reg_word( 0x1fb0009c, scu_sstr);

        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        AN7583_PON_XFI_1G_PMA_BringUp();
        
        /////////////////////////////////// PCS Start  ///////////////////////////////////
        if(UBOOT_DEBUG_LOG)printf("PON SGMII Mode PCS Init\n");
        //printf("ETH SGMII Mode PCS Init\n");
         write_reg_word(0x1fa80034,0x31120029);
         write_reg_word(0x1fa80a24,0x1);
         //b'xsgmii_init\r\n'
         write_reg_word(0x1fa80a00,0x4c9cc000);
         write_reg_word(0x1fa84018,0x24);
         //b'sgmii_init\r\n'
         write_reg_word(0x1fa86018,0x7070707);
         write_reg_word(0x1fa86020,0xff);
         //b'SGMII_Interrupt_init\r\n'
         write_reg_word(0x1fa84014,0x1);
         //b'xsgmii 2,int enable 1\r\n'
         //b'\r\n'
         //b' request_irq() (irq number: 138) OK \r\n'
         write_reg_word(0x1fa80a20,0x1);
         write_reg_word(0x1fa8414c,0x1);
         //b'rg_INTERRUPT_EN_0 1\r\n'
         //b'xsgmii: sync_int exit 1\r\n'
         //b'force mode\r\n'
         //b'xSGMII_AN_API 2, enable : 0\r\n'
         //b'sgmii_an 0\r\n'
         write_reg_word(0x1fa80000,0x140);
         //b'sgmii_rate_api rate 0\r\n'
         //b'SGMII_1G\r\n'
         write_reg_word(0x1fa86000,0xc000c11);
         write_reg_word(0x1fa84018,0x24);
         
         if(UBOOT_DEBUG_LOG)printf("PON SGMII Mode Init exit\n");

    }    
    if((serdes_intf[4][2]!= '0') && (serdes_intf[4][2] != '4'))
    {
        if(UBOOT_DEBUG_LOG)printf("Serdes1 Uboot Init Finished\n");   
    } 
    /////////////////////////////////// AN7583 Serdes 2 Start  ///////////////////////////////////  
    len_eth = strlen(serdes_intf[SERDES_ETH]);
    if(len_eth < 2)
    {
        printf("error eth_len=%d\n",len_eth);
        return;
    }
    if((serdes_intf[0][len_eth-1] != '4') && (serdes_intf[0][len_eth-1] != '5'))
    {
        //printf("Serdes2 Uboot Init, ETH VER = AN7583_ETH.0.09.16\n"); 

        switch(serdes_intf[0][len_eth-1])
        {
            case SERDES_ETH_XFI_MODE        : printf("Serdes2 XFI MODE Init, ETH VER = AN7583_ETH.0.09.16\n");break;
            case SERDES_ETH_USXGMII_MODE    : printf("Serdes2 USXGMII MODE Init, ETH VER = AN7583_ETH.0.09.16\n");break;
            case SERDES_ETH_HSGMII_MODE     : printf("Serdes2 HSGMII MODE Init, ETH VER = AN7583_ETH.0.09.16\n");break;
            case SERDES_ETH_SGMII_MODE      : printf("Serdes2 SGMII MODE Init, ETH VER = AN7583_ETH.0.09.16\n");break;
            default :break;
        }

    } 	
    if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_XFI_MODE))
    {
        if(UBOOT_DEBUG_LOG)printf("ETH XFI Mode Init\n");
        //SCU Setting
        wan_config = read_reg_word(0x1fb00070);    
        scu_ssr3 = read_reg_word(0x1fb00094);    
        write_reg_word(0x1fb00070 ,(wan_config&0xFEFFFFFF));//Set as WAN CONFIG[24] as ETH XFI Mode     
        scu_ssr3 = scu_ssr3 & 0xFFFF9FFF;
        scu_ssr3 = (scu_ssr3 | (0x1<<13));
        write_reg_word(0x1fb00094,scu_ssr3);
        
        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        //printf("ETH XFI Mode PMA Mode Init\n");
        AN7583_XFI_10G_PMA_BringUp();


        /////////////////////////////////// PCS Start  ///////////////////////////////////
        //printf("ETH XFI Mode PCS Mode Init\n");
		write_reg_word(0x1fa75c34,0x01e11111);
		write_reg_word(0x1fa74100,0x10010001);

		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa75bc0,0x0);
		write_reg_word(0x1fa75bc4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		write_reg_word(0x1fa75bd8,0x0);
		write_reg_word(0x1fa75bdc,0x0);
		write_reg_word(0x1fa75be4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);

		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa75bf8,0x6330000);
        
		//b'usxgmii_rate_api rate 0 : 10GUSXGMII_10G\r\n'
		write_reg_word(0x1fa75bfc,0x1001);
		write_reg_word(0x1fa76000,0xc000c11);
		write_reg_word(0x1fa7602c,0x104);
      
  		//set mac reg init
  		//write_reg_word(0x1fa09000,0x77FE2800);
		write_reg_word(0x1fa09000,0x71082800);
  		//printf("set mac reg init\n");
      
		//b'_rg_usxgmii_an_control_1 1001\r\n'
		//b'_rg_rate_adapt_ctrl_0 c000c11\r\n'
		//b'_rg_rate_adapt_ctrl_11 104\r\n'
		//b'USXGMII_10G exit\r\n'
		//b'usxgmii_pcs_an_ctrl7\r\n'
		write_reg_word(0x1fa75c20,0x1000);
		//printf("ETH XFI_10G exit\n");
        
        ///////////////////////////////////  PCS End   ///////////////////////////////////        
    }
    else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_HSGMII_MODE)) 
    {
        if(UBOOT_DEBUG_LOG)printf("ETH HSGMII Mode Init\n");
        //SCU Setting
        wan_config = read_reg_word(0x1fb00070);    
        scu_ssr3 = read_reg_word(0x1fb00094);    
        write_reg_word(0x1fb00070 ,(wan_config&0xFEFFFFFF));//Set as WAN CONFIG[24] as ETH XFI Mode   
        scu_ssr3 = scu_ssr3 & 0xFFFF9FFF;
        scu_ssr3 = (scu_ssr3 | (0x2<<13));
        write_reg_word(0x1fb00094,scu_ssr3); 
        /////////////////////////////////// PMA Start  ///////////////////////////////////
        //printf("ETH HSGMII Mode PMA Init\n");
        
        // XFI_DIG_reset
        write_reg_word(0x1fa7e460 ,0x0);
        
        // XFI_TXPLL
        write_reg_word(0x1fa7e854 ,0x1000000);
        write_reg_word(0x1fa7e004 ,0x1100a00);
        write_reg_word(0x1fa7e004 ,0x1100a01);
        write_reg_word(0x1fa7e444 ,0x11309c4);
        write_reg_word(0x1fa7e444 ,0x11300fa);
        write_reg_word(0x1fa7e448 ,0x9b14a0);
        write_reg_word(0x1fa7e448 ,0x9b0210);
        write_reg_word(0x1fa7e440 ,0x409bf);
        write_reg_word(0x1fa7e440 ,0x40026);
        write_reg_word(0x1fa7e468 ,0x28000);
        write_reg_word(0x1fa7e028 ,0x303);
        write_reg_word(0x1fa7e024 ,0x10100);
        write_reg_word(0x1fa7e794 ,0x1000000);
        write_reg_word(0x1fa7e798 ,0x3e800000);
        write_reg_word(0x1fa7e048 ,0x7d000000);
        write_reg_word(0x1fa7e04c ,0x7d000000);
        
        // XFI_TX
        write_reg_word(0x1fa7f0c4 ,0x1010400);
        write_reg_word(0x1fa7f0c4 ,0x1010401);
        write_reg_word(0x1fa7f000 ,0x10040001);
        write_reg_word(0x1fa7f000 ,0x10040101);
        write_reg_word(0x1fa7e874 ,0x1000000);
        write_reg_word(0x1fa7e874 ,0x1010000);
        write_reg_word(0x1fa7e778 ,0x1000000);
        write_reg_word(0x1fa7e778 ,0x1000100);
        write_reg_word(0x1fa7e780 ,0x100);
        write_reg_word(0x1fa7e780 ,0x1000100);
        write_reg_word(0x1fa7e778 ,0x1000100);
        write_reg_word(0x1fa7e780 ,0x1000100);
        write_reg_word(0x1fa7e77c ,0x1000000);
        write_reg_word(0x1fa7e77c ,0x1040000);
        write_reg_word(0x1fa7e784 ,0x100);
        write_reg_word(0x1fa7e784 ,0x101);
        write_reg_word(0x1fa7e580 ,0x1);
        
        // XFI_RX
        write_reg_word(0x1fa7e08c ,0x101);
        write_reg_word(0x1fa7e104 ,0x2);
        write_reg_word(0x1fa7e08c ,0x101);
        write_reg_word(0x1fa7e090 ,0x3e80002);
        write_reg_word(0x1fa7e09c ,0x3e80002);
        write_reg_word(0x1fa7e094 ,0x3e80002);
        write_reg_word(0x1fa7e098 ,0x3e80002);
        write_reg_word(0x1fa7e120 ,0x103);
        write_reg_word(0x1fa7e068 ,0x24001f0);
        write_reg_word(0x1fa7e068 ,0x24001c0);
        write_reg_word(0x1fa7e088 ,0x0);
        write_reg_word(0x1fa7e088 ,0x1);
        write_reg_word(0x1fa7f0d4 ,0x4cc31030);
        write_reg_word(0x1fa7f120 ,0x3ff00);
        write_reg_word(0x1fa7f0dc ,0x0);
        write_reg_word(0x1fa7e814 ,0x1000000);
        write_reg_word(0x1fa7e814 ,0x1010000);
        write_reg_word(0x1fa7e88c ,0x100);
        write_reg_word(0x1fa7f0d4 ,0x4cc318b0);
        write_reg_word(0x1fa7e88c ,0x103);
        write_reg_word(0x1fa7e360 ,0x300);
        write_reg_word(0x1fa7e294 ,0x3);
        write_reg_word(0x1fa7e300 ,0x1010100);
        write_reg_word(0x1fa7f0f8 ,0x4010806);
        write_reg_word(0x1fa7f0d8 ,0x100010a);
        write_reg_word(0x1fa7f10c ,0x70604);
        write_reg_word(0x1fa7f0cc ,0x1000000);
        write_reg_word(0x1fa7f0d8 ,0x101010a);
        write_reg_word(0x1fa7f0d8 ,0x101010b);
        write_reg_word(0x1fa7f0e8 ,0x2000001);
        write_reg_word(0x1fa7f0cc ,0x1000000);
        write_reg_word(0x1fa7e76c ,0x1000000);
        write_reg_word(0x1fa7e76c ,0x1010000);
        write_reg_word(0x1fa7e374 ,0x0);
        
        // XFI_ANA
        write_reg_word(0x1fa7f084 ,0x101031b);
        write_reg_word(0x1fa7f088 ,0x0);
        write_reg_word(0x1fa7f088 ,0x1);
        write_reg_word(0x1fa7f0a8 ,0x10000);
        write_reg_word(0x1fa7f0a8 ,0x10100);
        write_reg_word(0x1fa7f064 ,0x1040001);
        write_reg_word(0x1fa7f064 ,0x1040000);
        write_reg_word(0x1fa7f068 ,0x300);
        write_reg_word(0x1fa7f068 ,0x0);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f080 ,0x82);
        write_reg_word(0x1fa7f080 ,0x0);
        write_reg_word(0x1fa7f07c ,0x0);
        write_reg_word(0x1fa7f084 ,0x1010000);
        write_reg_word(0x1fa7f098 ,0x0);
        write_reg_word(0x1fa7f050 ,0x1f05000c);
        write_reg_word(0x1fa7f050 ,0x1f05001e);
        write_reg_word(0x1fa7f054 ,0x180005);
        write_reg_word(0x1fa7f054 ,0x180b05);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f054 ,0x180b02);
        write_reg_word(0x1fa7f074 ,0x1);
        write_reg_word(0x1fa7f078 ,0x4040401);
        write_reg_word(0x1fa7f078 ,0x4040701);
        write_reg_word(0x1fa7f058 ,0x30003ff);
        write_reg_word(0x1fa7f058 ,0x30002ff);
        write_reg_word(0x1fa7f05c ,0x101);
        write_reg_word(0x1fa7f058 ,0x30002e4);
        write_reg_word(0x1fa7f054 ,0x180b02);
        write_reg_word(0x1fa7f094 ,0x10010);
        write_reg_word(0x1fa7e858 ,0x100);
        write_reg_word(0x1fa7f05c ,0x101);
        write_reg_word(0x1fa7f070 ,0x4000e03);
        write_reg_word(0x1fa7f074 ,0x10001);
        write_reg_word(0x1fa7f094 ,0x1000f);
        write_reg_word(0x1fa7f070 ,0x4000e03);
        write_reg_word(0x1fa7f074 ,0x10001);
        write_reg_word(0x1fa7f074 ,0x10001);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f0c0 ,0x4020000);
        write_reg_word(0x1fa7f0c0 ,0x2020000);
        write_reg_word(0x1fa7f118 ,0x1000000);
        write_reg_word(0x1fa7f118 ,0x1010000);
        write_reg_word(0x1fa7f118 ,0x1010100);
        write_reg_word(0x1fa7f118 ,0x1010100);
        write_reg_word(0x1fa7f11c ,0x401);
        write_reg_word(0x1fa7f0e8 ,0x800001);
        write_reg_word(0x1fa7f0e0 ,0x1078000);
        write_reg_word(0x1fa7f100 ,0x100);
        write_reg_word(0x1fa7f110 ,0x0);
        
        // XFI_TXPLL_ON
        write_reg_word(0x1fa7e074 ,0x10000);
        write_reg_word(0x1fa7e078 ,0x404);
        write_reg_word(0x1fa7e07c ,0x40004);
        write_reg_word(0x1fa7e080 ,0xff000000);
        write_reg_word(0x1fa7e080 ,0xff0000d0);
        write_reg_word(0x1fa7e118 ,0xa000a01);
        write_reg_word(0x1fa7e118 ,0xa01);
        write_reg_word(0x1fa7e118 ,0x1);
        write_reg_word(0x1fa7e11c ,0x1);
        write_reg_word(0x1fa7e160 ,0x1003100);
        write_reg_word(0x1fa7e160 ,0x1002e00);
        write_reg_word(0x1fa7e160 ,0x1012e01);
        write_reg_word(0x1fa7e164 ,0x500);
        write_reg_word(0x1fa7e164 ,0x0);
        write_reg_word(0x1fa7e100 ,0xc80005);
        write_reg_word(0x1fa7e100 ,0xa0005);
        write_reg_word(0x1fa7e144 ,0x0);
        write_reg_word(0x1fa7e06c ,0x1940);
        write_reg_word(0x1fa7e06c ,0x3f40);
        write_reg_word(0x1fa7e06c ,0x13f40);
        write_reg_word(0x1fa7e070 ,0x18);
        write_reg_word(0x1fa7e48c ,0x1000203);
        write_reg_word(0x1fa7e48c ,0x1000202);
        write_reg_word(0x1fa7e170 ,0xa503);
        write_reg_word(0x1fa7e170 ,0xa502);
        write_reg_word(0x1fa7e174 ,0x4010000);
        write_reg_word(0x1fa7e184 ,0x40001ff);
        write_reg_word(0x1fa7e178 ,0x20403);
        write_reg_word(0x1fa7e320 ,0x10101);
        write_reg_word(0x1fa7e200 ,0x101);
        write_reg_word(0x1fa7e200 ,0x1);
        write_reg_word(0x1fa7e148 ,0x109f0901);
        write_reg_word(0x1fa7e148 ,0x109f0a01);
        write_reg_word(0x1fa7e148 ,0x10f00a01);
        write_reg_word(0x1fa7e148 ,0x8700a01);
        write_reg_word(0x1fa7e854 ,0x1000000);
        write_reg_word(0x1fa7e854 ,0x1010000);
        write_reg_word(0x1fa7e000 ,0x1000000);
        write_reg_word(0x1fa7e854 ,0x1010100);
        write_reg_word(0x1fa7e854 ,0x1010101);
        write_reg_word(0x1fa7f094 ,0x1000f);
        write_reg_word(0x1fa7f060 ,0x101);
        write_reg_word(0x1fa7f094 ,0xf);
        write_reg_word(0x1fa7f04c ,0x0);
        
        // XFI_TXPLL_ON
        write_reg_word(0x1fa7e260 ,0x1);
        write_reg_word(0x1fa7e410 ,0x100);
        write_reg_word(0x1fa7e410 ,0x101);
        write_reg_word(0x1fa7e260 ,0x101);
        
        // XFI_RX_preset
        write_reg_word(0x1fa7f114 ,0x40200);
        write_reg_word(0x1fa7f114 ,0x20200);
        write_reg_word(0x1fa7f110 ,0x3000000);
        write_reg_word(0x1fa7f10c ,0xf0604);
        write_reg_word(0x1fa7f10c ,0xe0604);
        write_reg_word(0x1fa7e114 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1010001);
        write_reg_word(0x1fa7e818 ,0x100);
        write_reg_word(0x1fa7e768 ,0x1000000);
        write_reg_word(0x1fa7e330 ,0x100);
        write_reg_word(0x1fa7e33c ,0x1010001);
        write_reg_word(0x1fa7e330 ,0x100);
        write_reg_word(0x1fa7e33c ,0x1000001);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e10c ,0x1010001);
        write_reg_word(0x1fa7e114 ,0x10000);
        
        // XFI_RX_on
        write_reg_word(0x1fa7e824 ,0x1000000);
        write_reg_word(0x1fa7e824 ,0x1010000);
        write_reg_word(0x1fa7e824 ,0x1010100);
        write_reg_word(0x1fa7e824 ,0x1010101);
        write_reg_word(0x1fa7e81c ,0x0);
        write_reg_word(0x1fa7e81c ,0x100);
        write_reg_word(0x1fa7e81c ,0x101);
        write_reg_word(0x1fa7e894 ,0x100);
        write_reg_word(0x1fa7e894 ,0x101);
        write_reg_word(0x1fa7e84c ,0x1000000);
        write_reg_word(0x1fa7e84c ,0x1010000);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        write_reg_word(0x1fa7e34c ,0x1000000);
        write_reg_word(0x1fa7e34c ,0x1010000);
        write_reg_word(0x1fa7e34c ,0x1010100);
        write_reg_word(0x1fa7e34c ,0x1010101);
        write_reg_word(0x1fa7e350 ,0x1);
        write_reg_word(0x1fa7e38c ,0x1);
        write_reg_word(0x1fa7f0fc ,0x80505);
        write_reg_word(0x1fa7f0fc ,0x60505);
        write_reg_word(0x1fa7e108 ,0x1010001);
        write_reg_word(0x1fa7e108 ,0x1000001);
        write_reg_word(0x1fa7e108 ,0x1);
        write_reg_word(0x1fa7e10c ,0x1010000);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7f100 ,0x100);
        write_reg_word(0x1fa7f108 ,0x0);
        write_reg_word(0x1fa7e460 ,0x2);
        write_reg_word(0x1fa7e460 ,0x22);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        
        // XFI_RX_L2R
        write_reg_word(0x1fa7e818 ,0x1010100);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        
        // XFI_RX_OSCal
        write_reg_word(0x1fa7e33c ,0x1000000);
        write_reg_word(0x1fa7e330 ,0x101);
        write_reg_word(0x1fa7e83c ,0x1000000);
        write_reg_word(0x1fa7e83c ,0x1010000);
        write_reg_word(0x1fa7e840 ,0x1000000);
        write_reg_word(0x1fa7e840 ,0x1010000);
        write_reg_word(0x1fa7e840 ,0x1010100);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e110 ,0x1);
        
        // XFI_RX_pical
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e15c ,0x400);
        write_reg_word(0x1fa7e118 ,0x8);
        write_reg_word(0x1fa7e204 ,0x1000101);
        write_reg_word(0x1fa7e328 ,0x0);
        write_reg_word(0x1fa7e334 ,0x1010001);
        write_reg_word(0x1fa7e328 ,0x0);
        write_reg_word(0x1fa7e334 ,0x1010000);
        write_reg_word(0x1fa7e31c ,0x1010100);
        write_reg_word(0x1fa7e318 ,0x0);
        write_reg_word(0x1fa7e324 ,0x10101);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e30c ,0x0);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e328 ,0x100);
        write_reg_word(0x1fa7e328 ,0x101);
        write_reg_word(0x1fa7e318 ,0x100);
        write_reg_word(0x1fa7e110 ,0x101);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e318 ,0x0);
        write_reg_word(0x1fa7e30c ,0x1);
        
        // XFI_RX_pdos
        write_reg_word(0x1fa7e894 ,0x1000101);
        write_reg_word(0x1fa7e894 ,0x1010101);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e304 ,0x1010101);
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e084 ,0x101);
        write_reg_word(0x1fa7e084 ,0x1);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x10100);
        write_reg_word(0x1fa7e084 ,0x1);
        write_reg_word(0x1fa7e084 ,0x0);
        write_reg_word(0x1fa7e200 ,0x20001);
        write_reg_word(0x1fa7e328 ,0x101);
        write_reg_word(0x1fa7e334 ,0x1000000);
        write_reg_word(0x1fa7e208 ,0x10100);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e328 ,0x10101);
        write_reg_word(0x1fa7e208 ,0x10101);
        write_reg_word(0x1fa7e110 ,0x10000);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e084 ,0x0);
        write_reg_word(0x1fa7e084 ,0x100);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e084 ,0x100);
        write_reg_word(0x1fa7e084 ,0x101);
        write_reg_word(0x1fa7e894 ,0x1010101);
        write_reg_word(0x1fa7e894 ,0x1000101);
        
        // XFI_RX_feos
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e144 ,0x30);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1000100);
        write_reg_word(0x1fa7e204 ,0x1010001);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e32c ,0x10000);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e110 ,0x1000001);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e110 ,0x0);
        
        // XFI_RX_sdcal
        write_reg_word(0x1fa7e898 ,0x100);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e204 ,0x10101);
        write_reg_word(0x1fa7e32c ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e338 ,0x1000000);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x10100);
        write_reg_word(0x1fa7e114 ,0x10001);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e898 ,0x100);
        write_reg_word(0x1fa7e83c ,0x1010000);
        write_reg_word(0x1fa7e83c ,0x1000000);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e840 ,0x1000101);
        write_reg_word(0x1fa7e840 ,0x1000100);
        
        // XFI_phy_status
        write_reg_word(0x1fa7e114 ,0x10100);
        write_reg_word(0x1fa7e10c ,0x1000000);
        
        // XFI_DIG_reset_release
        write_reg_word(0x1fa7e460 ,0x7f);
        
        //XFI_L2D
        write_reg_word(0x1fa7e818 ,0x1010101);
        write_reg_word(0x1fa7e818 ,0x1000101);
        write_reg_word(0x1fa7e818 ,0x1010101);
        
        // XFI_RX_rxrdy
        write_reg_word(0x1fa7e114 ,0x1010100);
        write_reg_word(0x1fa7e10c ,0x0);
        write_reg_word(0x1fa7e460 ,0x7e);
        write_reg_word(0x1fa7e460 ,0x30fff);
        
        ///////////////////////////////////  PMA End   ///////////////////////////////////


        /////////////////////////////////// PCS Start  ///////////////////////////////////
        write_reg_word(0x1fa70a00,0x4c9cc000);
		write_reg_word(0x1fa74018,0x34);
		//b'hsgmii_init\r\n'
		//b'xsgmii 1 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 1, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa70000,0x140);
		//b'hsgmii\r\n'
		//b'HGMII_2.5G\r\n'
		write_reg_word(0x1fa76000,0xc000c11);
		//b'HSGMII_2.5 exit\r\n'
		//printf("HSGMII_2.5 exit\n");        
        ///////////////////////////////////  PCS End   ///////////////////////////////////
    }
    else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_SGMII_MODE)) 
    {
        if(UBOOT_DEBUG_LOG)printf("ETH SGMII Mode Init\n");
        //SCU Setting
        wan_config = read_reg_word(0x1fb00070);    
        scu_ssr3 = read_reg_word(0x1fb00094);    
        write_reg_word(0x1fb00070 ,(wan_config&0xFEFFFFFF));//Set as WAN CONFIG[24] as ETH XFI Mode   
        scu_ssr3 = scu_ssr3 & 0xFFFF9FFF;
        scu_ssr3 = (scu_ssr3 | (0x2<<13));
        write_reg_word(0x1fb00094,scu_ssr3);
        /////////////////////////////////// PMA Start  ///////////////////////////////////
        //printf("ETH SGMII Mode PMA Init\n");
        
        // XFI_DIG_reset_hold
        write_reg_word(0x1fa7e460 ,0x0);
        
        // XFI_TXPLL
        write_reg_word(0x1fa7e854 ,0x1000000);
        write_reg_word(0x1fa7e004 ,0x1100a00);
        write_reg_word(0x1fa7e004 ,0x1100a01);
        write_reg_word(0x1fa7e444 ,0x11309c4);
        write_reg_word(0x1fa7e444 ,0x11300fa);
        write_reg_word(0x1fa7e448 ,0x9b14a0);
        write_reg_word(0x1fa7e448 ,0x9b0210);
        write_reg_word(0x1fa7e440 ,0x409bf);
        write_reg_word(0x1fa7e440 ,0x40026);
        write_reg_word(0x1fa7e468 ,0x28000);
        write_reg_word(0x1fa7e028 ,0x303);
        write_reg_word(0x1fa7e024 ,0x10100);
        write_reg_word(0x1fa7e794 ,0x1000000);
        write_reg_word(0x1fa7e798 ,0x32000000);
        write_reg_word(0x1fa7e048 ,0x64000000);
        write_reg_word(0x1fa7e04c ,0x64000000);
        
        
        // XFI_TX
        write_reg_word(0x1fa7f0c4 ,0x1010400);
        write_reg_word(0x1fa7f0c4 ,0x1010401);
        write_reg_word(0x1fa7f000 ,0x10040001);
        write_reg_word(0x1fa7f000 ,0x10040101);
        write_reg_word(0x1fa7e874 ,0x1000000);
        write_reg_word(0x1fa7e874 ,0x1010000);
        write_reg_word(0x1fa7e778 ,0x1000000);
        write_reg_word(0x1fa7e778 ,0x1000100);
        write_reg_word(0x1fa7e780 ,0x100);
        write_reg_word(0x1fa7e780 ,0x1000100);
        write_reg_word(0x1fa7e778 ,0x1000100);
        write_reg_word(0x1fa7e780 ,0x1000100);
        write_reg_word(0x1fa7e77c ,0x1000000);
        write_reg_word(0x1fa7e77c ,0x1020000);
        write_reg_word(0x1fa7e784 ,0x100);
        write_reg_word(0x1fa7e784 ,0x101);
        write_reg_word(0x1fa7e580 ,0x1);
        
        // XFI_RX
        write_reg_word(0x1fa7e08c ,0x101);
        write_reg_word(0x1fa7e104 ,0x2);
        write_reg_word(0x1fa7e08c ,0x101);
        write_reg_word(0x1fa7e090 ,0x3e80002);
        write_reg_word(0x1fa7e09c ,0x3e80002);
        write_reg_word(0x1fa7e094 ,0x3e80002);
        write_reg_word(0x1fa7e098 ,0x3e80002);
        write_reg_word(0x1fa7e120 ,0x103);
        write_reg_word(0x1fa7e068 ,0x24001f0);
        write_reg_word(0x1fa7e068 ,0x24001c0);
        write_reg_word(0x1fa7e088 ,0x0);
        write_reg_word(0x1fa7e088 ,0x1);
        write_reg_word(0x1fa7f0d4 ,0x4cc31030);
        write_reg_word(0x1fa7f120 ,0x3ff00);
        write_reg_word(0x1fa7f0dc ,0x0);
        write_reg_word(0x1fa7e814 ,0x1000000);
        write_reg_word(0x1fa7e814 ,0x1010000);
        write_reg_word(0x1fa7e88c ,0x100);
        write_reg_word(0x1fa7f0d4 ,0x4cc318b0);
        write_reg_word(0x1fa7e88c ,0x103);
        write_reg_word(0x1fa7e360 ,0x300);
        write_reg_word(0x1fa7e294 ,0x3);
        write_reg_word(0x1fa7e300 ,0x1010100);
        write_reg_word(0x1fa7f0f8 ,0x4010808);
        write_reg_word(0x1fa7f0d8 ,0x100010a);
        write_reg_word(0x1fa7f10c ,0x70604);
        write_reg_word(0x1fa7f0cc ,0x1000000);
        write_reg_word(0x1fa7f0d8 ,0x101010a);
        write_reg_word(0x1fa7f0d8 ,0x1010129);
        write_reg_word(0x1fa7f0e8 ,0x2000003);
        write_reg_word(0x1fa7f0cc ,0x1000000);
        write_reg_word(0x1fa7e76c ,0x1000000);
        write_reg_word(0x1fa7e76c ,0x1030000);
        write_reg_word(0x1fa7e374 ,0x0);
        
        // XFI_ANA
        write_reg_word(0x1fa7f084 ,0x101031b);
        write_reg_word(0x1fa7f088 ,0x0);
        write_reg_word(0x1fa7f088 ,0x1);
        write_reg_word(0x1fa7f0a8 ,0x10000);
        write_reg_word(0x1fa7f0a8 ,0x10100);
        write_reg_word(0x1fa7f064 ,0x1040001);
        write_reg_word(0x1fa7f064 ,0x1040000);
        write_reg_word(0x1fa7f068 ,0x300);
        write_reg_word(0x1fa7f068 ,0x0);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f080 ,0x82);
        write_reg_word(0x1fa7f080 ,0x0);
        write_reg_word(0x1fa7f07c ,0x0);
        write_reg_word(0x1fa7f084 ,0x1010000);
        write_reg_word(0x1fa7f098 ,0x0);
        write_reg_word(0x1fa7f050 ,0x1f05000c);
        write_reg_word(0x1fa7f050 ,0x1f050031);
        write_reg_word(0x1fa7f054 ,0x180005);
        write_reg_word(0x1fa7f054 ,0x180b05);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f054 ,0x180b02);
        write_reg_word(0x1fa7f074 ,0x3000001);
        write_reg_word(0x1fa7f078 ,0x4040401);
        write_reg_word(0x1fa7f078 ,0x4040701);
        write_reg_word(0x1fa7f058 ,0x30003ff);
        write_reg_word(0x1fa7f058 ,0x30002ff);
        write_reg_word(0x1fa7f05c ,0x101);
        write_reg_word(0x1fa7f058 ,0x30002e4);
        write_reg_word(0x1fa7f054 ,0x180b02);
        write_reg_word(0x1fa7f094 ,0x10010);
        write_reg_word(0x1fa7e858 ,0x100);
        write_reg_word(0x1fa7f05c ,0x101);
        write_reg_word(0x1fa7f070 ,0x4000b03);
        write_reg_word(0x1fa7f074 ,0x3000001);
        write_reg_word(0x1fa7f094 ,0x1000f);
        write_reg_word(0x1fa7f070 ,0x4000b03);
        write_reg_word(0x1fa7f074 ,0x3000001);
        write_reg_word(0x1fa7f06c ,0x1000003);
        write_reg_word(0x1fa7f0c0 ,0x4020000);
        write_reg_word(0x1fa7f0c0 ,0x2020000);
        write_reg_word(0x1fa7f118 ,0x1000000);
        write_reg_word(0x1fa7f118 ,0x1010000);
        write_reg_word(0x1fa7f118 ,0x1010100);
        write_reg_word(0x1fa7f11c ,0x401);
        write_reg_word(0x1fa7f0e8 ,0x800003);
        write_reg_word(0x1fa7f0e0 ,0x1078000);
        write_reg_word(0x1fa7f100 ,0x100);
        write_reg_word(0x1fa7f110 ,0x0);
        
        // XFI_TXPLL_ON
        write_reg_word(0x1fa7e074 ,0x10000);
        write_reg_word(0x1fa7e078 ,0x404);
        write_reg_word(0x1fa7e07c ,0x40004);
        write_reg_word(0x1fa7e080 ,0xff0000d0);
        write_reg_word(0x1fa7e118 ,0xa000a01);
        write_reg_word(0x1fa7e118 ,0xa01);
        write_reg_word(0x1fa7e118 ,0x1);
        write_reg_word(0x1fa7e11c ,0x1);
        write_reg_word(0x1fa7e160 ,0x1003100);
        write_reg_word(0x1fa7e160 ,0x1002e00);
        write_reg_word(0x1fa7e160 ,0x1002e01);
        write_reg_word(0x1fa7e160 ,0x1012e01);
        write_reg_word(0x1fa7e164 ,0x500);
        write_reg_word(0x1fa7e164 ,0x0);
        write_reg_word(0x1fa7e100 ,0xc80005);
        write_reg_word(0x1fa7e100 ,0xa0005);
        write_reg_word(0x1fa7e144 ,0x0);
        write_reg_word(0x1fa7e06c ,0x1940);
        write_reg_word(0x1fa7e06c ,0x3f40);
        write_reg_word(0x1fa7e06c ,0x13f40);
        write_reg_word(0x1fa7e070 ,0x18);
        write_reg_word(0x1fa7e48c ,0x1000203);
        write_reg_word(0x1fa7e48c ,0x1000202);
        write_reg_word(0x1fa7e170 ,0xa503);
        write_reg_word(0x1fa7e170 ,0xa502);
        write_reg_word(0x1fa7e174 ,0x4010000);
        write_reg_word(0x1fa7e184 ,0x40001ff);
        write_reg_word(0x1fa7e178 ,0x20403);
        write_reg_word(0x1fa7e320 ,0x10101);
        write_reg_word(0x1fa7e200 ,0x101);
        write_reg_word(0x1fa7e200 ,0x1);
        write_reg_word(0x1fa7e148 ,0x109f0a01);
        write_reg_word(0x1fa7e148 ,0x10f00a01);
        write_reg_word(0x1fa7e148 ,0x8700a01);
        write_reg_word(0x1fa7e854 ,0x1000000);
        write_reg_word(0x1fa7e854 ,0x1010000);
        write_reg_word(0x1fa7e000 ,0x1000000);
        write_reg_word(0x1fa7e854 ,0x1010100);
        write_reg_word(0x1fa7e854 ,0x1010101);
        write_reg_word(0x1fa7f094 ,0x1000f);
        write_reg_word(0x1fa7f060 ,0x101);
        write_reg_word(0x1fa7f094 ,0xf);
        write_reg_word(0x1fa7f04c ,0x0);
        
        // XFI_TX_ON
        write_reg_word(0x1fa7e260 ,0x1);
        write_reg_word(0x1fa7e410 ,0x100);
        write_reg_word(0x1fa7e410 ,0x101);
        write_reg_word(0x1fa7e260 ,0x101);
        
        // XFI_RX_preset
        write_reg_word(0x1fa7f114 ,0x40200);
        write_reg_word(0x1fa7f114 ,0x20200);
        write_reg_word(0x1fa7f110 ,0x3000000);
        write_reg_word(0x1fa7f10c ,0x70604);
        write_reg_word(0x1fa7e114 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1010001);
        write_reg_word(0x1fa7e818 ,0x100);
        write_reg_word(0x1fa7e768 ,0x1000000);
        write_reg_word(0x1fa7e330 ,0x100);
        write_reg_word(0x1fa7e33c ,0x1010001);
        write_reg_word(0x1fa7e330 ,0x100);
        write_reg_word(0x1fa7e33c ,0x1000001);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e10c ,0x1010001);
        write_reg_word(0x1fa7e114 ,0x10000);
        
        // XFI_RX_on
        write_reg_word(0x1fa7e824 ,0x1000000);
        write_reg_word(0x1fa7e824 ,0x1010000);
        write_reg_word(0x1fa7e824 ,0x1010100);
        write_reg_word(0x1fa7e824 ,0x1010101);
        write_reg_word(0x1fa7e81c ,0x0);
        write_reg_word(0x1fa7e81c ,0x100);
        write_reg_word(0x1fa7e81c ,0x101);
        write_reg_word(0x1fa7e894 ,0x100);
        write_reg_word(0x1fa7e894 ,0x101);
        write_reg_word(0x1fa7e84c ,0x1000000);
        write_reg_word(0x1fa7e84c ,0x1010000);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        write_reg_word(0x1fa7e34c ,0x1000000);
        write_reg_word(0x1fa7e34c ,0x1010000);
        write_reg_word(0x1fa7e34c ,0x1010100);
        write_reg_word(0x1fa7e34c ,0x1010101);
        write_reg_word(0x1fa7e350 ,0x1);
        write_reg_word(0x1fa7e38c ,0x1);
        write_reg_word(0x1fa7f0fc ,0x80505);
        write_reg_word(0x1fa7e108 ,0x1010001);
        write_reg_word(0x1fa7e108 ,0x1000001);
        write_reg_word(0x1fa7e108 ,0x1);
        write_reg_word(0x1fa7e10c ,0x1010000);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7f100 ,0x100);
        write_reg_word(0x1fa7f108 ,0x0);
        write_reg_word(0x1fa7e460 ,0x2);
        write_reg_word(0x1fa7e460 ,0x22);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        
        // XFI_RX_L2R
        write_reg_word(0x1fa7e818 ,0x1010100);
        write_reg_word(0x1fa7e818 ,0x1000100);
        write_reg_word(0x1fa7e818 ,0x1010100);
        
        // XFI_RX_OSCal
        write_reg_word(0x1fa7e33c ,0x1000000);
        write_reg_word(0x1fa7e330 ,0x101);
        write_reg_word(0x1fa7e83c ,0x1000000);
        write_reg_word(0x1fa7e83c ,0x1010000);
        write_reg_word(0x1fa7e840 ,0x1000000);
        write_reg_word(0x1fa7e840 ,0x1010000);
        write_reg_word(0x1fa7e840 ,0x1010100);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e110 ,0x1);
        
        // XFI_RX_pical
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e15c ,0x400);
        write_reg_word(0x1fa7e118 ,0x8);
        write_reg_word(0x1fa7e204 ,0x1000101);
        write_reg_word(0x1fa7e328 ,0x0);
        write_reg_word(0x1fa7e334 ,0x1010001);
        write_reg_word(0x1fa7e328 ,0x0);
        write_reg_word(0x1fa7e334 ,0x1010000);
        write_reg_word(0x1fa7e31c ,0x1010100);
        write_reg_word(0x1fa7e318 ,0x0);
        write_reg_word(0x1fa7e324 ,0x10101);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e30c ,0x0);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e328 ,0x100);
        write_reg_word(0x1fa7e328 ,0x101);
        write_reg_word(0x1fa7e318 ,0x100);
        write_reg_word(0x1fa7e110 ,0x101);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e318 ,0x0);
        write_reg_word(0x1fa7e30c ,0x1);
        
        // XFI_RX_pdos
        write_reg_word(0x1fa7e894 ,0x1000101);
        write_reg_word(0x1fa7e894 ,0x1010101);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e304 ,0x1010101);
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e084 ,0x101);
        write_reg_word(0x1fa7e084 ,0x1);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x10100);
        write_reg_word(0x1fa7e084 ,0x1);
        write_reg_word(0x1fa7e084 ,0x0);
        write_reg_word(0x1fa7e200 ,0x20001);
        write_reg_word(0x1fa7e328 ,0x101);
        write_reg_word(0x1fa7e334 ,0x1000000);
        write_reg_word(0x1fa7e208 ,0x10100);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e328 ,0x10101);
        write_reg_word(0x1fa7e208 ,0x10101);
        write_reg_word(0x1fa7e110 ,0x10000);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e084 ,0x0);
        write_reg_word(0x1fa7e084 ,0x100);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e084 ,0x100);
        write_reg_word(0x1fa7e084 ,0x101);
        write_reg_word(0x1fa7e894 ,0x1010101);
        write_reg_word(0x1fa7e894 ,0x1000101);
        
        // XFI_RX_feos
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e308 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1010100);
        write_reg_word(0x1fa7e144 ,0x30);
        write_reg_word(0x1fa7e32c ,0x0);
        write_reg_word(0x1fa7e338 ,0x1000100);
        write_reg_word(0x1fa7e204 ,0x1010001);
        write_reg_word(0x1fa7e110 ,0x0);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e108 ,0x0);
        write_reg_word(0x1fa7e32c ,0x10000);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e110 ,0x1000001);
        write_reg_word(0x1fa7e110 ,0x1);
        write_reg_word(0x1fa7e110 ,0x0);
        
        // XFI_RX_sdcal
        write_reg_word(0x1fa7e898 ,0x100);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e204 ,0x10101);
        write_reg_word(0x1fa7e32c ,0x10000);
        write_reg_word(0x1fa7e10c ,0x1000000);
        write_reg_word(0x1fa7e338 ,0x1000000);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e204 ,0x1010101);
        write_reg_word(0x1fa7e32c ,0x10100);
        write_reg_word(0x1fa7e114 ,0x10001);
        write_reg_word(0x1fa7e114 ,0x10000);
        write_reg_word(0x1fa7e898 ,0x100);
        write_reg_word(0x1fa7e83c ,0x1010000);
        write_reg_word(0x1fa7e83c ,0x1000000);
        write_reg_word(0x1fa7e840 ,0x1010101);
        write_reg_word(0x1fa7e840 ,0x1000101);
        write_reg_word(0x1fa7e840 ,0x1000100);
        
        // XFI_phy_status
        write_reg_word(0x1fa7e114 ,0x10100);
        write_reg_word(0x1fa7e10c ,0x1000000);
        
        // XFI_DIG_reset_release
        write_reg_word(0x1fa7e460 ,0x7f);
        
        //XFI_L2D
        write_reg_word(0x1fa7e818 ,0x1010101);
        write_reg_word(0x1fa7e818 ,0x1000101);
        write_reg_word(0x1fa7e818 ,0x1010101);
        
        // XFI_RX_rxrdy
        write_reg_word(0x1fa7e114 ,0x1010100);
        write_reg_word(0x1fa7e10c ,0x0);
        write_reg_word(0x1fa7e460 ,0x7e);
        write_reg_word(0x1fa7e460 ,0x30fff);
        
        ///////////////////////////////////  PMA End   ///////////////////////////////////


        /////////////////////////////////// PCS Start  ///////////////////////////////////
        //printf("ETH SGMII Mode PCS Init\n");
		write_reg_word(0x1fa70034,0x31120029);
		write_reg_word(0x1fa70a24,0x1);
		//b'xsgmii_init\r\n'
		write_reg_word(0x1fa70a00,0x4c9cc000);
		write_reg_word(0x1fa74018,0x24);
		//b'sgmii_init\r\n'
		write_reg_word(0x1fa76018,0x7070707);
		write_reg_word(0x1fa76020,0xff);
		//b'SGMII_Interrupt_init\r\n'
		write_reg_word(0x1fa74014,0x1);
		//b'xsgmii 2,int enable 1\r\n'
		//b'\r\n'
		//b' request_irq() (irq number: 138) OK \r\n'
		write_reg_word(0x1fa70a20,0x1);
		write_reg_word(0x1fa7414c,0x1);
		//b'rg_INTERRUPT_EN_0 1\r\n'
		//b'xsgmii: sync_int exit 1\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 2, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa70000,0x140);
		//b'sgmii_rate_api rate 0\r\n'
		//b'SGMII_1G\r\n'
		write_reg_word(0x1fa76000,0xc000c11);
		write_reg_word(0x1fa74018,0x24);
		//b'SGMII_1G exit\r\n'
		//printf("SGMII_1G exit\n");        
        ///////////////////////////////////  PCS End   ///////////////////////////////////

    }
    else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_USXGMII_MODE))
    {
        if(UBOOT_DEBUG_LOG)printf("ETH USXGMII Mode Init\n");
        //SCU Setting
        wan_config = read_reg_word(0x1fb00070);    
        scu_ssr3 = read_reg_word(0x1fb00094);    
        write_reg_word(0x1fb00070 ,(wan_config&0xFEFFFFFF));//Set as WAN CONFIG[24] as ETH XFI Mode   
        scu_ssr3 = scu_ssr3 & 0xFFFF9FFF;
        scu_ssr3 = (scu_ssr3 | (0x1<<13));
        write_reg_word(0x1fb00094,scu_ssr3);
        
        /////////////////////////////////// PMA Start  ///////////////////////////////////       
        //printf("ETH USXGMII Mode PMA Init\n");
        AN7583_XFI_10G_PMA_BringUp();


        /////////////////////////////////// PCS Start  ///////////////////////////////////
        //printf("ETH USXGMII Mode PCS Init\n");
        write_reg_word(0x1fa74100,0x10010001);
        //b'PCS E0/E1 solution\r\n'
		write_reg_word(0x1fa75b2c,0x0);
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa75bc0,0x0);
		write_reg_word(0x1fa75bc4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		write_reg_word(0x1fa75bd8,0x0);
		write_reg_word(0x1fa75bdc,0x0);
		write_reg_word(0x1fa75be4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'AN mode\r\n'
		//printf("AN mode\n");
		//b'xSGMII_AN_API 0, enable : 1\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa75bf8,0x6330001);
		//b'_rg_usxgmii_an_control_0 6330001\r\n'
		//b'_rg_usxgmii_an_control_1 0\r\n'
		//b'xSGMII_AN_AutoSetting\r\n'
		write_reg_word(0x1fa75C20,0x110000);
		write_reg_word(0x1fa76000,0xc11);
		
        //set mac reg init
      	write_reg_word(0x1fa09000,0x71082800);
      	//printf("set mac reg init\n");
#if 1 //Interrupt for USXGMII		
		//enable USXGMII interrupt(pcs)
		usxgmii_pcs_int_init(1);
		//ARM interrupt settings
		gic_dic_set_icenable(50);
		gic_dic_set_enable(50);
		irq_register(50, (void (*)(void))AN7583_usxgmii_isr,1);
#endif	
		//printf("USXGMII_10G exit\n");	
    }
    if((serdes_intf[0][len_eth-1] != '4') && (serdes_intf[0][len_eth-1] != '5'))
    {
        if(UBOOT_DEBUG_LOG)printf("Serdes2 Uboot Init Finished\n");   
    }


//////////////////////////////   AN7583 SERDES3 Start  ///////////////////////////////
    len_eth = strlen(serdes_intf[SERDES_PCIE1]);
    if(len_eth < 2)
    {
        printf("error P0_len=%d\n",len_eth);
        return;
    }
    if((serdes_intf[SERDES_PCIE1][2] != '0') && (serdes_intf[SERDES_PCIE1][2] != '1') && (serdes_intf[SERDES_PCIE1][2] != '5'))
    {
        //printf("Serdes3 Uboot Init, P0 VER = AN7583_P0.0.09.16\n"); 
        switch(serdes_intf[SERDES_PCIE1][2])
        {
            case SERDES_P0_USXGMII  : printf("Serdes3 USXGMII MODE Init, P0 VER = AN7583_P0.0.09.16\n"); 
            case SERDES_P0_XFI      : printf("Serdes3 XFI MODE Init, P0 VER = AN7583_P0.0.09.16\n");break;
            default :break;
        }
    }


      
   if((serdes_intf[SERDES_PCIE1][1] == SERDES_ETHERLAN) && (serdes_intf[SERDES_PCIE1][2] == SERDES_P0_USXGMII))
   {
	    scu_sstr=read_reg_word( 0x1fb0009c) & 0xFFFFE7FF;
		scu_sstr=scu_sstr & 0xFFFFE7FF;
		scu_sstr=scu_sstr | (1<<11);
		write_reg_word( 0x1fb0009c, scu_sstr);
        ///////////////////////////////////  PMA  ///////////////////////////////////		     
        if(UBOOT_DEBUG_LOG)printf("P0 USXGMII Init\n");
        AN7583_PCIE0_10G_PMA_BringUp();
       
        ///////////////////////////////////  PCS  ///////////////////////////////////                 
        if(UBOOT_DEBUG_LOG)printf("P0 USXGMII Mode PCS Init\n");
        write_reg_word(0x1fc75c34,0x1e11111);
		write_reg_word(0x1fc74100,0x10010001);

		//printf("AN mode\n");

		write_reg_word(0x1fc75bf8,0x6330001);
		write_reg_word(0x1fc75C20,0x110000);
		write_reg_word(0x1fc76000,0xc11);
		
        //set mac reg init
      	write_reg_word(0x1fa04000,0x71082800);
      	//printf("set mac reg init\n");
	
		//printf("P0 USXGMII_10G exit\n");

   }
   else if((serdes_intf[SERDES_PCIE1][1] == SERDES_ETHERLAN) && (serdes_intf[SERDES_PCIE1][2] == SERDES_P0_XFI)) 
   {
	    scu_sstr=read_reg_word( 0x1fb0009c) & 0xFFFFE7FF;
		scu_sstr=scu_sstr & 0xFFFFE7FF;
		scu_sstr=scu_sstr | (1<<11);
		write_reg_word( 0x1fb0009c, scu_sstr);        
       ///////////////////////////////////  PMA  ///////////////////////////////////            
       if(UBOOT_DEBUG_LOG)printf("AN7583 P0 XFI Mode Init\n");
       AN7583_PCIE0_10G_PMA_BringUp();  
       ///////////////////////////////////  PCS  /////////////////////////////////// 
       //printf("P0 XFI Mode PCS Init\n");
       write_reg_word(0x1fc75c34,0x01e11111);
       write_reg_word(0x1fc74100,0x10010001);
       write_reg_word(0x1fc75bf8,0x6330000);
       
       write_reg_word(0x1fc75bfc,0x1001);
       write_reg_word(0x1fc76000,0xc000c11);
       write_reg_word(0x1fc7602c,0x104);
       
       //set mac reg init
       write_reg_word(0x1fa04000,0x71082800);
       //printf("set mac reg init\n");
       
       
       write_reg_word(0x1fc75c20,0x1000);
       //printf("P0 XFI_10G exit\n");
   }
   if((serdes_intf[SERDES_PCIE1][2] != '0') && (serdes_intf[SERDES_PCIE1][2] != '1') && (serdes_intf[SERDES_PCIE1][2] != '5'))
   {
       if(UBOOT_DEBUG_LOG)printf("Serdes3 Uboot Init Finished\n");   
   }


    
///////////////////////////////////    Qphy   ////////////////////////////////////
		len_eth = strlen(serdes_intf[SERDES_PCIE2]);
	if(len_eth < 2)
	{
		printf("error P1_len=%d\n",len_eth);
		return;
	}
	printf("QP HSGMII VER = AN7583_ETH.0.3.240613\n");
	//printf("getenv : %s\n",serdes_intf[SERDES_PCIE2]);
	if(serdes_intf[SERDES_PCIE2][len_eth-1] == SERDES_P1_SGMII_MODE)
    {
		//printf("QP P1 HSGMII start\n");
		write_reg_word( 0x1fa54518, 0x60);
		write_reg_word( 0x1fa5e690, 0x6800000);
		write_reg_word( 0x1fa5f030, 0x2aa0004);
		write_reg_word( 0x1fa5a30c, 0x3400000);
		write_reg_word( 0x1fa5e004, 0x200);
		write_reg_word( 0x1fa5e008, 0x8400000);
		write_reg_word( 0x1fa5e408, 0x21170315);
		write_reg_word( 0x1fa5f03c, 0x122802a2);
		write_reg_word( 0x1fa5e410, 0x4);
		write_reg_word( 0x1fa5a324, 0x2);
		write_reg_word( 0x1fa5f02c, 0xa840010);
		write_reg_word( 0x1fa5f028, 0x40000);
		write_reg_word( 0x1fa5e248, 0x7a000000);
		write_reg_word( 0x1fa5a330, 0x7);
		write_reg_word( 0x1fa5e408, 0x21560315);
		write_reg_word( 0x1fa5e230, 0x7a000000);
		write_reg_word( 0x1fa5f00c, 0x14);
		write_reg_word( 0x1fa5f018, 0x4000f00);
		write_reg_word( 0x1fa5f008, 0xa5560000);
		write_reg_word( 0x1fa5e63c, 0x4010);
		write_reg_word( 0x1fa5e23c, 0x10100);
		write_reg_word( 0x1fa5e23c, 0x1010100);
		write_reg_word( 0x1fa5e208, 0x1000a);
		write_reg_word( 0x1fa5f004, 0xe00);
		write_reg_word( 0x1fa5e128, 0x4002000);
		write_reg_word( 0x1fa5e120, 0xf00002aa);
		write_reg_word( 0x1fa5e640, 0x64);
		write_reg_word( 0x1fa5e644, 0x2710);
		write_reg_word( 0x1fa5e400, 0x60000);
		write_reg_word( 0x1fa5e400, 0x60001);
		udelay(100);
		scu_sstr=read_reg_word( 0x1fb0009c);
		scu_sstr=scu_sstr|(0x1<<13);
		write_reg_word( 0x1fb0009c, scu_sstr);
		write_reg_word( 0x1fa5a330, 0x7);
		write_reg_word( 0x1fa50a00, 0xc9cc400);
		write_reg_word( 0x1fa50000, 0x140);
		write_reg_word( 0x1fa56000, 0xc000c00);
		write_reg_word( 0x1fa5602c, 0x4);
		write_reg_word( 0x1fa50000, 0x8140);
	}

	len_eth = strlen(serdes_intf[SERDES_USB]);
	if(len_eth < 2)
	{
		printf("error U0_len=%d\n",len_eth);
		return;
	}
	if(serdes_intf[SERDES_USB][len_eth-1] == SERDES_U0_SGMII_MODE)
    {
		//printf("QP U0 HSGMII start\n");
		write_reg_word( 0x1fa64518, 0x60);
		write_reg_word( 0x1fa6e690, 0x6800000);
		write_reg_word( 0x1fa6f030, 0x2aa0004);
		write_reg_word( 0x1fa6a30c, 0x3400000);
		write_reg_word( 0x1fa6e004, 0x200);
		write_reg_word( 0x1fa6e008, 0x8400000);
		write_reg_word( 0x1fa6e408, 0x21170315);
		write_reg_word( 0x1fa6f03c, 0x122802a2);
		write_reg_word( 0x1fa6e410, 0x4);
		write_reg_word( 0x1fa6a324, 0x2);
		write_reg_word( 0x1fa6f02c, 0xa840010);
		write_reg_word( 0x1fa6f028, 0x40000);
		write_reg_word( 0x1fa6e248, 0x7a000000);
		write_reg_word( 0x1fa6a330, 0x7);
		write_reg_word( 0x1fa6e408, 0x21560315);
		write_reg_word( 0x1fa6e230, 0x7a000000);
		write_reg_word( 0x1fa6f00c, 0x14);
		write_reg_word( 0x1fa6f018, 0x4000f00);
		write_reg_word( 0x1fa6f008, 0xa5560000);
		write_reg_word( 0x1fa6e63c, 0x4010);
		write_reg_word( 0x1fa6e23c, 0x10100);
		write_reg_word( 0x1fa6e23c, 0x1010100);
		write_reg_word( 0x1fa6e208, 0x1000a);
		write_reg_word( 0x1fa6f004, 0xe00);
		write_reg_word( 0x1fa6e128, 0x4002000);
		write_reg_word( 0x1fa6e120, 0xf00002aa);
		write_reg_word( 0x1fa6e640, 0x64);
		write_reg_word( 0x1fa6e644, 0x2710);
		write_reg_word( 0x1fa6e400, 0x60000);
		write_reg_word( 0x1fa6e400, 0x60001);
		udelay(100);
		write_reg_word( 0x1fa6a330, 0x7);
		write_reg_word( 0x1fa60a00, 0xc9cc400);
		write_reg_word( 0x1fa60000, 0x140);
		write_reg_word( 0x1fa66000, 0xc000c00);
		write_reg_word( 0x1fa6602c, 0x4);
		write_reg_word( 0x1fa60000, 0x8140);
	}

	return;
#else
	unsigned char* serdes_intf[SERDES_INTF_MAX];
    //int i = 0;

	rg_type_t(HAL_RG_PXP_CDR_PR_INJ_MODE) RG_PXP_CDR_PR_INJ_MODE;
	rg_type_t(HAL_rg_force_da_pxp_cdr_pr_lpf_c_en) rg_force_da_pxp_cdr_pr_lpf_c_en;
	rg_type_t(HAL_rg_force_da_pxp_cdr_pr_idac) rg_force_da_pxp_cdr_pr_idac;
	rg_type_t(HAL_rg_force_da_pxp_cdr_pr_pieye_pwdb) rg_force_da_pxp_cdr_pr_pieye_pwdb;
	rg_type_t(HAL_SS_RX_FLL_b) SS_RX_FLL_b;	
	rg_type_t(HAL_SS_RX_FLL_1) SS_RX_FLL_1;
	rg_type_t(HAL_RO_RX_FREQDET) RO_RX_FREQDET;	
	//rg_type_t(HAL_SS_RX_FREQ_DET_2) SS_RX_FREQ_DET_2;
	//rg_type_t(HAL_SS_RX_FREQ_DET_1) SS_RX_FREQ_DET_1;
	//rg_type_t(HAL_SS_RX_FREQ_DET_4) SS_RX_FREQ_DET_4;	
	//rg_type_t(HAL_SS_RX_FREQ_DET_3) SS_RX_FREQ_DET_3;
	rg_type_t(HAL_SW_RST_SET) SW_RST_SET; 	

	uint PrCal_Serach = 0 , RO_FL_Out = 0;
  	uint pr_idac = 0  , RO_pr_idac = 0;
  	int cdr_pr_idac_tmp = 0, RO_state_freqdet = 0, turn_pr_idac_bit_position = 0;
	int RO_FL_Out_diff = 0, RO_FL_Out_diff_tmp = 0xffff;
	int len_eth = 0;
	
	
	serdes_intf[SERDES_ETH] = serdes_intf_env("serdes_ethernet");
    serdes_intf[SERDES_USB] = serdes_intf_env("serdes_usb1");
    serdes_intf[SERDES_PCIE1] = serdes_intf_env("serdes_wifi1");
    serdes_intf[SERDES_PCIE2] = serdes_intf_env("serdes_wifi2");
	serdes_intf[SERDES_PON] = serdes_intf_env("serdes_pon");

	len_eth = strlen(serdes_intf[SERDES_ETH]);
	if(len_eth < 2)
	{
		printf("error eth_len=%d\n",len_eth);
		return;
	}

	/*only support eth_serdes phy*/
    if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_XFI_MODE))
    {
		uint FL_Out_target = 0x9EDF ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
		u32 rg_value = 0;
        
        //FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
      	write_reg_word(0x1fb00094, 0xe0002820);//add
		//FT 50R restore
		rg_value = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114, rg_value);
        //b''
		//b'echo "1 0 0 0 0">/proc/xsgmii\r\n'
		//b'op = 1,xsgmii 0,mod 0,an 0,rate 0,buf 31,num 5,count a\r\n'
		//b'xsgmii_api: serdes 1,xsgmii 0,mod 0,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa7a000,0x10040001);
		//b'JCPLL BringUp 0\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa7a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa7a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa7a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa7a01c,0x104);
		write_reg_word(0x1fa7a020,0x30000);
		write_reg_word(0x1fa7a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa7a038,0x0);
		write_reg_word(0x1fa7a034,0x0);
		write_reg_word(0x1fa7a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa7a004,0x180000);
		write_reg_word(0x1fa7a008,0x101f0a);
		write_reg_word(0x1fa7a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa7a02c,0x4010100);
		write_reg_word(0x1fa7a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa7b800,0x25800000);
		write_reg_word(0x1fa7b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa7a014,0x10000);
		write_reg_word(0x1fa7a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa7a010,0x1000300);
		write_reg_word(0x1fa7a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa7a048,0xf20ff);
		write_reg_word(0x1fa7a024,0x5010100);
		write_reg_word(0x1fa7a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa7b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa7a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa7a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa7a068,0x0);
		write_reg_word(0x1fa7a06c,0x1010003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa7a080,0x0);
		write_reg_word(0x1fa7a07c,0x0);
		write_reg_word(0x1fa7a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa7a050,0x1f05000f);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa7a074,0x1000001);
		write_reg_word(0x1fa7a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa7b798,0x8400000);
		write_reg_word(0x1fa7b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa7a058,0x30004e4);
		write_reg_word(0x1fa7a05c,0x101);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa7a05c,0x1);
		write_reg_word(0x1fa7a074,0x1000001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa7a094,0x1000f);
		write_reg_word(0x1fa7a070,0x4000b03);
		write_reg_word(0x1fa7a074,0x1000001);
		write_reg_word(0x1fa7a06c,0x1010003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa7b854,0x1010101);
		udelay(200);
		//b'TX BringUp 0\r\n'
		//b'tx_rate_ctrl 0\r\n'
		write_reg_word(0x1fa7b580,0x2);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa7a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa7b874,0x1010000);
		write_reg_word(0x1fa7b77c,0x1050101);
		write_reg_word(0x1fa7b784,0x102);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa7b778,0x1010101);
		write_reg_word(0x1fa7b780,0x10b);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa7b260,0x101);
		//b'RX BringUp 0\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		write_reg_word(0x1fa7b374,0x2);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa7b184,0x40003ff);
		write_reg_word(0x1fa7a148,0x1010101);
		write_reg_word(0x1fa7a144,0x1000000);
		write_reg_word(0x1fa7a11c,0x2000401);
		write_reg_word(0x1fa7b004,0xc100a01);
		write_reg_word(0x1fa7a13c,0x20000);
		write_reg_word(0x1fa7a120,0x3ff08);
		write_reg_word(0x1fa7b320,0x10101);
		write_reg_word(0x1fa7b48c,0x1000202);
		write_reg_word(0x1fa7a0dc,0x0);
		write_reg_word(0x1fa7b80c,0x1000000);
		write_reg_word(0x1fa7b814,0x1010000);
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 2\r\n'
		write_reg_word(0x1fa7b88c,0x0);
		write_reg_word(0x1fa7b768,0x0);
		//b'RX_FE_VOS\r\n'
		//write_reg_word(0x1fa7b79c,0x10000);
		//b'RX_IMP 1\r\n'
		write_reg_word(0x1fa7a114,0x1040000);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b390,0x100001);
		write_reg_word(0x1fa7b394,0xffff0000);
		write_reg_word(0x1fa7b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa7a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa7b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa7b08c,0x101);
		write_reg_word(0x1fa7b104,0x2);
		write_reg_word(0x1fa7b090,0x320002);
		write_reg_word(0x1fa7b09c,0x320002);
		write_reg_word(0x1fa7b094,0x320002);
		write_reg_word(0x1fa7b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa7b76c,0x1000000);
		write_reg_word(0x1fa7a0dc,0x0);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa7a0e8,0x2000000);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa7a0f8,0x4010808);
		write_reg_word(0x1fa7a0fc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa7b120,0x103);
		write_reg_word(0x1fa7b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa7b38c,0x1);
		write_reg_word(0x1fa7b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b33c,0x1010100);
		write_reg_word(0x1fa7b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa7a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1000100);
		write_reg_word(0x1fa7b81c,0x100);
		write_reg_word(0x1fa7b894,0x100);
		write_reg_word(0x1fa7b84c,0x1000000);
		write_reg_word(0x1fa7b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa7a114,0x1020200);
		write_reg_word(0x1fa7a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa7a0d8,0x10242);
		write_reg_word(0x1fa7a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1010101);
		write_reg_word(0x1fa7b81c,0x101);
		write_reg_word(0x1fa7b894,0x101);
		write_reg_word(0x1fa7b84c,0x1010000);
		write_reg_word(0x1fa7b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x100);
		//b'RX_PRCal 0\r\n'
        write_reg_word(0x1fa7b460,0x20);
        write_reg_word(0x1fa7b150,0x9f439e7b);
        write_reg_word(0x1fa7b14c,0x7fff7fff);
        write_reg_word(0x1fa7b158,0x3307);
        write_reg_word(0x1fa7b154,0x9f439e7b);
        write_reg_word(0x1fa7a0f4,0x1000000);
        write_reg_word(0x1fa7b820,0x1010100);
        write_reg_word(0x1fa7b794,0x1010000);
        write_reg_word(0x1fa7b824,0x1010101);
        write_reg_word(0x1fa7b824,0x1000101);
        write_reg_word(0x1fa7b824,0x1010101);
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa7b158,0x3300);
		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa7b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa7b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init 1\r\n'
		write_reg_word(0x1fa74100,0x10010001);
		//b'PCS E0/E1 solution\r\n'
		write_reg_word(0x1fa75b2c,0x0);
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa75bc0,0x0);
		write_reg_word(0x1fa75bc4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		write_reg_word(0x1fa75bd8,0x0);
		write_reg_word(0x1fa75bdc,0x0);
		write_reg_word(0x1fa75be4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 0, enable : 0\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa75bf8,0x6330000);
		//b'_rg_usxgmii_an_control_0 6330000\r\n'
		//b'_rg_usxgmii_an_control_1 0\r\n'
		//b'usxgmii_rate_api rate 0 : 10GUSXGMII_10G\r\n'
		write_reg_word(0x1fa75bfc,0x1001);
		write_reg_word(0x1fa76000,0xc000c11);
		write_reg_word(0x1fa7602c,0x104);
      
      		//set mac reg init
      		write_reg_word(0x1fa09000,0x71082800);
      		printf("set mac reg init\n");
      
		//b'_rg_usxgmii_an_control_1 1001\r\n'
		//b'_rg_rate_adapt_ctrl_0 c000c11\r\n'
		//b'_rg_rate_adapt_ctrl_11 104\r\n'
		//b'USXGMII_10G exit\r\n'
		//b'usxgmii_pcs_an_ctrl7\r\n'
		write_reg_word(0x1fa75c20,0x1000);
		printf("XFI_10G exit\n");
		//b'xSGMII_Solution : 1\r\n'
		return ;
	}else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_HSGMII_MODE)) {
		uint FL_Out_target = 0xA000;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
		u32 rg_value = 0;
        
        //FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
      	write_reg_word(0x1fb00094, 0xe0004820); 
		//FT 50R restore
		rg_value = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114, rg_value);
        //b''
        //b'echo "1 1 0 0 0">/proc/xsgmii\r\n'
        //b'op = 1,xsgmii 1,mod 0,an 0,rate 0,buf 31,num 5,count a\r\n'
        //b'xsgmii_api: serdes 1,xsgmii 1,mod 0,rate 0,an 0\r\n'
        //b'xsgmii_chg 1\r\n'
        write_reg_word(0x1fa7a000,0x10040001);
        //b'JCPLL BringUp 1\r\n'
        //b'JCPLL_LDO\r\n'
        write_reg_word(0x1fa7a048,0x1020ff);
        //b'JCPLL_RSTB\r\n'
        write_reg_word(0x1fa7a01c,0x3000004);
		udelay(2000);
        write_reg_word(0x1fa7a01c,0x3000104);
        //b'JCPLL_EN\r\n'
        write_reg_word(0x1fa7b828,0x1000000);
        //b'JCPLL_SDM\r\n'
        write_reg_word(0x1fa7a01c,0x104);
        write_reg_word(0x1fa7a020,0x30000);
        write_reg_word(0x1fa7a024,0x5010100);
        //b'JCPLL_SSC\r\n'
        write_reg_word(0x1fa7a038,0x0);
        write_reg_word(0x1fa7a034,0x0);
        write_reg_word(0x1fa7a030,0x3018);
        //b'JCPLL_LPF\r\n'
        write_reg_word(0x1fa7a004,0x180000);
        write_reg_word(0x1fa7a008,0x101f0a);
        write_reg_word(0x1fa7a00c,0x2ff0000);
        //b'JCPLL_VCO\r\n'
        write_reg_word(0x1fa7a02c,0x4010100);
        write_reg_word(0x1fa7a030,0x301d);
        //b'JCPLL_PCW\r\n'
        write_reg_word(0x1fa7b800,0x25800000);
        write_reg_word(0x1fa7b79c,0x10000);
        //b'JCPLL_DIV\r\n'
        write_reg_word(0x1fa7a014,0x10000);
        write_reg_word(0x1fa7a02c,0x4010100);
        //b'JCPLL_KBand\r\n'
        write_reg_word(0x1fa7a010,0x1000300);
        write_reg_word(0x1fa7a00c,0x2e40000);
        //b'JCPLL_TCL\r\n'
        write_reg_word(0x1fa7a048,0x1020ff);
        write_reg_word(0x1fa7a024,0x5010100);
        write_reg_word(0x1fa7a028,0x10400);
        //b'JCPLL_EN\r\n'
        write_reg_word(0x1fa7b828,0x1010000);
        //b'JCPLL_Out\r\n'
        write_reg_word(0x1fa7b828,0x1010101);
		udelay(200);
        //b'TXPLL BringUp\r\n'
        //b'TXPLL_VCOLDO_Out\r\n'
        write_reg_word(0x1fa7a084,0x101031b);
        //b'TXPLL_RSTB\r\n'
        write_reg_word(0x1fa7a064,0x1040001);
		udelay(2000);
        //b'TXPLL_EN\r\n'
        write_reg_word(0x1fa7b854,0x1000000);
        //b'TXPLL_SDM\r\n'
        write_reg_word(0x1fa7a068,0x0);
        write_reg_word(0x1fa7a06c,0x1000003);
        //b'TXPLL_SSC\r\n'
        write_reg_word(0x1fa7a080,0x0);
        write_reg_word(0x1fa7a07c,0x0);
        write_reg_word(0x1fa7a084,0x1010000);
        //b'TXPLL_LPF\r\n'
        write_reg_word(0x1fa7a050,0x1f05000a);
        write_reg_word(0x1fa7a054,0x5);
        //b'TXPLL_VCO\r\n'
        write_reg_word(0x1fa7a074,0x1);
        write_reg_word(0x1fa7a078,0x4040701);
        //b'TXPLL_PCW\r\n'
        write_reg_word(0x1fa7b798,0xa000000);
        write_reg_word(0x1fa7b794,0x1000000);
        //b'TXPLL_KBand\r\n'
        write_reg_word(0x1fa7a058,0x30004e4);
        write_reg_word(0x1fa7a05c,0x101);
        write_reg_word(0x1fa7a054,0x5);
        //b'TXPLL_DIV\r\n'
        write_reg_word(0x1fa7a05c,0x1);
        write_reg_word(0x1fa7a074,0x10001);
        //b'TXPLL_TCL\r\n'
        write_reg_word(0x1fa7a094,0x1000f);
        write_reg_word(0x1fa7a070,0x4000d03);
        write_reg_word(0x1fa7a074,0x10001);
        write_reg_word(0x1fa7a06c,0x1000003);
        //b'TXPLL_EN\r\n'
        write_reg_word(0x1fa7b854,0x1010000);
        //b'TXPLL_Out\r\n'
        write_reg_word(0x1fa7b854,0x1010101);
		udelay(200);
        //b'TX BringUp 1\r\n'
        //b'tx_rate_ctrl 1\r\n'
        write_reg_word(0x1fa7b580,0x1);
        //b'TX_CONFIG\r\n'
        write_reg_word(0x1fa7a0c4,0x1010401);
		udelay(1);
        write_reg_word(0x1fa7b874,0x1010000);
        write_reg_word(0x1fa7b77c,0x1040101);
        write_reg_word(0x1fa7b784,0x101);
        //b'TX_FIR_Load_Para swing 2, len 4, cn1 0, c0b b, c1 1, en 1\r\n'
        //b'TX_FIR\r\n'
        write_reg_word(0x1fa7b778,0x100010b);
        write_reg_word(0x1fa7b780,0x101);
        //b'TX_RSTB\r\n'
        write_reg_word(0x1fa7b260,0x101);
        //b'RX BringUp 1\r\n'
        //b'RX_INIT\r\n'
        //b'rx_rate_ctrl\r\n'
        write_reg_word(0x1fa7b374,0x0);
        //b'RX_Path_Init\r\n'
        write_reg_word(0x1fa7b184,0x40003ff);
        write_reg_word(0x1fa7a148,0x1010101);
        write_reg_word(0x1fa7a144,0x1000000);
        write_reg_word(0x1fa7a11c,0x2000401);
        write_reg_word(0x1fa7b004,0xc100a01);
        write_reg_word(0x1fa7a13c,0x20000);
        write_reg_word(0x1fa7a120,0x3ff08);
        write_reg_word(0x1fa7b320,0x10101);
        write_reg_word(0x1fa7b48c,0x1000202);
        write_reg_word(0x1fa7a0dc,0x0);
        write_reg_word(0x1fa7b80c,0x1000000);
        write_reg_word(0x1fa7b814,0x1010000);
        write_reg_word(0x1fa7a10c,0x70604);
        //b'RX_FE,force 0, Gain 1, Peaking 1\r\n'
        write_reg_word(0x1fa7b88c,0x0);
        write_reg_word(0x1fa7b768,0x0);
        //b'RX_FE_VOS\r\n'
        write_reg_word(0x1fa7b79c,0x10100);
        //b'RX_IMP 1\r\n'
        write_reg_word(0x1fa7a114,0x1040000);
        //b'RX_FLL_PR_FMeter\r\n'
        write_reg_word(0x1fa7b390,0x100001);
        write_reg_word(0x1fa7b394,0xffff0000);
        write_reg_word(0x1fa7b39c,0x3107);
        //b'RX_REV\r\n'
        write_reg_word(0x1fa7a0d4,0xc8c31030);
        //b'RX_Rdy_TimeOut\r\n'
        write_reg_word(0x1fa7b100,0xa0005);
        //b'RX_CalBoundry_Init\r\n'
        write_reg_word(0x1fa7b08c,0x101);
        write_reg_word(0x1fa7b104,0x2);
        write_reg_word(0x1fa7b090,0x320002);
        write_reg_word(0x1fa7b09c,0x320002);
        write_reg_word(0x1fa7b094,0x320002);
        write_reg_word(0x1fa7b098,0x320002);
        //b'RX_BySerdes\r\n'
        //b'RX_OSR\r\n'
        write_reg_word(0x1fa7b76c,0x1010000);
        write_reg_word(0x1fa7a0dc,0x100);
        //b'CDR_LPF_RATIO\r\n'
        write_reg_word(0x1fa7a0e8,0x2000001);
        //b'CDR_PR\r\n'
        write_reg_word(0x1fa7a0f8,0x4010806);
        write_reg_word(0x1fa7a0fc,0x60606);
        //b'RX_EYE_Mon\r\n'
        write_reg_word(0x1fa7b120,0x103);
        write_reg_word(0x1fa7b088,0x1);
        //b'RX_SYS_En\r\n'
        write_reg_word(0x1fa7b38c,0x1);
        write_reg_word(0x1fa7b000,0x1000000);
		udelay(500);
        //b'RX_FLL_PR_FMeter\r\n'
        write_reg_word(0x1fa7b33c,0x1010100);
        write_reg_word(0x1fa7b330,0x1);
        //b'RX_CMLEQ_EN\r\n'
        write_reg_word(0x1fa7a118,0x1010100);
        //b'RX_CDR_PR\r\n'
        write_reg_word(0x1fa7a10c,0xe0604);
        //b'RX_CDR_xxx_Pwdb\r\n'
        write_reg_word(0x1fa7b824,0x1000100);
        write_reg_word(0x1fa7b81c,0x100);
        write_reg_word(0x1fa7b894,0x100);
        write_reg_word(0x1fa7b84c,0x1000000);
        write_reg_word(0x1fa7b34c,0x0);
        //b'RX_SigDet\r\n'
        write_reg_word(0x1fa7a114,0x1020200);
        write_reg_word(0x1fa7a110,0x3000200);
        //b'RX_SigDet_Pwdb\r\n'
        write_reg_word(0x1fa7b350,0x0);
        //b'PXP_RX_PHYCK\r\n'
        write_reg_word(0x1fa7a0d8,0x1010b);
        write_reg_word(0x1fa7a0cc,0x1000000);
		udelay(100);
        //b'RX_CDR_xxx_Pwdb\r\n'
        write_reg_word(0x1fa7b824,0x1010101);
        write_reg_word(0x1fa7b81c,0x101);
        write_reg_word(0x1fa7b894,0x101);
        write_reg_word(0x1fa7b84c,0x1010000);
        write_reg_word(0x1fa7b34c,0x1010101);
        //b'RX_SigDet_Pwdb\r\n'
        write_reg_word(0x1fa7b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 1\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x100);
		//b'RX_PRCal 1\r\n'
		write_reg_word(0x1fa7b460,0x20);
		write_reg_word(0x1fa7b150,0xa0649f9c);
		write_reg_word(0x1fa7b14c,0x4e204e20);
		write_reg_word(0x1fa7b158,0x3307);
		write_reg_word(0x1fa7b154,0xa0649f9c);
		write_reg_word(0x1fa7a0f4,0x1000000);
		write_reg_word(0x1fa7b820,0x1010100);
		write_reg_word(0x1fa7b794,0x1010000);
		write_reg_word(0x1fa7b824,0x1010101);
		write_reg_word(0x1fa7b824,0x1000101);
		write_reg_word(0x1fa7b824,0x1010101);

		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa7b158,0x3300);
		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		//---------------------------------------------
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa7b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n
		write_reg_word(0x1fa7b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
        //b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
        write_reg_word(0x1fa7b818,0x10100);
        //b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
        write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
        //b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
        write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
        //b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
        write_reg_word(0x1fa7b818,0x1010101);
        //b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
        write_reg_word(0x1fa7b818,0x10101);
        //b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
        write_reg_word(0x1fa7b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init\r\n'
		write_reg_word(0x1fa70a00,0x4c9cc000);
		write_reg_word(0x1fa74018,0x34);
		//b'hsgmii_init\r\n'
		//b'xsgmii 1 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 1, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa70000,0x140);
		//b'hsgmii\r\n'
		//b'HGMII_2.5G\r\n'
		write_reg_word(0x1fa76000,0xc000c11);
		//b'HSGMII_2.5 exit\r\n'
		printf("HSGMII_2.5 exit\n");
		//b'xSGMII_Solution : 1\r\n'

	}else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_SGMII_MODE)) {
		uint FL_Out_target = 0xA3D6 ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
		u32 rg_value = 0;
        
        //FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
      	write_reg_word(0x1fb00094, 0xe0004820);
		//FT 50R restore
		rg_value = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114, rg_value);
        //b''
		//b'echo "1 2 0 0 0">/proc/xsgmii\r\n'
		//b'op = 1,xsgmii 2,mod 0,an 0,rate 0,buf 31,num 5,count a\r\n'
		//b'xsgmii_api: serdes 1,xsgmii 2,mod 0,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa7a000,0x10040001);
		//b'JCPLL BringUp 2\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa7a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa7a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa7a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa7a01c,0x104);
		write_reg_word(0x1fa7a020,0x30000);
		write_reg_word(0x1fa7a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa7a038,0x0);
		write_reg_word(0x1fa7a034,0x0);
		write_reg_word(0x1fa7a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa7a004,0x180000);
		write_reg_word(0x1fa7a008,0x101f0a);
		write_reg_word(0x1fa7a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa7a02c,0x4010100);
		write_reg_word(0x1fa7a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa7b800,0x25800000);
		write_reg_word(0x1fa7b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa7a014,0x10000);
		write_reg_word(0x1fa7a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa7a010,0x1000300);
		write_reg_word(0x1fa7a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa7a048,0x1020ff);
		write_reg_word(0x1fa7a024,0x5010100);
		write_reg_word(0x1fa7a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa7b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa7a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa7a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa7a068,0x0);
		write_reg_word(0x1fa7a06c,0x1000003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa7a080,0x0);
		write_reg_word(0x1fa7a07c,0x0);
		write_reg_word(0x1fa7a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa7a050,0x1f05000f);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa7a074,0x3000001);
		write_reg_word(0x1fa7a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa7b798,0x8000000);
		write_reg_word(0x1fa7b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa7a058,0x30004e4);
		write_reg_word(0x1fa7a05c,0x101);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa7a05c,0x1);
		write_reg_word(0x1fa7a074,0x3000001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa7a094,0x1000f);
		write_reg_word(0x1fa7a070,0x4000b03);
		write_reg_word(0x1fa7a074,0x3000001);
		write_reg_word(0x1fa7a06c,0x1000003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa7b854,0x1010101);
		udelay(200);
		//b'TX BringUp 2\r\n'
		//b'tx_rate_ctrl 2\r\n'
		write_reg_word(0x1fa7b580,0x1);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa7a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa7b874,0x1010000);
		write_reg_word(0x1fa7b77c,0x1020101);
		write_reg_word(0x1fa7b784,0x101);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 0, c0b c, c1 0, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa7b778,0x100010c);
		write_reg_word(0x1fa7b780,0x100);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa7b260,0x101);
		//b'RX BringUp 2\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		write_reg_word(0x1fa7b374,0x0);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa7b184,0x40003ff);
		write_reg_word(0x1fa7a148,0x1010101);
		write_reg_word(0x1fa7a144,0x1000000);
		write_reg_word(0x1fa7a11c,0x2000401);
		write_reg_word(0x1fa7b004,0xc100a01);
		write_reg_word(0x1fa7a13c,0x20000);
		write_reg_word(0x1fa7a120,0x3ff08);
		write_reg_word(0x1fa7b320,0x10101);
		write_reg_word(0x1fa7b48c,0x1000202);
		write_reg_word(0x1fa7a0dc,0x0);
		write_reg_word(0x1fa7b80c,0x1000000);
		write_reg_word(0x1fa7b814,0x1010000);
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 1\r\n'
		write_reg_word(0x1fa7b88c,0x0);
		write_reg_word(0x1fa7b768,0x0);
		//b'RX_FE_VOS\r\n'
		write_reg_word(0x1fa7b79c,0x10100);
		//b'RX_IMP 1\r\n'
		write_reg_word(0x1fa7a114,0x1040000);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b390,0x100001);
		write_reg_word(0x1fa7b394,0xffff0000);
		write_reg_word(0x1fa7b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa7a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa7b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa7b08c,0x101);
		write_reg_word(0x1fa7b104,0x2);
		write_reg_word(0x1fa7b090,0x320002);
		write_reg_word(0x1fa7b09c,0x320002);
		write_reg_word(0x1fa7b094,0x320002);
		write_reg_word(0x1fa7b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa7b76c,0x1030000);
		write_reg_word(0x1fa7a0dc,0x100);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa7a0e8,0x2000003);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa7a0f8,0x4010808);
		write_reg_word(0x1fa7a0fc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa7b120,0x103);
		write_reg_word(0x1fa7b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa7b38c,0x1);
		write_reg_word(0x1fa7b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b33c,0x1010100);
		write_reg_word(0x1fa7b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa7a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1000100);
		write_reg_word(0x1fa7b81c,0x100);
		write_reg_word(0x1fa7b894,0x100);
		write_reg_word(0x1fa7b84c,0x1000000);
		write_reg_word(0x1fa7b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa7a114,0x1020200);
		write_reg_word(0x1fa7a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa7a0d8,0x10129);
		write_reg_word(0x1fa7a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1010101);
		write_reg_word(0x1fa7b81c,0x101);
		write_reg_word(0x1fa7b894,0x101);
		write_reg_word(0x1fa7b84c,0x1010000);
		write_reg_word(0x1fa7b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 2\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x100);
		//b'RX_PRCal 2\r\n'
		write_reg_word(0x1fa7b460,0x20);
		write_reg_word(0x1fa7b150,0xa43aa372);
		write_reg_word(0x1fa7b14c,0x7fff7fff);
		write_reg_word(0x1fa7b158,0x3307);
		write_reg_word(0x1fa7b154,0xa43aa372);
		write_reg_word(0x1fa7a0f4,0x1000000);
		write_reg_word(0x1fa7b820,0x1010100);
		write_reg_word(0x1fa7b794,0x1010000);
		write_reg_word(0x1fa7b824,0x1010101);
		write_reg_word(0x1fa7b824,0x1000101);
		write_reg_word(0x1fa7b824,0x1010101);

		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa7b158,0x3300);
		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		//---------------------------------------------
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa7b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa7b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
        //b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
        write_reg_word(0x1fa7b818,0x10100);
        //b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
        write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
        //b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
        write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
        //b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
        write_reg_word(0x1fa7b818,0x1010101);
        //b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
        write_reg_word(0x1fa7b818,0x10101);
        //b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
        write_reg_word(0x1fa7b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'Solution0\r\n'
		write_reg_word(0x1fa70034,0x31120029);
		write_reg_word(0x1fa70a24,0x1);
		//b'xsgmii_init\r\n'
		write_reg_word(0x1fa70a00,0x4c9cc000);
		write_reg_word(0x1fa74018,0x24);
		//b'sgmii_init\r\n'
		write_reg_word(0x1fa76018,0x7070707);
		write_reg_word(0x1fa76020,0xff);
		//b'SGMII_Interrupt_init\r\n'
		write_reg_word(0x1fa74014,0x1);
		//b'xsgmii 2,int enable 1\r\n'
		//b'\r\n'
		//b' request_irq() (irq number: 138) OK \r\n'
		write_reg_word(0x1fa70a20,0x1);
		write_reg_word(0x1fa7414c,0x1);
		//b'rg_INTERRUPT_EN_0 1\r\n'
		//b'xsgmii: sync_int exit 1\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 2, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa70000,0x140);
		//b'sgmii_rate_api rate 0\r\n'
		//b'SGMII_1G\r\n'
		write_reg_word(0x1fa76000,0xc000c11);
		write_reg_word(0x1fa74018,0x24);
		//b'SGMII_1G exit\r\n'
		printf("SGMII_1G exit\n");
		//b'xSGMII_Solution : 1\r\n'

	}else if((serdes_intf[0][len_eth-2] == SERDES_ETHERLAN) && (serdes_intf[0][len_eth-1] == SERDES_ETH_USXGMII_MODE)){
		uint FL_Out_target = 0x9EDF ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
        u32 rg_value = 0;

		if((len_eth>=3) &&(serdes_intf[0][len_eth-3] == '2'))   //phy B
		{
			miiStationWrite45(0,4,0xc441,8);
		}
		//FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
      	write_reg_word(0x1fb00094, 0xe0002820);//add
		//FT 50R restore
		rg_value =	RG_R_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa7b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value =	RG_R_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa7a000, PXP_BASE_OFFSET, (u32)0x114, rg_value);
    	//b''
		//b'echo "1 0 1 0 0">/proc/xsgmii\r\n'
		//b'op = 1,xsgmii 0,mod 1,an 0,rate 0,buf 31,num 5,count a\r\n'
		//b'xsgmii_api: serdes 1,xsgmii 0,mod 1,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa7a000,0x10040001);
		//b'JCPLL BringUp 0\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa7a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa7a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa7a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa7a01c,0x104);
		write_reg_word(0x1fa7a020,0x30000);
		write_reg_word(0x1fa7a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa7a038,0x0);
		write_reg_word(0x1fa7a034,0x0);
		write_reg_word(0x1fa7a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa7a004,0x180000);
		write_reg_word(0x1fa7a008,0x101f0a);
		write_reg_word(0x1fa7a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa7a02c,0x4010100);
		write_reg_word(0x1fa7a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa7b800,0x25800000);
		write_reg_word(0x1fa7b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa7a014,0x10000);
		write_reg_word(0x1fa7a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa7a010,0x1000300);
		write_reg_word(0x1fa7a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa7a048,0xf20ff);
		write_reg_word(0x1fa7a024,0x5010100);
		write_reg_word(0x1fa7a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa7b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa7b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa7a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa7a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa7a068,0x0);
		write_reg_word(0x1fa7a06c,0x1010003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa7a080,0x0);
		write_reg_word(0x1fa7a07c,0x0);
		write_reg_word(0x1fa7a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa7a050,0x1f05000f);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa7a074,0x1000001);
		write_reg_word(0x1fa7a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa7b798,0x8400000);
		write_reg_word(0x1fa7b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa7a058,0x30004e4);
		write_reg_word(0x1fa7a05c,0x101);
		write_reg_word(0x1fa7a054,0x180b02);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa7a05c,0x1);
		write_reg_word(0x1fa7a074,0x1000001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa7a094,0x1000f);
		write_reg_word(0x1fa7a070,0x4000b03);
		write_reg_word(0x1fa7a074,0x1000001);
		write_reg_word(0x1fa7a06c,0x1010003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa7b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa7b854,0x1010101);
		udelay(200);
		//b'TX BringUp 0\r\n'
		//b'tx_rate_ctrl 0\r\n'
		write_reg_word(0x1fa7b580,0x2);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa7a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa7b874,0x1010000);
		write_reg_word(0x1fa7b77c,0x1050101);
		write_reg_word(0x1fa7b784,0x102);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa7b778,0x1010101);
		write_reg_word(0x1fa7b780,0x10b);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa7b260,0x101);
		//b'RX BringUp 0\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		write_reg_word(0x1fa7b374,0x2);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa7b184,0x40003ff);
		write_reg_word(0x1fa7a148,0x1010101);
		write_reg_word(0x1fa7a144,0x1000000);
		write_reg_word(0x1fa7a11c,0x2000401);
		write_reg_word(0x1fa7b004,0xc100a01);
		write_reg_word(0x1fa7a13c,0x20000);
		write_reg_word(0x1fa7a120,0x3ff08);
		write_reg_word(0x1fa7b320,0x10101);
		write_reg_word(0x1fa7b48c,0x1000202);
		write_reg_word(0x1fa7a0dc,0x0);
		write_reg_word(0x1fa7b80c,0x1000000);
		write_reg_word(0x1fa7b814,0x1010000);
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 2\r\n'
		write_reg_word(0x1fa7b88c,0x0);
		write_reg_word(0x1fa7b768,0x0);
		//b'RX_FE_VOS\r\n'
		//write_reg_word(0x1fa7b79c,0x10000);
		//b'RX_IMP 1\r\n'
		write_reg_word(0x1fa7a114,0x1040000);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b390,0x100001);
		write_reg_word(0x1fa7b394,0xffff0000);
		write_reg_word(0x1fa7b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa7a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa7b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa7b08c,0x101);
		write_reg_word(0x1fa7b104,0x2);
		write_reg_word(0x1fa7b090,0x320002);
		write_reg_word(0x1fa7b09c,0x320002);
		write_reg_word(0x1fa7b094,0x320002);
		write_reg_word(0x1fa7b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa7b76c,0x1000000);
		write_reg_word(0x1fa7a0dc,0x0);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa7a0e8,0x2000000);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa7a0f8,0x4010808);
		write_reg_word(0x1fa7a0fc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa7b120,0x103);
		write_reg_word(0x1fa7b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa7b38c,0x1);
		write_reg_word(0x1fa7b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa7b33c,0x1010100);
		write_reg_word(0x1fa7b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa7a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa7a10c,0x70604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1000100);
		write_reg_word(0x1fa7b81c,0x100);
		write_reg_word(0x1fa7b894,0x100);
		write_reg_word(0x1fa7b84c,0x1000000);
		write_reg_word(0x1fa7b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa7a114,0x1020200);
		write_reg_word(0x1fa7a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa7a0d8,0x10242);
		write_reg_word(0x1fa7a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa7b824,0x1010101);
		write_reg_word(0x1fa7b81c,0x101);
		write_reg_word(0x1fa7b894,0x101);
		write_reg_word(0x1fa7b84c,0x1010000);
		write_reg_word(0x1fa7b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa7b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x100);
		//b'RX_PRCal 1\r\n'
        write_reg_word(0x1fa7b460,0x20);
        write_reg_word(0x1fa7b150,0x9f439e7b);
        write_reg_word(0x1fa7b14c,0x7fff7fff);
        write_reg_word(0x1fa7b158,0x3307);
        write_reg_word(0x1fa7b154,0x9f439e7b);
        write_reg_word(0x1fa7a0f4,0x1000000);
        write_reg_word(0x1fa7b820,0x1010100);
        write_reg_word(0x1fa7b794,0x1010000);
        write_reg_word(0x1fa7b824,0x1010101);
        write_reg_word(0x1fa7b824,0x1000101);
        write_reg_word(0x1fa7b824,0x1010101);
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa7b158,0x3300);

		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa7b158,0x3300);
		write_reg_word(0x1fa7b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa7a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa7b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa7b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa7b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa7b818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa7b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa7b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa7b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa7b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init 1\r\n'
		write_reg_word(0x1fa74100,0x10010001);
        //b'PCS E0/E1 solution\r\n'
		write_reg_word(0x1fa75b2c,0x0);
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa75bc0,0x0);
		write_reg_word(0x1fa75bc4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		write_reg_word(0x1fa75bd8,0x0);
		write_reg_word(0x1fa75bdc,0x0);
		write_reg_word(0x1fa75be4,0x0);
		write_reg_word(0x1fa75bc8,0x0);
		write_reg_word(0x1fa75bcc,0x0);
		write_reg_word(0x1fa75be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'AN mode\r\n'
		printf("AN mode\n");
		//b'xSGMII_AN_API 0, enable : 1\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa75bf8,0x6330001);
		//b'_rg_usxgmii_an_control_0 6330001\r\n'
		//b'_rg_usxgmii_an_control_1 0\r\n'
		//b'xSGMII_AN_AutoSetting\r\n'
		write_reg_word(0x1fa76000,0xc11);
        //set mac reg init
      	write_reg_word(0x1fa09000,0x71082800);
      	printf("set mac reg init\n");
#if 1 //Interrupt for USXGMII		
		//enable USXGMII interrupt(pcs)
		usxgmii_pcs_int_init(1);
		//ARM interrupt settings
		gic_dic_set_icenable(50);
		gic_dic_set_enable(50);
		irq_register(50,usxgmii_isr,1);
#endif	
		printf("USXGMII_10G exit\n");
		return ;
	}else if((serdes_intf[3][1] == SERDES_ETHERLAN) && (serdes_intf[3][2] == SERDES_WIFI2_USXGMII_MODE)){
		uint FL_Out_target = 0x9EDF ;
		u32 pma_start = L1_PMA_BASE_OFFSET - 0x1000; //offset os 0x1000
        u32 rg_c77c = 0;
        u32 rg_1fc = 0;
        u32 rg_value = 0;
		
        //FT 50R retrive
		//TX 50R
	    rg_c77c = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_c77c &= 0x00000107;
	    rg_1fc = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_1fc &= 0x00030000;	
		write_reg_word(0x1fb0009c, 0x909);//pcie_xsi1_sel 
        //FT 50R restore
        //TX 50R
	    rg_value = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_value &= 0xfffffef8;	    
	    rg_value |= rg_c77c;	   
	    RG_W_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c, rg_value);	   
	    //RX 50R
	    rg_value = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_value &= 0xfffcffff;	  
	    rg_value |= rg_1fc;
	    RG_W_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc, rg_value);	   
        
        //root/echo "4 0 1 0 0">/proc/xsgmii
		//b''
		//b'echo "4 0 1 0 0">/proc/xsgmii\r\n'
		//b'op = 4,xsgmii 0,mod 1,an 0,rate 0,buf 34,num 5,count a\r\n'
		//b'xsgmii_api: serdes 4,xsgmii 0,mod 1,rate 0,an 0\r\n'
		//b'xSGMII_Solution : 3\r\n'
		//b'Solution3\r\n'
		//b'Solution3  =c000c00\r\n'
		write_reg_word(0x1fa66000,0x0);
		//b'xsgmii_chg 1\r\n'
		//b'PCIE PLL ON \r\n'
		//b'PLL_BringUp\r\n'
		//b'JCPLL BringUp 0\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa5a000,0x12040001);
		udelay(2000);
		write_reg_word(0x1fa5a04c,0x20ff0000);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa5a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa5a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa5b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa5a01c,0x104);
		write_reg_word(0x1fa5a020,0x30000);
		write_reg_word(0x1fa5a024,0x5010100);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa5a03c,0x0);
		write_reg_word(0x1fa5a038,0x0);
		write_reg_word(0x1fa5a040,0x1010000);
		write_reg_word(0x1fa5a030,0x300);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa5a004,0x180000);
		write_reg_word(0x1fa5a008,0x101f0a);
		write_reg_word(0x1fa5a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa5a02c,0x4010100);
		write_reg_word(0x1fa5a030,0x303);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa5b800,0x25800000);
		write_reg_word(0x1fa5b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa5a014,0x10000);
		write_reg_word(0x1fa5a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa5a010,0x1000300);
		write_reg_word(0x1fa5a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa5a050,0x1000f);
		write_reg_word(0x1fa5a024,0x5010100);
		write_reg_word(0x1fa5a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa5b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa5b828,0x1010101);
		udelay(200);
		//b'TXPLL_BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa5a08c,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa5a068,0x1000001);
		write_reg_word(0x1fa5a06c,0x10400);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa5b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa5a06c,0x10400);
		write_reg_word(0x1fa5a070,0x10000);
		write_reg_word(0x1fa5a074,0x3010100);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa5a088,0x0);
		write_reg_word(0x1fa5a084,0x0);
		write_reg_word(0x1fa5a08c,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa5a054,0xf000001);
		write_reg_word(0x1fa5a058,0x21f0500);
		write_reg_word(0x1fa5a05c,0xff00180b);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa5a07c,0x1010000);
		write_reg_word(0x1fa5a080,0x40407);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa5b798,0x8400000);
		write_reg_word(0x1fa5b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa5a060,0x1030004);
		write_reg_word(0x1fa5a05c,0xe400180b);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa5a064,0x1000000);
		write_reg_word(0x1fa5a07c,0x1010000);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa5a09c,0x1000f);
		write_reg_word(0x1fa5a074,0x3010100);
		write_reg_word(0x1fa5a078,0x104000b);
		write_reg_word(0x1fa5a07c,0x1010000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa5b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa5b854,0x1010101);
		udelay(200);
		//b'Phya_BringUp\r\n'
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa5a0e8,0x1010401);
		udelay(1);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1050000);
		write_reg_word(0x1fa5c414,0x0);
		//b'TX_BringUp\r\n'
		//b'TX_rate_ctrl\r\n'
		write_reg_word(0x1fa5c910,0x2);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa5a0e8,0x1010401);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1050000);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa5c778,0x1010101);
		write_reg_word(0x1fa5c780,0x10b);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa5c260,0x101);
		//b'RX_BringUp M2\r\n'
		//b'RX_INIT M2\r\n'
		//b'RX_rate_ctrl\r\n'
		write_reg_word(0x1fa5c910,0x22);
		write_reg_word(0x1fa5c178,0x20403);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa5c184,0x40003ff);
		write_reg_word(0x1fa5a234,0x1000000);
		write_reg_word(0x1fa5a238,0x1010101);
		write_reg_word(0x1fa5a200,0x1000000);
		write_reg_word(0x1fa5a228,0x2000000);
		write_reg_word(0x1fa5a208,0x3ff0008);
		write_reg_word(0x1fa5c320,0x10101);
		write_reg_word(0x1fa5c004,0xc100a01);
		write_reg_word(0x1fa5c48c,0x1000202);
		write_reg_word(0x1fa5a1bc,0x0);
		write_reg_word(0x1fa5c814,0x1010000);
		write_reg_word(0x1fa5a1f0,0x7);
		write_reg_word(0x1fa5a1ec,0x60100);
		write_reg_word(0x1fa5a1e8,0x10000);
		write_reg_word(0x1fa5c80c,0x1000000);
		//b'RX_FE,force 0, Gain 1, Peaking 2\r\n'
		write_reg_word(0x1fa5c88c,0x0);
		write_reg_word(0x1fa5c768,0x0);
		//b'FLL_FMeter(0)\r\n'
		write_reg_word(0x1fa5c390,0x100001);
		write_reg_word(0x1fa5c394,0xffff0000);
		write_reg_word(0x1fa5c39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa5a1b4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa5c100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa5c08c,0x101);
		write_reg_word(0x1fa5c104,0x2);
		write_reg_word(0x1fa5c090,0x320002);
		write_reg_word(0x1fa5c09c,0x320002);
		write_reg_word(0x1fa5c094,0x320002);
		write_reg_word(0x1fa5c098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa5c76c,0x1000000);
		write_reg_word(0x1fa5a1bc,0x0);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa5a1c8,0x2000000);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa5a1d8,0x4010808);
		write_reg_word(0x1fa5a1dc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa5c120,0x103);
		write_reg_word(0x1fa5c088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa5c38c,0x1);
		write_reg_word(0x1fa5c000,0x1000000);
		udelay(500);
		//b'FLL_FMeter(1)\r\n'
		write_reg_word(0x1fa5c330,0x1);
		write_reg_word(0x1fa5c33c,0x1010100);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa5a200,0x1010101);
		write_reg_word(0x1fa5a1fc,0x0);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa5a1f0,0x7);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa5c824,0x1000100);
		write_reg_word(0x1fa5c81c,0x100);
		write_reg_word(0x1fa5c894,0x100);
		write_reg_word(0x1fa5c84c,0x1000000);
		write_reg_word(0x1fa5c34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa5a1f8,0x20200);
		write_reg_word(0x1fa5a1f4,0x3000002);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa5c350,0x0);
		//b'RX_SigDet_En\r\n'
		write_reg_word(0x1fa5c16c,0x1);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa5a1b8,0x10242);
		write_reg_word(0x1fa5a1ac,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c81c,0x101);
		write_reg_word(0x1fa5c894,0x101);
		write_reg_word(0x1fa5c84c,0x1010000);
		write_reg_word(0x1fa5c34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa5c350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa5c818,0x100);
		//b'RX_PRCal 0\r\n'
		write_reg_word(0x1fa5c460,0x20);
		write_reg_word(0x1fa5c150,0x9f439e7b);
		write_reg_word(0x1fa5c14c,0x7fff7fff);
		write_reg_word(0x1fa5c158,0x3307);
		write_reg_word(0x1fa5c154,0x9f439e7b);
		write_reg_word(0x1fa5a1d4,0x1000000);
		write_reg_word(0x1fa5c820,0x1010100);
		write_reg_word(0x1fa5c794,0x10000);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
		{		
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
	
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);		
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);
			//abs
			/*		
					if (RO_FL_Out > FL_Out_target)
					{ 
					RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
					}
					else if (RO_FL_Out < FL_Out_target)
					{
					RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
					}
					else RO_FL_Out_diff = 0;
			*/			
			RO_FL_Out_diff =abs(FL_Out_target -RO_FL_Out);	
			if(RO_FL_Out > FL_Out_target)
			{        
				RO_FL_Out_diff_tmp = RO_FL_Out_diff;
				cdr_pr_idac_tmp = (PrCal_Serach<<8);
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
			}
		}
	
		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
		{
			pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
			
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
	
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);
	
			if(RO_FL_Out < FL_Out_target)
			{
				pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}
			else
			{
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}   
		
		}
		
		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);
	
	
		write_reg_word(0x1fa5c158,0x3300);
		write_reg_word(0x1fa5c158,0x3303);
	
		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 
	
		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.dat.value = RG_R_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.hal.rg_pxp_cdr1_pr_inj_force_off = 0;
		//RG_W_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE,RG_PXP_CDR1_PR_INJ_MODE.dat.value);
		write_reg_word(0x1fa5a1d4,0x0);
	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
		write_reg_word(0x1fa5c820,0x1010000);
		
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
		//SS_RX_FLL_b.hal.rg_load_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		write_reg_word(0x1fa5c19c,0x1);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
		
	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x10101);
		
		//SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		write_reg_word(0x1fa5c460,0x0);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa5c818,0x10001);
		//b'RSTB sel8, val 0 lane =1\r\n'
		write_reg_word(0x1fa5c460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1 lane =1\r\n'
		write_reg_word(0x1fa5c460,0xfff);
		udelay(100);
		//b'RX_OSCAL_SEQ\r\n'
		//b'RX_L2R\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		//write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
        write_reg_word(0x1fa5c818,0x10100);//226
		//b'RX_pical\r\n'
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c15c,0x400);
		write_reg_word(0x1fa5c118,0xa000a08);
		write_reg_word(0x1fa5c204,0x1000101);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010001);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010000);
		write_reg_word(0x1fa5c31c,0x1010100);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c324,0x10101);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1010001);
		write_reg_word(0x1fa5c30c,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c328,0x100);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c318,0x100);
		write_reg_word(0x1fa5c110,0x100);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c30c,0x1);
		//b'RX_pdos\r\n'
		write_reg_word(0x1fa5c894,0x1010101);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c304,0x1010101);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x10100);
		write_reg_word(0x1fa5c084,0x0);
		write_reg_word(0x1fa5c200,0x20101);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c334,0x1000000);
		write_reg_word(0x1fa5c208,0x10100);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000000);
		write_reg_word(0x1fa5c328,0x10101);
		write_reg_word(0x1fa5c208,0x10101);
		write_reg_word(0x1fa5c110,0x10000);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c084,0x100);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c084,0x101);
		write_reg_word(0x1fa5c894,0x1000101);
		//b'RX_feos\r\n'
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c144,0x30);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1000100);
		write_reg_word(0x1fa5c204,0x1010001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c110,0x1000001);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c360,0x50000);
		//b'RX_sdcal\r\n'
		//write_reg_word(0x1fa5c898,0x100);
        write_reg_word(0x1fa5c898,0x101);//226
		write_reg_word(0x1fa5c840,0x101);
		write_reg_word(0x1fa5c204,0x10101);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c10c,0x1010000);
		write_reg_word(0x1fa5c338,0x1000000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c32c,0x10100);
		write_reg_word(0x1fa5c114,0x1);
		udelay(2000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c898,0x100);
		write_reg_word(0x1fa5c83c,0x1000000);
		write_reg_word(0x1fa5c840,0x1000100);
		//b'RX_OS_rdy\r\n'
		write_reg_word(0x1fa5c114,0x100);
		write_reg_word(0x1fa5c114,0x0);
		//b'DIG_reset\r\n'
		write_reg_word(0x1fa5c460,0x0);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_L2D\r\n'
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa5c818,0x10001);
		udelay(1000);
		//b'RX_rxrdy\r\n'
		write_reg_word(0x1fa5c114,0x1000000);
		write_reg_word(0x1fa5c10c,0x10000);
		write_reg_word(0x1fa5c460,0xffe);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa5c818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa5c818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init : 0, Lane 1\r\n'
		//b'usxgmii_init 1\r\n'
		write_reg_word(0x1fa64100,0x10010001);
		//b'PCS E0/E1/E3 SWR solution\r\n'
		//b'HIBER_OFF\r\n'
		write_reg_word(0x1fa65900,0x40);
		write_reg_word(0x1fa65b2c,0x0);
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa65bc0,0x0);
		write_reg_word(0x1fa65bc4,0x0);
		write_reg_word(0x1fa65bc8,0x0);
		write_reg_word(0x1fa65bcc,0x0);
		write_reg_word(0x1fa65be0,0x0);
		write_reg_word(0x1fa65bd8,0x0);
		write_reg_word(0x1fa65bdc,0x0);
		write_reg_word(0x1fa65be4,0x0);
		write_reg_word(0x1fa65bc8,0x0);
		write_reg_word(0x1fa65bcc,0x0);
		write_reg_word(0x1fa65be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'AN mode\r\n'
		printf("AN mode\n");
		//b'xSGMII_AN_API 0, enable : 1\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa65bf8,0x6330001);
		//b'_rg_usxgmii_an_control_0 1,6330001\r\n'
		//b'_rg_usxgmii_an_control_1 1,1601\r\n'
		//b'xSGMII_AN_AutoSetting\r\n'
		write_reg_word(0x1fa66000,0x11);
		printf("PCIE1 USXGMII_10G exit\n");
	}else if((serdes_intf[3][1] == SERDES_ETHERLAN) && (serdes_intf[3][2] == SERDES_WIFI2_HSGMII_MODE)){
		uint FL_Out_target = 0xA000 ;
		u32 pma_start = L1_PMA_BASE_OFFSET - 0x1000; //offset os 0x1000
        u32 rg_c77c = 0;
        u32 rg_1fc = 0;		
        u32 rg_value = 0;		
		
        //FT 50R retrive
		//TX 50R
	    rg_c77c = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_c77c &= 0x00000107;	    
	    rg_1fc = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_1fc &= 0x00030000;	
		write_reg_word(0x1fb0009c, 0x1109);//pcie_xsi1_sel
		//FT 50R restore
		//TX 50R
	    rg_value = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_value &= 0xfffffef8;	    
	    rg_value |= rg_c77c;	   
	    RG_W_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c, rg_value);	   
	    //RX 50R
	    rg_value = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_value &= 0xfffcffff;	  
	    rg_value |= rg_1fc;
	    RG_W_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc, rg_value);	   
        //root/echo "4 1 0 0 0">/proc/xsgmii
		//b''
		//b'echo "4 1 0 0 0">/proc/xsgmii
		//b'op = 4,xsgmii 1,mod 0,an 0,rate 0,buf 34,num 5,count a
		//b'xsgmii_api: serdes 4,xsgmii 1,mod 0,rate 0,an 0
		//b'xSGMII_Solution : 3
		//b'Solution3
		//b'Solution3  =c000c00
		write_reg_word(0x1fa66000,0x0);
		//b'xsgmii_chg 1
		//b'PCIE PLL ON 
		//b'PLL_BringUp
		//b'JCPLL BringUp 1
		//b'JCPLL_LDO
		write_reg_word(0x1fa5a000,0x12040001);
		udelay(2000);
		write_reg_word(0x1fa5a04c,0x20ff0000);
		//b'JCPLL_RSTB
		write_reg_word(0x1fa5a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa5a01c,0x3000104);
		//b'JCPLL_EN
		write_reg_word(0x1fa5b828,0x1000000);
		//b'JCPLL_SDM
		write_reg_word(0x1fa5a01c,0x104);
		write_reg_word(0x1fa5a020,0x30000);
		write_reg_word(0x1fa5a024,0x5010100);
		//b'JCPLL_SDM
		write_reg_word(0x1fa5a03c,0x0);
		write_reg_word(0x1fa5a038,0x0);
		write_reg_word(0x1fa5a040,0x1010000);
		write_reg_word(0x1fa5a030,0x300);
		//b'JCPLL_LPF
		write_reg_word(0x1fa5a004,0x180000);
		write_reg_word(0x1fa5a008,0x101f0a);
		write_reg_word(0x1fa5a00c,0x2ff0000);
		//b'JCPLL_VCO
		write_reg_word(0x1fa5a02c,0x4010100);
		write_reg_word(0x1fa5a030,0x303);
		//b'JCPLL_PCW
		write_reg_word(0x1fa5b800,0x25800000);
		write_reg_word(0x1fa5b79c,0x10000);
		//b'JCPLL_DIV
		write_reg_word(0x1fa5a014,0x10000);
		write_reg_word(0x1fa5a02c,0x4010100);
		//b'JCPLL_KBand
		write_reg_word(0x1fa5a010,0x1000300);
		write_reg_word(0x1fa5a00c,0x2e40000);
		//b'JCPLL_TCL
		write_reg_word(0x1fa5a050,0x10010);
		write_reg_word(0x1fa5a024,0x5010100);
		write_reg_word(0x1fa5a028,0x10400);
		//b'JCPLL_EN
		write_reg_word(0x1fa5b828,0x1010000);
		//b'JCPLL_Out
		write_reg_word(0x1fa5b828,0x1010101);
		udelay(200);
		//b'TXPLL_BringUp
		//b'TXPLL_VCOLDO_Out
		write_reg_word(0x1fa5a08c,0x101031b);
		//b'TXPLL_RSTB
		write_reg_word(0x1fa5a068,0x1000001);
		write_reg_word(0x1fa5a06c,0x10400);
		udelay(2000);
		//b'TXPLL_EN
		write_reg_word(0x1fa5b854,0x1000000);
		//b'TXPLL_SDM
		write_reg_word(0x1fa5a06c,0x10400);
		write_reg_word(0x1fa5a070,0x10000);
		write_reg_word(0x1fa5a074,0x3010000);
		//b'TXPLL_SSC
		write_reg_word(0x1fa5a088,0x0);
		write_reg_word(0x1fa5a084,0x0);
		write_reg_word(0x1fa5a08c,0x1010000);
		//b'TXPLL_LPF
		write_reg_word(0x1fa5a054,0xa000001);
		write_reg_word(0x1fa5a058,0x51f0500);
		write_reg_word(0x1fa5a05c,0xff000000);
		//b'TXPLL_VCO
		write_reg_word(0x1fa5a07c,0x1000000);
		write_reg_word(0x1fa5a080,0x40407);
		//b'TXPLL_PCW
		write_reg_word(0x1fa5b798,0xa000000);
		write_reg_word(0x1fa5b794,0x1000000);
		//b'TXPLL_KBand
		write_reg_word(0x1fa5a060,0x1030004);
		write_reg_word(0x1fa5a05c,0xe4000000);
		//b'TXPLL_DIV
		write_reg_word(0x1fa5a064,0x1000000);
		write_reg_word(0x1fa5a07c,0x1000100);
		//b'TXPLL_TCL
		write_reg_word(0x1fa5a09c,0x1000f);
		write_reg_word(0x1fa5a074,0x3010000);
		write_reg_word(0x1fa5a078,0x104000d);
		write_reg_word(0x1fa5a07c,0x1000100);
		//b'TXPLL_EN
		write_reg_word(0x1fa5b854,0x1010000);
		//b'TXPLL_Out
		write_reg_word(0x1fa5b854,0x1010101);
		udelay(200);
		//b'Phya_BringUp
		//b'TX_CONFIG
		write_reg_word(0x1fa5a0e8,0x1010401);
		udelay(1);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1040000);
		write_reg_word(0x1fa5c414,0x820);
		//b'TX_BringUp
		//b'TX_rate_ctrl
		write_reg_word(0x1fa5c910,0x1);
		//b'TX_CONFIG
		write_reg_word(0x1fa5a0e8,0x1010401);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1040000);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 0, c0b b, c1 1, en 1
		//b'TX_FIR
		write_reg_word(0x1fa5c778,0x100010b);
		write_reg_word(0x1fa5c780,0x101);
		//b'TX_RSTB
		write_reg_word(0x1fa5c260,0x101);
		//b'RX_BringUp M2
		//b'RX_INIT M2
		//b'RX_rate_ctrl
		//b'RX_rate_ctrl FLL
		write_reg_word(0x1fa5c910,0x1);
		write_reg_word(0x1fa5c178,0x10403);
		//b'RX_Path_Init
		write_reg_word(0x1fa5c184,0x40003ff);
		write_reg_word(0x1fa5a234,0x1000000);
		write_reg_word(0x1fa5a238,0x1010101);
		write_reg_word(0x1fa5a200,0x1000000);
		write_reg_word(0x1fa5a228,0x2000000);
		write_reg_word(0x1fa5a208,0x3ff0008);
		write_reg_word(0x1fa5c320,0x10101);
		write_reg_word(0x1fa5c004,0xc100a01);
		write_reg_word(0x1fa5c48c,0x1000202);
		write_reg_word(0x1fa5a1bc,0x0);
		write_reg_word(0x1fa5c814,0x1010000);
		write_reg_word(0x1fa5a1f0,0x7);
		write_reg_word(0x1fa5a1ec,0x60100);
		write_reg_word(0x1fa5a1e8,0x10000);
		write_reg_word(0x1fa5c80c,0x1000000);
		//b'RX_FE,force 0, Gain 1, Peaking 1
		write_reg_word(0x1fa5c88c,0x0);
		write_reg_word(0x1fa5c768,0x0);
		write_reg_word(0x1fa5c79c,0x100);
		//b'FLL_FMeter(0)
		write_reg_word(0x1fa5c390,0x100001);
		write_reg_word(0x1fa5c394,0xffff0000);
		write_reg_word(0x1fa5c39c,0x3107);
		//b'RX_REV
		write_reg_word(0x1fa5a1b4,0xc8c31030);
		//b'RX_Rdy_TimeOut
		write_reg_word(0x1fa5c100,0xa0005);
		//b'RX_CalBoundry_Init
		write_reg_word(0x1fa5c08c,0x101);
		write_reg_word(0x1fa5c104,0x2);
		write_reg_word(0x1fa5c090,0x320002);
		write_reg_word(0x1fa5c09c,0x320002);
		write_reg_word(0x1fa5c094,0x320002);
		write_reg_word(0x1fa5c098,0x320002);
		//b'RX_BySerdes
		//b'RX_OSR
		write_reg_word(0x1fa5c76c,0x1010000);
		write_reg_word(0x1fa5a1bc,0x100);
		//b'CDR_LPF_RATIO
		write_reg_word(0x1fa5a1c8,0x2000001);
		//b'CDR_PR
		write_reg_word(0x1fa5a1d8,0x4010806);
		write_reg_word(0x1fa5a1dc,0x60606);
		//b'RX_EYE_Mon
		write_reg_word(0x1fa5c120,0x103);
		write_reg_word(0x1fa5c088,0x1);
		//b'RX_SYS_En
		write_reg_word(0x1fa5c38c,0x1);
		write_reg_word(0x1fa5c000,0x1000000);
		udelay(500);
		//b'FLL_FMeter(1)
		write_reg_word(0x1fa5c330,0x1);
		write_reg_word(0x1fa5c33c,0x1010100);
		//b'RX_CMLEQ_EN
		write_reg_word(0x1fa5a200,0x1010101);
		write_reg_word(0x1fa5a1fc,0x0);
		//b'RX_CDR_PR
		write_reg_word(0x1fa5a1f0,0x106);
		//b'RX_CDR_xxx_Pwdb
		write_reg_word(0x1fa5c824,0x1000100);
		write_reg_word(0x1fa5c81c,0x100);
		write_reg_word(0x1fa5c894,0x100);
		write_reg_word(0x1fa5c84c,0x1000000);
		write_reg_word(0x1fa5c34c,0x0);
		//b'RX_SigDet
		write_reg_word(0x1fa5a1f8,0x20200);
		write_reg_word(0x1fa5a1f4,0x3000002);
		//b'RX_SigDet_Pwdb
		write_reg_word(0x1fa5c350,0x0);
		//b'RX_SigDet_En
		write_reg_word(0x1fa5c16c,0x1);
		//b'PXP_RX_PHYCK
		write_reg_word(0x1fa5a1b8,0x1010b);
		write_reg_word(0x1fa5a1ac,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c81c,0x101);
		write_reg_word(0x1fa5c894,0x101);
		write_reg_word(0x1fa5c84c,0x1010000);
		write_reg_word(0x1fa5c34c,0x1010101);
		//b'RX_SigDet_Pwdb
		write_reg_word(0x1fa5c350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 1
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x100);
		//b'RX_PRCal 1
		write_reg_word(0x1fa5c460,0x20);
		write_reg_word(0x1fa5c150,0xa0649f9c);
		write_reg_word(0x1fa5c14c,0x4e204e20);
		write_reg_word(0x1fa5c158,0x3307);
		write_reg_word(0x1fa5c154,0xa0649f9c);
		write_reg_word(0x1fa5a1d4,0x1000000);
		write_reg_word(0x1fa5c820,0x1010100);
		write_reg_word(0x1fa5c794,0x10000);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
		{		
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
	
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);		
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);
			//abs
			/*		
					if (RO_FL_Out > FL_Out_target)
					{ 
					RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
					}
					else if (RO_FL_Out < FL_Out_target)
					{
					RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
					}
					else RO_FL_Out_diff = 0;
			*/			
			RO_FL_Out_diff =abs(FL_Out_target -RO_FL_Out);	
			if(RO_FL_Out > FL_Out_target)
			{        
				RO_FL_Out_diff_tmp = RO_FL_Out_diff;
				cdr_pr_idac_tmp = (PrCal_Serach<<8);
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
			}
		}
	
		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
		{
			pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
			
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
	
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);
	
			if(RO_FL_Out < FL_Out_target)
			{
				pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}
			else
			{
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}   
		
		}
		
		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);
	
	
		write_reg_word(0x1fa5c158,0x3300);
		write_reg_word(0x1fa5c158,0x3303);
	
		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 
	
		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.dat.value = RG_R_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.hal.rg_pxp_cdr1_pr_inj_force_off = 0;
		//RG_W_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE,RG_PXP_CDR1_PR_INJ_MODE.dat.value);
		write_reg_word(0x1fa5a1d4,0x0);
	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
		write_reg_word(0x1fa5c820,0x1010000);
		
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
		//SS_RX_FLL_b.hal.rg_load_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		write_reg_word(0x1fa5c19c,0x1);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
		
	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x10101);
		
		//SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		write_reg_word(0x1fa5c460,0x0);
		//b'RO_state_freqdet = 0x0
		//RX_PRCal_end--------------------------------
	
		//b'RX_CDR_LPF_RSTB mode 1, sel0
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		//b'RSTB sel8, val 0 lane =1
		write_reg_word(0x1fa5c460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1 lane =1
		write_reg_word(0x1fa5c460,0xfff);
		udelay(100);
		//b'RX_OSCAL_SEQ
		//b'RX_L2R
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x10100);
		//b'RX_pical
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c15c,0x400);
		write_reg_word(0x1fa5c118,0xa000a08);
		write_reg_word(0x1fa5c204,0x1000101);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010001);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010000);
		write_reg_word(0x1fa5c31c,0x1010100);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c324,0x10101);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1010001);
		write_reg_word(0x1fa5c30c,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c328,0x100);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c318,0x100);
		write_reg_word(0x1fa5c110,0x100);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c30c,0x1);
		//b'RX_pdos
		write_reg_word(0x1fa5c894,0x1010101);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c304,0x1010101);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x10100);
		write_reg_word(0x1fa5c084,0x0);
		write_reg_word(0x1fa5c200,0x20101);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c334,0x1000000);
		write_reg_word(0x1fa5c208,0x10100);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000000);
		write_reg_word(0x1fa5c328,0x10101);
		write_reg_word(0x1fa5c208,0x10101);
		write_reg_word(0x1fa5c110,0x10000);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c084,0x100);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c084,0x101);
		write_reg_word(0x1fa5c894,0x1000101);
		//b'RX_feos
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c144,0x30);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1000100);
		write_reg_word(0x1fa5c204,0x1010001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c110,0x1000001);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c360,0x50000);
		//b'RX_sdcal
		write_reg_word(0x1fa5c898,0x101);//226
		write_reg_word(0x1fa5c840,0x101);
		write_reg_word(0x1fa5c204,0x10101);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c10c,0x1010000);
		write_reg_word(0x1fa5c338,0x1000000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c32c,0x10100);
		write_reg_word(0x1fa5c114,0x1);
		udelay(2000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c898,0x100);
		write_reg_word(0x1fa5c83c,0x1000000);
		write_reg_word(0x1fa5c840,0x1000100);
		//b'RX_OS_rdy
		write_reg_word(0x1fa5c114,0x100);
		write_reg_word(0x1fa5c114,0x0);
		//b'DIG_reset
		write_reg_word(0x1fa5c460,0x0);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_L2D
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		udelay(1000);
		//b'RX_rxrdy
		write_reg_word(0x1fa5c114,0x1000000);
		write_reg_word(0x1fa5c10c,0x10000);
		write_reg_word(0x1fa5c460,0xffe);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_CDR_RST
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		//b'xSGMII_Solution : 0
		//b'xsgmii_init : 1, Lane 1
		//b'usxgmii_init
		write_reg_word(0x1fa60a00,0x4c9cc000);
		write_reg_word(0x1fa64018,0x34);
		//b'hsgmii_init
		//b'xsgmii 1 int enable 0
		//b'force mode
		//b'xSGMII_AN_API 1, enable : 0
		//b'sgmii_an 0
		write_reg_word(0x1fa60000,0x140);
		//b'hsgmii
		//b'HGMII_2.5G
		write_reg_word(0x1fa66000,0xc000011);
		printf("PCIE1 HSGMII_2.5 exit\n");
		//b'xSGMII_Solution : 1
	}else if((serdes_intf[3][1] == SERDES_ETHERLAN) && (serdes_intf[3][2] == SERDES_WIFI2_XFI_MODE)){
		uint FL_Out_target = 0x9EDF ;
		u32 pma_start = L1_PMA_BASE_OFFSET - 0x1000; //offset os 0x1000
        u32 rg_c77c = 0;
        u32 rg_1fc = 0;
        u32 rg_value = 0;
		
        //FT 50R retrive
        //TX 50R
	    rg_c77c = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_c77c &= 0x00000107;
	    rg_1fc = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_1fc &= 0x00030000;	
		write_reg_word(0x1fb0009c, 0x909);//pcie_xsi1_sel
	    //FT 50R restore
         //TX 50R
	    rg_value = RG_R_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_c77c;	   
	    RG_W_PL(0x1fa5c000, L1_PMA_BASE_OFFSET, (u32)0xc77c, rg_value);
	   
	    //RX 50R
	    rg_value = RG_R_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc);
	    rg_value &= 0xfffcffff;	  
	    rg_value |= rg_1fc;
	    RG_W_PL(0x1fa5a000, PXP_BASE_OFFSET, (u32)0x1fc, rg_value);
        
        //root/echo "4 0 0 0 0">/proc/xsgmii
		//b''
		//b'echo "4 0 0 0 0">/proc/xsgmii
		//b'op = 4,xsgmii 0,mod 0,an 0,rate 0,buf 34,num 5,count a
		//b'xsgmii_api: serdes 4,xsgmii 0,mod 0,rate 0,an 0
		//b'xSGMII_Solution : 3
		//b'Solution3
		//b'Solution3  =c000c00
		write_reg_word(0x1fa66000,0x0);
		//b'xsgmii_chg 1
		//b'PCIE PLL ON 
		//b'PLL_BringUp
		//b'JCPLL BringUp 0
		//b'JCPLL_LDO
		write_reg_word(0x1fa5a000,0x12040001);
		udelay(2000);
		write_reg_word(0x1fa5a04c,0x20ff0000);
		//b'JCPLL_RSTB
		write_reg_word(0x1fa5a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa5a01c,0x3000104);
		//b'JCPLL_EN
		write_reg_word(0x1fa5b828,0x1000000);
		//b'JCPLL_SDM
		write_reg_word(0x1fa5a01c,0x104);
		write_reg_word(0x1fa5a020,0x30000);
		write_reg_word(0x1fa5a024,0x5010100);
		//b'JCPLL_SDM
		write_reg_word(0x1fa5a03c,0x0);
		write_reg_word(0x1fa5a038,0x0);
		write_reg_word(0x1fa5a040,0x1010000);
		write_reg_word(0x1fa5a030,0x300);
		//b'JCPLL_LPF
		write_reg_word(0x1fa5a004,0x180000);
		write_reg_word(0x1fa5a008,0x101f0a);
		write_reg_word(0x1fa5a00c,0x2ff0000);
		//b'JCPLL_VCO
		write_reg_word(0x1fa5a02c,0x4010100);
		write_reg_word(0x1fa5a030,0x303);
		//b'JCPLL_PCW
		write_reg_word(0x1fa5b800,0x25800000);
		write_reg_word(0x1fa5b79c,0x10000);
		//b'JCPLL_DIV
		write_reg_word(0x1fa5a014,0x10000);
		write_reg_word(0x1fa5a02c,0x4010100);
		//b'JCPLL_KBand
		write_reg_word(0x1fa5a010,0x1000300);
		write_reg_word(0x1fa5a00c,0x2e40000);
		//b'JCPLL_TCL
		write_reg_word(0x1fa5a050,0x1000f);
		write_reg_word(0x1fa5a024,0x5010100);
		write_reg_word(0x1fa5a028,0x10400);
		//b'JCPLL_EN
		write_reg_word(0x1fa5b828,0x1010000);
		//b'JCPLL_Out
		write_reg_word(0x1fa5b828,0x1010101);
		udelay(200);
		//b'TXPLL_BringUp
		//b'TXPLL_VCOLDO_Out
		write_reg_word(0x1fa5a08c,0x101031b);
		//b'TXPLL_RSTB
		write_reg_word(0x1fa5a068,0x1000001);
		write_reg_word(0x1fa5a06c,0x10400);
		udelay(2000);
		//b'TXPLL_EN
		write_reg_word(0x1fa5b854,0x1000000);
		//b'TXPLL_SDM
		write_reg_word(0x1fa5a06c,0x10400);
		write_reg_word(0x1fa5a070,0x10000);
		write_reg_word(0x1fa5a074,0x3010100);
		//b'TXPLL_SSC
		write_reg_word(0x1fa5a088,0x0);
		write_reg_word(0x1fa5a084,0x0);
		write_reg_word(0x1fa5a08c,0x1010000);
		//b'TXPLL_LPF
		write_reg_word(0x1fa5a054,0xf000001);
		write_reg_word(0x1fa5a058,0x21f0500);
		write_reg_word(0x1fa5a05c,0xff00180b);
		//b'TXPLL_VCO
		write_reg_word(0x1fa5a07c,0x1010000);
		write_reg_word(0x1fa5a080,0x40407);
		//b'TXPLL_PCW
		write_reg_word(0x1fa5b798,0x8400000);
		write_reg_word(0x1fa5b794,0x1000000);
		//b'TXPLL_KBand
		write_reg_word(0x1fa5a060,0x1030004);
		write_reg_word(0x1fa5a05c,0xe400180b);
		//b'TXPLL_DIV
		write_reg_word(0x1fa5a064,0x1000000);
		write_reg_word(0x1fa5a07c,0x1010000);
		//b'TXPLL_TCL
		write_reg_word(0x1fa5a09c,0x1000f);
		write_reg_word(0x1fa5a074,0x3010100);
		write_reg_word(0x1fa5a078,0x104000b);
		write_reg_word(0x1fa5a07c,0x1010000);
		//b'TXPLL_EN
		write_reg_word(0x1fa5b854,0x1010000);
		//b'TXPLL_Out
		write_reg_word(0x1fa5b854,0x1010101);
		udelay(200);
		//b'Phya_BringUp
		//b'TX_CONFIG
		write_reg_word(0x1fa5a0e8,0x1010401);
		udelay(1);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1050000);
		write_reg_word(0x1fa5c414,0x0);
		//b'TX_BringUp
		//b'TX_rate_ctrl
		write_reg_word(0x1fa5c910,0x2);
		//b'TX_CONFIG
		write_reg_word(0x1fa5a0e8,0x1010401);
		write_reg_word(0x1fa5c874,0x1010000);
		write_reg_word(0x1fa5c77c,0x1050000);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1
		//b'TX_FIR
		write_reg_word(0x1fa5c778,0x1010101);
		write_reg_word(0x1fa5c780,0x10b);
		//b'TX_RSTB
		write_reg_word(0x1fa5c260,0x101);
		//b'RX_BringUp M2
		//b'RX_INIT M2
		//b'RX_rate_ctrl
		write_reg_word(0x1fa5c910,0x22);
		write_reg_word(0x1fa5c178,0x20403);
		//b'RX_Path_Init
		write_reg_word(0x1fa5c184,0x40003ff);
		write_reg_word(0x1fa5a234,0x1000000);
		write_reg_word(0x1fa5a238,0x1010101);
		write_reg_word(0x1fa5a200,0x1000000);
		write_reg_word(0x1fa5a228,0x2000000);
		write_reg_word(0x1fa5a208,0x3ff0008);
		write_reg_word(0x1fa5c320,0x10101);
		write_reg_word(0x1fa5c004,0xc100a01);
		write_reg_word(0x1fa5c48c,0x1000202);
		write_reg_word(0x1fa5a1bc,0x0);
		write_reg_word(0x1fa5c814,0x1010000);
		write_reg_word(0x1fa5a1f0,0x7);
		write_reg_word(0x1fa5a1ec,0x60100);
		write_reg_word(0x1fa5a1e8,0x10000);
		write_reg_word(0x1fa5c80c,0x1000000);
		//b'RX_FE,force 0, Gain 1, Peaking 2
		write_reg_word(0x1fa5c88c,0x0);
		write_reg_word(0x1fa5c768,0x0);
		//b'FLL_FMeter(0)
		write_reg_word(0x1fa5c390,0x100001);
		write_reg_word(0x1fa5c394,0xffff0000);
		write_reg_word(0x1fa5c39c,0x3107);
		//b'RX_REV
		write_reg_word(0x1fa5a1b4,0xc8c31030);
		//b'RX_Rdy_TimeOut
		write_reg_word(0x1fa5c100,0xa0005);
		//b'RX_CalBoundry_Init
		write_reg_word(0x1fa5c08c,0x101);
		write_reg_word(0x1fa5c104,0x2);
		write_reg_word(0x1fa5c090,0x320002);
		write_reg_word(0x1fa5c09c,0x320002);
		write_reg_word(0x1fa5c094,0x320002);
		write_reg_word(0x1fa5c098,0x320002);
		//b'RX_BySerdes
		//b'RX_OSR
		write_reg_word(0x1fa5c76c,0x1000000);
		write_reg_word(0x1fa5a1bc,0x0);
		//b'CDR_LPF_RATIO
		write_reg_word(0x1fa5a1c8,0x2000000);
		//b'CDR_PR
		write_reg_word(0x1fa5a1d8,0x4010808);
		write_reg_word(0x1fa5a1dc,0x80606);
		//b'RX_EYE_Mon
		write_reg_word(0x1fa5c120,0x103);
		write_reg_word(0x1fa5c088,0x1);
		//b'RX_SYS_En
		write_reg_word(0x1fa5c38c,0x1);
		write_reg_word(0x1fa5c000,0x1000000);
		udelay(500);
		//b'FLL_FMeter(1)
		write_reg_word(0x1fa5c330,0x1);
		write_reg_word(0x1fa5c33c,0x1010100);
		//b'RX_CMLEQ_EN
		write_reg_word(0x1fa5a200,0x1010101);
		write_reg_word(0x1fa5a1fc,0x0);
		//b'RX_CDR_PR
		write_reg_word(0x1fa5a1f0,0x7);
		//b'RX_CDR_xxx_Pwdb
		write_reg_word(0x1fa5c824,0x1000100);
		write_reg_word(0x1fa5c81c,0x100);
		write_reg_word(0x1fa5c894,0x100);
		write_reg_word(0x1fa5c84c,0x1000000);
		write_reg_word(0x1fa5c34c,0x0);
		//b'RX_SigDet
		write_reg_word(0x1fa5a1f8,0x20200);
		write_reg_word(0x1fa5a1f4,0x3000002);
		//b'RX_SigDet_Pwdb
		write_reg_word(0x1fa5c350,0x0);
		//b'RX_SigDet_En
		write_reg_word(0x1fa5c16c,0x1);
		//b'PXP_RX_PHYCK
		write_reg_word(0x1fa5a1b8,0x10242);
		write_reg_word(0x1fa5a1ac,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c81c,0x101);
		write_reg_word(0x1fa5c894,0x101);
		write_reg_word(0x1fa5c84c,0x1010000);
		write_reg_word(0x1fa5c34c,0x1010101);
		//b'RX_SigDet_Pwdb
		write_reg_word(0x1fa5c350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x100);
		//b'RX_PRCal 0
		write_reg_word(0x1fa5c460,0x20);
		write_reg_word(0x1fa5c150,0x9f439e7b);
		write_reg_word(0x1fa5c14c,0x7fff7fff);
		write_reg_word(0x1fa5c158,0x3307);
		write_reg_word(0x1fa5c154,0x9f439e7b);
		write_reg_word(0x1fa5a1d4,0x1000000);
		write_reg_word(0x1fa5c820,0x1010100);
		write_reg_word(0x1fa5c794,0x10000);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
		{		
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
	
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);		
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
			rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
			RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);
			//abs
			/*		
					if (RO_FL_Out > FL_Out_target)
					{ 
					RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
					}
					else if (RO_FL_Out < FL_Out_target)
					{
					RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
					}
					else RO_FL_Out_diff = 0;
			*/			
			RO_FL_Out_diff =abs(FL_Out_target -RO_FL_Out);	
			if(RO_FL_Out > FL_Out_target)
			{        
				RO_FL_Out_diff_tmp = RO_FL_Out_diff;
				cdr_pr_idac_tmp = (PrCal_Serach<<8);
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
			}
		}
	
		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
		{
			pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
			rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
			RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
			write_reg_word(0x1fa5c158,0x3300);
			write_reg_word(0x1fa5c158,0x3303);
			
			udelay(5000);
			RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
			RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
	
			printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);
	
			if(RO_FL_Out < FL_Out_target)
			{
				pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}
			else
			{
				cdr_pr_idac_tmp = pr_idac;
				printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
			}   
		
		}
		
		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);
	
	
		write_reg_word(0x1fa5c158,0x3300);
		write_reg_word(0x1fa5c158,0x3303);
	
		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _RO_RX_FREQDET);	  
		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 
	
		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.dat.value = RG_R_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		//Lane1
		//RG_PXP_CDR1_PR_INJ_MODE.hal.rg_pxp_cdr1_pr_inj_force_off = 0;
		//RG_W_PL(_pxSGMII->phya_base,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR1_PR_INJ_MODE,RG_PXP_CDR1_PR_INJ_MODE.dat.value);
		write_reg_word(0x1fa5a1d4,0x0);
	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		//rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
		write_reg_word(0x1fa5c820,0x1010000);
		
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
	
		//SS_RX_FLL_b.hal.rg_load_en = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		write_reg_word(0x1fa5c19c,0x1);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa5c000,pma_start,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
		
	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		//rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		write_reg_word(0x1fa5c824,0x1000101);
		write_reg_word(0x1fa5c824,0x1010101);
		write_reg_word(0x1fa5c824,0x10101);
		
		//SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		//RG_W_PL(0x1fa5c000,pma_start,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		write_reg_word(0x1fa5c460,0x0);
		//b'RO_state_freqdet = 0x0
		//RX_PRCal_end--------------------------------
	
		//b'RX_CDR_LPF_RSTB mode 1, sel0
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		//b'RSTB sel8, val 0 lane =1
		write_reg_word(0x1fa5c460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1 lane =1
		write_reg_word(0x1fa5c460,0xfff);
		udelay(100);
		//b'RX_OSCAL_SEQ
		//b'RX_L2R
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x10100);//226
		//b'RX_pical
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c15c,0x400);
		write_reg_word(0x1fa5c118,0xa000a08);
		write_reg_word(0x1fa5c204,0x1000101);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010001);
		write_reg_word(0x1fa5c328,0x0);
		write_reg_word(0x1fa5c334,0x1010000);
		write_reg_word(0x1fa5c31c,0x1010100);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c324,0x10101);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1010001);
		write_reg_word(0x1fa5c30c,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c328,0x100);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c318,0x100);
		write_reg_word(0x1fa5c110,0x100);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c318,0x0);
		write_reg_word(0x1fa5c30c,0x1);
		//b'RX_pdos
		write_reg_word(0x1fa5c894,0x1010101);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c304,0x1010101);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x10100);
		write_reg_word(0x1fa5c084,0x0);
		write_reg_word(0x1fa5c200,0x20101);
		write_reg_word(0x1fa5c328,0x101);
		write_reg_word(0x1fa5c334,0x1000000);
		write_reg_word(0x1fa5c208,0x10100);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x1000000);
		write_reg_word(0x1fa5c328,0x10101);
		write_reg_word(0x1fa5c208,0x10101);
		write_reg_word(0x1fa5c110,0x10000);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c084,0x100);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c084,0x101);
		write_reg_word(0x1fa5c894,0x1000101);
		//b'RX_feos
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c10c,0x1010001);
		write_reg_word(0x1fa5c308,0x1010101);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1010100);
		write_reg_word(0x1fa5c144,0x30);
		write_reg_word(0x1fa5c32c,0x0);
		write_reg_word(0x1fa5c338,0x1000100);
		write_reg_word(0x1fa5c204,0x1010001);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c108,0x0);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c110,0x1000001);
		udelay(2000);
		write_reg_word(0x1fa5c110,0x1);
		write_reg_word(0x1fa5c110,0x0);
		write_reg_word(0x1fa5c360,0x50000);
		//b'RX_sdcal
		write_reg_word(0x1fa5c898,0x101);//226
		write_reg_word(0x1fa5c840,0x101);
		write_reg_word(0x1fa5c204,0x10101);
		write_reg_word(0x1fa5c32c,0x10000);
		write_reg_word(0x1fa5c10c,0x1010000);
		write_reg_word(0x1fa5c338,0x1000000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c204,0x1010101);
		write_reg_word(0x1fa5c32c,0x10100);
		write_reg_word(0x1fa5c114,0x1);
		udelay(2000);
		write_reg_word(0x1fa5c114,0x0);
		write_reg_word(0x1fa5c898,0x100);
		write_reg_word(0x1fa5c83c,0x1000000);
		write_reg_word(0x1fa5c840,0x1000100);
		//b'RX_OS_rdy
		write_reg_word(0x1fa5c114,0x100);
		write_reg_word(0x1fa5c114,0x0);
		//b'DIG_reset
		write_reg_word(0x1fa5c460,0x0);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_L2D
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		udelay(1000);
		//b'RX_rxrdy
		write_reg_word(0x1fa5c114,0x1000000);
		write_reg_word(0x1fa5c10c,0x10000);
		write_reg_word(0x1fa5c460,0xffe);
		write_reg_word(0x1fa5c460,0xfff);
		//b'RX_CDR_RST
		//b'RX_CDR_LFP_L2D mode 1, sel 0
		write_reg_word(0x1fa5c818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0
		write_reg_word(0x1fa5c818,0x1000100);
		udelay(7000);
		//b'RX_CDR_LPF_RSTB mode 1, sel1
		write_reg_word(0x1fa5c818,0x1010100);
		udelay(1000);
		//b'RX_CDR_LFP_L2D mode 1, sel 1
		write_reg_word(0x1fa5c818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1
		write_reg_word(0x1fa5c818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1
		write_reg_word(0x1fa5c818,0x10001);
		//b'xSGMII_Solution : 0
		//b'xsgmii_init : 0, Lane 1
		//b'usxgmii_init 1
		write_reg_word(0x1fa64100,0x10010001);
		//b'PCS E0/E1/E3 SWR solution
		//b'HIBER_OFF
		write_reg_word(0x1fa65900,0x40);
		write_reg_word(0x1fa65b2c,0x0);
		//b'usxgmii_pcs_int en 0
		write_reg_word(0x1fa65bc0,0x0);
		write_reg_word(0x1fa65bc4,0x0);
		write_reg_word(0x1fa65bc8,0x0);
		write_reg_word(0x1fa65bcc,0x0);
		write_reg_word(0x1fa65be0,0x0);
		write_reg_word(0x1fa65bd8,0x0);
		write_reg_word(0x1fa65bdc,0x0);
		write_reg_word(0x1fa65be4,0x0);
		write_reg_word(0x1fa65bc8,0x0);
		write_reg_word(0x1fa65bcc,0x0);
		write_reg_word(0x1fa65be0,0x0);
		//b'xsgmii 0 int enable 0
		//b'force mode
		//b'xSGMII_AN_API 0, enable : 0
		//b'usxgmii_an
		write_reg_word(0x1fa65bf8,0x6330000);
		//b'_rg_usxgmii_an_control_0 1,6330000
		//b'_rg_usxgmii_an_control_1 1,1601
		//b'usxgmii_rate_api rate 0 : 10GUSXGMII_10G
		write_reg_word(0x1fa65bfc,0x1001);
		write_reg_word(0x1fa66000,0xc000011);
		write_reg_word(0x1fa6602c,0x104);
		//b'_rg_usxgmii_an_control_1 1001
		//b'_rg_rate_adapt_ctrl_0 c000011
		//b'_rg_rate_adapt_ctrl_11 104
		//b'USXGMII_10G exit
		//b'usxgmii_pcs_an_ctrl7
		//b'usxgmii_pcs_an_ctrl7
		write_reg_word(0x1fa65c20,0x1000);
		//b'xSGMII_Solution : 1\r\n
		printf("PCIE1 XFI_10G exit\n");
			
	}else if((serdes_intf[SERDES_PON][1] == SERDES_ETHERWAN) && (serdes_intf[SERDES_PON][2] == SERDES_PON_HSGMII_MODE)){
		uint FL_Out_target = 0xA000 ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
        u32 rg_value = 0;
		//FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa8b000, PMA_BASE_OFFSET,(u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
		
		write_reg_word(0x1fb00070, 0x11);//wan_sel:HSGMII MODE
		write_reg_word(0x1fb0009c, 0x509);//pon_xsi_sel
		
		//FT 50R restore
		rg_value = RG_R_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value = RG_R_PL(0x1fa8a000, (u32)PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |=rg_114;
	    RG_W_PL(0x1fa8a000, (u32)PXP_BASE_OFFSET, (u32)0x114, rg_value);		
		
		//b'echo "2 1 0 0 0">/proc/xsgmii\r\n'
		//b'op = 2,xsgmii 1,mod 0,an 0,rate 0,buf 32,num 5,count a\r\n'
		//b'xsgmii_api: serdes 2,xsgmii 1,mod 0,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa8a000,0x10040001);
		//b'JCPLL BringUp 1\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa8a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa8a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa8a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa8a01c,0x104);
		write_reg_word(0x1fa8a020,0x30000);
		write_reg_word(0x1fa8a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa8a038,0x0);
		write_reg_word(0x1fa8a034,0x0);
		write_reg_word(0x1fa8a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa8a004,0x180000);
		write_reg_word(0x1fa8a008,0x101f0a);
		write_reg_word(0x1fa8a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa8a02c,0x4010100);
		write_reg_word(0x1fa8a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa8b800,0x25800000);
		write_reg_word(0x1fa8b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa8a014,0x10000);
		write_reg_word(0x1fa8a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa8a010,0x1000300);
		write_reg_word(0x1fa8a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa8a048,0x1020ff);
		write_reg_word(0x1fa8a024,0x5010100);
		write_reg_word(0x1fa8a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa8b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa8a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa8a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa8a068,0x0);
		write_reg_word(0x1fa8a06c,0x1000003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa8a080,0x0);
		write_reg_word(0x1fa8a07c,0x0);
		write_reg_word(0x1fa8a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa8a050,0x1f05000a);
		write_reg_word(0x1fa8a054,0x5);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa8a074,0x1);
		write_reg_word(0x1fa8a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa8b798,0xa000000);
		write_reg_word(0x1fa8b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa8a058,0x30004e4);
		write_reg_word(0x1fa8a05c,0x101);
		write_reg_word(0x1fa8a054,0x5);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa8a05c,0x1);
		write_reg_word(0x1fa8a074,0x10001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa8a094,0x1000f);
		write_reg_word(0x1fa8a070,0x4000d03);
		write_reg_word(0x1fa8a074,0x10001);
		write_reg_word(0x1fa8a06c,0x1000003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa8b854,0x1010101);
		udelay(200);
		//b'TX BringUp 1\r\n'
		//b'tx_rate_ctrl 1\r\n'
		write_reg_word(0x1fa8b580,0x1);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa8a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa8b874,0x1010000);
		write_reg_word(0x1fa8b77c,0x1040000);
		write_reg_word(0x1fa8b784,0x101);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 0, c0b b, c1 1, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa8b778,0x100010b);
		write_reg_word(0x1fa8b780,0x101);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa8b260,0x101);
		//b'RX BringUp 1\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		//b'RX_rate_ctrl FLL\r\n'
		write_reg_word(0x1fa8b178,0x10403);
		write_reg_word(0x1fa8b374,0x0);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa8b184,0x40003ff);
		write_reg_word(0x1fa8a148,0x1010101);
		write_reg_word(0x1fa8a144,0x1000000);
		write_reg_word(0x1fa8a11c,0x2000401);
		write_reg_word(0x1fa8b004,0xc100a01);
		write_reg_word(0x1fa8a13c,0x20000);
		write_reg_word(0x1fa8a120,0x3ff08);
		write_reg_word(0x1fa8b320,0x10101);
		write_reg_word(0x1fa8b48c,0x1000202);
		write_reg_word(0x1fa8a0dc,0x0);
		write_reg_word(0x1fa8b80c,0x1000000);
		write_reg_word(0x1fa8b814,0x1010000);
		write_reg_word(0x1fa8a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 1\r\n'
		write_reg_word(0x1fa8b88c,0x0);
		write_reg_word(0x1fa8b768,0x0);
		//b'RX_FE_VOS\r\n'
		write_reg_word(0x1fa8b79c,0x10100);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b390,0x100001);
		write_reg_word(0x1fa8b394,0xffff0000);
		write_reg_word(0x1fa8b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa8a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa8b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa8b08c,0x101);
		write_reg_word(0x1fa8b104,0x2);
		write_reg_word(0x1fa8b090,0x320002);
		write_reg_word(0x1fa8b09c,0x320002);
		write_reg_word(0x1fa8b094,0x320002);
		write_reg_word(0x1fa8b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa8b76c,0x1010000);
		write_reg_word(0x1fa8a0dc,0x100);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa8a0e8,0x2000001);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa8a0f8,0x4010806);
		write_reg_word(0x1fa8a0fc,0x60606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa8b120,0x103);
		write_reg_word(0x1fa8b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa8b38c,0x1);
		write_reg_word(0x1fa8b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b33c,0x1010100);
		write_reg_word(0x1fa8b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa8a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa8a10c,0xe0604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1000100);
		write_reg_word(0x1fa8b81c,0x100);
		write_reg_word(0x1fa8b894,0x100);
		write_reg_word(0x1fa8b84c,0x1000000);
		write_reg_word(0x1fa8b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa8a114,0x20200);
		write_reg_word(0x1fa8a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa8a0d8,0x1010b);
		write_reg_word(0x1fa8a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b81c,0x101);
		write_reg_word(0x1fa8b894,0x101);
		write_reg_word(0x1fa8b84c,0x1010000);
		write_reg_word(0x1fa8b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 1\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x100);
		//b'RX_PRCal 1\r\n'
		write_reg_word(0x1fa8b460,0x20);
		write_reg_word(0x1fa8b150,0xa0649f9c);
		write_reg_word(0x1fa8b14c,0x4e204e20);
		write_reg_word(0x1fa8b158,0x3307);
		write_reg_word(0x1fa8b154,0xa0649f9c);
		write_reg_word(0x1fa8a0f4,0x1000000);
		write_reg_word(0x1fa8b820,0x1010100);
		write_reg_word(0x1fa8b794,0x1010000);
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b824,0x1000101);
		write_reg_word(0x1fa8b824,0x1010101);

		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa8b158,0x3300);
		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa8b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa8b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init\r\n'
		write_reg_word(0x1fa80a00,0x4c9cc000);
		write_reg_word(0x1fa84018,0x34);
		//b'hsgmii_init\r\n'
		//b'Int init\r\n'
		//b'xsgmii 1 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 1, enable : 0\r\n'
		//b'sgmii_an 0\r\n'
		write_reg_word(0x1fa80000,0x140);
		//b'hsgmii\r\n'
		//b'HGMII_2.5G\r\n'
		write_reg_word(0x1fa86000,0xc000c11);
		//b'HSGMII_2.5 exit\r\n'
		printf("PON HSGMII_2.5 exit\n");
		//b'xSGMII_Solution : 1\r\n'
		
	}else if((serdes_intf[SERDES_PON][1] == SERDES_ETHERWAN) && (serdes_intf[SERDES_PON][2] == SERDES_PON_XFI_MODE)){
		uint FL_Out_target = 0x9EDF ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
        u32 rg_value = 0;
		//FT 50R retrive
		//TX 50R
	    rg_b77c =	RG_R_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 =	RG_R_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000; 
      			
		write_reg_word(0x1fb00070,0x12);//wan_sel:BASER MODE
		write_reg_word(0x1fb0009c,0x309);//pon_xsi_sel
		
		//FT 50R restore
		rg_value =	RG_R_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c ,rg_value);
        //RX 50R	
 	    rg_value =	RG_R_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114 ,rg_value);		
		
        //b'echo "2 0 0 0 0">/proc/xsgmii\r\n'
		//b'op = 2,xsgmii 0,mod 0,an 0,rate 0,buf 32,num 5,count a\r\n'
		//b'xsgmii_api: serdes 2,xsgmii 0,mod 0,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa8a000,0x10040001);
		//b'JCPLL BringUp 0\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa8a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa8a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa8a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa8a01c,0x104);
		write_reg_word(0x1fa8a020,0x30000);
		write_reg_word(0x1fa8a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa8a038,0x0);
		write_reg_word(0x1fa8a034,0x0);
		write_reg_word(0x1fa8a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa8a004,0x180000);
		write_reg_word(0x1fa8a008,0x101f0a);
		write_reg_word(0x1fa8a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa8a02c,0x4010100);
		write_reg_word(0x1fa8a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa8b800,0x25800000);
		write_reg_word(0x1fa8b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa8a014,0x10000);
		write_reg_word(0x1fa8a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa8a010,0x1000300);
		write_reg_word(0x1fa8a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa8a048,0xf20ff);
		write_reg_word(0x1fa8a024,0x5010100);
		write_reg_word(0x1fa8a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa8b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa8a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa8a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa8a068,0x0);
		write_reg_word(0x1fa8a06c,0x1010003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa8a080,0x0);
		write_reg_word(0x1fa8a07c,0x0);
		write_reg_word(0x1fa8a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa8a050,0x1f05000f);
		write_reg_word(0x1fa8a054,0x180b02);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa8a074,0x1000001);
		write_reg_word(0x1fa8a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa8b798,0x8400000);
		write_reg_word(0x1fa8b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa8a058,0x30004e4);
		write_reg_word(0x1fa8a05c,0x101);
		write_reg_word(0x1fa8a054,0x180b02);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa8a05c,0x1);
		write_reg_word(0x1fa8a074,0x1000001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa8a094,0x1000f);
		write_reg_word(0x1fa8a070,0x4000b03);
		write_reg_word(0x1fa8a074,0x1000001);
		write_reg_word(0x1fa8a06c,0x1010003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa8b854,0x1010101);
		udelay(200);
		//b'TX BringUp 0\r\n'
		//b'tx_rate_ctrl 0\r\n'
		write_reg_word(0x1fa8b580,0x2);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa8a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa8b874,0x1010000);
		write_reg_word(0x1fa8b77c,0x1050000);
		write_reg_word(0x1fa8b784,0x102);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa8b778,0x1010101);
		write_reg_word(0x1fa8b780,0x10b);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa8b260,0x101);
		//b'RX BringUp 0\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		write_reg_word(0x1fa8b374,0x2);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa8b184,0x40003ff);
		write_reg_word(0x1fa8a148,0x1010101);
		write_reg_word(0x1fa8a144,0x1000000);
		write_reg_word(0x1fa8a11c,0x2000401);
		write_reg_word(0x1fa8b004,0xc100a01);
		write_reg_word(0x1fa8a13c,0x20000);
		write_reg_word(0x1fa8a120,0x3ff08);
		write_reg_word(0x1fa8b320,0x10101);
		write_reg_word(0x1fa8b48c,0x1000202);
		write_reg_word(0x1fa8a0dc,0x0);
		write_reg_word(0x1fa8b80c,0x1000000);
		write_reg_word(0x1fa8b814,0x1010000);
		write_reg_word(0x1fa8a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 2\r\n'
		write_reg_word(0x1fa8b88c,0x0);
		write_reg_word(0x1fa8b768,0x0);
		//b'RX_FE_VOS\r\n'
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b390,0x100001);
		write_reg_word(0x1fa8b394,0xffff0000);
		write_reg_word(0x1fa8b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa8a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa8b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa8b08c,0x101);
		write_reg_word(0x1fa8b104,0x2);
		write_reg_word(0x1fa8b090,0x320002);
		write_reg_word(0x1fa8b09c,0x320002);
		write_reg_word(0x1fa8b094,0x320002);
		write_reg_word(0x1fa8b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa8b76c,0x1000000);
		write_reg_word(0x1fa8a0dc,0x0);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa8a0e8,0x2000000);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa8a0f8,0x4010808);
		write_reg_word(0x1fa8a0fc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa8b120,0x103);
		write_reg_word(0x1fa8b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa8b38c,0x1);
		write_reg_word(0x1fa8b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b33c,0x1010100);
		write_reg_word(0x1fa8b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa8a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa8a10c,0x70604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1000100);
		write_reg_word(0x1fa8b81c,0x100);
		write_reg_word(0x1fa8b894,0x100);
		write_reg_word(0x1fa8b84c,0x1000000);
		write_reg_word(0x1fa8b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa8a114,0x20200);
		write_reg_word(0x1fa8a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa8a0d8,0x10242);
		write_reg_word(0x1fa8a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b81c,0x101);
		write_reg_word(0x1fa8b894,0x101);
		write_reg_word(0x1fa8b84c,0x1010000);
		write_reg_word(0x1fa8b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x100);
		//b'RX_PRCal 0\r\n'
		write_reg_word(0x1fa8b460,0x20);
		write_reg_word(0x1fa8b150,0x9f439e7b);
		write_reg_word(0x1fa8b14c,0x7fff7fff);
		write_reg_word(0x1fa8b158,0x3307);
		write_reg_word(0x1fa8b154,0x9f439e7b);
		write_reg_word(0x1fa8a0f4,0x1000000);
		write_reg_word(0x1fa8b820,0x1010100);
		write_reg_word(0x1fa8b794,0x1010000);
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b824,0x1000101);
		write_reg_word(0x1fa8b824,0x1010101);
		
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa8b158,0x3300);
		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa8b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa8b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init 1\r\n'
		write_reg_word(0x1fa84100,0x10010001);
		//b'PCS E0/E1/E3 SWR solution\r\n'
		//b'HIBER_OFF\r\n'
		write_reg_word(0x1fa85900,0x40);
		write_reg_word(0x1fa85b2c,0x0);
		//b'Int init\r\n'
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa85bc0,0x0);
		write_reg_word(0x1fa85bc4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);
		write_reg_word(0x1fa85bd8,0x0);
		write_reg_word(0x1fa85bdc,0x0);
		write_reg_word(0x1fa85be4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'force mode\r\n'
		//b'xSGMII_AN_API 0, enable : 0\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa85bf8,0x6330000);
		//b'_rg_usxgmii_an_control_0 6330000\r\n'
		//b'_rg_usxgmii_an_control_1 0\r\n'
		//b'usxgmii_rate_api rate 0 : 10GUSXGMII_10G\r\n'
		write_reg_word(0x1fa85bfc,0x1001);
		write_reg_word(0x1fa86000,0xc000c11);
		write_reg_word(0x1fa8602c,0x104);
		//b'_rg_usxgmii_an_control_1 1001\r\n'
		//b'_rg_rate_adapt_ctrl_0 c000c11\r\n'
		//b'_rg_rate_adapt_ctrl_11 104\r\n'
		//b'USXGMII_10G exit\r\n'
		//b'usxgmii_pcs_an_ctrl7\r\n'
		write_reg_word(0x1fa85c20,0x1000);
		printf("PON XFI_10G exit\n");
		//b'xSGMII_Solution : 1\r\n'
		
	}else if((serdes_intf[SERDES_PON][1] == SERDES_ETHERWAN) && (serdes_intf[SERDES_PON][2] == SERDES_PON_USXGMII_MODE)){
		uint FL_Out_target = 0x9EDF ;
        u32 rg_b77c = 0;
        u32 rg_114 = 0;
        u32 rg_value = 0;
		//FT 50R retrive
		//TX 50R
	    rg_b77c = RG_R_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_b77c &= 0x00000107;	    
        //RX 50R
 	    rg_114 = RG_R_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_114 &= 0x03000000;		
			
		write_reg_word(0x1fb00070,0x12);//wan_sel:BASER MODE
		write_reg_word(0x1fb0009c,0x309);//pon_xsi_sel
		
		//FT 50R restore
		rg_value =	RG_R_PL(0x1fa8a000, PMA_BASE_OFFSET, (u32)0xb77c);
	    rg_value &= 0xfffffef8;	  
	    rg_value |= rg_b77c;
	    RG_W_PL(0x1fa8b000, PMA_BASE_OFFSET, (u32)0xb77c, rg_value);
        //RX 50R	
 	    rg_value =	RG_R_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114);
	    rg_value &= 0xfcffffff;    
	    rg_value |= rg_114;
	    RG_W_PL(0x1fa8a000, PXP_BASE_OFFSET, (u32)0x114, rg_value);
		
        //b'echo "2 0 1 0 0">/proc/xsgmii\r\n'
		//b'op = 2,xsgmii 0,mod 1,an 0,rate 0,buf 32,num 5,count a\r\n'
		//b'xsgmii_api: serdes 2,xsgmii 0,mod 1,rate 0,an 0\r\n'
		//b'xsgmii_chg 1\r\n'
		write_reg_word(0x1fa8a000,0x10040001);
		//b'JCPLL BringUp 0\r\n'
		//b'JCPLL_LDO\r\n'
		write_reg_word(0x1fa8a048,0x1020ff);
		//b'JCPLL_RSTB\r\n'
		write_reg_word(0x1fa8a01c,0x3000004);
		udelay(2000);
		write_reg_word(0x1fa8a01c,0x3000104);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1000000);
		//b'JCPLL_SDM\r\n'
		write_reg_word(0x1fa8a01c,0x104);
		write_reg_word(0x1fa8a020,0x30000);
		write_reg_word(0x1fa8a024,0x5010100);
		//b'JCPLL_SSC\r\n'
		write_reg_word(0x1fa8a038,0x0);
		write_reg_word(0x1fa8a034,0x0);
		write_reg_word(0x1fa8a030,0x3018);
		//b'JCPLL_LPF\r\n'
		write_reg_word(0x1fa8a004,0x180000);
		write_reg_word(0x1fa8a008,0x101f0a);
		write_reg_word(0x1fa8a00c,0x2ff0000);
		//b'JCPLL_VCO\r\n'
		write_reg_word(0x1fa8a02c,0x4010100);
		write_reg_word(0x1fa8a030,0x301d);
		//b'JCPLL_PCW\r\n'
		write_reg_word(0x1fa8b800,0x25800000);
		write_reg_word(0x1fa8b79c,0x10000);
		//b'JCPLL_DIV\r\n'
		write_reg_word(0x1fa8a014,0x10000);
		write_reg_word(0x1fa8a02c,0x4010100);
		//b'JCPLL_KBand\r\n'
		write_reg_word(0x1fa8a010,0x1000300);
		write_reg_word(0x1fa8a00c,0x2e40000);
		//b'JCPLL_TCL\r\n'
		write_reg_word(0x1fa8a048,0xf20ff);
		write_reg_word(0x1fa8a024,0x5010100);
		write_reg_word(0x1fa8a028,0x10400);
		//b'JCPLL_EN\r\n'
		write_reg_word(0x1fa8b828,0x1010000);
		//b'JCPLL_Out\r\n'
		write_reg_word(0x1fa8b828,0x1010101);
		udelay(200);
		//b'TXPLL BringUp\r\n'
		//b'TXPLL_VCOLDO_Out\r\n'
		write_reg_word(0x1fa8a084,0x101031b);
		//b'TXPLL_RSTB\r\n'
		write_reg_word(0x1fa8a064,0x1040001);
		udelay(2000);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1000000);
		//b'TXPLL_SDM\r\n'
		write_reg_word(0x1fa8a068,0x0);
		write_reg_word(0x1fa8a06c,0x1010003);
		//b'TXPLL_SSC\r\n'
		write_reg_word(0x1fa8a080,0x0);
		write_reg_word(0x1fa8a07c,0x0);
		write_reg_word(0x1fa8a084,0x1010000);
		//b'TXPLL_LPF\r\n'
		write_reg_word(0x1fa8a050,0x1f05000f);
		write_reg_word(0x1fa8a054,0x180b02);
		//b'TXPLL_VCO\r\n'
		write_reg_word(0x1fa8a074,0x1000001);
		write_reg_word(0x1fa8a078,0x4040701);
		//b'TXPLL_PCW\r\n'
		write_reg_word(0x1fa8b798,0x8400000);
		write_reg_word(0x1fa8b794,0x1000000);
		//b'TXPLL_KBand\r\n'
		write_reg_word(0x1fa8a058,0x30004e4);
		write_reg_word(0x1fa8a05c,0x101);
		write_reg_word(0x1fa8a054,0x180b02);
		//b'TXPLL_DIV\r\n'
		write_reg_word(0x1fa8a05c,0x1);
		write_reg_word(0x1fa8a074,0x1000001);
		//b'TXPLL_TCL\r\n'
		write_reg_word(0x1fa8a094,0x1000f);
		write_reg_word(0x1fa8a070,0x4000b03);
		write_reg_word(0x1fa8a074,0x1000001);
		write_reg_word(0x1fa8a06c,0x1010003);
		//b'TXPLL_EN\r\n'
		write_reg_word(0x1fa8b854,0x1010000);
		//b'TXPLL_Out\r\n'
		write_reg_word(0x1fa8b854,0x1010101);
		udelay(200);
		//b'TX BringUp 0\r\n'
		//b'tx_rate_ctrl 0\r\n'
		write_reg_word(0x1fa8b580,0x2);
		//b'TX_CONFIG\r\n'
		write_reg_word(0x1fa8a0c4,0x1010401);
		udelay(1);
		write_reg_word(0x1fa8b874,0x1010000);
		write_reg_word(0x1fa8b77c,0x1050000);
		write_reg_word(0x1fa8b784,0x102);
		//b'TX_FIR_Load_Para swing 2, len 4, cn1 1, c0b 1, c1 b, en 1\r\n'
		//b'TX_FIR\r\n'
		write_reg_word(0x1fa8b778,0x1010101);
		write_reg_word(0x1fa8b780,0x10b);
		//b'TX_RSTB\r\n'
		write_reg_word(0x1fa8b260,0x101);
		//b'RX BringUp 0\r\n'
		//b'RX_INIT\r\n'
		//b'rx_rate_ctrl\r\n'
		write_reg_word(0x1fa8b374,0x2);
		//b'RX_Path_Init\r\n'
		write_reg_word(0x1fa8b184,0x40003ff);
		write_reg_word(0x1fa8a148,0x1010101);
		write_reg_word(0x1fa8a144,0x1000000);
		write_reg_word(0x1fa8a11c,0x2000401);
		write_reg_word(0x1fa8b004,0xc100a01);
		write_reg_word(0x1fa8a13c,0x20000);
		write_reg_word(0x1fa8a120,0x3ff08);
		write_reg_word(0x1fa8b320,0x10101);
		write_reg_word(0x1fa8b48c,0x1000202);
		write_reg_word(0x1fa8a0dc,0x0);
		write_reg_word(0x1fa8b80c,0x1000000);
		write_reg_word(0x1fa8b814,0x1010000);
		write_reg_word(0x1fa8a10c,0x70604);
		//b'RX_FE,force 0, Gain 1, Peaking 2\r\n'
		write_reg_word(0x1fa8b88c,0x0);
		write_reg_word(0x1fa8b768,0x0);
		//b'RX_FE_VOS\r\n'
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b390,0x100001);
		write_reg_word(0x1fa8b394,0xffff0000);
		write_reg_word(0x1fa8b39c,0x3107);
		//b'RX_REV\r\n'
		write_reg_word(0x1fa8a0d4,0xc8c31030);
		//b'RX_Rdy_TimeOut\r\n'
		write_reg_word(0x1fa8b100,0xa0005);
		//b'RX_CalBoundry_Init\r\n'
		write_reg_word(0x1fa8b08c,0x101);
		write_reg_word(0x1fa8b104,0x2);
		write_reg_word(0x1fa8b090,0x320002);
		write_reg_word(0x1fa8b09c,0x320002);
		write_reg_word(0x1fa8b094,0x320002);
		write_reg_word(0x1fa8b098,0x320002);
		//b'RX_BySerdes\r\n'
		//b'RX_OSR\r\n'
		write_reg_word(0x1fa8b76c,0x1000000);
		write_reg_word(0x1fa8a0dc,0x0);
		//b'CDR_LPF_RATIO\r\n'
		write_reg_word(0x1fa8a0e8,0x2000000);
		//b'CDR_PR\r\n'
		write_reg_word(0x1fa8a0f8,0x4010808);
		write_reg_word(0x1fa8a0fc,0x80606);
		//b'RX_EYE_Mon\r\n'
		write_reg_word(0x1fa8b120,0x103);
		write_reg_word(0x1fa8b088,0x1);
		//b'RX_SYS_En\r\n'
		write_reg_word(0x1fa8b38c,0x1);
		write_reg_word(0x1fa8b000,0x1000000);
		udelay(500);
		//b'RX_FLL_PR_FMeter\r\n'
		write_reg_word(0x1fa8b33c,0x1010100);
		write_reg_word(0x1fa8b330,0x1);
		//b'RX_CMLEQ_EN\r\n'
		write_reg_word(0x1fa8a118,0x1010100);
		//b'RX_CDR_PR\r\n'
		write_reg_word(0x1fa8a10c,0x70604);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1000100);
		write_reg_word(0x1fa8b81c,0x100);
		write_reg_word(0x1fa8b894,0x100);
		write_reg_word(0x1fa8b84c,0x1000000);
		write_reg_word(0x1fa8b34c,0x0);
		//b'RX_SigDet\r\n'
		write_reg_word(0x1fa8a114,0x20200);
		write_reg_word(0x1fa8a110,0x3000200);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x0);
		//b'PXP_RX_PHYCK\r\n'
		write_reg_word(0x1fa8a0d8,0x10242);
		write_reg_word(0x1fa8a0cc,0x1000000);
		udelay(100);
		//b'RX_CDR_xxx_Pwdb\r\n'
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b81c,0x101);
		write_reg_word(0x1fa8b894,0x101);
		write_reg_word(0x1fa8b84c,0x1010000);
		write_reg_word(0x1fa8b34c,0x1010101);
		//b'RX_SigDet_Pwdb\r\n'
		write_reg_word(0x1fa8b350,0x1);
		udelay(100);
		//b'RX_PR_CAL_SEQ 0\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x100);
		//b'RX_PRCal 0\r\n'
		write_reg_word(0x1fa8b460,0x20);
		write_reg_word(0x1fa8b150,0x9f439e7b);
		write_reg_word(0x1fa8b14c,0x7fff7fff);
		write_reg_word(0x1fa8b158,0x3307);
		write_reg_word(0x1fa8b154,0x9f439e7b);
		write_reg_word(0x1fa8a0f4,0x1000000);
		write_reg_word(0x1fa8b820,0x1010100);
		write_reg_word(0x1fa8b794,0x1010000);
		write_reg_word(0x1fa8b824,0x1010101);
		write_reg_word(0x1fa8b824,0x1000101);
		write_reg_word(0x1fa8b824,0x1010101);
		
		
		for (PrCal_Serach = 1; PrCal_Serach < 8 ; PrCal_Serach++)
  		{

		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = PrCal_Serach<<8;		
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,(u32)(rg_force_da_pxp_cdr_pr_idac.dat.value));

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

	 	udelay(5000);

		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);		
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		RO_pr_idac = rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac;
	 	printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n" ,RO_pr_idac , RO_FL_Out);

		//abs
		if (RO_FL_Out > FL_Out_target) RO_FL_Out_diff = RO_FL_Out - FL_Out_target;
		else if (RO_FL_Out < FL_Out_target) RO_FL_Out_diff = FL_Out_target - RO_FL_Out;
		else RO_FL_Out_diff = 0;
		
	 	if(RO_FL_Out > FL_Out_target)
	 	{        
			RO_FL_Out_diff_tmp = RO_FL_Out_diff;
			cdr_pr_idac_tmp = (PrCal_Serach<<8);
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);		 
	 	}
  		}

  		for (turn_pr_idac_bit_position = 7; turn_pr_idac_bit_position > -1 ; turn_pr_idac_bit_position--)
  		{
		pr_idac = cdr_pr_idac_tmp |(0x1<<turn_pr_idac_bit_position); 
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_da_pxp_cdr_pr_idac = pr_idac;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);

	 	write_reg_word(0x1fa8b158,0x3300);

		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);	  
	    
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
		RO_FL_Out = RO_RX_FREQDET.hal.ro_fl_out;

		printf("pr_idac = 0x%x ,RO_FL_Out = 0x%x\n",pr_idac,RO_FL_Out);

		if(RO_FL_Out < FL_Out_target)
		{
	        pr_idac &= ~(0x1<<turn_pr_idac_bit_position);
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}
		else
		{
			cdr_pr_idac_tmp = pr_idac;
			printf("cdr_pr_idac_tmp = 0x%x\n",cdr_pr_idac_tmp);
		}   
	  
  		}

		rg_force_da_pxp_cdr_pr_idac.dat.value = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		printf("sel_cdr_pr_idac = 0x%x\n",cdr_pr_idac_tmp);

		write_reg_word(0x1fa8b158,0x3300);
		write_reg_word(0x1fa8b158,0x3303);

		udelay(5000);
		RO_RX_FREQDET.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _RO_RX_FREQDET);	  
  		RO_state_freqdet = RO_RX_FREQDET.hal.ro_fbck_lock; 

		printf("RO_state_freqdet = 0x%x\n",RO_state_freqdet);

		//Load_Band
		RG_PXP_CDR_PR_INJ_MODE.dat.value = RG_R_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE);
		rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en);
		rg_force_da_pxp_cdr_pr_idac.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac);
		SS_RX_FLL_b.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b);
		SS_RX_FLL_1.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value = RG_R_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb);
		
		RG_PXP_CDR_PR_INJ_MODE.hal.rg_pxp_cdr_pr_inj_force_off = 0;
		RG_W_PL(0x1fa8a000,PXP_BASE_OFFSET,(u32) _RG_PXP_CDR_PR_INJ_MODE,RG_PXP_CDR_PR_INJ_MODE.dat.value);
		
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_c_en = 0;	
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_c_en = 0;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_da_pxp_cdr_pr_lpf_r_en = 1;
		rg_force_da_pxp_cdr_pr_lpf_c_en.hal.rg_force_sel_da_pxp_cdr_pr_lpf_r_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_lpf_c_en,rg_force_da_pxp_cdr_pr_lpf_c_en.dat.value);
			
		rg_force_da_pxp_cdr_pr_idac.hal.rg_force_sel_da_pxp_cdr_pr_idac = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_idac,rg_force_da_pxp_cdr_pr_idac.dat.value);
		
		SS_RX_FLL_b.hal.rg_load_en = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_b,SS_RX_FLL_b.dat.value);
		SS_RX_FLL_1.hal.rg_ipath_idac = cdr_pr_idac_tmp;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SS_RX_FLL_1,SS_RX_FLL_1.dat.value);
			
		
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 0;	
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_da_pxp_cdr_pr_pwdb = 1;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);	
		rg_force_da_pxp_cdr_pr_pieye_pwdb.hal.rg_force_sel_da_pxp_cdr_pr_pwdb = 0;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _rg_force_da_pxp_cdr_pr_pieye_pwdb,rg_force_da_pxp_cdr_pr_pieye_pwdb.dat.value);
		
		SW_RST_SET.hal.rg_sw_ref_rst_n = RX_PRCal_REF_RESETB_HI_EN;
		RG_W_PL(0x1fa8b000,PMA_BASE_OFFSET,(u32) _SW_RST_SET,SW_RST_SET.dat.value);
		//return RO_state_freqdet;
		//RX_PRCal_end--------------------------------
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'RSTB sel8, val 0\r\n'
		write_reg_word(0x1fa8b460,0x20);
		udelay(100);
		//b'RSTB sel8, val 1\r\n'
		write_reg_word(0x1fa8b460,0xfff);
		udelay(100);
		//b'RX_CDR_RST\r\n'
		//b'RX_CDR_LFP_L2D mode 1, sel 0\r\n'
		write_reg_word(0x1fa8b818,0x10100);
		//b'RX_CDR_LPF_RSTB mode 1, sel0\r\n'
		write_reg_word(0x1fa8b818,0x1000100);
		udelay(700);
		//b'RX_CDR_LPF_RSTB mode 1, sel1\r\n'
		write_reg_word(0x1fa8b818,0x1010100);
		udelay(100);
		//b'RX_CDR_LFP_L2D mode 1, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x1010101);
		//b'RX_CDR_LPF_RSTB mode 0, sel1\r\n'
		write_reg_word(0x1fa8b818,0x10101);
		//b'RX_CDR_LFP_L2D mode 0, sel 1\r\n'
		write_reg_word(0x1fa8b818,0x10001);
		//b'xSGMII_Solution : 0\r\n'
		//b'xsgmii_init\r\n'
		//b'usxgmii_init 1\r\n'
		write_reg_word(0x1fa84100,0x10010001);
		//b'PCS E0/E1/E3 SWR solution\r\n'
		//b'HIBER_OFF\r\n'
		write_reg_word(0x1fa85900,0x40);
		write_reg_word(0x1fa85b2c,0x0);
		//b'Int init\r\n'
		//b'usxgmii_pcs_int en 0\r\n'
		write_reg_word(0x1fa85bc0,0x0);
		write_reg_word(0x1fa85bc4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);
		write_reg_word(0x1fa85bd8,0x0);
		write_reg_word(0x1fa85bdc,0x0);
		write_reg_word(0x1fa85be4,0x0);
		write_reg_word(0x1fa85bc8,0x0);
		write_reg_word(0x1fa85bcc,0x0);
		write_reg_word(0x1fa85be0,0x0);
		//b'xsgmii 0 int enable 0\r\n'
		//b'AN mode\r\n'
		printf("AN mode\n");
		//b'xSGMII_AN_API 0, enable : 1\r\n'
		//b'usxgmii_an\r\n'
		write_reg_word(0x1fa85bf8,0x6330001);
		//b'_rg_usxgmii_an_control_0 6330001\r\n'
		//b'_rg_usxgmii_an_control_1 0\r\n'
		//b'xSGMII_AN_AutoSetting\r\n'
		write_reg_word(0x1fa86000,0xc11);
		printf("PON USXGMII_10G exit\n");

	}
	return ;
#endif
	return ;
}



