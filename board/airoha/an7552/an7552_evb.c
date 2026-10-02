/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <common.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/tc3162.h>

#ifdef CONFIG_OPEN_IMAGE
#include "bootlib.h"
#endif /* CONFIG_OPEN_IMAGE */

/* Register base address */
#define IO_PHYS				(0x10000000)

#if defined(TCSUPPORT_CPU_EN7581) || defined(TCSUPPORT_CPU_AN7583) || defined(TCSUPPORT_CPU_AN7552)
#define IOMUX_1		(IO_PHYS + 0xFA20214)
#else
#define IOMUX_1		(IO_PHYS + 0xFA20210)
#endif
DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	/* enable HW switch LED0 */
	unsigned int val;
	val = readl(IOMUX_1);
	val |= (0x1 << 3) | (0x1 << 5) | (0x1 << 7) | (0x1 << 9);
	writel(val, IOMUX_1);

	return 0;
}
