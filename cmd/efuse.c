// SPDX-License-Identifier: GPL-2.0+
/*
 * Implements the 'bd' command to show board information
 *
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <env.h>
#include <lmb.h>
#include <net.h>
#include <video.h>
#include <vsprintf.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <display_options.h>
#include <linux/arm-smccc.h>


static long efuse_command(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned long r0 = 0, r1 = 0, r2 = 0, r3 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	if (argc < 4)
		return CMD_RET_USAGE;

	r0 = 0x82000001;
	r1 = *((unsigned int *) argv[1]);
	r2 = simple_strtoul(argv[2], NULL, 16);
	r3 = simple_strtoul(argv[3], NULL, 16);

#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	return res.a0;
#else
	do_smc(r0, r1, r2, r3);
	return 0;
#endif
}

U_BOOT_CMD(
		efuse,   4,      0,      efuse_command,
		"efuse - efuse command\n",
		"efuse usage:\n"
		"	efuse CMD ARG1 ARG2\n"
);










static long efuse_pvt_command(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	unsigned long r0 = 0, r1 = 0, r2 = 0, r3 = 0;
	unsigned long test_item;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	
	test_item = *((unsigned int *) argv[1]);
	
	if(test_item == 0x6e7572){ /* 'run' in hex */
		r0 = 0x82000001;
		r1 = 0x59454B53;
		r2 = 0x81800000;
		r3 = 0xa0;
	}else if (test_item == 0x6b6863){ /* 'chk' in hex */
		r0 = 0x82000001;
		r1 = 0x59454B43;
		r2 = 0;
		r3 = 0;
	}else{
		printf("  unsupport %lu \n", test_item);
	}
	


#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	printf("  return with %lu \n", res.a0);
	
	/* check the return value here */

	
	return res.a0;
#else
	do_smc(r0, r1, r2, r3);
	return 0;
#endif



}

U_BOOT_CMD(
		efuse_pvt,   2,      0,      efuse_pvt_command,
		"efuse_pvt - efuse_pvt command\n",
		"efuse_pvt usage:\n"
		"	efuse_pvt CMD ARG1 ARG2\n"
);

