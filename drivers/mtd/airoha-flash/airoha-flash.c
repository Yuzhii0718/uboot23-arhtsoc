// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 AIROHA Inc
 */

#include <common.h>
#include <command.h>
#include <malloc.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include "spi/spi_nand_flash.h"

extern int flash_init(unsigned long rom_base);
extern int flash_read(unsigned long from,
	unsigned long len, unsigned long *retlen, unsigned char *buf);
extern int flash_write(unsigned long to,
	unsigned long len, unsigned long *retlen, const unsigned char *buf);
extern int flash_erase(unsigned long addr, unsigned long size);

static int airoha_flash_probe(struct udevice *dev); 

// TODO: 
// static int do_airoha_flash(struct cmd_tbl *cmdtp, int flag, int argc,
// 		   char *const argv[])
// {
// 
// 	if ( strcmp("forceeraseall",  argv[1]) ) {
// 		struct SPI_NAND_FLASH_INFO_T	flash_info_t;
// 		/* copy pasta from uboot-14 */
// 		printf("force erase all flash\n");
// 
// 		SPI_NAND_Flash_Get_Flash_Info(&flash_info_t);
// 		for(int addr = 0; addr < (flash_info_t.device_size / flash_info_t.erase_size); addr++) {
// 			spi_nand_erase_block(addr);
// 		}
// 	}
// 
// }
// 
// 
// 
// U_BOOT_CMD(
// 	airoha_flash, CONFIG_SYS_MAXARGS, 1, do_airoha_flash,
// 	"airoha flash commands",
// 	"[forceeraseall] \n"
// 	"forceeraseall - delete all flash blocks"
// );
// 




static struct mtd_info airoha_flash_mtd_info;

/* airoha_flash_write: write wrapper for MTD to SPI driver */
static int airoha_flash_write(struct mtd_info *mtd, loff_t to, size_t len,
	size_t *retlen, const u_char *buf) {
	unsigned long rlen = 0;

	flash_write(to, len, &rlen, buf);
	*retlen = len; /* for some reason the flashhal doesnt do it */
	return 0;
}

/* airoha_flash_read: read wrapper for MTD to SPI driver*/
static int airoha_flash_read(struct mtd_info *mtd, loff_t from, size_t len,
 size_t *retlen, u_char *buf) {
	unsigned long rlen = 0;

	flash_read(from, len, &rlen, buf);
	*retlen = len; /* for some reason the flashhal doesnt do it */
	return 0;
}

/* airoha_flash_erase: erase wrapper for MTD TO SPI driver */
static int airoha_flash_erase(struct mtd_info *mtd, struct erase_info *instr) {
	return flash_erase(instr->addr, instr->len);
}

static void airoha_flash_sync(struct mtd_info *mtd) {
	/* not sure what it do ? */
}

#ifdef INCLUDE_FLASH_SPINOR
static int airoha_flash_probe(struct udevice *dev)
{
	unsigned long eraseSize = 0;
	unsigned long size = 0;

	flash_init(0);

	size = spiflash_sizeGet();
	eraseSize = spiflash_eraseSizeGet();

	printf("size = 0x%x, erase size = 0x%x.\n", size, eraseSize);

	airoha_flash_mtd_info = (struct mtd_info){
		.name = "airoha-flash",
		.type = MTD_NORFLASH,
		.flags = MTD_WRITEABLE,
		.writesize = 1,
		.writebufsize = 1,
		._erase = airoha_flash_erase,
		._read  = airoha_flash_read,
		._write = airoha_flash_write,
		._sync = airoha_flash_sync,
		.size = size /* after bbt setup this will change */,
		.priv = NULL,
		.dev = dev,
		.erasesize = eraseSize,
		.oobsize = 0
	};
	if (add_mtd_device(&airoha_flash_mtd_info)) {  /* attach mtd device */
		printf("[Airoha flash bringup failed, please check with SoC Team]\n");
	}
	add_mtd_partitions_of(&airoha_flash_mtd_info); /* DTS */

	return 0;
}
#else /* not define INCLUDE_FLASH_SPINOR */
static int airoha_flash_probe(struct udevice *dev)
{
	flash_init(0); /* This will detect flash and all */
	
	struct SPI_NAND_FLASH_INFO_T	ptr_dev_info_t;
	SPI_NAND_Flash_Get_Flash_Info(&ptr_dev_info_t);
	extern int nand_flash_avalable_size;

	airoha_flash_mtd_info = (struct mtd_info){ 
		.name = "airoha-flash",
		.type = MTD_NANDFLASH,
		.flags = MTD_WRITEABLE,
		.writesize = ptr_dev_info_t.page_size,
		/* must be >= writesize, otherwise UBI rejects the device */
		.writebufsize = ptr_dev_info_t.page_size,
		._erase = airoha_flash_erase,
		._read  = airoha_flash_read,
		._write = airoha_flash_write,
		._sync = airoha_flash_sync,
		.size = nand_flash_avalable_size /* after bbt setup this will change */,
		.priv = NULL,
		.dev = dev,
		.erasesize = ptr_dev_info_t.erase_size,
		.oobsize = ptr_dev_info_t.oob_size
	};
	if (add_mtd_device(&airoha_flash_mtd_info)) {  /* attach mtd device */
		printf("[Airoha flash bringup failed, please check with SoC Team]\n");
	}
	add_mtd_partitions_of(&airoha_flash_mtd_info); /* DTS */
	return 0 ; 
}
#endif /* INCLUDE_FLASH_SPINOR */

static const struct udevice_id airoha_flash_dts_ids[] = {
	{ .compatible = "airoha-flash" },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(arioha_flash) = {
	.name = "aiorha-flash (legacy)",
	.id = UCLASS_MTD,
	.of_match = airoha_flash_dts_ids,
	.priv_auto	= 0, /* current architecture does not require any memory */
	.probe = airoha_flash_probe,
};
