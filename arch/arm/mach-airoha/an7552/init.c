// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 AIROHA Inc
 */
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <ram.h>
#include <asm/arch/misc.h>
#include <asm/global_data.h>
#include <asm/sections.h>
#include <dm/uclass.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <asm/tc3162.h>

#define L2_CFG_BASE		0x10200000
#define L2_CFG_SIZE		0x1000
#define L2_SHARE_CFG_MP0	0x7f0
#define L2_SHARE_MODE_OFF	BIT(8)
DECLARE_GLOBAL_DATA_PTR;
int arht_soc_early_init(void)
{
	struct udevice *dev;
	int ret;
	ret = uclass_first_device_err(UCLASS_RAM, &dev);
	if (ret)
		return ret;
	return 0;
}
int dram_init(void)
{
	uint64_t size = 0;

	size = (uint64_t)GET_DRAM_SIZE * (uint64_t)SZ_1M;

	/* Uboot only support 32bit dram size (2GB)*/ 
	if (size > SZ_2G)
		size = SZ_2G;
	gd->ram_size = size;
	gd->ram_base = 0x80000000;
	return 0;
}

void reset_cpu(void)
{
	writel(0x80000000, 0x1FB00040);
	while(1);
}

int print_cpuinfo(void)
{
	printf("CPU:   Airoha AN7552\n");
	return 0;
}
