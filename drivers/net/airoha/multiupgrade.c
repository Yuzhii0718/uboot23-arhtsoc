/*
 * Copyright (c) 2024 AIROHA Inc
 *
 * Author: Shubham Jain <shubham.jain@airoha.com>
		   Zhengping Zhang <zhengping.zhang@airoha.com>
 */

#include "skbuff.h"
#include "eth.h"
#include "multiupgrade.h"
#include "linux/stddef.h"
#include "common.h"
#include <asm/tc3162.h>
#include "arht_eth.h"
#include <airoha/trx.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <malloc.h>
#include <spi.h>
#include <spi_flash.h>
#include <search.h>
#include <errno.h>
#include <uuid.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <dm/device-internal.h>
#include <u-boot/crc.h>
#include <linux/mtd/mtd.h>
#include <airoha/arhtglobal.h>
#include <mmc.h>
#include <ecnt_flash.h>

#if defined(TCSUPPORT_CT)
#include <flash_layout/tc_partition.h>
#endif

unsigned long long int i=0;
int multicastupgrade_finished = 0;
void MultiUpgradeHandle(sk_buff *skb);
extern unsigned int get_fip_offset(void);
extern int fit_all_image_verify(const void *fit);
extern int mmc_bootflag_read(struct mmc *mmcptr, int *bootflag);
extern int swap_bootflag(void);
extern int do_reset(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]);
extern void* get_fdt_data_blob(const void *fit);
extern int get_key_words_offset(const void *fdt, int node, char *key);

typedef enum
{
	IMG_TCLINUX,
	IMG_TCLINUXALLINONE,
	IMG_TCBOOT,
	IMG_UNKNOWN,
} IMGTYPE;

extern struct mtd_info mtd;
extern unsigned int image_read_mode;
extern void setEtherRateLimit (int disableRateLimit);
extern int upgrade_verify(char *buf);



/*
   pkt format is shown as below:		
   byte 0~1 	pkt type		
   byte 2~3 	pkt sequence
   byte 4~11	pkt tag
   byte 12~15	total length
   byte 16~19	CRC of firmware fragment
   byte 20~23	CRC of whole firmware 
   1024bytes	data
 */

#define PART_FDT_MAGIC 				        0xD00DFEED
#define LAST_MULTIPKT					0x40
#define NOT_LAST_MULTIPKT				0x20
#define IMG_LEN_OFFSET					20
/*
SKYmulticast software img_len_offset 26
*/
#define LT_OFFSET					8
#define CRC_OFFSET					28
#define MULTI_UPGRADE_PKT_HEADLEN 	   		 32
#define MULTI_UPGRADE_DATA_LEN 		    		1024
#define TCLINUX_OFFSET 					0x0
#define FLASH_ERASE_SIZE				0x10000
#define TCBOOT_BASE    				    	0x0
#if defined(CONFIG_TPL)
#define PRELOADER_SIZE				0x80000
#endif
#define MULTI_BUF_BASE					0x81800000
#define ENV_OFFSET					0x7C000
#undef ENV_SIZE
#define ENV_SIZE					0x4000
#define TRX_SIZE					sizeof(struct trx_header)
#define GPTE_SIZE					0x4000
#define SECTOR_SIZE					0x200
#define TCLINUX_LBA					0x420
#define SECURE_MAGIC 				0xaa640001
#define DECRYPTED_FIT_ADDR 			0x8b000000

#if defined(TCSUPPORT_TCBOOT_1MB_SIZE)
#define TCBOOT_SIZE					0x100000
#else
#define TCBOOT_SIZE					0x80000
#endif

#if defined(TCSUPPORT_OPENWRT) || defined(RDKB_BUILD)
#define ROMFILE_SIZE					0x0
#else
#define ROMFILE_SIZE					0x40000
#endif

#define CONFIG_SYS_MAX_NAND_DEVICE 500
unsigned long long int receive_len = 0;
char startMultiUpgrade = 0;
extern u32 reservearea_size;
static int z=0;
static int cnt=0;
static char mtd_name[16] = {0};
static struct mtd_info nand_info[CONFIG_SYS_MAX_NAND_DEVICE];

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

static int mtd_op(struct mtd_info *mtd,unsigned long offset,unsigned long size, const char* mtd_dev, unsigned long load_addr)
{
	struct erase_info ei;
	size_t ret_len = 0;
	int ret;
	
	ret = setup_mtd_device(&mtd, mtd_dev);
	if (ret) {
		printf("ERROR: Invalid U-Boot partition!\n");
		return -1;
	}
	ei.mtd = mtd;
	ei.addr = offset;
	ei.len = size;
	printf("erase: partition=%s, len=0x%lx\n", mtd_dev, size);
	ret = mtd_erase(mtd, &ei);
	if (ret)
		return -1;
	printf("write: src=0x%lx, offset=0x%lx, len=0x%lx, dst=0x%x\n", load_addr,offset, size, 0);
	ret = mtd_write(mtd, offset, size, &ret_len, (void*)load_addr);
	if (ret)
		return -1;

	return 0;
}

static int mmc_op(unsigned long addr, unsigned long size,unsigned long load_addr)
{
	struct mmc *mmc;
	u32 blk, cnt, n;

	printf("mmc_op: addr=0x%lx, size=0x%lx, load_addr=0x%lx\n", addr,size, load_addr);
	mmc = __init_mmc_device(0, false, MMC_MODES_END);
	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return -1;
	}
	blk = addr / mmc->read_bl_len;
	cnt = size / mmc->read_bl_len;
	if((size % mmc->read_bl_len) != 0) {
		cnt++;
	}
	
	printf("MMC write: dev # %d, block # %d, count %d ...\n", 0, blk, cnt);
	n = blk_dwrite(mmc_get_blk_desc(mmc), blk, cnt, (const void *)load_addr);
	printf("%d blocks written: %s\n", n, (n == cnt) ? "OK" : "ERROR");
	if (!(n == cnt))
		return -1;

	return 0;
}

static int mmc_block_data_backup(char *partname, struct disk_partition *partition,unsigned long offset,bool backup)
{
	struct mmc *mmc;
	ulong n;

	mmc = __init_mmc_device(0, false, MMC_MODES_END);
	
	if(0 == mmc_partitions_parse(partition,partname))
	{
		printf("[%s]%s partition found:start 0x" LBAF " size 0x" LBAF "\n",__func__,partname,partition->start,partition->size);
	}
	else
		return -1;

	if(backup)
	{
		printf("MMC read: dev # %d, block # %lu, count %lu ... ", 0, partition->start, partition->size);
		n = blk_dread(mmc_get_blk_desc(mmc), partition->start, partition->size, (void *)(MULTI_BUF_BASE+offset));
		printf("%s to read %lu blocks to 0x%lx mem addr\n", (n == partition->size) ? "Success" : "Fail", n, MULTI_BUF_BASE+offset);
	}

	printf("MMC erase: dev # %d, block # %lu, count %lu ... ",0, partition->start, partition->size);

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		return -1;
	}

	n = blk_derase(mmc_get_blk_desc(mmc), partition->start, partition->size);
	printf("%s to erase %lu blocks on %s partition\n", (n == partition->size) ? "Success" : "Fail", n, partname);

	return 0;
}

__attribute__((unused))static void mmc_block_data_restore(struct disk_partition partition,const void *load_addr)
{
	struct mmc *mmc;
	ulong n;	

	if(!load_addr)
		return;
	
	mmc = __init_mmc_device(0, false, MMC_MODES_END);

	if (mmc_getwp(mmc) == 1) {
		printf("Error: card is write protected!\n");
		goto end;
	}

	printf("[%s]write %s partition from 0x" LBAF " with 0x" LBAF "block\n",__func__,partition.name,partition.start,partition.size);

	n = blk_dwrite(mmc_get_blk_desc(mmc), partition.start, partition.size, load_addr);

	if(partition.size != n)
		goto end;

	printf("%s Success\n",__func__);
end:
	return;
}


static void mtd_show_parts(struct mtd_info *mtd, int level)
{
	struct mtd_info *part;
	int i;
	list_for_each_entry(part, &mtd->partitions, node) {
		for (i = 0; i < level; i++)
			printf("\t");
		printf("  - 0x%012llx-0x%012llx : \"%s\"\n",part->offset, part->offset + part->size, part->name);
		cnt++;
		nand_info[z].name= part->name;
		nand_info[z].size= part->size;
		nand_info[z].offset= part->offset;	
		printf("cnt: %d \n ",cnt);
		mtd_show_parts(part, level + 1);
		++z;
	}
}


static void mtd_show_device(struct mtd_info *mtd)
{	
	/* Device */
	printf("* %s\n", mtd->name);
#if defined(CONFIG_DM)
	if (mtd->dev) {
		printf("  - device: %s\n", mtd->dev->name);
		printf("  - parent: %s\n", mtd->dev->parent->name);
		printf("  - driver: %s\n", mtd->dev->driver->name);
	}
#endif
	printf("  - block size: 0x%x bytes\n", mtd->erasesize);
	printf("  - min I/O: 0x%x bytes\n", mtd->writesize);

	if (mtd->oobsize) {
		printf("  - OOB size: %u bytes\n", mtd->oobsize);
		printf("  - OOB available: %u bytes\n", mtd->oobavail);
	}

	if (mtd->ecc_strength) {
		printf("  - ECC strength: %u bits\n", mtd->ecc_strength);
		printf("  - ECC step size: %u bytes\n", mtd->ecc_step_size);
		printf("  - bitflip threshold: %u bits\n",
		       mtd->bitflip_threshold);
	}

	printf("  - 0x%012llx-0x%012llx : \"%s\"\n",
	      mtd->offset, mtd->offset + mtd->size, mtd->name);
	
	printf("cnt: %d \n ",cnt);
	mtd_show_parts(mtd, 1);
	
}

void read_partitions_from_dts(void)
{	
	struct mtd_info *mtd;
	mtd_probe_devices();
	
	mtd_for_each_device(mtd) {
		if (!mtd_is_partition(mtd))
		{
			strncpy(mtd_name,mtd->name,sizeof(mtd_name));
			mtd_show_device(mtd);
		}
	}

	printf("\n returning from read part dts\n");
}
		
int multiupgrade_process(sk_buff *skb, char *mac)
{ 
#if !defined(TCSUPPORT_C1_OBM)
	if(mac[0] & 0x1)
#endif
	{
		if((memcmp((skb->data + MULTIPKT_TAG_OFFSET), MULTI_UPGRADE_PKT_TAG, (sizeof(MULTI_UPGRADE_PKT_TAG)-1)) == 0)||(memcmp((skb->data + MULTIPKT_TAG_OFFSET), MULTI_UPGRADE_PKT_TAG1, (sizeof(MULTI_UPGRADE_PKT_TAG1)-1)) == 0)){
			multicastupgrade_started =1;
			if (skb->len < 1048) {
				printf("skb->len < 1048 \r\n");
				return 0;
			}					
			if(startMultiUpgrade == 0){
				printf("\nStartMultiUpgrade\n");
				setEtherRateLimit (1);
				startMultiUpgrade = 1;
			}
			MultiUpgradeHandle(skb);
			return 0;
		}				
	}	
	if(startMultiUpgrade == 1){
		return 0;
	}
	return 1;
}

#if defined(TCSUPPORT_CT)
void multiupgrade_erase_romfile()
{
	unsigned long romfile_offset = 0;
	unsigned long backuprf_offset = 0;
	struct erase_info er;
	struct mtd_info *mtd_art,*mtd_romfile;
	unsigned long long int  retlen;
	
	memset(&er, 0, sizeof(er));
	if(setup_mtd_device(&mtd_art, CONFIG_SYS_ART_PARTITION_NAME)) 
	{
		printf("%s: Failed to get %s partition\n", __func__, CONFIG_SYS_ART_PARTITION_NAME);
		return -1;
	}
	er.mtd = mtd_art;
	er.addr = BACKUPROMFILE_RA_OFFSET;
	er.len = BACKUPROMFILE_RA_SIZE;
	if(mtd_erase(mtd_art, &er))
	{
		printf("%s: Failed to erase 0x%x size\n", __func__, er.len);
		return -1;
	}
	printf("mtd erase bk romfile ,offset = 0x%x, len = 0x%x .\n ",er.addr,er.len);

	memset(&er, 0, sizeof(er));
	if(setup_mtd_device(&mtd_romfile, "romfile")) 
	{
		printf("%s: Failed to get romfile partition\n", __func__);
		return -1;
	}
	er.mtd = mtd_romfile;
	er.addr = 0;
	er.len = mtd_romfile->size;
	if(mtd_erase(mtd_romfile, &er))
	{
		printf("%s: Failed to erase 0x%x size\n", __func__, er.len);
		return -1;
	}
	printf("mtd erase romfile ,offset = 0x%x, len = 0x%x .\n ",er.addr,er.len);
}

void UpdateBootEnv()
{
	struct mtd_info *mtd_bootloader;
	int ret = 0;
	size_t ret_len = 0; 

	ret = setup_mtd_device(&mtd_bootloader, CONFIG_SYS_UBOOT_PARTITION_NAME);
	if (ret) 
	{
		printf("ERROR: Invalid partition \n");
		return -1;
	}
			
	ret = mtd_read(mtd_bootloader, ENV_OFFSET, ENV_SIZE, &ret_len, MULTI_BUF_BASE+ENV_OFFSET);
	if (ret) 
	{
		printf(" Failed to fetch bootflag value from bootloader partititon");
		return -1;
	}
	printf("update boot env !  \n\r");

}
#endif

void MultiWriteImage(char *ptr, unsigned long datalen, int isAllinone)
{	
	void *fdt_pt = NULL;
	unsigned int *reg = NULL;
	unsigned long offset = 0;
	unsigned char flag = 0;
	unsigned long long int ret; 
	unsigned long long int checksize = 0;
	unsigned long long int crc = 0;		
	unsigned long long int cal_check_sum = 0;
	struct mtd_info *mtd_bootloader, *mtd_kernel , *mtd1, *mtd2;
	static int temp_idx=1;
	int i, mmcbootflag = 0, lenp = 0;
	unsigned long long uImage_addr = MULTI_BUF_BASE;
	struct trx_header* trx_H = NULL;
	struct disk_partition part, master, slave;

	memset(&master, 0, sizeof(master));
	memset(&slave, 0, sizeof(slave));
	if(!is_emmc())
		read_partitions_from_dts();
	else
	{
		if(mmc_partitions_parse(&master, "tclinux") != 0 ||
		   mmc_partitions_parse(&slave, "tclinux_slave") != 0)
			return;
	}
		
	switch(isAllinone) {
		case IMG_TCBOOT:
			printf("multi-upgrade tcboot.bin file\n");
			printf("Write to flash from 0x%X to 0x%X with 0x%X bytes\n", MULTI_BUF_BASE, TCBOOT_BASE, TCBOOT_SIZE);
			if(is_emmc())
			{
				/*flash from 2 LBA, tcboot.bin skip 2*512*/
			 	ret = mmc_op(0x400,datalen-0x400,MULTI_BUF_BASE+0x400);
			}
			else
			{
				ret = mtd_op(mtd_bootloader,0,datalen,CONFIG_SYS_UBOOT_PARTITION_NAME,MULTI_BUF_BASE);
			}
			if(ret) { 
				printf("final write failed\n"); 
			}
			printf("tcboot written DONEONEONEOEOE\n");	
			multicastupgrade_finished =1;
			break;

		case IMG_TCLINUX:
			printf("multi-upgrade tclinux.bin file\n");
			printf("Write to flash from 0x%X to 0x%X with 0x%lX bytes\n", MULTI_BUF_BASE, TCLINUX_OFFSET, datalen);

#if !defined(CONFIG_TPL)
			if (upgrade_verify((char *)uImage_addr) != 0)
			{
				printf("Verify image fail!!!\r\n");
				return;
			}
			printf("Verify image success, start imageUpgrade\n");

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
			if (image_read_mode==1)
				uImage_addr = DECRYPTED_FIT_ADDR;
			else
				uImage_addr += get_fip_offset();	
						
			trx_H = (struct trx_header *)uImage_addr;
			uImage_addr += sizeof(struct trx_header);
#else
			trx_H = (struct trx_header*)((unsigned char*)MULTI_BUF_BASE + get_fip_offset());
			uImage_addr += sizeof(struct trx_header) + get_fip_offset();
#endif
				
			if (datalen < (trx_H->len))
			{
				printf("receive image size 0x%lx is less than the length 0x%lx which stored in trx !!!",datalen, (unsigned long)trx_H->len);
				return;
			}
			else
			{
				checksize = trx_H->len - trx_H->header_len;
				cal_check_sum = crc32_no_comp(DEFAULT_CRC,(const unsigned char*)uImage_addr, checksize);
				crc = __swab32(ntohl(trx_H->crc32));
			
				if (cal_check_sum != crc)
				{
					printf("crc check error!!! \r\n");
					return;
				}
			}
#endif

			if(is_emmc())
				/*flash from 420 LBA*/
				ret = mmc_op(master.start*master.blksz,datalen,MULTI_BUF_BASE);
			else
				ret = mtd_op(mtd_kernel,TCLINUX_OFFSET,datalen,CONFIG_SYS_TCLINUX_PARTITION_NAME,MULTI_BUF_BASE);
		
			if (ret)
			{
				printf("tclinux write failed!!! \r\n");
				return;
			}
			printf("\r\n TCLINUX upgrade finished !\n");
			multicastupgrade_finished = 1; 
			break;

		case IMG_TCLINUXALLINONE:
			
#if defined(CONFIG_TPL)
			if (upgrade_verify((char *)MULTI_BUF_BASE) != 0)
			{
				printf("Verify image failed!!!\r\n");
				return;
			}

			if (crc32_no_comp(DEFAULT_CRC,(const unsigned char*)MULTI_BUF_BASE , PRELOADER_SIZE-CONFIG_ENV_SIZE) != 0)
			{
				printf("ERROR: CRC/Hash check failed !\n");
				return;
			}
			
			printf("CRC check success, start imageUpgrade\n");
#endif			
			if(is_emmc())
			{
				mmc_block_data_backup("rootfs_data",&part,datalen,false);
				/*flash from 1 LBA--include 1 blcok gpth , 1022 block tcboot, 32 block gpte,tclinux.bin(secure header + trx header + ...)*/
				ret = mmc_op(0x200,datalen-0x200,MULTI_BUF_BASE+0x200);
				if(ret)
				{
					printf("[mmc]allinone write failed\n"); 
					multicastupgrade_fail =1;
					return;
				}

				debug("[current gpt] master start = 0x%08x,  slave start = 0x%08x\n",master.start, slave.start);
				if(0 == update_gpt_info(MULTI_BUF_BASE + CONFIG_ENV_OFFSET))
				{
					/*use new gpt partitions*/
					memset(&master, 0, sizeof(master));
					memset(&slave, 0, sizeof(slave));
					if(mmc_partitions_parse(&master, "tclinux") != 0 || 
						mmc_partitions_parse(&slave, "tclinux_slave") != 0)
					{
						printf("[mmc]fail to parse new gpt info\n");
						multicastupgrade_fail =1;						
						return;
					}
					debug("[new gpt] master start = 0x%08x,  slave start = 0x%08x\n",master.start, slave.start);
				}
				
				/*flash tclinux_slave+filesystem_slave--from 1e420 LBA, skip 1 block Mbr, 1 block gpth, 1022 block tcboot, 32 block gpte*/
				ret = mmc_op(slave.start*slave.blksz,datalen-master.start*master.blksz,MULTI_BUF_BASE+master.start*master.blksz);
				if(ret)
				{
					printf("[mmc]slave write failed\n");
					multicastupgrade_fail =1;
					return;
				}

				mmc_bootflag_read(__init_mmc_device(0, false, MMC_MODES_END), &mmcbootflag);
				if(mmcbootflag != 0)
					swap_bootflag();
			}
			else
			{
				printf("mtd name: %s. allinone size : %d\n ",mtd_name,datalen);
#if defined(TCSUPPORT_CT)
				UpdateBootEnv();
#endif		

				multicastupgrade_fail =1;
				if(mtd_op(mtd1,0,datalen,mtd_name,MULTI_BUF_BASE)) 
				{ 
					printf("mtd write boot and master part failed\n");
					return;
				}
#if !defined(TCSUPPORT_CT)
				else
				{
#if defined(CONFIG_TPL)
					if(!(fdt_pt = get_fdt_data_blob((const void *)(MULTI_BUF_BASE + TCBOOT_SIZE))))
#else
					if(!(fdt_pt = get_fdt_data_blob((const void *)(MULTI_BUF_BASE + TCBOOT_SIZE + TRX_SIZE + get_fip_offset()))))
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
					if(mtd_op(mtd2, fdt32_to_cpu(reg[0]), fdt32_to_cpu(reg[1]), mtd_name, (unsigned long)(MULTI_BUF_BASE+TCBOOT_SIZE)))
#else
					/*fdt32_to_cpu(reg[1]):tclinux_slave part start address, fdt32_to_cpu(reg[3]):tclinux_slave part length*/
					if(mtd_op(mtd2, fdt32_to_cpu(reg[1]), fdt32_to_cpu(reg[3]), mtd_name, (unsigned long)(MULTI_BUF_BASE+TCBOOT_SIZE)))
#endif
					{
						printf("mtd write slave part failed\n");
						return;
					}
				}
#endif
				multicastupgrade_fail =0;
#if defined(TCSUPPORT_CT)
				multiupgrade_erase_romfile();
#else
				update_slave_offset_info(MULTI_BUF_BASE, datalen);
				mtd_read_art(CAL_BOOTFLAG, &flag, sizeof(unsigned char));
				if(flag == '1')
				{
					flag = '0';
					mtd_write_art(CAL_BOOTFLAG, &flag, sizeof(unsigned char));
				}
#endif
			}

			printf("tcallinone written successfully\n");
			multicastupgrade_finished = 1;
			break;
		default:
			printf("Unknown image.\n");
			break;
	}

	printf("upgrade finished !\n");

}

IMGTYPE get_image_type(void)
{
#if defined(CONFIG_TPL)
		unsigned long tclinux_fdt_addr = MULTI_BUF_BASE;
#else
#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC

	unsigned int magic = 0;
	unsigned long long store_addr = MULTI_BUF_BASE;
	magic = *(const unsigned int *)store_addr;
	bool is_allinone = false;

	if(SECURE_MAGIC != magic){
		is_allinone = true;
		if(is_emmc())
			store_addr+=(TCBOOT_SIZE+GPTE_SIZE);
		else
			store_addr+=(TCBOOT_SIZE+ROMFILE_SIZE);

		magic = *(const unsigned int *)store_addr;
	}
	
	
	if(SECURE_MAGIC == magic)
	{
		if (upgrade_verify((char *)store_addr) != 0)
		{
			printf("Verify image failed!!!\r\n");
			return;
		}
		printf("Verify image success, start imageUpgrade\n");
	}
	else
	{	
		printf(" FIP no magic num!!!\r\n");
		return;
	}
	
	if (image_read_mode==1)
		store_addr = DECRYPTED_FIT_ADDR;
	else
		store_addr += get_fip_offset();

	magic = *(const unsigned int *)store_addr;
	if(TRX_MAGIC2 == magic){
		store_addr += sizeof(struct trx_header);
	}	
#endif

	unsigned long tclinux_fdt_addr = MULTI_BUF_BASE+TRX_SIZE+get_fip_offset();
#endif
	unsigned long allinone_fdt_addr = MULTI_BUF_BASE;

#if defined(CONFIG_TPL)
	if(is_emmc())
		allinone_fdt_addr = MULTI_BUF_BASE+PRELOADER_SIZE+GPTE_SIZE;
	else
		allinone_fdt_addr = MULTI_BUF_BASE+PRELOADER_SIZE;	
#else
#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
	if(is_allinone)
		allinone_fdt_addr=store_addr;
	else
		tclinux_fdt_addr=store_addr;
#else
	if(is_emmc())
		allinone_fdt_addr = MULTI_BUF_BASE+TCBOOT_SIZE+GPTE_SIZE+TRX_SIZE+get_fip_offset();
	else
		allinone_fdt_addr = MULTI_BUF_BASE+TCBOOT_SIZE+ROMFILE_SIZE+TRX_SIZE+get_fip_offset();
#endif
#endif

	
	
	if(PART_FDT_MAGIC == (unsigned long) fdt_magic(tclinux_fdt_addr)){
		if (fit_all_image_verify((const void *) tclinux_fdt_addr)== -1){
			printf("hash check error!!! \r\n");
			return IMG_UNKNOWN;
		}
		return IMG_TCLINUX;
	}
	else{
		if(PART_FDT_MAGIC == (unsigned long) fdt_magic(allinone_fdt_addr)){
			printf(" it is tclinux_allineone!!! \r\n"); 
			if (fit_all_image_verify((const void *) allinone_fdt_addr)== -1){
				printf("hash check error!!! \r\n");
				return IMG_UNKNOWN;
			}			
		}
		else{
			printf("\n Not all in one image");
			return IMG_UNKNOWN;
		}
		return IMG_TCLINUXALLINONE;
	}

}

void MultiUpgradeHandle(sk_buff *skb)
{ 
	unsigned long LenAndType = __swab32(ntohl(*(unsigned long *)(skb->data + LT_OFFSET)));
	unsigned long pktseqMask = 0xffff;
	unsigned long  pktseq = ((LenAndType >> 16) & pktseqMask);
	unsigned long long int imagelen = __swab32(ntohl(*(unsigned long *)(skb->data + IMG_LEN_OFFSET)));
	unsigned char last_packet = 0;
	static int oldSeq=-1;
	static int startSeq = -1;
	static int maxSeq = 0, recvSeq = 0;
	IMGTYPE isallinone = IMG_UNKNOWN;
	

	if(i%1000==0)
	{	i=0;
		printf("*"); 
	}
	++i;
	
	if(multicastupgrade_finished == 1){
		printf("Finished multiupgrade\n");
		return;
	}
	if (startSeq == -1)
	{ 
		startSeq = pktseq;
		oldSeq = pktseq - 1; 		
		receive_len = 0;
		maxSeq = imagelen / MULTI_UPGRADE_DATA_LEN;
		if (imagelen % MULTI_UPGRADE_DATA_LEN)
			maxSeq++;
		
	}
	/*		check seq	*/
	if (pktseq!=((oldSeq+1) & pktseqMask))	
	{
		if ((pktseq != 1) || (oldSeq != (maxSeq & pktseqMask))) // on only one condition it would be fine: pktseq == 1 && oldSeq == macSeq
		{
			goto error;
		}
	}

	oldSeq = pktseq;        // always assign current packet seq to old seq, or the (oldSeq != maxSeq) would be false after the loop meets the end
	recvSeq++;
	if((startSeq != -1) && (maxSeq == recvSeq)) 
	{
		last_packet = 1;
	}
	else
	{
		last_packet = 0;
	}
	//memcpy(MULTI_BUF_BASE + ((((recvSeq-1) + (startSeq - 1)) % maxSeq)*MULTI_UPGRADE_DATA_LEN), skb->data + MULTI_UPGRADE_PKT_HEADLEN, MULTI_UPGRADE_DATA_LEN);
	memcpy((void *)(uintptr_t)(MULTI_BUF_BASE + ((((recvSeq-1) + (startSeq - 1)) % maxSeq)*MULTI_UPGRADE_DATA_LEN)), skb->data + MULTI_UPGRADE_PKT_HEADLEN, MULTI_UPGRADE_DATA_LEN);

	receive_len += MULTI_UPGRADE_DATA_LEN;
 
	if(last_packet == 1){
		printf(" Last Packet Found\n"); 
		
		if (imagelen == 0) {
        printf("Error: no file loaded via multicast.\r\n");
		multicastupgrade_fail =1;
		return;
		}	

		if(IMG_UNKNOWN == (isallinone = get_image_type()))
		{
			multicastupgrade_fail =1;
			goto error;
		}
	
		
			MultiWriteImage((void*)MULTI_BUF_BASE, imagelen,isallinone);
			setEtherRateLimit (0);
		
	}

	return;
	
	error:
						startSeq = -1;
						maxSeq = 0;
						recvSeq = 0;
						return;

}
