#include <common.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <mmc.h>

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

int mmc_partitions_parse(struct disk_partition *partinfo, char *partName)
{
	int i = 0;
	struct blk_desc *blk_dev_desc = NULL;

	if ((blk_dev_desc = blk_get_dev("mmc", 0)) == NULL) 
	{
		printf("%s: mmc dev 0 NOT available\n",__func__);
		return -1;
	}

	for (i = 1; i <= MAX_SEARCH_PARTITIONS; i++) 
	{
		part_get_info(blk_dev_desc, i, partinfo);
		if((!strncmp(partinfo->name, partName, strlen(partName))) && 
			(partinfo->name[strlen(partName)] == '\0'))
			return 0;
	}

	return -1;
}