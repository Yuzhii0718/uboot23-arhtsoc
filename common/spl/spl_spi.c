// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/raw/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 */

#include <common.h>
#include <image.h>
#include <log.h>
#include <spi.h>
#include <spi_flash.h>
#include <errno.h>
#include <spl.h>
#include <asm/global_data.h>
#include <dm/ofnode.h>
#include <ecnt_flash.h>
#ifndef TCSUPPORT_NEW_SPI
#include <spi/spi_nand_flash.h>
#endif
extern int nand_flash_avalable_size;
#if 0
extern int size_art[];
#endif

/* *  spl load buffer-internal use
* ---------- +--------------------+
*            |                    |
* ---------- +--------------------+
*            |slave_partition_info|
* ---------- +--------------------+
*            |   legacy_img_hdr   |
* 0x8a000000 +--------------------+
*/

/* *  kernel load addr-export to Uboot
* 0x80088000 +--------------------+
*            |      need_swap     |
* ---------- +--------------------+
*            |       bootflag     |
* ---------- +--------------------+
*/
#if CONFIG_IS_ENABLED(OS_BOOT)
/*
 * Load the kernel, check for a valid header we can parse, and if found load
 * the kernel and then device tree.
 */
static int spi_load_image_os(struct spl_image_info *spl_image,
			     struct spl_boot_device *bootdev,
			     struct spi_flash *flash,
			     struct legacy_img_hdr *header)
{
	int err;

	/* Read for a header, parse or error out. */
	spi_flash_read(flash, CFG_SYS_SPI_KERNEL_OFFS, sizeof(*header),
		       (void *)header);

	if (image_get_magic(header) != IH_MAGIC)
		return -1;

	err = spl_parse_image_header(spl_image, bootdev, header);
	if (err)
		return err;

	spi_flash_read(flash, CFG_SYS_SPI_KERNEL_OFFS,
		       spl_image->size, (void *)spl_image->load_addr);

	/* Read device tree. */
	spi_flash_read(flash, CFG_SYS_SPI_ARGS_OFFS,
		       CFG_SYS_SPI_ARGS_SIZE,
		       (void *)CONFIG_SYS_SPL_ARGS_ADDR);

	return 0;
}
#endif

static ulong spl_spi_fit_read(struct spl_load_info *load, ulong sector,
			      ulong count, void *buf)
{
	struct spi_flash *flash = load->dev;
	ulong ret;

	ret = spi_flash_read(flash, sector, count, buf);
	if (!ret)
		return count;
	else
		return 0;
}

unsigned int __weak spl_spi_get_uboot_offs(struct spi_flash *flash)
{
	return CONFIG_SYS_SPI_U_BOOT_OFFS;
}

u32 __weak spl_spi_boot_bus(void)
{
	return CONFIG_SF_DEFAULT_BUS;
}

u32 __weak spl_spi_boot_cs(void)
{
	return CONFIG_SF_DEFAULT_CS;
}

/*
 * The main entry for SPI booting. It's necessary that SDRAM is already
 * configured and available since this code loads the main U-Boot image
 * from SPI into SDRAM and starts it from there.
 */

static ulong legacy_spl_spi_fit_read(struct spl_load_info *load, ulong sector, ulong count, void *buf) {


	ulong ret; 

	flash_read(sector, count, &ret, buf) ; 

	return count; 
}

static int parse_slave_info_from_flash(slave_partition_info *record, unsigned int offset)
{
	int err,ret = 0;
	unsigned int crc = 0;
	
	err = flash_read(offset, sizeof(slave_partition_info), &ret, record);
	if (err) 
	{
		printf("%s: Failed to read from SPI flash (err=%d)\n",__func__, err);
		return -1;
	}

	if(record->magic != SLAVE_PARTITION_INFO_MAGIC)
	{
		printf("%s: invalid slave offset because of wrong magic(0x%x)\n",__func__, record->magic);
		return -1;
	}

	crc = crc32(record->magic, (unsigned char *)&record->offset, sizeof(unsigned int));
	if(record->crc != crc)
	{
		printf("%s: invalid slave offset because of crc mismatch, [origin](0x%x),[now](0x%x)\n",__func__, record->crc, crc);
		return -1;
	}
			
	printf("%s: Valid slave offset get from flash\n",__func__);
	return 0;
}

static unsigned int spl_spi_get_slave_offs(struct legacy_img_hdr *header, unsigned int slave_info_offset)
{
	unsigned int defualt_slave_offset;
	slave_partition_info *record;

	defualt_slave_offset = ofnode_conf_read_int("u-boot,spl-payload-offset",
								defualt_slave_offset);
	debug("%s: slave off read from conf = 0x%x\n",__func__, defualt_slave_offset);
	record = (slave_partition_info *)((void *)header - sizeof(*record));
	debug("%s: slave_info_offset at 0x%x, record at %p\n",__func__, slave_info_offset, record);
	if(0 == parse_slave_info_from_flash(record, slave_info_offset))
		return record->offset;

	return defualt_slave_offset;
}

static int spl_spi_reload_image(struct spl_image_info *spl_image, struct spl_load_info *info,
								unsigned char *bootflag, unsigned char *swap,
								struct legacy_img_hdr *header, unsigned int slave_info_offset)
{
	int err = 0;
	unsigned int payload_offs;
	int retlen;
	
	if(*bootflag == '0')
		payload_offs = spl_spi_get_slave_offs(header, slave_info_offset);
	else
		payload_offs = spl_spi_get_uboot_offs(NULL);
	

	err = flash_read(payload_offs, sizeof(*header), &retlen, (u_char*) header);
	if (err) {
		printf("%s: Failed to read from SPI flash (err=%d)\n",
			  __func__, err);
		return err;
	}

	if(image_get_magic(header) == FDT_MAGIC)
	{
		*swap = '1';
		*bootflag = (*bootflag=='1'?'0':'1');
		printf("Reloading...from %s\n", *bootflag=='1'?"slave":"master");
		return spl_load_simple_fit(spl_image, info,
					  payload_offs,
					  header);
	}

	/*not a correct Fit Image*/
	return -1;
}

static int spl_spi_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	int err = 0;
	unsigned int payload_offs;
	struct spi_flash *flash;
	struct legacy_img_hdr *header;
	unsigned int sf_bus = spl_spi_boot_bus();
	unsigned int sf_cs = spl_spi_boot_cs();

	header = spl_get_load_buffer(-sizeof(*header), sizeof(*header));

#ifdef TCSUPPORT_NEW_SPI
	/*
	 * Load U-Boot image from SPI flash into RAM
	 * In DM mode: defaults speed and mode will be
	 * taken from DT when available
	 */
	flash = spi_flash_probe(sf_bus, sf_cs,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("SPI probe failed.\n");
		return -ENODEV;
	}
	payload_offs = spl_spi_get_uboot_offs(flash);
#else 
	flash_init();
#if 0
	unsigned int art_size = 0;
	unsigned int art_offset = 0;
	int i = 0;
	CALIBRATION_LAYOUT cal = CAL_BOOTFLAG;
#endif
	int ret = 0;
	struct SPI_NAND_FLASH_INFO_T ptr_dev_info_t;
	SPI_NAND_Flash_Get_Flash_Info(&ptr_dev_info_t);
	unsigned int bootflag_offset = nand_flash_avalable_size - 2 * ptr_dev_info_t.erase_size;
	unsigned int slave_info_offset = nand_flash_avalable_size - ptr_dev_info_t.erase_size;
	unsigned char *need_swap = (unsigned char *)KERNEL_LOAD_ADDR; /*kernel load address(not used in tpl),put two u-char here*/
	unsigned char *bootflag = need_swap + 1;

	debug("[%s]nand_flash_avalable_size = %d,block size = 0x%x,bootflag offset = 0x%x\n",
			__func__, nand_flash_avalable_size,ptr_dev_info_t.erase_size,bootflag_offset);
#if 0
	debug("[spl_spi_load_image]nand_flash_avalable_size = %d\n",nand_flash_avalable_size);
	if (CONFIG_IS_ENABLED(OF_REAL)) {
			art_size = ofnode_conf_read_int("art_partition_size",
						art_size);
	}

	debug("[spl_spi_load_image]art_partition_size = 0x%x\n",art_size);
	art_offset = nand_flash_avalable_size - art_size;

	while(i < cal)
		art_offset += size_art[i++];

	debug("[spl_spi_load_image]bootflag offset = 0x%x, i = %d, bootflag size = 0x%x\n",art_offset,i,size_art[i]);
	err = flash_read(art_offset, sizeof(unsigned char), &ret, bootflag);
#endif
	err = flash_read(bootflag_offset, sizeof(unsigned char), &ret, bootflag);
	if (err) {
		printf("%s: Failed to bootflag read from SPI flash (err=%d)\n",
			  __func__, err);
try:
		*bootflag = '0';
	}
	if(*bootflag == '1')
	{
		printf("boot from slave\n");
		payload_offs = spl_spi_get_slave_offs(header, slave_info_offset);
	}
	else
	{
		printf("boot from master\n");
		payload_offs = spl_spi_get_uboot_offs(flash);
	}
	debug("[spl_spi_load_image]sf_bus=%u,sf_cs=%u,payload_offs=0x%x\n",sf_bus,sf_cs,payload_offs);
#endif
	/*if (CONFIG_IS_ENABLED(OF_REAL)) {
		payload_offs = ofnode_conf_read_int("u-boot,spl-payload-offset",
						    payload_offs);
	}*/

#if CONFIG_IS_ENABLED(OS_BOOT)
	if (spl_start_uboot() || spi_load_image_os(spl_image, bootdev, flash, header))
#endif
	{
		/* Load u-boot, mkimage header is 64 bytes. */
#ifdef TCSUPPORT_NEW_SPI
		err = spi_flash_read(flash, payload_offs, sizeof(*header),
				     (void *)header);
#else
		int retlen; 
		err = flash_read(payload_offs, sizeof(*header), &retlen, (u_char*) header);
#endif
		if (err) {
			debug("%s: Failed to read from SPI flash (err=%d)\n",
			      __func__, err);
			return err;
		}

		if (IS_ENABLED(CONFIG_SPL_LOAD_FIT_FULL) &&
		    image_get_magic(header) == FDT_MAGIC) {
			err = spi_flash_read(flash, payload_offs,
					     roundup(fdt_totalsize(header), 4),
					     (void *)CONFIG_SYS_LOAD_ADDR);
			if (err)
				return err;
			err = spl_parse_image_header(spl_image, bootdev,
					(struct legacy_img_hdr *)CONFIG_SYS_LOAD_ADDR);
		} else if (IS_ENABLED(CONFIG_TPL_LOAD_FIT)) {
			struct spl_load_info load;

			load.dev = flash;
			load.priv = NULL;
			load.filename = NULL;
			load.bl_len = 1;
#ifndef TCSUPPORT_NEW_SPI
			load.read = legacy_spl_spi_fit_read;
#else 
			load.read = spl_spi_fit_read;
#endif
			if (image_get_magic(header) == FDT_MAGIC) {
				debug("Found FIT\n");
				err = spl_load_simple_fit(spl_image, &load,
							  payload_offs,
							  header);
#ifndef TCSUPPORT_NEW_SPI
				if(err)
					return spl_spi_reload_image(spl_image, &load, bootflag, need_swap, header, slave_info_offset);

				/*Load Success, no need to swap bootflag*/
				*need_swap = '0';
			}
			else
				return spl_spi_reload_image(spl_image, &load, bootflag, need_swap, header, slave_info_offset);
#else
			}
#endif
		} else if (IS_ENABLED(CONFIG_SPL_LOAD_IMX_CONTAINER)) {
			struct spl_load_info load;

			load.dev = flash;
			load.priv = NULL;
			load.filename = NULL;
			load.bl_len = 1;
			load.read = spl_spi_fit_read;

			err = spl_load_imx_container(spl_image, &load,
						     payload_offs);
		} else {
			err = spl_parse_image_header(spl_image, bootdev, header);
			if (err)
				return err;
			err = spi_flash_read(flash, payload_offs + spl_image->offset,
					     spl_image->size,
					     (void *)spl_image->load_addr);
		}
		if (IS_ENABLED(CONFIG_SPI_FLASH_SOFT_RESET)) {
			err = spi_nor_remove(flash);
			if (err)
				return err;
		}
	}

	return err;
}
/* Use priorty 1 so that boards can override this */
SPL_LOAD_IMAGE_METHOD("SPI", 1, BOOT_DEVICE_SPI, spl_spi_load_image);
