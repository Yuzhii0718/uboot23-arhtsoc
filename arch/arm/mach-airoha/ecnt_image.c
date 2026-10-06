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
#include <linux/compat.h>
#include <linux/kernel.h>
#include <linux/libfdt.h>
#include <linux/mtd/mtd.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <env.h>
#include <env_internal.h>
#include <u-boot/crc.h>
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
#define DECRYPTED_FIT_ADDR (0x8b000000)
#define TPL_BOOTFLAG_ADDR  (0x80088000)
#ifdef CONFIG_OPEN_IMAGE
#include <bootlib.h>
#include <net.h>
#endif

extern unsigned int get_fip_offset(void);
extern int upgrade_verify(char *buf);
extern uint32_t crc32_no_comp(uint32_t crc, const unsigned char *buf, uint len);
extern int swap_bootflag(void);
extern int setup_mtd_device(struct mtd_info **mtd, const char* mtd_dev);

typedef struct
{
	unsigned long tclinux_size;
	unsigned long gpt_header_off;
	unsigned long gpt_size;
} ImageConf;


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
#ifdef TCSUPPORT_DM_VERITY
	.tclinux_size = 0xa00000,
#else
	.tclinux_size = 0x3200000,
#endif
	.gpt_header_off = 0x0,
	.gpt_size = 0x0,
};

unsigned int image_read_mode = 0;
const ImageConf *current_conf;

/* implemented in ecnt_bootargs.c */
int bootargs_init(unsigned int bootflag);

int init_image_parameter(void)
{
	if(is_emmc()) 
		current_conf = &emmc_conf;
	else
		current_conf = &spi_conf;

	return 0;
}

static int flash_op(unsigned long mmcaddr, unsigned long mtdaddr, unsigned long size, const char* mtd_dev, unsigned long memaddr)
{
	struct mmc *mmc;
	u32 blk, cnt, n;
	int m = -1;
	struct mtd_info *mtd;
	struct erase_info ei;
	size_t ret_len = 0;
		
	if(is_emmc()) 
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if(mmc_getwp(mmc) == 1) 
		{
			printf("Error: card is write protected!\n");
			return -1;
		}
		blk = mmcaddr / mmc->read_bl_len;
		cnt = (mmcaddr + size) / mmc->read_bl_len;
		if(((mmcaddr + size) % mmc->read_bl_len) != 0) 
		{
			cnt++;
		}
		cnt -= blk;

		printf("MMC write: dev # %d, block # %d, count %d ...\n", 0, blk, cnt);
		n = blk_dwrite(mmc_get_blk_desc(mmc), blk, cnt, (const void *)memaddr);
		printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
		if (n != cnt)
			return -1;
	}
	else 
	{
		if(setup_mtd_device(&mtd, mtd_dev)) 
		{
			printf("ERROR: Invalid U-Boot partition!\n");
			return -1;
		}
		ei.mtd = mtd;
		ei.addr = mtdaddr;
		ei.len = size;
		m = mtd_erase(mtd, &ei);
		printf("mtd erase: partition=%s, addr=0x%x, len=0x%x %s\n", mtd_dev, (unsigned int)mtdaddr, (unsigned int)size, ((m)? "ERROR": "OK"));
		if(m)
			return -1;

		printf("mtd write: src=0x%lx, len=0x%lx, dst=0x%lx %s\n", (unsigned long)memaddr, (unsigned long)size, (unsigned long)mtdaddr, ((m)? "ERROR": "OK"));
		m = mtd_write(mtd, mtdaddr, size, &ret_len, (char *)memaddr);
		if(m)
			return -1;
	}
	return 0;
}

#if 0
static int flash_op_tp(unsigned long addr, unsigned long size, const char* mtd_dev, const char* pBuf)
{
	// mmc blk upgrade fw
	struct mmc *mmc = NULL;
	u32 blk = 0;
	u32 cnt = 0;
	u32 n = 0;

	// mtd blk upgrade fw
	struct mtd_info *mtd = NULL;
	struct erase_info ei;
	size_t ret_len = 0;
	int ret = 0;

	if (NULL == pBuf)
	{
		printf("ERROR: Invalid pFileAddr!\n");
		return -1;
	}
	
	if (is_emmc()) 
	{
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if (mmc_getwp(mmc) == 1) 
		{
			printf("Error: card is write protected!\n");
			return -1;
		}
		blk = addr / mmc->read_bl_len;
		cnt = (addr + size) / mmc->read_bl_len;
		if(((addr + size) % mmc->read_bl_len) != 0) 
		{
			cnt++;
		}
		cnt -= blk;

		printf("MMC write: dev # %d, block # %d, count %d ...\n", 0, blk, cnt);
		n = blk_dwrite(mmc_get_blk_desc(mmc), blk, cnt, (const void *)pBuf);
		printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
		if (!(n == cnt))
		{
			printf("Error: blk_dwrite error\n");
			return -1;
		}
	}
	else
	{
		
		ret = setup_mtd_device(&mtd, mtd_dev);
		if (ret) 
		{
			printf("ERROR: Invalid U-Boot partition!\n");
			return -1;
		}

		memset(&ei,0,sizeof(ei));

		ei.mtd = mtd;
		ei.addr = 0;
		ei.len = size;
		printf("erase: partition=%s, addr=0x%lx, len=0x%lx\n", mtd_dev, 0, size);
		ret = mtd_erase(mtd, &ei);
		if (ret)
		{
			printf("ERROR: mtd_erase error !\n");
			return -1;
		}
		printf("write: src=0x%lx, len=0x%lx, dst=0x%lx\n", pBuf, size, 0);
		ret = mtd_write(mtd, 0, size, &ret_len, pBuf);
		if (ret)
		{
			printf("ERROR: mtd_write error !\n");
			return -1;
		}
	}
	return 0;
}
#endif/*CONFIG_OPEN_IMAGE*/

int ecnt_allinone_check(unsigned long long allinone_img_addr)
{
#if defined(CONFIG_TPL)
	unsigned int checkvalue = 0;
	unsigned int checksize = TCLINUX_OFFSET;
	unsigned long long fit_addr = 0;

	if (upgrade_verify((char *)allinone_img_addr) != 0)
	{
		printf("Verify image failed!!!\r\n");
		return -1;
	}

	fit_addr = allinone_img_addr + TCLINUX_OFFSET + current_conf->gpt_size;

	if (fit_all_image_verify((const void *)fit_addr)!= 1)
	{
		printf("hash check error!!! \r\n");
		return -1;
	}
	
	checkvalue = crc32_no_comp(DEFAULT_CRC,(const unsigned char*)allinone_img_addr , checksize-CONFIG_ENV_SIZE);
	if (checkvalue != 0)
	{
		printf("ERROR: CRC/Hash check failed !\n");
		return -1;
	}

	printf("CRC check success, start imageUpgrade\n");
	return 0;
#else
	return 0;
#endif
}

int get_key_words_offset(const void *fdt, int node, char *key)
{
	char *label;
	int subnode;
	
	fdt_for_each_subnode(subnode, fdt, node) {	
		debug("%s--%s\n", fdt_get_name(fdt, subnode, NULL),key);
		label = (char *)fdt_getprop(fdt, subnode, "label", NULL);
		if(label)
			debug("\t\t%s\n", label);
		if(label && 0 == strcmp(label,key))
			return subnode;

		int result = get_key_words_offset(fdt, subnode, key);
		if(result != -1)
			return result;
	}

	return -1;
}

void* get_fdt_data_blob(const void *fit)
{
	int images_noffset;
	int noffset;
	int ndepth;
	int data_offset;
	int fdt_totalsize = fdt_totalsize(fit);
	char *desc;
	
	images_noffset = fdt_path_offset(fit, FIT_IMAGES_PATH);
	if (images_noffset < 0)
	{
		printf("%s:Can't find images parent node '%s' (%s)\n",__func__, FIT_IMAGES_PATH, fdt_strerror(images_noffset));
		return NULL;
	}
	for (ndepth = 0, noffset = fdt_next_node(fit, images_noffset, &ndepth);
		(noffset >= 0) && (ndepth > 0);
		noffset = fdt_next_node(fit, noffset, &ndepth))
	{
		if (ndepth == 1)
		{
			debug("%s:node offset of '%s' = %d\n", __func__, fit_get_name(fit, noffset, NULL),noffset);
			if(!fit_get_desc(fit, noffset, &desc) && !strcmp(desc, FDT_BLOB_DESCRIPTION))
				debug("%s:Find fdt blob node '%s' at offset %d\n", __func__, fit_get_name(fit, noffset, NULL),noffset);
			else
				continue;

			if (!fit_image_get_data_offset(fit, noffset, &data_offset)) {
				data_offset += ((fdt_totalsize + 3) & ~3);
				debug("%s:fdt blob node [%s]: offset = %d, [external]data_offset = %d\n", __func__, fit_get_name(fit, noffset, NULL),noffset,data_offset);
				return (void*)(fit + data_offset);
			}
		}
	}
	return NULL;
}

int update_slave_offset_info(const void *image, ulong allinone_size)
{
	void *fdt = NULL;
	int offset = -1;
	int len;
	slave_partition_info *record = (slave_partition_info *)(image + allinone_size);
	
	fdt = get_fdt_data_blob(image + TCLINUX_OFFSET);
	if(!fdt)
		return -1;

	offset = get_key_words_offset(fdt, 0, "tclinux_slave");
	if(-1 == offset)
		return -1;

	const unsigned int *reg = fdt_getprop(fdt, offset, "reg", &len);

	if(!reg || len < 2)
		return -1;

	record->magic = SLAVE_PARTITION_INFO_MAGIC;
#if defined(CONFIG_TARGET_AN7552_TPL)
	record->offset = fdt32_to_cpu(reg[0]);
#else
	record->offset = fdt32_to_cpu(reg[1]);
#endif
	record->crc = crc32(record->magic, (unsigned char *)&record->offset, sizeof(unsigned int));
	debug("%s:magic = 0x%x, offset = 0x%x, crc = 0x%x\n",__func__,record->magic,record->offset,record->crc);

	if(mtd_write_art(CAL_SLAVE_INFO, record, sizeof(slave_partition_info)))
	{
		printf("%s fail\n",__func__);
		return -1;
	}

	return 0;
}

int update_gpt_info(const char *env_content)
{
	struct mmc *mmc = __init_mmc_device(0, false, MMC_MODES_END);
	env_t *ep = NULL;
	char *partitions = NULL;

	debug("%s:env reside at %p\n",__func__, env_content);	
	partitions = env_get("partitions");
	debug("%s:current env partition:%s\n",__func__, partitions);

	if (env_import(env_content, 1, H_EXTERNAL))
		return -1;
	
	ep = (env_t *)env_content;
	gd->env_addr = (ulong)&ep->data;
	
	partitions = env_get("partitions");
	debug("%s:New env partition:%s\n",__func__, partitions);
	
	if(gpt_default(mmc_get_blk_desc(mmc), partitions))
	{
		printf("%s failed\n",__func__);
		return -1;
	}

	printf("%s success\n",__func__);
	return 0;
}

void ecnt_ImageUpgrade(int fw_type)
{
	ulong img_size, offset = 0;
	char *filename;
	void *fdt_pt = NULL;
	unsigned int *reg = NULL;
	unsigned int checkvalue = 0;
	unsigned int checksize = 0;
	unsigned long long uImage_addr = CONFIG_SYS_LOAD_ADDR;
	int mmcbootflag = 0, lenp = 0;
	unsigned char flag = 0;
	struct disk_partition master, slave;
#if !defined(CONFIG_TPL)
	struct trx_header* trx_H = NULL;
	unsigned int check_size = 0;
	unsigned int crc = 0;
	unsigned int cal_check_sum = 0;
	unsigned int magic = 0;
#endif
#ifdef TCSUPPORT_NEW_SPI
	const char* mtd_name = "spi-nand0";
#else 
	const char* mtd_name = "airoha-flash"; 
#endif
#ifdef CONFIG_OPEN_IMAGE
	char *pBufAddr = NULL;
	LINUX_FILE_TAG *pTpTag = NULL;
#endif

	memset(&master, 0, sizeof(master));
	memset(&slave, 0, sizeof(slave));
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
		//memmove((const void *)CONFIG_SYS_LOAD_ADDR, ((const void *)CONFIG_SYS_LOAD_ADDR + current_conf->gpt_header_off), (img_size - current_conf->gpt_header_off));
		memmove((void *)CONFIG_SYS_LOAD_ADDR, ((void *)CONFIG_SYS_LOAD_ADDR + current_conf->gpt_header_off), (img_size - current_conf->gpt_header_off));
		if(flash_op((TCBOOT_OFFSET + current_conf->gpt_header_off), 0, (img_size - current_conf->gpt_header_off), CONFIG_SYS_UBOOT_PARTITION_NAME, (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
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
		    printf("Verify image success, start imageUpgrade\n");
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
		}
#if !defined(CONFIG_TPL)
		if (img_size < (trx_H->len))
		{
			printf("receive image size 0x%lx is less than the length 0x%lx which stored in trx !!!",img_size, (unsigned long)trx_H->len);
			return;
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
				return;
			}
		}
#endif
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, img_size, CONFIG_SYS_TCLINUX_PARTITION_NAME, (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
			printf("Flash Op Failed\n");
		}
		printf("\r\nupgrade finished !\n");
	}
	else if (!strcmp(filename, env_get("kernel_sep_filename"))) 
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
		    printf("Verify image success, start imageUpgrade\n");
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
		}
#if !defined(CONFIG_TPL)
		if (img_size < (trx_H->len))
		{
			printf("receive image size 0x%lx is less than the length 0x%lx which stored in trx !!!",img_size, (unsigned long)trx_H->len);
			return;
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
				return;
			}
		}
#endif
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, img_size, "KernelA", (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
			printf("Flash Op Failed\n");
		}
		printf("\n");
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, img_size, "KernelB", (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
			printf("Flash Op Failed\n");
		}
		printf("\r\nupgrade finished !\n");
	}
	else if (!strcmp(filename, env_get("rootfs_filename"))) 
	{
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, img_size, "RootfsA", (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
			printf("Flash Op Failed\n");
		}
		printf("\n");
		if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, img_size, "RootfsB", (unsigned long)CONFIG_SYS_LOAD_ADDR)) {
			printf("Flash Op Failed\n");
		}
		printf("\r\nupgrade finished !\n");
	}
	else if (!strcmp(filename, "tclinux_allinone")) 
	{
		if(!is_emmc())
		{
			if(ecnt_allinone_check(uImage_addr))
				return;
			
			if(flash_op(TCBOOT_OFFSET, 0, img_size, mtd_name, (unsigned long)CONFIG_SYS_LOAD_ADDR)) 
			{
				printf("mtd write boot and master part failed\n");
				return;
			}
			else
			{
#if defined(CONFIG_TPL)
				if(!(fdt_pt = get_fdt_data_blob((const void *)(uImage_addr + TCLINUX_OFFSET))))
#else
				if(!(fdt_pt = get_fdt_data_blob((const void *)(uImage_addr + TCLINUX_OFFSET + sizeof(struct trx_header) + get_fip_offset()))))
#endif
					return;
				else
				{
					if((offset = get_key_words_offset(fdt_pt, 0, "tclinux_slave")) == -1)
						return;
					reg = (unsigned int *)fdt_getprop(fdt_pt, offset, "reg", &lenp);
					if(!reg || lenp < 2)
						return;
				}
#if defined(CONFIG_TARGET_AN7552_TPL)
				/*fdt32_to_cpu(reg[0]):tclinux_slave part start address, fdt32_to_cpu(reg[1]):tclinux_slave part length*/
				if(flash_op(0, fdt32_to_cpu(reg[0]), fdt32_to_cpu(reg[1]), mtd_name, (unsigned long)(CONFIG_SYS_LOAD_ADDR+TCLINUX_OFFSET)))
#else
				/*fdt32_to_cpu(reg[1]):tclinux_slave part start address, fdt32_to_cpu(reg[3]):tclinux_slave part length*/
				if(flash_op(0, fdt32_to_cpu(reg[1]), fdt32_to_cpu(reg[3]), mtd_name, (unsigned long)(CONFIG_SYS_LOAD_ADDR+TCLINUX_OFFSET)))
#endif
				{
					printf("mtd write slave part failed\n");
					return;
				}
			}

			update_slave_offset_info((const void *)uImage_addr, img_size);
			mtd_read_art(CAL_BOOTFLAG, &flag, sizeof(unsigned char));
			if(flag == '1')
			{
				flag = '0';
				mtd_write_art(CAL_BOOTFLAG, &flag, sizeof(unsigned char));
			}

			printf("\r\nupgrade allinone finished !\n");
		}
	}
	else if (!strcmp(filename, "tclinux_allinone_emmc")) 
	{
		if(is_emmc())
		{
			if(ecnt_allinone_check(uImage_addr))
				return;

			/*skip MBR 0 LBA*/
			memmove((void *)CONFIG_SYS_LOAD_ADDR, ((void *)(CONFIG_SYS_LOAD_ADDR + 0x200)), img_size-0x200);
			/*flash from 1 LBA*/ 
			if(flash_op(TCBOOT_OFFSET + 0x200, 0, img_size-0x200, NULL, (unsigned long)CONFIG_SYS_LOAD_ADDR))
			{
				printf("[mmc]boot and master part write Failed\n");
				return;
			}

			update_gpt_info((const char *)(CONFIG_SYS_LOAD_ADDR + ECNT_ALLINONE_ENV_OFFSET - 0x200));
			if(mmc_partitions_parse(&master, "tclinux") != 0 ||
			   mmc_partitions_parse(&slave, "tclinux_slave") != 0)
				return;
			else
			{
				if(flash_op(slave.start*slave.blksz, 0, img_size-master.start*master.blksz, NULL, ((unsigned long)(CONFIG_SYS_LOAD_ADDR-0x200+master.start*master.blksz))))
				{
					printf("[mmc]slave part write Failed\n");
					return;
				}
				else
				{
					mmc_bootflag_read(__init_mmc_device(0, false, MMC_MODES_END), &mmcbootflag);
					if(mmcbootflag != 0)
						swap_bootflag();
				}
			}

			printf("\r\nupgrade allinone finished !\n");
			/*need to consider backup gpth and gpte at the end of LBA*/
		}
	}
	#ifdef CONFIG_OPEN_IMAGE
	else if (!strcmp(filename, "boot.bin"))
	{
		if (!is_emmc())
		{
			printf("\r\n upgrading boot.bin  \n");
			const char* mtd_name = "bootloader";
			if (flash_op(0, TCBOOT_OFFSET, img_size, mtd_name, (unsigned long)CONFIG_SYS_LOAD_ADDR)) 
			{
				printf("Flash Op Failed\n");
				return;
			}

			printf("\r\nupgrade boot.bin success !\n");
		}
	}
#if 0
	else if (strstr(filename, "_FLASH.bin"))
	{
		printf("\r\n upgrading %s  \n",filename);
		if(!is_emmc())
		{
		#ifdef TCSUPPORT_NEW_SPI
			const char* mtd_name = "spi-nand1";
		#else 
			const char* mtd_name = "airoha-flash";
		#endif
			if(flash_op(0, TCBOOT_OFFSET, img_size, mtd_name, (unsigned long)CONFIG_SYS_LOAD_ADDR)) 
			{
				printf("Flash Op Failed\n");
			}
			
			boot_set_boot_index(0);
			printf("\r\nupgrade %s success !\n",filename);
		}
	}
#endif
	else if (strstr(filename, "_UP.bin"))
	{
		printf("\r\n upgrading %s  \n",filename);
		if (!is_emmc())
		{
			const char* mtd_name_kernel = "kernel";
			const char* mtd_name_rootfs = "rootfs";
			/* UP.bin: tptag + kernel + rootfs */
			pBufAddr = (char *)CONFIG_SYS_LOAD_ADDR;
			pTpTag = (LINUX_FILE_TAG *)CONFIG_SYS_LOAD_ADDR;
			
			if (img_size != (pTpTag->totalImageLen + sizeof(LINUX_FILE_TAG)))
			{
				printf("wrong image(file len %x, tag file len %x)\n", (unsigned int)img_size, (unsigned int)(pTpTag->totalImageLen + sizeof(LINUX_FILE_TAG)));
				return;
			}

			/* write kernel to flash */
			if (flash_op(0, KERNEL_OFFSET, pTpTag->kernelLen, mtd_name_kernel, (unsigned long)(pBufAddr + pTpTag->kernelAddress + sizeof(LINUX_FILE_TAG))))
			{
				printf("write kernel failed\n");
				return;
			}

			/* write rootfs to flash */
			if (flash_op(0, ROOTFS_OFFSET, pTpTag->rootfsLen, mtd_name_rootfs, (unsigned long)(pBufAddr + pTpTag->rootfsAddress + sizeof(LINUX_FILE_TAG))))
			{
				printf("write rootfs failed\n");
				return;
			}
		#ifdef INCLUDE_DUAL_IMAGE
			boot_set_boot_index(0);
		#endif /* INCLUDE_DUAL_IMAGE */
			printf("\r\nupgrade %s finished !\n",filename);
		}
	}
	#endif/*CONFIG_OPEN_IMAGE*/
}

#ifdef CONFIG_OPEN_IMAGE
/*
 * The uIP web upgrade flow (uip/apps/webserver/httpd.c, handle_update) calls
 * upgrade_firmware() with the uploaded image.  In the TP-Link SDK that
 * function lives in the Aginet component (tplink/AginetConfigV3), which is not
 * part of this GPL tree.  Do what the local image upgrade does for the
 * tclinux image instead: write it into the tclinux partition (on UBI boards
 * that partition holds the UBI volumes, so the image goes in verbatim).
 */
int upgrade_firmware(uint8_t *pFirmwareAddr, uint32_t firmwareLength)
{
	if ((NULL == pFirmwareAddr) || (0 == firmwareLength))
	{
		printf("upgrade_firmware: nothing to write\n");
		return -1;
	}

	printf("upgrade_firmware: writing %u bytes from 0x%08lx to %s\n",
			(unsigned int)firmwareLength, (unsigned long)pFirmwareAddr,
			CONFIG_SYS_TCLINUX_PARTITION_NAME);

	flush_cache((unsigned long)pFirmwareAddr, firmwareLength);

	init_image_parameter();

	if(flash_op(TCLINUX_OFFSET + current_conf->gpt_size, 0, firmwareLength,
			CONFIG_SYS_TCLINUX_PARTITION_NAME, (unsigned long)pFirmwareAddr))
	{
		printf("upgrade_firmware: Flash Op Failed\n");
		return -1;
	}

	printf("upgrade_firmware: upgrade finished !\n");

	return 0;
}
#endif /* CONFIG_OPEN_IMAGE */

__attribute__((unused))static int get_fdt_node_offset_len(unsigned char *buf, int images_noffset, const char *node, void **offset, u32 *len)
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
		printf("get fdt data error:0x%08u\n", (u32)(uintptr_t)(*offset));
		return -1;
	}
	return 0;

}

int get_tclinux_imginfo(struct tclinux_imginfo *info)
{
	struct trx_header *trx_info = NULL;
	int images_noffset;
	//uint ret = 0;
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
	//images_noffset = fdt_path_offset((unsigned long)buf, "/images/kernel");
	images_noffset = fdt_path_offset((void*)buf, "/images/kernel");
	if (images_noffset < 0) {
		printf("Can't find images parent node /image (%s)\n", fdt_strerror(images_noffset));
		return -1;
	}

	data = (void *)fdt_getprop(buf, images_noffset, "data-offset", &len);

	//printf("kernel_off data = %x readl = %x\n",data,readl(data));
	printf("kernel_off data = %p readl = %#010x\n",(void*)data, (uint32_t)readl(data));


	info->kernel_off = __swab32((readl(data)));

	data = (void *)fdt_getprop(buf, images_noffset, "data-size", &len);
	

	info->kernel_size = __swab32((readl(data)));
	//printf("kernel_size data = %x readl = %x\n",data,readl(data));
	printf("kernel_off data = %p readl = %#010x\n",(void*)data, (uint32_t)readl(data));

#ifndef TCSUPPORT_DM_VERITY

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
	images_noffset = fdt_path_offset((void*)buf, "/images/ramdisk");
#else
	images_noffset = fdt_path_offset((void*)buf, "/images/filesystem");
    

#endif

	if (images_noffset < 0) {
		printf("Can't find images parent node /image (%s)\n", fdt_strerror(images_noffset));
		return -1;
	}

	data = (void *)fdt_getprop(buf, images_noffset, "data-offset", &len);
	
	//printf("rootfs_off data = %x readl = %x\n",data,readl(data));
	printf("rootfs_off data = %p readl = %#010x\n", (void*)data, (uint32_t)readl(data));


	info->rootfs_off = __swab32((readl(data)));

	data = (void *)fdt_getprop(buf, images_noffset, "data-size", &len);
	

	info->rootfs_size = __swab32((readl(data)));
	//printf("rootfs_size data = %x readl = %x\n",data,readl(data));
	printf("rootfs_size data = %p readl = %#010x\n",(void*)data,(uint32_t)readl(data));

	info->tclinux_size += info->rootfs_size;
#endif
	info->tclinux_size += info->kernel_size;

#else
	/*Not using external data, data is mandatory*/
	images_noffset = fdt_path_offset((void*)buf, "/images");
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

int get_image_total_size(void)
{
	const void *fit = (const void*)CONFIG_SYS_LOAD_ADDR;
	int node, images;
	unsigned int image_len;
	unsigned int *image_len_be;
	unsigned int total_size = 0;

#if !defined(CONFIG_TPL)
	total_size += get_fip_offset();
	total_size += sizeof(struct trx_header);
#endif

	total_size += fdt_totalsize(fit);
	images = fdt_path_offset(fit, FIT_IMAGES_PATH);
	fdt_for_each_subnode(node, fit, images) {
		image_len_be = (unsigned int *)fdt_getprop(fit, node, "data-size", NULL);
		image_len = be32_to_cpu(*image_len_be);
		total_size += round_up(image_len, PAGE_SIZE);
	}
	printf("Read FIT image total_size=%x\n", total_size);
	return total_size;
}



int mmc_image_copy(int bootFail, unsigned long long storeAddr)
{
	struct disk_partition freepart, rootpart, usepart;
	char *usename = NULL, *freename = NULL;
	struct mmc *mmc = NULL;
	u32 m = 0, n = 0, cnt = 0;
	
	memset(&freepart, 0, sizeof(freepart));
	memset(&rootpart, 0, sizeof(rootpart));
	memset(&usepart, 0, sizeof(rootpart));
	mmc = __init_mmc_device(0, false, MMC_MODES_END);
	freename = (bootFail == 1)? "tclinux": "tclinux_slave";
	usename = (bootFail == 1)? "tclinux_slave": "tclinux";

	if(mmc_partitions_parse(&freepart, freename) == 0 &&
	   mmc_partitions_parse(&usepart, usename) == 0 &&
	   mmc_partitions_parse(&rootpart, "filesystem") == 0)
	{
		cnt = freepart.size + rootpart.size;
		n = blk_dread(mmc_get_blk_desc(mmc), freepart.start, cnt, (uchar*)storeAddr);
		printf("%d blocks read to storeaddr: %s\n", n, (n == cnt) ? "OK" : "ERROR");
		if(n != cnt)
			return -1;

		env_set("rootfs_partition_name", ((bootFail == 1)? "filesystem": "filesystem_slave"));
		m = blk_dwrite(mmc_get_blk_desc(mmc), usepart.start, cnt, (uchar*)storeAddr);
		printf("%d blocks write into %s part: %s\n", m, usename, (m == cnt) ? "OK" : "ERROR");
		printf("mmc boot from %s\n",freename);
		if(m == cnt)
			tpl_bootflag_write((bootFail == 1)? 0:1);
	}
	else
		return -1;

	return 0;
}

int mtd_image_copy(int bootFail, unsigned long long storeAddr)
{
	char *usename = NULL, *freename = NULL;
	struct mtd_info *freemtd = NULL, *usemtd = NULL;
	ulong retLen = 0;
	u32 m = 0, n = 0;
	struct erase_info er;
	
	usename = (bootFail == 1)? CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME: CONFIG_SYS_TCLINUX_PARTITION_NAME;
	freename = (bootFail == 1)? CONFIG_SYS_TCLINUX_PARTITION_NAME: CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME;
	if(setup_mtd_device(&freemtd, freename) || 
	   setup_mtd_device(&usemtd, usename))
	{
		printf("Invalid mtd partition %s and %s!\n", freename, usename);
		return -1;
	}

	n = mtd_read(freemtd, 0, freemtd->size, &retLen, (uchar*)storeAddr);
	printf("mtd read %s part Len=0x%llx byte to storeaddr %s\n", freemtd->name, freemtd->size, (n)? "ERROR": "OK");
	if(n)
		return -1;

	memset(&er, 0, sizeof(er));
	er.mtd = usemtd;
	er.addr = 0;
	er.len = usemtd->size;
	if(mtd_erase(usemtd, &er))
		return -1;
	m = mtd_write(usemtd, 0, usemtd->size, &retLen, (uchar*)storeAddr);
	printf("\nmtd write into %s part Len=0x%llx byte %s\n", usemtd->name, usemtd->size, (m)? "ERROR": "OK");
	printf("mtd boot from %s\n",freename);
	if(!m)
		tpl_bootflag_write((bootFail == 1)? 0:1);
	if(!strcmp(freename, CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME))
		env_set("root", "/dev/mtdblock6 ro"); 

	return 0;
}

/*bootmok==0: bootm failed; bootmok!=0: bootm success*/
/*need_swap=='1': need swap bootflag; need_swap!='1': no need swap bootflag*/
int boot_exception_handle_tpl(ulong bootmok)
{
	int runboot = -1;
	unsigned long long storeAddr = CONFIG_SYS_LOAD_ADDR;
	char need_swap = *(char *)TPL_BOOTFLAG_ADDR;
	char tplboot = *((char *)TPL_BOOTFLAG_ADDR + 1);

	printf("   tplbootflag %sneed swap, need_swap=%c, tplboot=%c, bootmok=%lu\n", (need_swap=='1'?"":"not "),need_swap,tplboot,bootmok);

	/*bootm OK, tpl_setup OK*/
	if(bootmok != 0 && need_swap != '1')
		return 0;

	if(bootmok == 0 || need_swap == '1')
	{
		/*need_swap=='1': no_verify + copy + no_reset + modify_bootflag*/
		/*bootmok==0: verify + copy + reset + modify_bootflag*/
		if(need_swap=='1' && (tplboot == '1' || tplboot == '0'))
			runboot = (tplboot == '0')? 1: 0;
		else
			tpl_bootflag_read(&runboot);

		if(is_emmc())
		{
			if(!mmc_image_copy(runboot, storeAddr))
			{
				/*make fit-verify only the bootm command return fail,bootmok==0:bootm fail bootmok!=0:bootm success*/
				if(bootmok == 0 && fit_all_image_verify((const void *)storeAddr) != 1)
				{
					printf("mmc fit all image verify failed\n");
					return -1;
				}
			}
			else
				return -1;
		}
		else
		{
			if(!mtd_image_copy(runboot, storeAddr))
			{
				/*make fit-verify only the bootm command return fail,bootmok==0:bootm fail bootmok!=0:bootm success*/
				if(bootmok == 0 && fit_all_image_verify((const void *)storeAddr) != 1)
				{
					printf("mtd fit all image verify failed\n");
					return -1;
				}
			}
			else
				return -1;
		}

		if(bootmok == 0)
			do_reset(NULL, 0, 0, NULL);
	}
	return 0;
}

int boot_exception_handle(struct mmc *ismmc, struct disk_partition *usepart, char *spipart, unsigned long long storeAddr)
{
	u32 m = 0, n = 0, cnt = 0;
	uchar *name = NULL, *rootname = NULL;
	struct disk_partition freepart, rootpart;
	struct mtd_info *freemtd = NULL, *usemtd = NULL;
	ulong retLen = 0;
	struct erase_info er;
	
	if(ismmc != NULL)
	{
		memset(&freepart, 0, sizeof(freepart));
		memset(&rootpart, 0, sizeof(rootpart));
		name = (!strcmp(usepart->name, "tclinux_slave"))? "tclinux": "tclinux_slave";
		rootname = (!strcmp(usepart->name, "tclinux_slave"))? "filesystem": "filesystem_slave";
		
		if(mmc_partitions_parse(&freepart, name) == 0 &&
		   mmc_partitions_parse(&rootpart, "filesystem") == 0)
		{
	 		cnt = freepart.size + rootpart.size;
			n = blk_dread(mmc_get_blk_desc(ismmc), freepart.start, cnt, (uchar*)storeAddr);
			printf("%d blocks read to storeaddr: %s\n", n, (n == cnt) ? "OK" : "ERROR");
			if((n != cnt) || upgrade_verify((char *)storeAddr) != 0)
			{
				printf("Verify image Failed!!!\r\n");
				return -1;
			}

			swap_bootflag();
			env_set("rootfs_partition_name", rootname);
			m = blk_dwrite(mmc_get_blk_desc(ismmc), usepart->start, cnt, (uchar*)storeAddr);
			printf("%d blocks write into use part: %s\n", m, (m == cnt) ? "OK" : "ERROR");
			printf("mmc boot from %s\n",name);
		}
		else
			return -1;
	}
	else
	{
#ifdef TCSUPPORT_CF
		name = (!strcmp(spipart, "KernelB"))? "KernelA" : "KernelB";
#else		
		name = (!strcmp(spipart, CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME))? CONFIG_SYS_TCLINUX_PARTITION_NAME: CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME;
#endif
		if(setup_mtd_device(&freemtd, name) || 
			setup_mtd_device(&usemtd, spipart))
		{
			printf("Invalid mtd partition %s and %s!\n", name, spipart);
			return -1;
		}
		
		n = mtd_read(freemtd, 0, freemtd->size, &retLen, (uchar*)storeAddr);
		printf("mtd read %s part Len=0x%llx byte to storeaddr %s\n", freemtd->name, freemtd->size, (n)? "ERROR": "OK");
		if(n || upgrade_verify((char *)storeAddr) != 0)
		{
			printf("Verify mtd image Failed!!!\r\n");
			return -1;
		}

		swap_bootflag();
		memset(&er, 0, sizeof(er));
		er.mtd = usemtd;
		er.addr = 0;
		er.len = usemtd->size;
		if(mtd_erase(usemtd, &er))
			return -1;
		m = mtd_write(usemtd, 0, usemtd->size, &retLen, (uchar*)storeAddr);
		printf("\nmtd write into %s part Len=0x%llx byte %s\n", usemtd->name, usemtd->size, (m)? "ERROR": "OK");
		if(!strcmp(name, CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME))
			env_set("root", "/dev/mtdblock6 ro"); 
		
		printf("mtd boot from %s\n",name);
	}
	return 0;
}

static int mtd_load_kernel_image(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	u32 blk, cnt, n;
	int partUpdate = 1, ret = 0, bootvalue = 0;
	size_t ret_len = 0;
	struct mmc *mmc = NULL;
	struct disk_partition partMsg;
	struct mtd_info *mtd_kernel;
	unsigned long long store_addr = CONFIG_SYS_LOAD_ADDR;
	const char *rootfsName = "rootfs_partition_name";
	char* bootpart = NULL;
#if defined(CONFIG_TPL)
	char needswap = *(char *)TPL_BOOTFLAG_ADDR;
	char needboot = *((char *)TPL_BOOTFLAG_ADDR + 1);
#endif	
#ifdef TCSUPPORT_CF
	struct mtd_info *mtd_bootloader;
	uint32_t startAddr = 0x80000;
	uint8_t boot_flag_buf[1]={1};
#endif

	init_image_parameter();
#if !defined(CONFIG_TPL)
	store_addr -= sizeof(struct trx_header);
	store_addr -= get_fip_offset();
#endif

	tpl_bootflag_read(&bootvalue);
#if defined(CONFIG_TPL)
	if(needswap == '1' && (needboot == '1' || needboot == '0') && bootvalue != (needboot-'0'))
		bootvalue = needboot-'0';
	else
		*(char*)TPL_BOOTFLAG_ADDR = '0';
#endif

	if (is_emmc()) 
	{
		memset(&partMsg, 0, sizeof(partMsg));
		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if(bootvalue == 0)
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
			printf("MMC boot from %s, mmcbootflag=%d\n",((bootvalue == 0)? "tclinux": "tclinux_slave"), bootvalue);
		}

		n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, (u_char *)(uintptr_t)store_addr);
		printf("%d blocks read: %s\n", n, (n == cnt) ? "OK" : "ERROR");
		if (n != cnt) {
			printf("MMC read failed\n");
			return -1;
		}
	}
	else 
	{
#ifdef TCSUPPORT_CF
		if(setup_mtd_device(&mtd_bootloader, CONFIG_SYS_UBOOT_PARTITION_NAME)) 
		{
			printf("ERROR: Invalid partition \n");
			return -1;
		}
		
		if(mtd_read(mtd_bootloader, startAddr, sizeof(boot_flag_buf), &ret_len, boot_flag_buf)) 
		{
			printf(" Failed to fetch bootflag value from bootloader partititon");
			return -1;
		}
		
		bootpart = "KernelA";
		env_set(rootfsName, "RootfsA"); 
		if(boot_flag_buf[0] == '1') 
		{
			bootpart = "KernelB";
			env_set(rootfsName, "RootfsB"); 
		}
#else
		bootpart = (bootvalue==1)?CONFIG_SYS_TCLINUX_SLAVE_PARTITION_NAME:CONFIG_SYS_TCLINUX_PARTITION_NAME;
		if(bootvalue==1)
		{
			env_set(rootfsName, "filesystem_slave");
			env_set("root", "/dev/mtdblock6 ro"); 
		}
		else
			env_set(rootfsName, "filesystem");
#endif
		printf("    \n\nmtd boot from %s\n", bootpart);
		if(setup_mtd_device(&mtd_kernel, bootpart)) 
		{
			printf("ERROR: Invalid TCLinux partition!\n");
			return -1;
		}
		
		/*read tclinux only, do not read the blank data*/
#if !defined(CONFIG_TPL)
		ret = mtd_read(mtd_kernel, 0, get_fip_offset() + sizeof(struct trx_header) + (PAGE_SIZE * SZ_1K), &ret_len, (uchar*)store_addr);
#else
		ret = mtd_read(mtd_kernel, 0, (PAGE_SIZE * SZ_1K), &ret_len, (uchar*)store_addr);
#endif
		if(ret) 
		{
			printf("Unable to load header from image\n");
			return -1;
		}
		
		if(mtd_read(mtd_kernel, 0, get_image_total_size(), &ret_len, (uchar*)store_addr)) 
		{
			printf("Failed to load the image from %s.\r\n", bootpart);
			return -1;
		}
	}

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
	if (boot_verify((char *)(store_addr)))
	{
		printf("Verify image Failed!!!\r\n");
		return -1;
	}
#else
#if !defined(CONFIG_TPL)
	if (upgrade_verify((char *)store_addr) != 0)
	{
		printf("Verify image Failed!!!\r\n");
		if(boot_exception_handle(mmc, &partMsg, bootpart, store_addr))
			return -1;
	}
#endif
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

#ifdef CONFIG_OPEN_IMAGE
#if 0
static int macStrToEth(const char *pStr, unsigned char *pMac)
{
	int i = 0, c = 0;

	if ((NULL == pStr) || (NULL == pMac))
	{
		return -1;
	}

	for (i = 0; i < 6; i++)
	{
		if (*pStr == '-' || *pStr == ':')
		{
			pStr++;
		}

		if (*pStr >= '0' && *pStr <= '9')
		{
			c  = (unsigned char) (*pStr++ - '0');
		}
		else if (*pStr >= 'a' && *pStr <= 'f')
		{
			c  = (unsigned char) (*pStr++ - 'a') + 10;
		}
		else if (*pStr >= 'A' && *pStr <= 'F')
		{
			c  = (unsigned char) (*pStr++ - 'A') + 10;
		}
		else
		{
			return -1;
		}

		c <<= 4;

		if (*pStr >= '0' && *pStr <= '9')
		{
			c |= (unsigned char) (*pStr++ - '0');
		}
		else if (*pStr >= 'a' && *pStr <= 'f')
		{
			c |= (unsigned char) (*pStr++ - 'a') + 10;
		}
		else if (*pStr >= 'A' && *pStr <= 'F')
		{
			c |= (unsigned char) (*pStr++ - 'A') + 10;
		}
		else
		{
			return -1;
		}

		pMac[i] = (unsigned char) c;

	}

	return 0;
}
#endif

static int mtd_load_image(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
		IMAGE_TAG *pTag;
		
		char macStr[18] = "00:0A:EB:13:09:69";
		unsigned char * mtd_other_block = NULL;
		unsigned char mac[6] = {0};
		struct mtd_info *mtd_misc = NULL;

		char current_misc_partition [16] = {0};
		unsigned char *buf = (unsigned char *)CONFIG_SYS_LOAD_ADDR;

		int ret = 0;
		size_t ret_len = 0;
		struct mtd_info *mtd_kernel = NULL;
		IMAGE_TAG *Tag =NULL ;

		int index = 0;
		char current_boot_partition [16] = {0};

#ifdef CONFIG_USE_IRQ
		/* close mac before decompress	*/

		gic_dic_clear_enable_all_intr();
		gic_dic_clear_pending_all_intr();
#endif
#ifdef CONFIG_ECNT_MULTIUPGRADE
		VPint(CR_MAC_MACCR)=0;
		resetSwMAC3262();
#endif

//		boot_get_compressed_kernel(buf);
#if 1

#ifdef INCLUDE_DUAL_IMAGE
		index = boot_get_boot_index();
#endif/*INCLUDE_DUAL_IMAGE*/

		if(0 == index )
		{
			memcpy(current_boot_partition, CONFIG_SYS_KERNE_PARTITION_NAME, sizeof(current_boot_partition));
			//char* current_boot_partition = CONFIG_SYS_KERNE_PARTITION_NAME;
			printf("boot will active master image\n");
		}
		else
		{
			memcpy(current_boot_partition, CONFIG_SYS_KERNE_SLAVE_PARTITION_NAME, sizeof(current_boot_partition));
			//char* current_boot_partition = CONFIG_SYS_KERNE_SLAVE_PARTITION_NAME;
			printf("boot will active slave image\n");
		}
		printf("    \n\nboot to %s.. \n", current_boot_partition);

		ret = setup_mtd_device(&mtd_kernel, current_boot_partition);
		if (ret) {
			printf("ERROR: Invalid TCLinux partition!\n");
			return -1;
		}
		
		//read TAG_LEN
		ret = mtd_read(mtd_kernel, 0, TAG_LEN, &ret_len,buf );
		Tag = (IMAGE_TAG *)buf;

		//read kernel 

		ret = setup_mtd_device(&mtd_kernel, current_boot_partition);
		if (ret) {
			printf("ERROR: Invalid TCLinux partition!\n");
			return -1;
		}

		ret = mtd_read(mtd_kernel, 0, (Tag->kernelLen + TAG_LEN), &ret_len, buf);
		if (ret) {
			printf("Failed to load the image from %s.\r\n", current_boot_partition);
		}
#endif
		pTag = (IMAGE_TAG *)buf;
		if (pTag->tagVersion != TAG_VERSION_V3 && pTag->tagVersion != TAG_VERSION_V4)
		{
			printf("tp image format error.\n");
			return -1;
		}

		memcpy(current_misc_partition, CONFIG_SYS_MISC_PARTITION_NAME, sizeof(current_misc_partition));
		ret = setup_mtd_device(&mtd_misc, current_misc_partition);
		if (ret) {
			printf("ERROR: Invalid misc partition!\n");
			return -1;
		}

		//read mac from misc partition
		//offset MTD_OFS_OTHER + OTHER_OFS_MAC
		ret_len = 0;
		mtd_other_block = (unsigned char *)malloc(MTD_BLOCK_SIZE);

		if(NULL != mtd_other_block)
		{
			ret = mtd_read(mtd_misc, MTD_BLOCK_SIZE * MTD_IDX_OTHER, MTD_BLOCK_SIZE, &ret_len,mtd_other_block );
			if (ret) {
				printf("Failed to read mac  from %s.\r\n", current_misc_partition);
			}

			printf("Flash  MAC = %02x:%02x:%02x:%02x:%02x:%02x\n", mtd_other_block[OTHER_OFS_MAC],  mtd_other_block[OTHER_OFS_MAC+1], mtd_other_block[OTHER_OFS_MAC+2],
			mtd_other_block[OTHER_OFS_MAC+3], mtd_other_block[OTHER_OFS_MAC+4], mtd_other_block[OTHER_OFS_MAC+5]);

			memcpy(mac,&mtd_other_block[OTHER_OFS_MAC],sizeof(mac));
			free(mtd_other_block);
			mtd_other_block = NULL;

			if (is_valid_ethaddr(mac))
			{
				snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
							mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			}
		}
		else
		{
			printf("malloc mem for mtd_other_block fail \n");
		}
		env_set("ethaddr", macStr);	

#ifdef INCLUDE_DUAL_IMAGE
		if (boot_get_boot_index() == 1)
		{
			env_set("root", CONFIG_ENV_ROOT_SLAVE);
		}
		else
#endif /* INCLUDE_DUAL_IMAGE */
		{
			env_set("root", CONFIG_ENV_ROOT);
		}

		if(bootargs_init(0)) {
			printf("bootargs init fail.\n");
			return -1;
		}

#if 0
	else if (!strncmp(argv[1], "macset", 6))
	{
		unsigned char mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

		if (macStrToEth(argv[2], mac) != 0 || !is_valid_ether_addr(mac))
		{
			printf("Invalid MAC addr\n");
			return -1;
		}
		//flash_read(flash_base + MTD_OFS_OTHER, MTD_BLOCK_SIZE, &retlen, buf);
		memcpy(buf + OTHER_OFS_MAC, mac, sizeof(mac));
		flash_write(flash_base + MTD_OFS_OTHER, MTD_BLOCK_SIZE, &retlen, buf);
	}

	else if (!strncmp(argv[1], "bflag", 5))
	{
		int bflag = -1;
		bflag = simple_strtoul(argv[2], NULL, 10);
		if (bflag != 0 && bflag != 1)
		{
			printf("Invalid bflag\n");
			return -1;
		}
		boot_set_boot_index(bflag);

	}	
#endif

	return 0;
}
U_BOOT_CMD(
	ldtpimg,
	1,
	0,
	mtd_load_image,
	"load tp image from mtd to memory",
	""
)
#ifdef INCLUDE_DUAL_IMAGE
static int mtd_set_bflag(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
		if (2 != argc)
		{
			return CMD_RET_USAGE;
		}
		int bflag = -1;

		if(0 == strcmp(argv[1],"0"))
		{
			bflag = 0;
		}
		else if(0 == strcmp(argv[1],"1"))
		{
			bflag = 1;
		}

		if (bflag != 0 && bflag != 1)
		{
			printf("Invalid bflag\n");
			return -1;
		}
		boot_set_boot_index(bflag);
}
U_BOOT_CMD(
	bflag,
	2,
	0,
	mtd_set_bflag,
	"set tp bflag to flash",
	"<0|1>"
)
#endif /*INCLUDE_DUAL_IMAGE*/
#endif/*CONFIG_OPEN_IMAGE*/

