// SPDX-License-Identifier: GPL-2.0-only 
/*
 * Copyright (c) 2024 AIROHA Inc
 *
 *	Support utility commands for Airoha Flash
 *
 *	Author: Mohd Nomaan <mohd.nomaan@airoha.com>
 *
 ****************************************************************
 *	Commands supported now:
 *
 *	1)spinand forceeraseall 
 *		- Erases whole flash (including BMT/BBT region)
 *
 *****************************************************************
 */

#include <common.h>
#include <command.h>
#include <dm/device.h>
#include <linux/mtd/mtd.h>
#include <ecnt_flash.h>
#include <ecnt_image.h>
#include <linux/mtd/mtk_bmt.h>

extern struct bmt_desc bmtd;

extern int spi_nand_mark_bad_block(u32 offset);
extern int spi_nand_bad_block_info(void);
extern int spi_nand_recover_bad_block(u32 bad_offset);
int spinand_forceeraseall(void)
{
	struct mtd_info *mtd;
	struct erase_info erase_op ={};

	if(!is_emmc()){
		mtd = get_mtd_device_nm("spi-nand0");
		
		if(IS_ERR(mtd)){
			printf("Failed to get SPI-NAND device\n");
			return CMD_RET_FAILURE;
		}

		erase_op.mtd = mtd;
		erase_op.addr = 0;
		erase_op.len = bmtd.total_blks << bmtd.blk_shift;
		if(bmtd._erase(mtd, &erase_op)){
			printf("erase failed\n");
			return CMD_RET_FAILURE;
		}

		printf("SPI-NAND device erased successfully\n");
		printf("Please using emergency update to bring up\n");
	}
	else {
		printf("This command only support spi-nand, please use the mmc command instead\n");
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int spinand_command(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
    ulong addr, offset;
	char *cmd = argv[1];

	if (argc <2)
		return CMD_RET_USAGE;

    if (!strcmp(argv[1], "forceeraseall")) {
		ret = spinand_forceeraseall();
    } 
	else if (strcmp(cmd, "showbad") == 0) {
		printf("\nDevice 0 bad blocks:\n");
		spi_nand_bad_block_info();
		return 0;
	}
	else if (strcmp(cmd, "markbad") == 0) {
		argc -= 2;
		argv += 2;
		if (argc <= 0)
			goto usage;
		while (argc > 0) {
			addr = simple_strtoul(*argv, NULL, 16);
			if (spi_nand_mark_bad_block(addr)) {
				printf("block 0x%x NOT marked " "as bad! ERROR %d\n", (int)addr, ret);
				ret = 1;
			} else {
				printf("block 0x%x successfully " "marked as bad\n", (int)addr);
			}
			--argc;
			++argv;
		}
	return ret;
	}
	else if (strncmp(cmd, "scrub", 5) == 0) {
		int scrub_yes = argc > 2 && !strcmp("-y", argv[2]);
		int o = scrub_yes ? 3 : 2;
		int scrub = !strncmp(cmd, "scrub", 5);
		int args = 2;
		/*
		* Don't allow missing arguments to cause full chip/partition
		* erases -- easy to do accidentally, e.g. with a misspelled
		* variable name.
		*/
		if (argc != o + args)
			goto usage;
		offset = (int)simple_strtoull(argv[3], NULL, 16);
		if (spi_nand_recover_bad_block(offset))
			ret = 0;
		else
			ret = 1;
		printf("\nNAND %s: ", cmd);
		printf("%s\n", ret ? "ERROR" : "OK");
		return ret == 0 ? 0 : 1;
	}
usage:
	return CMD_RET_USAGE;
}

U_BOOT_CMD(
	spinand, 5,	0,	spinand_command,
	"spinand - spinand command\n",
	"spinand usage:\n"
	"	spinand forceeraseall\n"
	"	spinand showbad\n"
	"	spinand markbad <addr>\n"
	"   spinand scrub [-y] off size\n"
);
