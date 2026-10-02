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
	int ret;
	ret = fdtdec_setup_memory_banksize();
	if (ret)
		return ret;
	return fdtdec_setup_mem_size_base();
}

void reset_cpu(void)
{
	writel(0x80000000, 0x1FB00040);
	while(1);
}

int print_cpuinfo(void)
{
	printf("CPU:   Airoha EN7523\n");
	return 0;
}
