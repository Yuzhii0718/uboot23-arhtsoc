/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 AIROHA Inc
 *
 * Configuration for Airoha AN7581
 *
 * Copyright (C) 2023 Airoha Technology Corp.
 * Author: Shubham Jain <shubham.jain@airoha.com>
 */

#ifndef __AN7581_H
#define __AN7581_H

#include <linux/sizes.h>

/* options to enable the legacy driver support in u-boot-23 */ 
#define CONFIG_RAMFS_BASE 0x8B000000 // u-boot-2014.04-rc1/include/configs/an7581_evb.h
#define CONFIG_ECNT_UBOOT
#define TCSUPPORT_NAND_BMT
#define TCSUPPORT_NEW_SPIFLASH
#define TCSUPPORT_CPU_EN7512

/* Miscellaneous configurable options */

#define TCSUPPORT_CPU_EN7581 1
#define TCSUPPORT_CPU_EN7523 1
#define TCSUPPORT_CPU_EN7580 1
#define TCSUPPORT_UBOOT_64BIT 1
#define TC3262 1
#define TCSUPPORT_CPU_ARMV8_64
#define TCSUPPORT_CPU_ARMV8
#define TCSUPPORT_MT7510_FE
#define TCSUPPORT_MT7530_SWITCH_API
#define TCSUPPORT_LITTLE_ENDIAN

#ifdef CONFIG_PHYLIB
#undef CONFIG_PHYLIB
#endif

#define CONFIG_PHYLIB
#define CONFIG_CMD_MII
#define CONFIG_PHY_AIROHA_EN8811H

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
#define CONFIG_SERVERIP			192.168.1.16
#define TCBOOT_OFFSET			0x0
#define TFTP_LOAD_ADDR			0x81800000

/* GPT Entries Offset */
#ifdef CONFIG_EFI_PARTITION_ENTRIES_OFF
#undef CONFIG_EFI_PARTITION_ENTRIES_OFF
#endif

#ifdef TCSUPPORT_TCBOOT_1MB_SIZE
#define CONFIG_EFI_PARTITION_ENTRIES_OFF 0x100000
#else
#define CONFIG_EFI_PARTITION_ENTRIES_OFF 0x80000
#endif
#endif
