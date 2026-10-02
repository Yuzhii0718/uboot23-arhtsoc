#ifndef _ECNT_FLASH_H
#define _ECNT_FLASH_H

#include <linux/ctype.h>
#include <mmc.h>
#include <part.h>

#define SLAVE_PARTITION_INFO_MAGIC 0x534C4156
#define KERNEL_LOAD_ADDR 0x80088000
typedef enum {
    CAL_RESERVE = 0,
    CAL_WIFI_6G,
    CAL_BOB,
    CAL_WIFI_24G,
    CAL_WIFI_5G,
    CAL_PROLINECMD,
    CAL_BOOTFLAG,
    CAL_SLAVE_INFO,
    CAL_AREA_NUM
} CALIBRATION_LAYOUT;

typedef struct
{
	unsigned int magic;
	unsigned int offset;
	unsigned int crc;
} slave_partition_info;

bool is_emmc(void);
bool is_nor(void);
bool is_parallel(void);
struct mmc *__init_mmc_device(int dev, bool force_init, enum bus_mode speed_mode);
int mmc_partitions_parse(struct disk_partition *partinfo, char *partName);
int mtd_bootflag_read(int *boot);
int mtd_bootflag_write(u_char *buf);
int mtd_write_art(CALIBRATION_LAYOUT cal, void* addr, size_t size);
int mtd_read_art(CALIBRATION_LAYOUT cal, void* addr, size_t size);
int mmc_bootflag_write(struct mmc *mmcptr, u_char *bootflag);
int mmc_bootflag_read(struct mmc *mmcptr, int *bootflag);
int tpl_bootflag_write(int tplboot);
int tpl_bootflag_read(int* tplboot);
#endif