/*  Copyright(c) 2009-2024 TP-Link Systems Inc.	All rights reserved.
 *
 * file		bootlib.c
 * brief	
 * details	
 *
 * author	wangwenhao
 * version	1.0
 * date		4May17
 *
 * warning	
 *
 * history arg	1.0, 4May17
 */

#include <common.h>
#include <asm/byteorder.h>

#ifdef INCLUDE_MTD_TYPE_FS
/* SDK include */
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#endif /* INCLUDE_MTD_TYPE_FS */
//#include <flashhal.h>

#include "asm/tc3162.h"
#include "bootlib.h"
#ifdef INCLUDE_UIP_FWUPGRADE
#include "btn_gpio_def.h"
#endif /* INCLUDE_UIP_FWUPGRADE */

#include <linux/mtd/mtd.h>
#include <led.h>
#include <mtd.h>
#include <command.h>
#include <dm/uclass-internal.h>

#if defined(INCLUDE_WIFI_MTK_MT7992) || defined(INCLUDE_WIFI_MTK_MT7993) || defined(INCLUDE_WIFI_MTK_MT7996)
#include "reset_gpio_def.h"
#endif /* INCLUDE_WIFI_MTK_MT7992 || defined(INCLUDE_WIFI_MTK_MT7993) || INCLUDE_WIFI_MTK_MT7996 */
/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/

#ifdef INCLUDE_UIP_FWUPGRADE
#define BTN_PRESSED(x)  (!sdk_gpio_getval((x)->gpio) ^ (x)->reverse)
#endif /* INCLUDE_UIP_FWUPGRADE */

#ifdef INCLUDE_MTD_TYPE_FS

#ifndef __LINUX_JFFS2_H__
/* copy from kernel include/linux/jffs2.h */
/* Values we may expect to find in the 'magic' field */
#define JFFS2_OLD_MAGIC_BITMASK 0x1984
#define JFFS2_MAGIC_BITMASK 0x1985
#define KSAMTIB_CIGAM_2SFFJ 0x8519 /* For detecting wrong-endian fs */
#define JFFS2_EMPTY_BITMASK 0xffff
#define JFFS2_DIRTY_BITMASK 0x0000

#define JFFS2_MIN_DATA_LEN 128

#define JFFS2_NODE_ACCURATE 0x2000
/* INCOMPAT: Fail to mount the filesystem */
#define JFFS2_FEATURE_INCOMPAT 0xc000

#define JFFS2_NODETYPE_DIRENT (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 1)
#define JFFS2_NODETYPE_INODE (JFFS2_FEATURE_INCOMPAT | JFFS2_NODE_ACCURATE | 2)

/* copy end */
#endif /* __LINUX_JFFS2_H__ */


/***************************** Modify here if little endian ***************************************/
#define je16_to_cpu(x) ((x).v16)
#define je32_to_cpu(x) ((x).v32)
/***************************************** End ****************************************************/

#define CHAR2ID(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))


/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned char __u8;
typedef unsigned short __u16;
typedef unsigned int __u32;

#ifndef __LINUX_JFFS2_H__
/* copy from kernel include/linux/jffs2.h */
typedef struct {
	__u32 v32;
} __attribute__((packed)) jint32_t;

typedef struct {
	__u32 m;
} __attribute__((packed)) jmode_t;

typedef struct {
	__u16 v16;
} __attribute__((packed)) jint16_t;

struct jffs2_raw_dirent
{
	jint16_t magic;
	jint16_t nodetype;	/* == JFFS2_NODETYPE_DIRENT */
	jint32_t totlen;
	jint32_t hdr_crc;
	jint32_t pino;
	jint32_t version;
	jint32_t ino; /* == zero for unlink */
	jint32_t mctime;
	__u8 nsize;
	__u8 type;
	__u8 unused[2];
	jint32_t node_crc;
	jint32_t name_crc;
	__u8 name[0];
};

struct jffs2_raw_inode
{
	jint16_t magic;      /* A constant magic number.  */
	jint16_t nodetype;   /* == JFFS2_NODETYPE_INODE */
	jint32_t totlen;     /* Total length of this node (inc data, etc.) */
	jint32_t hdr_crc;
	jint32_t ino;        /* Inode number.  */
	jint32_t version;    /* Version number.  */
	jmode_t mode;       /* The file's type or mode.  */
	jint16_t uid;        /* The file's owner.  */
	jint16_t gid;        /* The file's group.  */
	jint32_t isize;      /* Total resultant size of this inode (used for truncations)  */
	jint32_t atime;      /* Last access time.  */
	jint32_t mtime;      /* Last modification time.  */
	jint32_t ctime;      /* Change time.  */
	jint32_t offset;     /* Where to begin to write.  */
	jint32_t csize;      /* (Compressed) data size */
	jint32_t dsize;	     /* Size of the node's data. (after decompression) */
	__u8 compr;       /* Compression algorithm used */
	__u8 usercompr;   /* Compression algorithm requested by the user */
	jint16_t flags;	     /* See JFFS2_INO_FLAG_* */
	jint32_t data_crc;   /* CRC for the (compressed) data.  */
	jint32_t node_crc;   /* CRC for the raw inode (excluding data)  */
	__u8 data[0];
};
/* copy end */
#endif /* __LINUX_JFFS2_H__ */

#endif /* INCLUDE_MTD_TYPE_FS */

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/
/* extern unsigned long kerSysReadFromFlash( void *toaddr, unsigned long fromaddr, unsigned long len ); */

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/

void LED_OEN(uint8_t x);
void turn_high_gpio(uint8_t x);
void turn_low_gpio(uint8_t x);

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/
static unsigned char *l_buf __maybe_unused = NULL;

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/

/************************************ SDK Specified ***********************************************/

static void __maybe_unused sdk_gpio_output_mode(unsigned short gpio)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	if (gpio > 31)
	{
		GPIO->GPIOCtrl |= GPIO_NUM_TO_MASK(gpio - 32);
	}
	else
	{
		GPIO->LEDCtrl &= ~GPIO_NUM_TO_MASK(gpio);
	}
	GPIO->GPIODir |= GPIO_NUM_TO_MASK(gpio);
#endif
	LED_OEN((unsigned char)gpio);
}

static void __maybe_unused sdk_gpio_setval(unsigned short gpio, unsigned char val)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	volatile int i;
	
	if (val)
	{
		GPIO->GPIOio |= GPIO_NUM_TO_MASK(gpio);
	}
	else
	{
		GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(gpio);
	}

	/* barrier for BCM, before set take effect */
	for (i = 0; i < 100; i++);
#endif
	if (val == 0)
	{
		turn_high_gpio((unsigned char)gpio);
	}
	else if (val == 1)
	{
		turn_low_gpio((unsigned char)gpio);
	}
}

#ifdef INCLUDE_MTD_TYPE_FS

static int sdk_flash_check_bad(unsigned int ofs)
{
	return check_bad_block(ofs);
}

#ifdef INCLUDE_FS_TYPE_JFFS2
static int sdk_flash_write_cleanmark(unsigned int ofs)
{
#define OOB_CM_SIZE 8
#if __BYTE_ORDER == __LITTLE_ENDIAN
	static unsigned short l_cleanmarker[] = {0x1985, 0x2003, 0x0008, 0x0000};
#else
	static unsigned short l_cleanmarker[] = {0x1985, 0x2003, 0x0000, 0x0008};
#endif
	/* MTK hw ECC not support jffs2, by wangwenhao 29Nov18 */
	/* ranand_write_oob((char *)l_cleanmarker, ofs, OOB_CM_SIZE); */
	return 0;
}
#endif /* INCLUDE_FS_TYPE_JFFS2 */

#endif /* INCLUDE_MTD_TYPE_FS */

#if 0/* defined(CFG_RAMAPP) */

static int sdk_flash_read(unsigned char *buf, unsigned int ofs, unsigned int len)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	int sect;
	unsigned char *start;
	
	ofs += FLASH_BASE;
	sect = flash_get_blk((int) ofs);
	start = flash_get_memptr(sect);
	
    return flash_read_buf( sect, (int) ofs - (int) start, buf, len );
#endif
	unsigned long retlen = 0;
	flash_read(ofs, len, &retlen, buf);
	return (int)retlen;
}

static int sdk_flash_erase(unsigned int ofs)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	int sect;

	ofs += FLASH_BASE;
	sect = flash_get_blk((int) ofs);

	if (flash_sector_erase_int(sect) < 0)
	{
		return -1;
	}
	return 0;
#endif

	return flash_erase(ofs, MTD_BLOCK_SIZE);;
}

static int sdk_flash_write(unsigned char *buf, unsigned int ofs, unsigned int len)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	int sect;
	unsigned char *start;

	ofs += FLASH_BASE;
	sect = flash_get_blk((int) ofs);
	start = flash_get_memptr(sect);
	
	return flash_write_buf(sect, (int) ofs - (int) start, buf, len);
#endif
	unsigned long retlen = 0;
	flash_write(ofs, len, &retlen, buf);
	return (int)retlen;
}

#ifdef INCLUDE_MTD_TYPE_FS
static unsigned int sdk_flash_get_size(void)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	return flash_get_total_size();
#endif
	return INCLUDE_FLASH_BIN_SIZE;
}

#ifdef INCLUDE_MTD_TYPE_FS
extern struct mtk_nand_host *host;
static int sdk_remove_oob(unsigned char *buf, unsigned int *plen)
{
	unsigned int page_size = host->mtd.writesize;
	unsigned int oob_size = host->mtd.oobsize;
	unsigned int len = *plen;
	unsigned char *src = buf;
	unsigned char *dst = buf;
	unsigned char *end = buf + len;

	if (buf == NULL || plen == NULL)
	{
		return -1;
	}

	if (len == 0)
	{
		return 0;
	}

	if (len % (page_size + oob_size))
	{
		printf("oob image size not aligned!\n");
		return -1;
	}

	page_size /= 4;
	oob_size /= 4;
	
	while(src < end)
	{
		memmove(dst, src, page_size);
		dst += page_size;
		src += (page_size + oob_size);
	}

	*plen = dst - buf;

	return 0;
}
#endif /* INCLUDE_MTD_TYPE_FS */
#endif /* INCLUDE_MTD_TYPE_FS */

#else /* CFG_RAMAPP */


/* extern int nand_read_buf(unsigned short blk, int offset, unsigned char *buffer, int len); */

static int sdk_flash_read(unsigned char *buf, unsigned int ofs, unsigned int len)
{
/*
 * brief	
 * By	wangwenhao, 21Nov18
 */
#if 0
	int sect;
	
	sect = ofs / MTD_BLOCK_SIZE;
	ofs = ofs & (MTD_BLOCK_SIZE - 1);
	
    return nand_read_buf( sect, (int) ofs, buf, len );
#else
	return 0;
#endif
}

static int __maybe_unused sdk_flash_erase(unsigned int ofs)
{
	return 0;
}

static int __maybe_unused sdk_flash_write(unsigned char *buf, unsigned int ofs, unsigned int len)
{
	return 0;
}

#ifdef INCLUDE_MTD_TYPE_FS
static unsigned int sdk_flash_get_size(void)
{
	return 0;
}

static int sdk_remove_oob(unsigned char *buf, unsigned int *plen)
{
	return 0;
}
#endif /* INCLUDE_MTD_TYPE_FS */
	
#endif /* CFG_RAMAPP */


/********************************** Generic Functions *********************************************/

void LED_OEN(uint8_t x)
{
	if(x>31)
	{
		if(x>47)
		{
			regWrite32(CR_GPIO_CTRL3,regRead32(CR_GPIO_CTRL3)|(1<<((x-48)*2)));
		}
		else
		{
			regWrite32(CR_GPIO_CTRL2,regRead32(CR_GPIO_CTRL2)|(1<<((x-32)*2)));
		}
		regWrite32(CR_GPIO_ODRAIN1,regRead32(CR_GPIO_ODRAIN1)|(1<<(x-32)));
	}
	else
	{
		if(x > 15)
		{
			regWrite32(CR_GPIO_CTRL1,regRead32(CR_GPIO_CTRL1)|(1<<((x-16)*2)));
		}
		else
		{
			regWrite32(CR_GPIO_CTRL,regRead32(CR_GPIO_CTRL)|(1<<(x*2)));
		}
		regWrite32(CR_GPIO_ODRAIN,regRead32(CR_GPIO_ODRAIN)|(1<<(x)));
	}
}

void turn_high_gpio(uint8_t x)
{
	if(x > 31 && x < 64)
	{
		/* GPIO 32 ~ 63 */
		regWrite32(CR_GPIO_DATA1,regRead32(CR_GPIO_DATA1)|(1<<(x-32)));
	}
	else if(x < 32)
	{
		/* GPIO 0 ~ 31 */
		regWrite32(CR_GPIO_DATA,regRead32(CR_GPIO_DATA)|(1<<x));
	}
	else
	{
		printf("error GPIO index:%d.\n", x);
	}
}

void turn_low_gpio(uint8_t x)
{
	if(x > 31 && x < 64)
	{
		/* GPIO 32 ~ 63 */
		regWrite32(CR_GPIO_DATA1,regRead32(CR_GPIO_DATA1)& ~(1<<(x-32)));
	}
	else if(x < 32)
	{
		/* GPIO 0 ~ 31 */
		regWrite32(CR_GPIO_DATA,regRead32(CR_GPIO_DATA)& ~(1<<x));
	}
	else
	{
		printf("error GPIO index:%d.\n", x);
	}
}

#ifdef INCLUDE_DUAL_IMAGE
static unsigned char boot_csum8(unsigned char *buf, unsigned int len)
{
	unsigned int i;
	unsigned char sum;

	for (i = 0, sum = 0; i < len; i++, buf++)
	{
		sum += *buf;
	}
	return sum;
}
#endif /* INCLUDE_DUAL_IMAGE */

#if 0
#ifdef INCLUDE_MTD_TYPE_FS
static int boot_part_read(unsigned int start, unsigned int end, 
					unsigned int *pofs, unsigned char *buf, unsigned int len)
{
	unsigned int ofs = start;
	unsigned int readlen = 0;
	
	if (pofs != NULL)
	{
		ofs = *pofs;
	}

	if (ofs & (MTD_BLOCK_SIZE - 1))
	{
		return -1;
	}

	while(ofs < end)
	{
		if (sdk_flash_check_bad(ofs) == 0)
		{
			if (len > MTD_BLOCK_SIZE)
			{
				readlen = MTD_BLOCK_SIZE;
			}
			else
			{
				readlen = len;
			}

			/* TODO: BCM change to page read, so no need pad. Add pad for other SDK? */
			if (sdk_flash_read(buf, ofs, readlen) == readlen)
			{
				len -= readlen;
				buf += readlen;

				if (len == 0)
				{
					if (pofs != NULL)
					{
						*pofs = ofs;
					}

					return 0;
				}
			}
		}
		ofs += MTD_BLOCK_SIZE;
	}

	return -1;
}

static int boot_part_write(unsigned int start, unsigned int end, 
					unsigned int *pofs, unsigned char *buf, unsigned int len)
{
	unsigned int ofs = 0;
	unsigned int writelen = 0;

	if (pofs != NULL)
	{
		ofs = *pofs;
	}

	if (ofs & (MTD_BLOCK_SIZE - 1))
	{
		printf("Offset not aligned\n");
		return -1;
	}

	for (ofs = start; ofs < end; ofs += MTD_BLOCK_SIZE)
	{
		if (sdk_flash_check_bad(ofs) == 0)
		{
			/* printf("Erase %08X\n", ofs); */
			if (sdk_flash_erase(ofs) == 0)
			{
				if (len == 0)
				{
#ifdef INCLUDE_FS_TYPE_JFFS2
#error never write cleanmark!!!!!!!!!!!!!!!!!!!!!!!!!!!
					sdk_flash_write_cleanmark(ofs);
#endif /* INCLUDE_FS_TYPE_JFFS2 */
					continue;
				}
				
				if (len > MTD_BLOCK_SIZE)
				{
					writelen = MTD_BLOCK_SIZE;
				}
				else
				{
					writelen = len;
				}

				/* printf("Write %08X len %08X\n", ofs, writelen); */
				if (sdk_flash_write(buf, ofs, writelen) == writelen)
				{
#ifdef INCLUDE_FS_TYPE_JFFS2
					sdk_flash_write_cleanmark(ofs);
#endif /* INCLUDE_FS_TYPE_JFFS2 */

					len -= writelen;
					buf += writelen;

					if (len == 0 && pofs != NULL)
					{
						*pofs = ofs;
					}
				}
			}
		}
	}

	if (len != 0)
	{
		return -1;
	}
	
	return 0;
}

/* This function returns the struct with the shared buff. Copy the value you need before reuse buff */
static struct jffs2_raw_dirent *boot_get_dirent(unsigned int start, unsigned int end, char *prefix)
{
	unsigned int current = 0;
	unsigned char *p;
	struct jffs2_raw_dirent *pdir;
	int prefix_len = strlen(prefix);
	int extra = 0;
	
	if (l_buf == NULL)
	{
		return NULL;
	}

	for(current = start; current < end; current += MTD_BLOCK_SIZE)
	{
		if (boot_part_read(start, end, &current, l_buf, MTD_BLOCK_SIZE) < 0)
		{
			continue;
		}

		p = l_buf;
		extra = 0;

		while(p < l_buf + MTD_BLOCK_SIZE)
		{
			pdir = (struct jffs2_raw_dirent *)p;
			
			if(je16_to_cpu(pdir->magic) == JFFS2_MAGIC_BITMASK)
			{
				if( je16_to_cpu(pdir->nodetype) == JFFS2_NODETYPE_DIRENT)
				{
					if (memcmp(prefix, pdir->name, prefix_len) == 0)
					{
						return pdir;
					}
				}

				p += (je32_to_cpu(pdir->totlen) + 0x03) & ~0x03;
				extra = 0;
			}
			else
			{
				if (extra < (JFFS2_MIN_DATA_LEN + sizeof(struct jffs2_raw_inode)) / 4)
				{
					extra++;
					p += 4;
				}
				else
				{
					break;
				}
			}
		}
	}

	return NULL;
}

#else /* INCLUDE_MTD_TYPE_FS */

static int boot_erase_write(unsigned char *buf, unsigned int ofs, unsigned int len)
{
	
	if (ofs & (MTD_BLOCK_SIZE - 1))
	{
		return -1;
	}

	while(len > MTD_BLOCK_SIZE)
	{
		sdk_flash_erase(ofs);
		len -= MTD_BLOCK_SIZE;
		ofs += MTD_BLOCK_SIZE;
	}

	if (len)
	{
		sdk_flash_erase(ofs);
	}

	return sdk_flash_write(buf, ofs, len);
}

#endif /* INCLUDE_MTD_TYPE_FS */
#endif /*if 0 */

#ifndef INCLUDE_MTD_TYPE_FS
/*
 * boot_read_image()/boot_write_image() use this for the raw (non filesystem)
 * image layouts.  The original implementation sits in the #if 0 block above
 * and its flash back ends (sdk_flash_erase()/sdk_flash_write()) are stubs in
 * this tree, so the raw layouts cannot be written here.  Fail loudly instead
 * of pretending success: the uIP web upgrade writes the image with the MTD
 * helpers in arch/arm/mach-airoha/ecnt_image.c.
 */
static int boot_erase_write(unsigned char *buf, unsigned int ofs, unsigned int len)
{
	printf("boot_erase_write: raw image layout not supported in this build\n");

	return -1;
}
#endif /* !INCLUDE_MTD_TYPE_FS */

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/

#if defined(INCLUDE_WIFI_MTK_MT7992) || defined(INCLUDE_WIFI_MTK_MT7993) || defined(INCLUDE_WIFI_MTK_MT7996)
void boot_reset(void)
{
	int list_len = sizeof(boot_reset_def) / sizeof(struct _BOOT_RESET_DEF);
	int i;
	int enit = -1;

	printf("reset enit wifi\n");

	for (i = 0; i < list_len; i++)
	{
		sdk_gpio_output_mode(boot_reset_def[i].gpio);
		if (CHAR2ID('E','N','I','T') == boot_reset_def[i].id)
		{
			enit = i;
		}
		sdk_gpio_setval(boot_reset_def[i].gpio, boot_reset_def[i].reverse);
	}
	if (enit >= 0)
	{
		udelay(500000);
		if (enit >= 0)
		{
			sdk_gpio_setval(boot_reset_def[enit].gpio, !boot_reset_def[enit].reverse);
		}
	}
}
#endif /* INCLUDE_WIFI_MTK_MT7992 || defined(INCLUDE_WIFI_MTK_MT7993) || INCLUDE_WIFI_MTK_MT7996 */

static int __maybe_unused setup_mtd_device(struct mtd_info **mtd, const char* mtd_dev)
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
#ifdef INCLUDE_DUAL_IMAGE
int boot_get_boot_index(void)
{
	IMG_BOOT_INFO boot_info;
	unsigned char sum = 0;

	memset(&boot_info, 0, sizeof(IMG_BOOT_INFO));
	int ret = 0,ret_len = 0;
	struct mtd_info *mtd_whole;
	unsigned char *mtd_bflag_block = NULL;
#ifdef INCLUDE_MTD_TYPE_FS
	if (boot_part_read(MTD_OFS_BFLAG, MTD_OFS_BFLAG + MTD_MISC_SIZE, 
				NULL, (unsigned char *)&boot_info, sizeof(IMG_BOOT_INFO)) != 0)
	{
		printf("Boot flag read failed, return image0\n");
		return 0;
	}
#else /* INCLUDE_MTD_TYPE_FS */

	char* current_bflag_partition = CONFIG_SYS_WHOLE_PARTITION_NAME;

	ret = setup_mtd_device(&mtd_whole, current_bflag_partition);
	if (ret) {
		printf("ERROR: Invalid reserve partition!\n");
		return;
	}

	//read bflag from whole partition, offset MTD_OFS_BFLAG

	mtd_bflag_block = (unsigned char *)malloc(MTD_BLOCK_SIZE);
	if(NULL == mtd_bflag_block)
	{
		printf("malloc mem for mtd_bflag_block fail \n");
		return 0;
	}
	ret = mtd_read(mtd_whole, MTD_OFS_BFLAG, MTD_BLOCK_SIZE, &ret_len, mtd_bflag_block);	
	if (ret) {
		printf("Failed to read bflag from %s, offset 0x%x .\r\n", CONFIG_SYS_WHOLE_PARTITION_NAME, MTD_OFS_BFLAG);
		free(mtd_bflag_block);
		mtd_bflag_block = NULL;
		return 0;
	}

	memcpy(&boot_info, mtd_bflag_block, sizeof(IMG_BOOT_INFO));
	free(mtd_bflag_block);
	mtd_bflag_block = NULL;
#endif /* INCLUDE_MTD_TYPE_FS */

	/* do csum here */
	sum = boot_info.csum;
	boot_info.csum = 0;

	if (boot_csum8((unsigned char *)&boot_info, sizeof(IMG_BOOT_INFO)) != sum)
	{
		printf("Boot flag csum incorrect\n");
		return 0;
	}

	//printf("boot_info.image[0].is_committed is %d,boot_info.image[0].is_active is %d,boot_info.image[0].is_valid is %d\n",boot_info.image[0].is_committed,boot_info.image[0].is_active,boot_info.image[0].is_valid);
	//printf("boot_info.image[1].is_committed is %d,boot_info.image[1].is_active is %d,boot_info.image[1].is_valid is %d\n",boot_info.image[1].is_committed,boot_info.image[1].is_active,boot_info.image[1].is_valid);
	if (boot_info.image[1].is_active && boot_info.image[1].is_valid)
	{
		return 1;	/* image 1 */
	}

	return 0;		/* image 0 */
}

int boot_set_boot_index(int index)
{
	IMG_BOOT_INFO boot_info;

#ifndef INCLUDE_MTD_TYPE_FS
	unsigned char *mtd_bflag_block = NULL;
	struct mtd_info *mtd_whole = NULL;
	struct erase_info ei = {0};
	int ret,ret_len = 0;
#endif

	if (index != 0 && index != 1)
	{
		return -1;
	}

	memset(&boot_info, 0, sizeof(IMG_BOOT_INFO));

#ifdef INCLUDE_MTD_TYPE_FS
	if (boot_part_read(MTD_OFS_BFLAG, MTD_OFS_BFLAG + MTD_MISC_SIZE, 
				NULL, (unsigned char *)&boot_info, sizeof(IMG_BOOT_INFO)) != 0)
	{
		printf("Boot flag read failed, image1 becomes invalid\n");
	}
#else /* INCLUDE_MTD_TYPE_FS */
	char* current_bflag_partition = CONFIG_SYS_WHOLE_PARTITION_NAME;

	ret = setup_mtd_device(&mtd_whole, current_bflag_partition);
	if (ret) {
		printf("ERROR: Invalid partition %s !\n", current_bflag_partition);
		return -1 ;
	}

	//read bflag
	mtd_bflag_block = (unsigned char *)malloc(MTD_BLOCK_SIZE);
	
	if(NULL == mtd_bflag_block)
	{
		printf("malloc mem for mtd_bflag_block fail \n");
		return -1;
	}
	ret = mtd_read(mtd_whole, MTD_OFS_BFLAG, MTD_BLOCK_SIZE, &ret_len, mtd_bflag_block );

	if (ret) {
		printf("Failed to read bflag from %s, offset 0x%x.\r\n", CONFIG_SYS_WHOLE_PARTITION_NAME, MTD_OFS_BFLAG);
		free(mtd_bflag_block);
		mtd_bflag_block = NULL;
		return -1;
	}

	memcpy(&boot_info, mtd_bflag_block, sizeof(IMG_BOOT_INFO));
#endif /* INCLUDE_MTD_TYPE_FS */

	if (index == 1)
	{
		boot_info.image[1].is_committed = 1;
		boot_info.image[1].is_active = 1;
		boot_info.image[1].is_valid = 1;
		boot_info.image[0].is_committed = 0;
		boot_info.image[0].is_active = 0;
	}
	else
	{
		boot_info.image[0].is_committed = 1;
		boot_info.image[0].is_active = 1;
		boot_info.image[0].is_valid = 1;
		boot_info.image[1].is_committed = 0;
		boot_info.image[1].is_active = 0;
	}
	boot_info.active_flag = 0;

	boot_info.csum = 0;
	boot_info.csum = boot_csum8((unsigned char *)&boot_info, sizeof(IMG_BOOT_INFO));
	//printf("after boot_info.image[0].is_committed is %d,boot_info.image[0].is_active is %d,boot_info.image[0].is_valid is %d\n",boot_info.image[0].is_committed,boot_info.image[0].is_active,boot_info.image[0].is_valid);
	//printf("after boot_info.image[1].is_committed is %d,boot_info.image[1].is_active is %d,boot_info.image[1].is_valid is %d\n",boot_info.image[1].is_committed,boot_info.image[1].is_active,boot_info.image[1].is_valid);
	memcpy(mtd_bflag_block, &boot_info, sizeof(IMG_BOOT_INFO));

#ifdef INCLUDE_MTD_TYPE_FS
	if (boot_part_write(MTD_OFS_BFLAG, MTD_OFS_BFLAG + MTD_MISC_SIZE, 
				NULL, (unsigned char *)&boot_info, sizeof(IMG_BOOT_INFO)) != 0)
	{
		return -1;
	}
#else /* INCLUDE_MTD_TYPE_FS */
	ret = setup_mtd_device(&mtd_whole, current_bflag_partition);

	if (ret) {
		printf("ERROR: Invalid reserve partition!\n");
		free(mtd_bflag_block);
		mtd_bflag_block = NULL;
		return -1 ;
	}

	memset(&ei, 0, sizeof(struct erase_info));
	ei.mtd = mtd_whole;
	ei.addr = MTD_OFS_BFLAG;
	ei.len = MTD_BLOCK_SIZE;
	ret = mtd_erase(mtd_whole, &ei); 

	if (ret) 
	{ 
		free(mtd_bflag_block);
		mtd_bflag_block = NULL;
		printf(" erase  partition %s, offset 0x%x, size 0x%x error\n", ei.mtd, ei.addr, ei.len);
		return -1 ;
	}
	ret = mtd_write(mtd_whole, MTD_OFS_BFLAG, MTD_BLOCK_SIZE, &ret_len, mtd_bflag_block);

	if(ret) { 
		free(mtd_bflag_block);
		mtd_bflag_block = NULL;
		printf("write partition %s, offset 0x%x, size 0x%x error\n", mtd_whole, MTD_OFS_BFLAG, MTD_BLOCK_SIZE);
		return -1 ;
	}

	free(mtd_bflag_block);
	mtd_bflag_block =NULL ;
#endif /* INCLUDE_MTD_TYPE_FS */

	return 0;
}
#endif /*INCLUDE_DUAL_IMAGE*/

#ifdef INCLUDE_MTD_TYPE_FS
int boot_get_filename(unsigned int start, unsigned int end, char *prefix, char *name)
{
	struct jffs2_raw_dirent *pdir;
	
	if (l_buf == NULL)
	{
		return -1;
	}

	pdir = boot_get_dirent(start, end, prefix);

	if (pdir == NULL)
	{
		return -1;
	}

	memcpy(name, pdir->name, pdir->nsize);
	name[pdir->nsize] = '\0';

	return 0;
}

int boot_read_file(unsigned int start, unsigned int end, 
						char *prefix, unsigned char *buf, unsigned int buflen)
{
	unsigned int current = 0;
	unsigned char *p;
	struct jffs2_raw_dirent *pdir;
	struct jffs2_raw_inode *pino;
	int ino = 0;
	int extra = 0;
	int count = 0;

	if (l_buf == NULL)
	{
		return -1;
	}

	pdir = boot_get_dirent(start, end, prefix);

	if (pdir == NULL)
	{
		return -1;
	}

	ino = je32_to_cpu(pdir->ino);

	for(current = start; current < end; current += MTD_BLOCK_SIZE)
	{
		if (boot_part_read(start, end, &current, l_buf, MTD_BLOCK_SIZE) < 0)
		{
			continue;
		}

		p = l_buf;
		extra = 0;

		while(p < l_buf + MTD_BLOCK_SIZE)
		{
			pino = (struct jffs2_raw_inode *)p;
			
			if(je16_to_cpu(pino->magic) == JFFS2_MAGIC_BITMASK)
			{
				if( je16_to_cpu(pino->nodetype) == JFFS2_NODETYPE_INODE)
				{
					if (je32_to_cpu(pino->ino) == ino)
					{
						unsigned int offset = je32_to_cpu(pino->offset);
						unsigned int size = je32_to_cpu(pino->dsize);
						unsigned int isize = je32_to_cpu(pino->isize);
						
						if (offset + size > buflen)
						{
							return -1;
						}

						count += size;
						memcpy(buf + offset, pino->data, size);

						/* printf("copy to 0x%08x size 0x%08x data 0x%08x\n", buf + offset, size, pino->data[size - 1]); */
						
						if (count >= isize)
						{
							return count;
						}
					}
				}

				p += (je32_to_cpu(pino->totlen) + 0x03) & ~0x03;
				extra = 0;
			}
			else
			{
				if (extra <= (JFFS2_MIN_DATA_LEN + sizeof(struct jffs2_raw_inode)) / 4)
				{
					extra++;
					p += 4;
				}
				else
				{
					break;
				}
			}
		}
	}	
	
	return -1;
}

#endif /* INCLUDE_MTD_TYPE_FS */

void boot_get_compressed_kernel(unsigned char *target)
{
#ifndef INCLUDE_MTD_TYPE_FS
	IMAGE_TAG *pTag = NULL;
#endif /* INCLUDE_MTD_TYPE_FS */
	
#ifdef INCLUDE_DUAL_IMAGE
	int index = 0;

	/* TODO: add active_flag resolve here */

	index = boot_get_boot_index();
	
	printf("Prepare to boot image%d\n", index);
	if (index == 1)
	{
#ifdef INCLUDE_MTD_TYPE_FS
		boot_read_file(MTD_OFS_KERNEL2, MTD_OFS_KERNEL2 + MTD_KERNEL_SIZE, 
					"linux", target, MTD_KERNEL_SIZE);
#else /* INCLUDE_MTD_TYPE_FS */
		sdk_flash_read(target, MTD_OFS_KERNEL2, TAG_LEN);
		pTag = (IMAGE_TAG *)target;
		sdk_flash_read(target, MTD_OFS_KERNEL2, pTag->kernelLen + TAG_LEN);
#endif /* INCLUDE_MTD_TYPE_FS */
	}
	else
#endif /* INCLUDE_DUAL_IMAGE */
	{
#ifdef INCLUDE_MTD_TYPE_FS
		boot_read_file(MTD_OFS_KERNEL, MTD_OFS_KERNEL + MTD_KERNEL_SIZE, 
					"linux", target, MTD_KERNEL_SIZE);
#else /* INCLUDE_MTD_TYPE_FS */

		sdk_flash_read(target, MTD_OFS_KERNEL, TAG_LEN);

		pTag = (IMAGE_TAG *)target;

		sdk_flash_read(target, MTD_OFS_KERNEL, pTag->kernelLen + TAG_LEN);
#endif /* INCLUDE_MTD_TYPE_FS */
	}
}

int boot_write_image(unsigned char *image, unsigned int size)
{
	/* TODO: add BE/LE transfer later */
	IMAGE_TAG *pTag = (IMAGE_TAG *)image;

	image += TAG_LEN;
	
	/* add CRC check here */
	
	/* flash bootloader */
	if (pTag->bootLen != 0)
	{
		printf("Start to write boot ofs %08X to %08X, length %08X\n",
					pTag->bootAddress, MTD_OFS_BOOT, pTag->bootLen);
#ifdef INCLUDE_MTD_TYPE_FS
		if (boot_part_write(MTD_OFS_BOOT, MTD_OFS_BOOT + MTD_BOOT_SIZE, 
					NULL, image + pTag->bootAddress, pTag->bootLen) != 0)
		{
			return -1;
		}
#else /* INCLUDE_MTD_TYPE_FS */
		boot_erase_write(image + pTag->bootAddress, MTD_OFS_BOOT, pTag->bootLen);
#endif /* INCLUDE_MTD_TYPE_FS */		
	}

	/* flash kernel */
	if (pTag->kernelLen != 0)
	{
		printf("Start to write kernel ofs %08X to %08X, length %08X\n",
					pTag->kernelAddress, MTD_OFS_KERNEL, pTag->kernelLen);
#ifdef INCLUDE_MTD_TYPE_FS
		if (boot_part_write(MTD_OFS_KERNEL, MTD_OFS_KERNEL + MTD_KERNEL_SIZE, 
					NULL, image + pTag->kernelAddress, pTag->kernelLen) != 0)
		{
			return -1;
		}
#else /* INCLUDE_MTD_TYPE_FS */
		boot_erase_write(image + pTag->kernelAddress, MTD_OFS_KERNEL, pTag->kernelLen);
#endif /* INCLUDE_MTD_TYPE_FS */		
	}

	/* flash rootfs */
	if (pTag->rootfsLen != 0)
	{
		printf("Start to write rootfs ofs %08X to %08X, length %08X\n",
					pTag->rootfsAddress, MTD_OFS_ROOTFS, pTag->rootfsLen);
#ifdef INCLUDE_MTD_TYPE_FS
		if (boot_part_write(MTD_OFS_ROOTFS, MTD_OFS_ROOTFS + MTD_ROOTFS_SIZE, 
					NULL, image + pTag->rootfsAddress, pTag->rootfsLen) != 0)
		{
			return -1;
		}
#else /* INCLUDE_MTD_TYPE_FS */
		boot_erase_write(image + pTag->rootfsAddress, MTD_OFS_ROOTFS, pTag->rootfsLen);
#endif /* INCLUDE_MTD_TYPE_FS */
	}

#ifdef INCLUDE_DUAL_IMAGE
	printf("Set boot flag to image0\n");
	/* update image tag */
	boot_set_boot_index(0);
#endif /* INCLUDE_DUAL_IMAGE */

	return 0;
}

#ifdef INCLUDE_MTD_TYPE_FS
int boot_write_oobimage(unsigned char *image, unsigned int size)
{
	int ret;

	/* do if need */
#if 0
	if ((ret = sdk_check_oob(image, &size)) != 0)
	{
		return ret;
	}
#endif
	
	if ((ret = sdk_remove_oob(image, &size)) != 0)
	{
		return ret;
	}

	if ((ret = boot_part_write(0, sdk_flash_get_size(), NULL, image, size)) != 0)
	{
		return ret;
	}
	
	return 0;
}

int boot_write_oobimage_partable(unsigned char *image, unsigned int size, 
									unsigned char* partable, unsigned int partable_len)
{
	int ret;
	unsigned int *partable_val = (unsigned int *)partable;
	unsigned int start, end, data_len;
	/* do if need */
#if 0
	if ((ret = sdk_check_oob(image, &size)) != 0)
	{
		return ret;
	}
#endif

	if (partable_len % 16)
	{
		printf("partable size not aligned!\n");
		return -1;
	}
	
	if ((ret = sdk_remove_oob(image, &size)) != 0)
	{
		return ret;
	}

	printf("flash image size 0x%08X\n", size);

	while(partable_len)
	{
		start = le32_to_cpu(*partable_val);
		end = le32_to_cpu(*(partable_val + 1));
		data_len = le32_to_cpu(*(partable_val + 2));
		
		if (start == 0xFFFFFFFF && end == 0xFFFFFFFF && data_len == 0xFFFFFFFF)
		{
			break;
		}

		start *=  MTD_BLOCK_SIZE;
		end = end * MTD_BLOCK_SIZE + MTD_BLOCK_SIZE;
		data_len *= MTD_BLOCK_SIZE;

		if (start >= end || start + data_len > end || end > size)
		{
			printf("partition data error, from 0x%08X to 0x%08X, len 0x%08X", start, end, data_len);
			break;
		}

		printf("start to write partition from 0x%08X to 0x%08X, len 0x%08X\n", start, end, data_len);
		boot_part_write(start, end, NULL, image + start, data_len);
		partable_val += 4;
		partable_len -= 16;
	}
	return 0;
}

/* must set this buf before using TYPE FS function */
void boot_set_block_buf(unsigned char *buf)
{
	l_buf = buf;
}

#endif /* INCLUDE_MTD_TYPE_FS */

#if defined(__IMAGE_H__)
image_header_t *boot_fake_image_header(image_header_t *hdr, IMAGE_TAG *pTag)
{
    memset(hdr, 0, sizeof(image_header_t));	/* Build new header */
    hdr->ih_magic = htonl(IH_MAGIC);
    hdr->ih_time  = 0;
    hdr->ih_size  = htonl(pTag->kernelLen);
    hdr->ih_load  = htonl(pTag->kernelTextAddr);
    hdr->ih_ep    = htonl(pTag->kernelEntryPoint);
    hdr->ih_dcrc  = 0;
    hdr->ih_os    = IH_OS_LINUX;
#if defined(__PPC__)
	hdr->ih_arch = IH_ARCH_PPC;
#elif defined(__ARM__)
	hdr->ih_arch = IH_ARCH_ARM;
#elif defined(__I386__)
	hdr->ih_arch = IH_ARCH_I386;
#elif defined(__mips__)
	hdr->ih_arch = IH_ARCH_MIPS;
#elif defined(__M68K__)
	hdr->ih_arch = IH_ARCH_M68K;
#elif defined(__microblaze__)
	hdr->ih_arch = IH_ARCH_MICROBLAZE;
#elif defined(__nios2__)
	hdr->ih_arch = IH_ARCH_NIOS2;
#else
# error Unknown CPU type
#endif
    hdr->ih_type  = IH_TYPE_KERNEL;
    hdr->ih_comp  = IH_COMP_LZMA;
    strncpy((char *)hdr->ih_name, "(none)", IH_NMLEN);
    hdr->ih_hcrc = 0;
    hdr->ih_hcrc = htonl(crc32(0, (unsigned char *)hdr, sizeof(image_header_t)));
    return hdr;
}
#endif

#ifdef INCLUDE_UIP_FWUPGRADE
int sdk_gpio_getval(unsigned char gpio)
{
	int val = 0;
	val = (regRead32(CR_GPIO_DATA) & (1<<gpio)) >> gpio;
	return val;
}

int check_fw_gpio(void)
{
	int i = 0;
	int val = 0;
	int list_len = sizeof(boot_btn_def) / sizeof(struct _BOOT_BTN_DEF);
	BOOT_BTN_DEF *reset_btn = NULL;

	for (i = 0; i < list_len; i++)
	{
		if (0 == strcmp("RST", boot_btn_def[i].id))
		{
			reset_btn = &boot_btn_def[i];
		}
	}

	val =  BTN_PRESSED(reset_btn);

	return val;

}

int up_file(void)
{
	run_command("httpd", 0);

	return 1;
}

#endif /* INCLUDE_UIP_FWUPGRADE */


/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/


