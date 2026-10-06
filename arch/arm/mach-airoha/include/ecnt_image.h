/*
 * (C) Copyright 2000-2009
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ECNT_IMAGE_H
#define _ECNT_IMAGE_H

#include <linux/ctype.h>
#define FDT_BLOB_DESCRIPTION "AIROHA U-Boot Flattened Device Tree blob"

struct tclinux_imginfo {
	u32 kernel_off;
	u32 kernel_size;
	u32 rootfs_off;
	u32 rootfs_size;
	u32 tclinux_size;
};

int get_tclinux_imginfo(struct tclinux_imginfo *info);
int update_slave_offset_info(const void *image, ulong allinone_size);
int update_gpt_info(const char *env_content);

/*
 * Offset of the environment blob inside the all-in-one upgrade image.
 *
 * The all-in-one image keeps the bootloader partition layout, so the
 * environment blob sits at the same offset the partition reserves for it.
 * Builds that store the environment at a fixed flash offset already have that
 * value in CONFIG_ENV_OFFSET; builds that keep it in a UBI volume do not
 * define CONFIG_ENV_OFFSET at all, so fall back to the image layout constant.
 */
#ifdef CONFIG_ENV_OFFSET
#define ECNT_ALLINONE_ENV_OFFSET	CONFIG_ENV_OFFSET
#else
#define ECNT_ALLINONE_ENV_OFFSET	0x7c000
#endif
#ifdef CONFIG_OPEN_IMAGE
#define TAG_LEN			(512)
#define CLOUD_ID_BYTE_LEN	(16)
#define TOKEN_LEN		(20)
#define MAGIC_NUM_LEN	(20)
#define SIG_LEN		(128)
#define BOOTLOADER_TAG_LEN	TAG_LEN
#define BOOT_OFFSET		(0x0)
#define KERNEL_OFFSET	(0x000000620000)
#define ROOTFS_OFFSET	(0x000001020000)

/*copy from platform\apps\private\user\clibs\cmm_lib\include\oal_partition.h*/
typedef struct _LINUX_FILE_TAG

{
	unsigned int tagVersion;							/* tag version number */  
	unsigned char hardwareId[CLOUD_ID_BYTE_LEN];		/* HWID for cloud */
	unsigned char firmwareId[CLOUD_ID_BYTE_LEN];		/* FWID for cloud */
	unsigned char oemId[CLOUD_ID_BYTE_LEN];				/* OEMID for cloud */
	unsigned int productId;								/* product id */  
	unsigned int productVer;							/* product version */
	unsigned int addHver;								/* Addtional hardware version */
	
	unsigned char imageValidToken[TOKEN_LEN];		/* image validation token - md5 checksum */
	unsigned char magicNum[MAGIC_NUM_LEN];			/* magic number */
	
	unsigned int kernelTextAddr;						/* text section address of kernel */
	unsigned int kernelEntryPoint;						/* entry point address of kernel */
	
	unsigned int totalImageLen;							/* the sum of kernelLen+rootfsLen+tagLen */
	
	unsigned int kernelAddress;		/* starting address (offset from the beginning of FILE_TAG) 
									 * of kernel image 
									 */
	unsigned int kernelLen;								/* length of kernel image */
	
	unsigned int rootfsAddress;							/* starting address (offset) of filesystem image */
	unsigned int rootfsLen;								/* length of filesystem image */

	unsigned int bootAddress;							/* starting address (offset) of bootloader image */
	unsigned int bootLen;								/* length of bootloader image */

	unsigned int swRevision;							/* software revision */
	unsigned int platformVer;							/* platform version */
	unsigned int specialVer;

	unsigned int binCrc32;								/* CRC32 for bin(kernel+rootfs) */

	unsigned int reserved1[13];							/* reserved for future */

	unsigned char sig[SIG_LEN];						/* signature for update */
	unsigned char resSig[SIG_LEN];					/* reserved for signature */

	unsigned int dmVersion;								/* from Device.DeviceInfo.X_DMVersion */
	unsigned int prehookFsAddress;						/* squashfs address for prehook exes */
	unsigned int prehookFsLen;							/* squashfs length for prehook exes */

	unsigned int reserved2[9];							/* reserved for future */
}LINUX_FILE_TAG;
#endif/*CONFIG_OPEN_IMAGE*/
#endif

