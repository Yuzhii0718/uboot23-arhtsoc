/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 

#ifndef __EN7523_H
#define __EN7523_H

#include <linux/sizes.h>

/* Miscellaneous configurable options */
#define TCSUPPORT_CPU_EN7523 1
#define TCSUPPORT_CPU_EN7580 1
#define TCSUPPORT_UBOOT_64BIT 1
#define TC3262 1
#define TCSUPPORT_CPU_ARMV8_64
#define TCSUPPORT_CPU_ARMV8
#define TCSUPPORT_MT7510_FE
#define TCSUPPORT_MT7530_SWITCH_API
#define TCSUPPORT_LITTLE_ENDIAN
/* Environment */

/* Defines for SPL */


#define CONFIG_SPI_ADDR			0x30000000
#define CONFIG_SYS_UBOOT_BASE		CONFIG_TEXT_BASE

#define CFG_SYS_INIT_RAM_ADDR CONFIG_TEXT_BASE
#define CFG_SYS_INIT_RAM_SIZE SZ_2M
// #define CONFIG_CUSTOM_SYS_INIT_SP_ADDR (CONFIG_TEXT_BASE + SZ_2M - GENERATED_GBL_DATA_SIZE)   
/* SPL -> Uboot */

/* UBoot -> Kernel */

/* DRAM */
#define CONFIG_SYS_SDRAM_BASE		0x80000000

/* Ethernet */

#define CONFIG_IPADDR			192.168.1.1
#define CONFIG_SERVERIP			192.168.1.2
#define TCBOOT_OFFSET			0x0
#define TFTP_LOAD_ADDR			0x81800000
#define TCLINUX_OFFSET	  		0x0
#endif
