// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 AIROHA Inc
 * Author: Shubham Jain <shubham.jain@airoha.com>
		   Zhengping Zhang <zhengping.zhang@airoha.com>
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <common.h>
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
#include <asm/armv8/mmu.h>
#include <asm/cache.h>
#include <asm/tc3162.h>

int print_cpuinfo(void)
{ 
	printf("CPU:   Airoha AN7583\n");
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

static struct mm_region an7581_mem_map[] = {
	{
		/* DDR */
		.virt = 0x80000000UL,
		.phys = 0x80000000UL,
		.size = 0x80000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_OUTER_SHARE,
	}, {
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x20000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		0,
	}
};
struct mm_region *mem_map = an7581_mem_map;
