// SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note 
/*
 * Copyright (c) 2024 AIROHA Inc
*/

#ifndef __UAPI_ECNT_GLOBAL_MACRO_H_
#define __UAPI_ECNT_GLOBAL_MACRO_H_

#define NO_OPERATION  	0
#define NO_HEADER		1
#define HTML_HEADER		2

#define MAX_ARG_NUM		3
#define MAX_NODE_NAME	32

#if/*TCSUPPORT_COMPILE*/ defined(TCSUPPORT_CMCC)
/* USE MAX_WAN_PVC_NUM/MAX_WAN_ENTRY_NUM, DO NOT USE it later */
#define PVC_NUM 	1
#else/*TCSUPPORT_COMPILE*/
/*Wan related*/
#ifndef PURE_BRIDGE
	#define PVC_NUM 8
#else
	#define PVC_NUM 4
#endif
#endif/*TCSUPPORT_COMPILE*/

#if defined(TCSUPPORT_CMCC)
#define MAX_WAN_PVC_NUM				1
#else
#define MAX_WAN_PVC_NUM				8
#endif
#define MAX_WAN_ENTRY_NUM			8
#define MAX_WAN_INTERFACE_NUM		(MAX_WAN_PVC_NUM*MAX_WAN_ENTRY_NUM)


#define MAX_SERVICE_NUM 8
/* USE MAX_WAN_PVC_NUM/MAX_WAN_ENTRY_NUM, DO NOT USE it later */
#define MAX_SMUX_NUM 8
#define MAX_WAN_IF_INDEX  (PVC_NUM*MAX_SMUX_NUM)
/* END. */


#define WAN_ENCAP_DYN_INT		0
#define WAN_ENCAP_STATIC_INT	1
#define WAN_ENCAP_PPP_INT		2
#define WAN_ENCAP_BRIDGE_INT	3
#define WAN_ENCAP_NONE_INT		4
#define MAX_LANHOST2_ENTRY_NUM	68
#define MAX_SUBDEVICEINFO_ENTRY_NUM 32
#if defined(TCSUPPORT_CUC)
#define CUC_MAX_SSID_NUM		4
#define CUC_MAX_SSIDAC_NUM		4
#endif
#define ECNT_UID				668
#define ECNT_GID				ECNT_UID
#endif/* __ECNT_GLOBAL_MACRO_H_ */

