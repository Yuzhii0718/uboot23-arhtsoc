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
#include <mmc.h>
#include <ecnt_flash.h>
#include <ecnt_image.h>


int readBootFlagFromFlash(void)
{
	struct mmc *mmc = NULL;
	int boot = -1;
	
	if(is_emmc())
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if(mmc_bootflag_read(mmc, &boot))
			return CMD_RET_FAILURE;
	}
	else
	{
		if(mtd_bootflag_read(&boot))
			return CMD_RET_FAILURE;
	}

	printf("current bootflag=%d\n", boot);
	return CMD_RET_SUCCESS;
}


int swap_bootflag(void)
{
	int boot = -1; //bootswap = -1;
	struct mmc *mmc = NULL;
	u_char bootbuf[512] = {0};
	
	if(is_emmc())
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if(mmc_bootflag_read(mmc, &boot))
			return CMD_RET_FAILURE;

		bootbuf[0] = (boot == 0)? '1': '0';
		if(mmc_bootflag_write(mmc, bootbuf))
			return CMD_RET_FAILURE;
	}
	else
	{
		if(mtd_bootflag_read(&boot))
			return CMD_RET_FAILURE;

		bootbuf[0] = (boot == 0)? '1': '0';
		if(mtd_bootflag_write(bootbuf))
			return CMD_RET_FAILURE;
	}

	printf("current bootflag=%d after swap\n", (bootbuf[0] - '0'));
	return CMD_RET_SUCCESS;
}


static int bootflag_command(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = CMD_RET_USAGE;
	
	if (argc < 2)
	    return CMD_RET_USAGE;

	if(!strncmp(argv[1], "read", 4)) 		
		ret = readBootFlagFromFlash();
	else if(!strncmp(argv[1], "swap", 4)) 		
		ret =swap_bootflag();
	else 		
		ret = CMD_RET_USAGE;

    return ret;
}



U_BOOT_CMD(
		bootflag,   2,      0,      bootflag_command,
		"bootflag read/swap/ command\n",
		"usage:\n"
		"	bootflag read\n"
		"	bootflag swap\n"
);

