#ifndef _ECNT_FLASH_H
#define _ECNT_FLASH_H

#include <linux/ctype.h>

bool is_emmc(void);
bool is_nor(void);
bool is_parallel(void);
struct mmc *__init_mmc_device(int dev, bool force_init, enum bus_mode speed_mode);
int mmc_partitions_parse(struct disk_partition *partinfo, char *partName);

#endif