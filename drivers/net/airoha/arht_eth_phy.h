// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#ifndef __ARHT_ETH_PHY_H
#define __ARHT_ETH_PHY_H
//sync kernel sys serdes order, ref. arht_serdes_cfg.h
#define UBOOT_DEBUG_LOG     0


#ifdef TCSUPPORT_CPU_AN7583
#define SERDES_ETH_XFI_MODE        '0'
#define SERDES_ETH_USXGMII_MODE    '1'
#define SERDES_ETH_HSGMII_MODE     '2'
#define SERDES_ETH_5GBaseR_MODE    '3' //no setting 20230706
#define SERDES_ETH_SGMII_MODE      '6'

#define SERDES_P1_SGMII_MODE      '2'
#define SERDES_U0_SGMII_MODE      '1'

#define SERDES_P0_USXGMII 	      '3'
#define SERDES_P0_XFI 	          '4'

#define SERDES_PON_XFI_MODE         '1'
#define SERDES_PON_USXGMII_MODE     '2'
#define SERDES_PON_HSGMII_MODE      '3'
#define SERDES_PON_SGMII_MODE       '5'
#define SERDES_PON_DSL_PHY          '7'

#else

#ifdef TCSUPPORT_CPU_AN7552	
#define SERDES_HSGMII_MODE     '1'
#else
#define SERDES_ETH_XFI_MODE        '0'
#define SERDES_ETH_USXGMII_MODE    '1'
#define SERDES_ETH_HSGMII_MODE     '2'
#define SERDES_ETH_5GBaseR_MODE    '3' //no setting 20230706
#define SERDES_ETH_SGMII_MODE      '4'

#define SERDES_WIFI2_PCIE0_MODE 	'0'
#define SERDES_WIFI2_UPCIE1_MODE 	'1'
#define SERDES_WIFI2_HSGMII_MODE 	'2'
#define SERDES_WIFI2_USXGMII_MODE 	'3'
#define SERDES_WIFI2_XFI_MODE 		'4'

#define SERDES_PON_HSGMII_MODE		'1'
#define SERDES_PON_XFI_MODE			'2'
#define SERDES_PON_USXGMII_MODE		'3'
#endif
#endif

void serdes_phy_init(void);

#endif

