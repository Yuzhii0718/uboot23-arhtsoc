#include <common.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <mmc.h>
#include <ecnt_flash.h>
#include <linux/mtd/mtd.h>
#include <mtd.h>

int size_art[] = 
{
	0x40000,	/*ART_RESERVE*/
	0x200000,	/*WIFI_6G_SIZE*/
	0x40000,	/*first 20000:for 18.06 2.4G(5G) cali, second 20000 for BOB_SIZE*/
	0x40000,	/*WIFI_24G_SIZE*/
	0x40000,	/*WIFI_5G_SIZE*/
	0x40000,	/*PROLINECMD_SIZE*/
	0x20000, 	/*BOOTFLAG_SIZE*/
	0x20000 	/*SLAVE_INFO_SIZE*/
};

bool is_emmc(void) {
	u32 val = readl(0x1fb000b8);
	return (val & ( 0x01 << 6));
}

bool is_nor(void) {
	u32 val = readl(0x1fa10114);
	return !(val & 0x02);
}

bool is_parallel(void) {
	u32 val = readl(0x1fa1155c);
	return (val & 0x04) == 0;
}

struct mmc *__init_mmc_device(int dev, bool force_init,
				     enum bus_mode speed_mode)
{
	struct mmc *mmc;
	mmc = find_mmc_device(dev);
	if (!mmc) {
		printf("no mmc device at slot %x\n", dev);
		return NULL;
	}

	if (!mmc_getcd(mmc))
		force_init = true;

	if (force_init)
		mmc->has_init = 0;

	if (IS_ENABLED(CONFIG_MMC_SPEED_MODE_SET))
		mmc->user_speed_mode = speed_mode;

	if (mmc_init(mmc))
		return NULL;

#ifdef CONFIG_BLOCK_CACHE
	struct blk_desc *bd = mmc_get_blk_desc(mmc);
	blkcache_invalidate(bd->uclass_id, bd->devnum);
#endif

	return mmc;
}

int setup_mtd_device(struct mtd_info **mtd, const char* mtd_dev)
{
	struct mtd_info *mtd_info;

	mtd_probe_devices();

	mtd_info = get_mtd_device_nm(mtd_dev);
	if (IS_ERR_OR_NULL(mtd_info)) {
		printf("MTD device %s not found, ret %ld\n", mtd_dev, PTR_ERR(mtd_info));
		return -1;
	}
	*mtd = mtd_info;

	return 0;
}

int mmc_partitions_parse(struct disk_partition *partinfo, char *partName)
{
	int i = 0;
	struct blk_desc *blk_dev_desc = NULL;

	if((blk_dev_desc = blk_get_dev("mmc", 0)) == NULL) 
	{
		printf("%s: mmc dev 0 NOT available\n",__func__);
		return -1;
	}

	for(i = 1; i <= MAX_SEARCH_PARTITIONS; i++) 
	{
		if(part_get_info(blk_dev_desc, i, partinfo))
		{
			printf("find partition %s failed\n",partName);
			return -1;
		}
		
		if((!strncmp(partinfo->name, partName, strlen(partName))) && 
			(partinfo->name[strlen(partName)] == '\0'))
			return 0;
	}

	return -1;
}

int mtd_write_art(CALIBRATION_LAYOUT cal, void* addr, size_t size)
{
	int i = 0;
	ulong retLen = 0, startAddr = 0;
	struct mtd_info *mtd_art = NULL;
	struct erase_info er;

	memset(&er, 0, sizeof(er));
	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("%s: Failed to get %s partition\n", __func__, CONFIG_SYS_ART_PARTITION_NAME);
		return -1;
	}

	while(i < cal)
		startAddr += size_art[i++];

	debug("%s: startAddr = 0x%lx, i = %d, erase size = 0x%x, write size = %zu\n", __func__, startAddr, i, mtd_art->erasesize, size);
	
	er.mtd = mtd_art;
	er.addr = startAddr;
	er.len = mtd_art->erasesize;
	if(mtd_erase(mtd_art, &er))
	{
		printf("%s: Failed to erase 0x%x size\n", __func__, mtd_art->erasesize);
		return -1;
	}
	if(mtd_write(mtd_art, startAddr, size, &retLen, (uchar*)addr))
	{
		printf("%s: Failed to write %zu bytes\n", __func__, size);
		return -1;
	}

	return 0;	
}

int mtd_read_art(CALIBRATION_LAYOUT cal, void* addr, size_t size)
{
	int i = 0;
	ulong retLen = 0, startAddr = 0;
	struct mtd_info *mtd_art = NULL;

	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("%s: Failed to get %s partition\n", __func__, CONFIG_SYS_ART_PARTITION_NAME);
		return -1;
	}
	
	while(i < cal)
		startAddr += size_art[i++];

	debug("%s: startAddr = 0x%lx, i = %d, read size = %zu\n", __func__, startAddr, i, size);

	if(mtd_read(mtd_art, startAddr, size, &retLen, (uchar*)addr)) 
	{
		printf("%s: Failed to read %zu bytes\n", __func__, size);
		return -1;
	}

	return 0;
}

int mtd_bootflag_write(u_char *buf)
{
	ulong retLen = 0, startAddr = 0;
	struct mtd_info *mtd_art = NULL;
	struct erase_info er;
#ifndef TCSUPPORT_CPU_EN7523
	ulong i = 0;
	CALIBRATION_LAYOUT cal = CAL_BOOTFLAG;
#endif


#ifdef TCSUPPORT_CF
	struct mtd_info *mtd_bootloader = NULL;
#endif
	memset(&er, 0, sizeof(er));
	
#ifdef TCSUPPORT_CF	
	if(setup_mtd_device(&mtd_bootloader, CONFIG_SYS_UBOOT_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition bootloader!\n");
		return -1;
	}
	startAddr = 0x80000;
	
	er.mtd = mtd_bootloader;
	er.addr = startAddr;
	er.len = mtd_bootloader->erasesize;
	if(mtd_erase(mtd_bootloader, &er))
		return -1;
	
	if(mtd_write(mtd_bootloader, startAddr, 1, &retLen, (uchar*)buf))
	{
		printf("mtd bootflag write failed\n");
		return -1;
	}
#else
	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition (art)!\n");
		return -1;
	}

#ifdef TCSUPPORT_CPU_EN7523
	startAddr = mtd_art->size - 0x40000;
#else	
	while(i < cal)
		startAddr += size_art[i++];
#endif
	
	er.mtd = mtd_art;
	er.addr = startAddr;
	er.len = mtd_art->erasesize;
	if(mtd_erase(mtd_art, &er))
		return -1;
	
	if(mtd_write(mtd_art, startAddr, 1, &retLen, (uchar*)buf))
	{
		printf("mtd bootflag write failed\n");
		return -1;
	}
#endif
	return 0;	
}

int mtd_bootflag_read(int *boot)
{
	ulong retLen = 0, startAddr = 0;
	uchar butbuf[1] = {0};
	struct mtd_info *mtd_art = NULL;
#ifndef TCSUPPORT_CPU_EN7523
	ulong i = 0;
	CALIBRATION_LAYOUT cal = CAL_BOOTFLAG;
#endif


#ifdef TCSUPPORT_CF

	
	struct mtd_info *mtd_bootloader = NULL;
	if(setup_mtd_device(&mtd_bootloader, CONFIG_SYS_UBOOT_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition bootloader!\n");
		return -1;
	}
	startAddr = 0x80000;
	
	if(mtd_read(mtd_bootloader, startAddr, sizeof(butbuf), &retLen, (uchar*)butbuf)) 
	{
		printf("mtd bootflag read failed\n");
		return -1;
	}
#else
	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition (art)!\n");
		return -1;
	}
	
#ifdef TCSUPPORT_CPU_EN7523
	startAddr = mtd_art->size - 0x40000;
#else
	while(i < cal)
		startAddr += size_art[i++];
#endif

	if(mtd_read(mtd_art, startAddr, sizeof(butbuf), &retLen, (uchar*)butbuf)) 
	{
		printf("mtd bootflag read failed\n");
		return -1;
	}
#endif
	if(butbuf[0] == '1')
		*boot = 1;
	else
		*boot = 0;

	return 0; 
}

int mmc_bootflag_write(struct mmc *mmcptr, u_char *bootflag)
{
	u32 blk = 0, n = 0, i = 0;
	struct disk_partition art;
	CALIBRATION_LAYOUT cal = CAL_BOOTFLAG;

	memset(&art, 0, sizeof(art));
	if(mmc_partitions_parse(&art, "art") != 0)
		return -1;
	art.start *= art.blksz;
	while(i < cal)
		art.start += size_art[i++];

	blk = art.start / mmcptr->read_bl_len;
	n = blk_dwrite(mmc_get_blk_desc(mmcptr), blk, 1, bootflag);
	if(n != 1)
	{
		printf("MMC bootflag write failed\n");
		return -1;
	}
	
	return 0;
}

int mmc_bootflag_read(struct mmc *mmcptr, int *bootflag)
{
	u32 blk = 0, n = 0, i = 0;
	char buf[512] = {0};
	struct disk_partition art;
	CALIBRATION_LAYOUT cal = CAL_BOOTFLAG;

	memset(&art, 0, sizeof(art));
	if(mmc_partitions_parse(&art, "art") != 0)
		return -1;
	art.start *= art.blksz;
	while(i < cal)
		art.start += size_art[i++];

	blk = art.start / mmcptr->read_bl_len;
	n = blk_dread(mmc_get_blk_desc(mmcptr), blk, 1, buf);

	if(buf[0] == '1')
		*bootflag = 1;
	else
		*bootflag = 0;
	
	return 0;
}

int tpl_bootflag_write(int tplboot)
{
	int boot = -1;
	struct mmc *mmc = NULL;
	u_char bootbuf[512] = {0};

	if(tplboot != 0 && tplboot != 1)
		return -1;

	if(is_emmc())
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		mmc_bootflag_read(mmc, &boot);
		if(boot != tplboot)
		{
			bootbuf[0] = (tplboot == 0)? '0': '1';
			if(mmc_bootflag_write(mmc, bootbuf))
				return -1;
		}
	}
	else
	{
		mtd_bootflag_read(&boot);
		if(boot != tplboot)
		{
			bootbuf[0] = (tplboot == 0)? '0': '1';
			if(mtd_bootflag_write(bootbuf))
				return -1;
		}
	}

	return 0;
}

int tpl_bootflag_read(int* tplboot)
{
	int boot = -1;
	struct mmc *mmc = NULL;

	if(is_emmc())
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if(mmc_bootflag_read(mmc, &boot))
			return -1;
	}
	else
	{
		if(mtd_bootflag_read(&boot))
			return -1;
	}

	if(boot != 0 && boot != 1)
	{
		printf("bootflag read failed, bootflag=[%d]\n",boot);
		return -1;
	}

	*tplboot = boot;
	return 0;
}

