/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 

#include <common.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/tc3162.h>

/* Register base address */
#define IO_PHYS				(0x10000000)
#if defined(TCSUPPORT_CPU_EN7581) || defined(TCSUPPORT_CPU_AN7552)
#define IOMUX_1		(IO_PHYS + 0xFA20214)
#else
#define IOMUX_1		(IO_PHYS + 0xFA20210)
#endif

DECLARE_GLOBAL_DATA_PTR;
static char *an7583_chip_name[] =
{
	"AN7583GT",
	"AN7583GIT",
	"AN7583CT",
	"AN7583DT",
	"Unknow Chip",
	"AN7583ST",
	"AN9510GT",
	"Unknow Chip",
	"AN7553GT",
	"AN7553CT",
	"AN7567GT",
	"AN7567CT",
	"AN7583ET",
	"AN7583EIT",
	"Unknow Chip",
	"Unknow Chip",
	"AN7583FG",
	"Unknow Chip",
	"AN7583FP",
	"AN7583FD",
	"Unknow Chip",
	"AN7583FS",
	"AN7583FF",
};

int board_init(void)
{
	char *name = "Unknow Chip.\n";
	if(isAN7583){		
		name = an7583_chip_name[GET_PACKAGE_ID];
	}
	printf("%s\n", name);
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

	/* enable HW switch LED0 */
	unsigned int val;
	val = readl(IOMUX_1);
	val |= (0x1 << 3) | (0x1 << 5) | (0x1 << 7) | (0x1 << 9);
	writel(val, IOMUX_1);

	return 0;
}
