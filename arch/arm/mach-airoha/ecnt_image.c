/*
 * (C) Copyright 2000-2009
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <blk.h>
#include <image.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <linux/libfdt.h>
#include <linux/mtd/mtd.h>
#include <mmc.h>
#include <ecnt_flash.h>
#include <ecnt_image.h>
#include <airoha/trx.h>

#define DBG_ECNT_IMAGE 0
#ifdef TCSUPPORT_TCBOOT_1MB_SIZE
#define TCLINUX_OFFSET 0x100000
#else
#define TCLINUX_OFFSET 0x80000
#endif
#define SECURE_MAGIC 0xaa640001
extern unsigned int get_fip_offset(void);
#define DECRYPTED_FIT_ADDR (0x8b000000)



unsigned int image_read_mode = 0;

typedef struct
{
	unsigned long tclinux_size;
	unsigned long gpt_header_off;
	unsigned long gpt_size;
} ImageConf;

int mmc_size_art[] = 
{
	0x40000,	/*ART_RESERVE*/
	0x200000,	/*WIFI_6G_SIZE*/
	0x40000,	/*first 20000:for 18.06 2.4G(5G) cali, second 20000 for BOB_SIZE*/
	0x40000,	/*WIFI_24G_SIZE*/
	0x40000,	/*WIFI_5G_SIZE*/
	0x40000,	/*PROLINECMD_SIZE*/
	0x40000 	/*BOOTFLAG_SIZE*/
};

enum {
    MMC_CAL_RESERVE = 0,
    MMC_CAL_WIFI_6G,
    MMC_CAL_BOB,
    MMC_CAL_WIFI_24G,
    MMC_CAL_WIFI_5G,
    MMC_CAL_PROLINECMD,
    MMC_CAL_BOOTFLAG,
    MMC_CAL_AREA_NUM
} MMC_CALIBRATION_LAYOUT;


const ImageConf emmc_conf = 
{
#ifdef TCSUPPORT_DM_VERITY
	.tclinux_size = 0xa00000,
#else
	.tclinux_size = 0x8000000,
#endif
	.gpt_header_off = 0x400,
	.gpt_size = 0x4000,
};

const ImageConf spi_conf = 
{
	.tclinux_size = 0x3200000,
	.gpt_header_off = 0x0,
	.gpt_size = 0x0,
};

const ImageConf *current_conf;
int init_image_parameter(void)
{
	if(is_emmc()) 
		current_conf = &emmc_conf;
	else
		current_conf = &spi_conf;

	return 0;
}

static int setup_mtd_device(struct mtd_info **mtd, const char* mtd_dev)
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

/* for eMMC flash, use mtd_dev as buffer address which will be write to eMMC flash */
static int flash_op(unsigned long addr, unsigned long size, const char* mtd_dev)
{
	if (is_emmc()) {
		// mmc blk upgrade fw
		struct mmc *mmc;
		u32 blk, cnt, n;
		char *pBuf = CONFIG_SYS_LOAD_ADDR;

		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if (mmc_getwp(mmc) == 1) {
			printf("Error: card is write protected!\n");
			return -1;
		}
		blk = addr / mmc->read_bl_len;
		cnt = (addr + size) / mmc->read_bl_len;
		if(((addr + size) % mmc->read_bl_len) != 0) {
			cnt++;
		}
		cnt -= blk;

		printf("MMC write: dev # %d, block # %d, count %d ...\n", 0, blk, cnt);
		n = blk_dwrite(mmc_get_blk_desc(mmc), blk, cnt, (const void *)pBuf);
		printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
		if (!(n == cnt))
			return -1;
	} else {
		// mtd blk upgrade fw
		struct mtd_info *mtd;
		struct erase_info ei;
		size_t ret_len = 0;
		int ret;

		ret = setup_mtd_device(&mtd, mtd_dev);
		if (ret) {
			printf("ERROR: Invalid U-Boot partition!\n");
			return -1;
		}
		ei.mtd = mtd;
		ei.addr = 0;
		ei.len = size;
		printf("erase: partition=%s, addr=0x%lx, len=0x%lx\n", mtd_dev, 0, size);
		ret = mtd_erase(mtd, &ei);
		if (ret)
			return -1;
		printf("write: src=0x%lx, len=0x%lx, dst=0x%lx\n", CONFIG_SYS_LOAD_ADDR, size, 0);
		ret = mtd_write(mtd, 0, size, &ret_len, CONFIG_SYS_LOAD_ADDR);
		if (ret)
			return -1;
	}
	return 0;
}

void ecnt_ImageUpgrade(int fw_type)
{
	ulong img_size;
	char *filename;
	uint ret = 0;
	unsigned int checkvalue = 0;
	unsigned int checksize = 0;
	unsigned long long uImage_addr = CONFIG_SYS_LOAD_ADDR;
	struct trx_header* trx_H = NULL;
	unsigned int check_value = 0;
	unsigned int check_size = 0;
	unsigned int crc = 0;
	unsigned int cal_check_sum = 0;
	unsigned int magic = 0;
	int mmcbootflag = 0;

	img_size = env_get_hex("filesize", 0);
	/* flush cache */
	if (img_size == 0) {
		printf("Error: no file loaded via tftp.\r\n");
	}
	
	flush_cache(CONFIG_SYS_LOAD_ADDR, img_size);
	init_image_parameter();
	filename = env_get("filename");
	if (!strcmp(filename, env_get("uboot_filename")))
	{
		/* TODO: please add CRC check here! */
		if (upgrade_verify((char *)CONFIG_SYS_LOAD_ADDR) != 0)
		{
			printf("Verify image failed!!!\r\n");
			return;
		}

		checksize = img_size;
		checkvalue = crc32_no_comp(DEFAULT_CRC,(const unsigned char*)CONFIG_SYS_LOAD_ADDR , checksize-CONFIG_ENV_SIZE);
		if (checkvalue != 0)
		{
			printf("ERROR: CRC/Hash check failed !\n");
			return;
		}
		else
		{
			printf("CRC check success, start imageUpgrade\n");
		}
		/* Perform U-Boot upgrade */
		memmove((const void *)CONFIG_SYS_LOAD_ADDR, ((const void *)CONFIG_SYS_LOAD_ADDR + current_conf->gpt_header_off), (img_size - current_conf->gpt_header_off));
		if(flash_op((TCBOOT_OFFSET + current_conf->gpt_header_off), (img_size - current_conf->gpt_header_off), CONFIG_SYS_UBOOT_PARTITION_NAME)) {
			printf("Flash Op Failed\n");
		}
		printf("\r\nupgrade finished !\n");
	}
	else if (!strcmp(filename, env_get("kernel_filename"))) 
	{
#if !defined(CONFIG_TPL)
		magic = *(const unsigned int *)uImage_addr;

		/*if burn a compatible fw, need to skip the secure and trx header
		otherwise just go to check the fit header*/
		if(SECURE_MAGIC == magic)
		{
			if (upgrade_verify((char *)uImage_addr) != 0)
			{
				printf("Verify image failed!!!\r\n");
				return;
			}
		    printf("CRC check success, start imageUpgrade\n");
			uImage_addr += get_fip_offset();
		}
		else
		{
			printf(" FIP no magic num!!!\r\n");
			return;
		}

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
		uImage_addr = DECRYPTED_FIT_ADDR;
#endif
		magic = *(const unsigned int *)uImage_addr;
		if(TRX_MAGIC2 == magic){
			trx_H = (struct trx_header *)uImage_addr;
			uImage_addr += sizeof(struct trx_header);
		}
#endif
		/* check tclinux hash*/
		if (fit_all_image_verify((const void *)uImage_addr)!= 1)
		{
			printf("hash check error!!! \r\n");
			return;
			//check_value = -1;
		}
#if !defined(CONFIG_TPL)
		if (img_size < (trx_H->len))
		{
			printf("receive image size 0x%lx is less than the length 0x%lx which stored in trx !!!",img_size, (unsigned long)trx_H->len);
				check_value = -1;
		}
		else
		{
			/* get image len from trx*/
			check_size = trx_H->len - trx_H->header_len;
			cal_check_sum = crc32_no_comp(DEFAULT_CRC, (const unsigned char*)uImage_addr, check_size);
			crc = __swab32(ntohl(trx_H->crc32));

			if (cal_check_sum != crc)
			{
				printf("crc check error!!! \r\n");
				check_value = -1;
			}
		}
#endif
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, img_size, CONFIG_SYS_TCLINUX_PARTITION_NAME)) {
			printf("Flash Op Failed\n");
		}
		printf("\r\nupgrade finished !\n");
	}
	else if (!strcmp(filename, "tclinux_allinone")) 
	{
		if(!is_emmc())
		{
			if(flash_op(TCBOOT_OFFSET, img_size, "spi-nand1")) 
			{
				printf("Flash Op Failed\n");
			}
			printf("\r\nupgrade allinone finished !\n");
		}
	}
	else if (!strcmp(filename, "tclinux_allinone_emmc")) 
	{
		if(is_emmc())
		{
			/*skip MBR 0 LBA*/	
			img_size -= 0x200;		
			memmove((const void *)CONFIG_SYS_LOAD_ADDR, ((const void *)(CONFIG_SYS_LOAD_ADDR + 0x200)), img_size);
			/*flash from 1 LBA*/
			if(flash_op(TCBOOT_OFFSET + 0x200, img_size, NULL)) 
			{
				printf("Flash Op Failed\n");
			}
			else
			{
				mmc_bootflag_read(__init_mmc_device(0, false, MMC_MODES_END), &mmcbootflag);
				if(mmcbootflag != 0)
					swap_bootflag();
			}
			printf("\r\nupgrade allinone finished !\n");
			/*need to consider backup gpth and gpte at the end of LBA*/
		}
	}
}

static int get_fdt_node_offset_len(unsigned char *buf, int images_noffset, const char *node, void **offset, u32 *len)
{
	int		noffset;

#if DBG_ECNT_IMAGE
	printf("check %s buf=0x%lx, images_noffset=0x%lx\n", node, buf, images_noffset);
#endif

	noffset = fdt_subnode_offset(buf, images_noffset, node);
#if DBG_ECNT_IMAGE
	printf("images_noffset=0x%lx, noffset=0x%lx\n", images_noffset, noffset);
#endif

	if (noffset < 0) {
		printf("Can't get node offset for image unit name: '%s' (%s)\n", node, fdt_strerror(noffset));
		return -1;
	}

	*offset = (void *)fdt_getprop(buf, noffset, "data", len);
#if DBG_ECNT_IMAGE
	printf("offset=0x%lx, len=0x%lx\n", *offset, *len);
#endif
	if (*offset == NULL) {
		printf("get fdt data error:0x%08u\n", (u32)(*offset));
		return -1;
	}
	return 0;

}

int get_tclinux_imginfo(struct tclinux_imginfo *info)
{
	struct trx_header *trx_info = NULL;
	int images_noffset;
	uint ret = 0;
	uint len;
	const void *data;
	u_char *buf_with_trx = (u_char *)CONFIG_SYS_LOAD_ADDR - sizeof(struct trx_header);
	u_char *buf = (u_char *)CONFIG_SYS_LOAD_ADDR;

	trx_info = (struct trx_header *)buf_with_trx;
	info->kernel_size = trx_info->kernel_len;
	info->rootfs_size = trx_info->rootfs_len;

	if (FDT_MAGIC != (unsigned long) fdt_magic(buf)) {
		printf("Error: Invalid image magic.\r\n");
		return -1;
	}
	info->tclinux_size = fdt_totalsize((unsigned long)buf);



#if 1
	/*mkimage -E: using external data, so data-offset is mandatory*/
	images_noffset = fdt_path_offset((unsigned long)buf, "/images/kernel");
	if (images_noffset < 0) {
		printf("Can't find images parent node /image (%s)\n", fdt_strerror(images_noffset));
		return -1;
	}

	data = (void *)fdt_getprop(buf, images_noffset, "data-offset", len);
	printf("kernel_off data = %x readl = %x\n",data,readl(data));


	info->kernel_off = __swab32((readl(data)));

	data = (void *)fdt_getprop(buf, images_noffset, "data-size", len);

	info->kernel_size = __swab32((readl(data)));
	printf("kernel_size data = %x readl = %x\n",data,readl(data));

#ifndef TCSUPPORT_DM_VERITY

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
	images_noffset = fdt_path_offset((unsigned long)buf, "/images/ramdisk");
#else
	images_noffset = fdt_path_offset((unsigned long)buf, "/images/filesystem");
#endif

	if (images_noffset < 0) {
		printf("Can't find images parent node /image (%s)\n", fdt_strerror(images_noffset));
		return -1;
	}

	data = (void *)fdt_getprop(buf, images_noffset, "data-offset", len);
	printf("rootfs_off data = %x readl = %x\n",data,readl(data));


	info->rootfs_off = __swab32((readl(data)));

	data = (void *)fdt_getprop(buf, images_noffset, "data-size", len);

	info->rootfs_size = __swab32((readl(data)));
	printf("rootfs_size data = %x readl = %x\n",data,readl(data));

	info->tclinux_size += info->rootfs_size;
#endif
	info->tclinux_size += info->kernel_size;

#else
	/*Not using external data, data is mandatory*/
	images_noffset = fdt_path_offset((unsigned long)buf, "/images");
	if (images_noffset < 0) {
		printf("Can't find images parent node /image (%s)\n", fdt_strerror(images_noffset));
		return -1;
	}

	if(get_fdt_node_offset_len(buf, images_noffset, "kernel@1", &data, &len)) {
		return -1;
	}
#if DBG_ECNT_IMAGE
	printf("kernel@1: data=0x%lx, buf=0x%lx, off=0x%lx, len=0x%lx\n", data, buf, (u32)data - (u32)buf, len);
#endif
	info->kernel_off = (u32)data - (u32)buf + get_fip_offset();

	if(get_fdt_node_offset_len(buf, images_noffset, "filesystem@1", &data, &len)) {
		return -1;
	}
#if DBG_ECNT_IMAGE
	printf("filesystem@1: data=0x%lx, buf=0x%lx, off=0x%lx, len=0x%lx\n", data, buf, (u32)data - (u32)buf, len);
#endif
	info->rootfs_off = (u32)data - (u32)buf + get_fip_offset();
#endif
#if DBG_ECNT_IMAGE
	printf("=== tclinux_size:0x%08x ===\n", info->tclinux_size);
	printf("=== kernel_off:0x%08x ===\n", info->kernel_off);
	printf("=== kernel_size:0x%08x ===\n", info->kernel_size);
	printf("=== rootfs_off:0x%08x ===\n", info->rootfs_off);
	printf("=== rootfs_size:0x%08x ===\n", info->rootfs_size);
#endif

	return 0;
}

int mtd_bootflag_write(u_char *buf)
{
	ulong i = 0, retLen = 0, startAddr = 0;
	unsigned long long artaddr = CONFIG_SYS_LOAD_ADDR;
	struct mtd_info *mtd_art = NULL;

	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition (art)!\n");
		return -1;
	}

	memset((uchar *)artaddr, 0, mtd_art->writesize);
	(*(uchar *)artaddr) = buf[0];
	while(i < MMC_CAL_BOOTFLAG)
		startAddr += mmc_size_art[i++];

	if(mtd_write(mtd_art, startAddr, mtd_art->writesize, &retLen, artaddr))
	{
		printf("mtd bootflag write failed\n");
		return -1;
	}

	return 0;	
}

int mtd_bootflag_read(int *boot)
{
	ulong i = 0, retLen = 0, startAddr = 0;
	unsigned long long artaddr = CONFIG_SYS_LOAD_ADDR;
	struct mtd_info *mtd_art = NULL;

	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("Invalid tclinux partition (art)!\n");
		return -1;
	}
   
    memset((uchar *)artaddr, 0, mtd_art->writesize);
	while(i < MMC_CAL_BOOTFLAG)
		startAddr += mmc_size_art[i++];

	if(mtd_read(mtd_art, startAddr, mtd_art->writesize, &retLen, artaddr)) 
	{
		printf("mtd bootflag read failed\n");
		return -1;
	}

	if((*(uchar *)artaddr) == '1')  
		*boot = 1;
	else
		*boot = 0;

	return 0;
}

int mmc_bootflag_write(struct mmc *mmcptr, u_char *bootflag)
{
	u32 blk = 0, n = 0, i = 0;
	struct disk_partition art;

	memset(&art, 0, sizeof(art));
	if(mmc_partitions_parse(&art, "art") != 0)
		return -1;
	art.start *= art.blksz;
	while(i < MMC_CAL_BOOTFLAG)
		art.start += mmc_size_art[i++];

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

	memset(&art, 0, sizeof(art));
	if(mmc_partitions_parse(&art, "art") != 0)
		return -1;
	art.start *= art.blksz;
	while(i < MMC_CAL_BOOTFLAG)
		art.start += mmc_size_art[i++];

	blk = art.start / mmcptr->read_bl_len;
	n = blk_dread(mmc_get_blk_desc(mmcptr), blk, 1, buf);

	if(buf[0] == '1')
		*bootflag = 1;
	else
		*bootflag = 0;
	
	return 0;
}

int boot_exception_handle(struct mmc *ismmc, struct disk_partition *usepart, char *spipart, unsigned long long storeAddr)
{
	u32 m = 0, n = 0, cnt = 0;
	uchar *name = NULL, *rootname = NULL;
	struct disk_partition freepart, rootpart;
	struct mtd_info *freemtd = NULL, *usemtd = NULL;
	ulong retLen = 0;

	if(ismmc != NULL)
	{
		memset(&freepart, 0, sizeof(freepart));
		memset(&rootpart, 0, sizeof(rootpart));

		name = (!strcmp(usepart->name, "tclinux_slave"))? "tclinux": "tclinux_slave";
		rootname = (!strcmp(usepart->name, "tclinux_slave"))? "filesystem": "filesystem_slave";

		if (mmc_partitions_parse(&freepart, name) == 0 &&
		    mmc_partitions_parse(&rootpart, rootname) == 0)
		{
	 		cnt = freepart.size + rootpart.size;

			n = blk_dread(mmc_get_blk_desc(ismmc), freepart.start, cnt, storeAddr);
			printf("%d blocks read to storeaddr: %s\n", n, (n == cnt) ? "OK" : "ERROR");
			if((n != cnt) || upgrade_verify((char *)storeAddr) != 0)
			{
				printf("Verify image still Failed when try the other image!!!\r\n");
				return -1;
			}

			swap_bootflag();
			env_set("rootfs_partition_name", rootname);
			printf("MMC boot from %s\n", name);
			m = blk_dwrite(mmc_get_blk_desc(ismmc), usepart->start, cnt, storeAddr);
			printf("%d blocks write into use part: %s\n", m, (m == cnt) ? "OK" : "ERROR");
		}
	}
	else
	{
		name = (!strcmp(spipart, CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME))? CONFIG_SYS_TCLINUX_PARTITION_NAME: CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME;
		if(setup_mtd_device(&freemtd, name) || 
			setup_mtd_device(&usemtd, spipart))
		{
			printf("Invalid mtd partition %s and %s!\n", name, spipart);
			return -1;
		}
		
		swap_bootflag();
		n = mtd_read(freemtd, 0, freemtd->size, &retLen, storeAddr);
		printf("mtd read %s part Len=0x%llx byte to storeaddr %s\n", freemtd->name, freemtd->size, (n)? "ERROR": "OK");
		if(n || upgrade_verify((char *)storeAddr) != 0)
		{
			printf("Verify mtd image Failed!!!\r\n");
			swap_bootflag();
			return -1;
		}

		m = mtd_write(usemtd, 0, usemtd->size, &retLen, storeAddr);
		printf("\nmtd write into %s part Len=0x%llx byte %s\n", usemtd->name, usemtd->size, (m)? "ERROR": "OK");

		if(!strcmp(name, CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME))
			env_set("root", "/dev/mtdblock6 ro"); 
		
		printf("MTD boot from %s\n",name);
	}

	return 0;
}

static int mtd_load_kernel_image(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	u32 blk, cnt, n;
	int mmcbootflag = 0, partUpdate = 1;
	int ret = 0, ret_len = 0, bootflag = 0;
	struct mmc *mmc = NULL;
	struct disk_partition partMsg;
	struct mtd_info *mtd_kernel, *mtd_kernel_slave, *mtd_art;
	unsigned long long store_addr = CONFIG_SYS_LOAD_ADDR;
	unsigned long long artpartition_tmp_addr = CONFIG_SYS_LOAD_ADDR;
	const char *rootfsName = "rootfs_partition_name";
	char* bootpart = NULL;
	
	init_image_parameter();
#if !defined(CONFIG_TPL)
	store_addr -= sizeof(struct trx_header);
	store_addr -= get_fip_offset();
#endif

	if (is_emmc()) 
	{
		memset(&partMsg, 0, sizeof(partMsg));
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		mmc_bootflag_read(mmc, &mmcbootflag);			
		if(mmcbootflag == 0)
		{
			env_set(rootfsName, "filesystem");
			if(mmc_partitions_parse(&partMsg, "tclinux") != 0)
				partUpdate = 0;
		}
		else
		{
			env_set(rootfsName, "filesystem_slave");
			if(mmc_partitions_parse(&partMsg, "tclinux_slave") != 0)
				partUpdate = 0;
		}

		if(partUpdate == 0)
		{
			blk = (TCLINUX_OFFSET + current_conf->gpt_size) / mmc->read_bl_len;
			cnt = (TCLINUX_OFFSET + current_conf->gpt_size + current_conf->tclinux_size) / mmc->read_bl_len;
			if(((TCLINUX_OFFSET + current_conf->gpt_size + current_conf->tclinux_size) % mmc->read_bl_len) != 0) 
			{
				cnt++;
			}
			cnt -= blk;
		}
		else if(partUpdate == 1)
		{
			blk = partMsg.start;
			cnt = partMsg.start + partMsg.size - blk;
			printf("MMC boot from %s, mmcbootflag=%d\n",((mmcbootflag == 0)? "tclinux": "tclinux_slave"), mmcbootflag);
		}

		n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, store_addr);
		printf("%d blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");

		if (n != cnt) {
			printf("MMC read failed\n");
			return -1;
		}
	}
	else 
	{
		ret = setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME);
		if (ret) 
		{
			printf("ERROR: Invalid TCLinux partition (art)!\n");
			return -1;
		}

		ret = mtd_read(mtd_art, 0, mtd_art->size, &ret_len, artpartition_tmp_addr);
		if (ret) 
		{
			printf(" Failed to load the image from %s.\r\n", CONFIG_SYS_ART_PARTITION_NAME);
			return -1;
		}
		
		artpartition_tmp_addr=artpartition_tmp_addr+mtd_art->size-0x40000;
		bootflag = (*(char*)artpartition_tmp_addr) - '0' ; //use hex format, not ascii
		bootpart = (bootflag==1)?CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME:CONFIG_SYS_TCLINUX_PARTITION_NAME;
		if(bootflag==1)
			env_set("root", "/dev/mtdblock6 ro"); 

		printf("    \n\nboot to %s.. \n", bootpart);
		ret = setup_mtd_device(&mtd_kernel, bootpart);
		if (ret) 
		{
			printf("ERROR: Invalid TCLinux partition!\n");
			return -1;
		}

		ret = mtd_read(mtd_kernel, 0, mtd_kernel->size, &ret_len, store_addr);
		if (ret) 
		{
			printf("Failed to load the image from %s.\r\n", bootpart);
		}
	}

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
	if (boot_verify((char *)(store_addr)))
	{
		printf("Verify image Failed!!!\r\n");
		return -1;
	}
#else
	if (upgrade_verify((char *)store_addr) != 0)
	{
		printf("Verify image Failed!!!\r\n");
		if(boot_exception_handle(mmc, &partMsg, bootpart, store_addr))
			return -1;
	}
#endif

	return 0;
}


U_BOOT_CMD(
	ldkernel,
	1,
	0,
	mtd_load_kernel_image,
	"load kernel image from mtd to memory",
	""
)
