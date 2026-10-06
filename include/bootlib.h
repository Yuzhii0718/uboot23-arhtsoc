/*  Copyright(c) 2009-2017 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		bootlib.h
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
#ifndef __BOOTLIB_H__
#define __BOOTLIB_H__


/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
/*
 * brief	copy from oal_sys.h
 * By	wangwenhao, 22May17
 */
/************************************* partition size defines *************************************/
/* MTD_BLOCK_SIZE defined in menuconfig */
/* MTD_IMAGE_SIZE defined in menuconfig */
/* MTD_BOOT_SIZE defined in menuconfig */
/* MTD_MISC_SIZE defined in menuconfig */
/* MTD_KERNEL_SIZE defined in menuconfig */
#ifdef INCLUDE_MTD_TYPE_FS
#define MTD_BFLAG_SIZE		(MTD_MISC_SIZE)
#define MTD_APP_SIZE		(MTD_IMAGE_SIZE - MTD_BOOT_SIZE - 2 * MTD_MISC_SIZE)
#else /* INCLUDE_MTD_TYPE_FS */
#define MTD_BFLAG_SIZE		(MTD_BLOCK_SIZE)
	#if defined(INCLUDE_LOG_PARTITION) && defined(INCLUDE_FLASH_SPINOR)
		/* if spi-nor flash, MTD_IMAGE_SIZE = ALL partition size sum */
		#define MTD_APP_SIZE            (MTD_IMAGE_SIZE - MTD_BOOT_SIZE - MTD_MISC_SIZE - MTD_LOG_PARTITION_SIZE)
	#else /* !(INCLUDE_LOG_PARTITION && INCLUDE_FLASH_SPINOR) */
		#define MTD_APP_SIZE		(MTD_IMAGE_SIZE - MTD_BOOT_SIZE - MTD_MISC_SIZE)
	#endif /* INCLUDE_LOG_PARTITION && INCLUDE_FLASH_SPINOR */
#endif /* INCLUDE_MTD_TYPE_FS */
#define MTD_ROOTFS_SIZE		(MTD_APP_SIZE - MTD_KERNEL_SIZE)

/************************************* partition offset defines ***********************************/
#define MTD_OFS_BOOT		(0)

#ifdef INCLUDE_MTD_TYPE_RAW1
#define MTD_OFS_KERNEL		(MTD_BOOT_SIZE)
#define MTD_OFS_ROOTFS		(MTD_BOOT_SIZE + MTD_KERNEL_SIZE)
	#if defined(INCLUDE_LOG_PARTITION) && defined(INCLUDE_FLASH_SPINOR)
		#define MTD_OFS_MISC		(MTD_IMAGE_SIZE - MTD_MISC_SIZE - MTD_LOG_PARTITION_SIZE)
	#else /* !(INCLUDE_LOG_PARTITION && INCLUDE_FLASH_SPINOR) */
		#define MTD_OFS_MISC		(MTD_IMAGE_SIZE - MTD_MISC_SIZE)
	#endif /* INCLUDE_LOG_PARTITION && INCLUDE_FLASH_SPINOR */
#endif /* INCLUDE_MTD_TYPE_RAW1 */

#ifdef INCLUDE_MTD_TYPE_RAW2
#define MTD_OFS_MISC		(MTD_BOOT_SIZE)
#define MTD_OFS_KERNEL		(MTD_BOOT_SIZE + MTD_MISC_SIZE)
#define MTD_OFS_ROOTFS		(MTD_BOOT_SIZE + MTD_MISC_SIZE + MTD_KERNEL_SIZE)
#endif /* INCLUDE_MTD_TYPE_RAW2 */

#ifdef INCLUDE_MTD_TYPE_FS
#define MTD_OFS_MISC		(MTD_BOOT_SIZE)
#define MTD_OFS_MISC_RO		(MTD_BOOT_SIZE)
#define MTD_OFS_MISC_RW		(MTD_BOOT_SIZE + MTD_MISC_SIZE)
#define MTD_OFS_KERNEL		(MTD_BOOT_SIZE + 2 * MTD_MISC_SIZE)
#define MTD_OFS_ROOTFS		(MTD_BOOT_SIZE + 2 * MTD_MISC_SIZE + MTD_KERNEL_SIZE)
#endif /* INCLUDE_MTD_TYPE_FS */

#define MTD_OFS_KERNEL2		(MTD_IMAGE_SIZE)
#define MTD_OFS_ROOTFS2		(MTD_IMAGE_SIZE + MTD_KERNEL_SIZE)

#ifdef INCLUDE_MTD_TYPE_FS
#define MTD_OFS_BFLAG		(2 * MTD_IMAGE_SIZE - 2 * MTD_MISC_SIZE)
#else /* INCLUDE_MTD_TYPE_FS */
#define MTD_OFS_BFLAG		(2 * MTD_IMAGE_SIZE - MTD_BLOCK_SIZE)
#endif  /* INCLUDE_MTD_TYPE_FS */


#define CONFIG_ENV_ROOT "/dev/mtdblock5 ro"
#define CONFIG_ENV_ROOT_SLAVE "/dev/mtdblock8 ro"
#define CONFIG_SYS_KERNE_SLAVE_PARTITION_NAME "kernel_slave"
#define CONFIG_SYS_KERNE_PARTITION_NAME "kernel"
#define CONFIG_SYS_MISC_PARTITION_NAME "misc"
#define CONFIG_SYS_RESERVE_PARTITION_NAME "reserve"
#define CONFIG_SYS_WHOLE_PARTITION_NAME "whole"
/************************************** block index defines ***************************************/
enum
{
	MTD_IDX_CONFIG = 0, /* Let DM config size to 2 * MTD_BLOCK_SIZE */

#ifdef INCLUDE_OPTION66
	MTD_IDX_ISP_CONFIG = 1,
#endif /* INCLUDE_OPTION66 */

	MTD_IDX_OTHER = 2, /* Let DM config size to 2 * MTD_BLOCK_SIZE */
	MTD_IDX_DATA,      /*store important data,such as cloud message*/
	MTD_IDX_WIFI,
	MTD_IDX_WIFI_5G,
#ifdef INCLUDE_WIFI_6G
	MTD_IDX_WIFI_6G,
#endif /* INCLUDE_WIFI_6G */

#ifdef INCLUDE_PON
	MTD_IDX_PON,
#endif /* INCLUDE_PON */

#ifdef INCLUDE_IMPORTANT_CONFIG
	MTD_IDX_IMCONF,
#endif /* INCLUDE_IMPORTANT_CONFIG */

#ifdef INCLUDE_DUAL_CONFIG
	MTD_IDX_CONFIG_BAK,
#endif /* INCLUDE_DUAL_CONFIG */
	
#ifdef INCLUDE_PARENTCONTROL_V2
	MTD_IDX_PC_HISTORY_0,
	#ifdef INCLUDE_PC_V2_SAVE_ALL_HISTORY
	MTD_IDX_PC_HISTORY_1, /* Placeholder, not used actually. */
	MTD_IDX_PC_HISTORY_2, /* Placeholder, not used actually. */
	#endif /* INCLUDE_PC_V2_SAVE_ALL_HISTORY */
#endif /* INCLUDE_PARENTCONTROL_V2 */
	
	/* ----------- add above this line ---------------- */
	MTD_IDX_MAX
};
/* end index in MISC partition */


/******************************** Aginet Config offset defines ************************************/
#ifdef INCLUDE_OPTION66
enum
{
	ISP_CONFIG_OFS_FLAG	= 0xF800,
#ifdef INCLUDE_OPTION66_NEED_TAG
	ISP_CONFIG_OFS_TAG 	= 0xF900,
#endif
	ISP_CONFIG_OFS_MAX
};
#endif  /* INCLUDE_OPTION66 */


/************************************** block offset defines **************************************/
#ifdef INCLUDE_MTD_TYPE_RAW1
#define MTD_OFS_CONFIG		(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_CONFIG * MTD_BLOCK_SIZE)
#define MTD_OFS_WIFI		(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_WIFI * MTD_BLOCK_SIZE)
#define MTD_OFS_OTHER		(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_OTHER * MTD_BLOCK_SIZE)
#ifdef INCLUDE_WIFI_DUALBAND
#define MTD_OFS_WIFI_5G		(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_WIFI_5G * MTD_BLOCK_SIZE)
#endif /* INCLUDE_WIFI_DUALBAND */
#ifdef INCLUDE_PON_MTK
#define MTD_OFS_PON			(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_PON * MTD_BLOCK_SIZE)
#endif /* INCLUDE_PON_MTK */
#ifdef INCLUDE_IMPORTANT_CONFIG
#define MTD_OFS_IMCONF		(MTD_IMAGE_SIZE - MTD_BLOCK_SIZE - MTD_IDX_IMCONF * MTD_BLOCK_SIZE)
#endif /* INCLUDE_IMPORTANT_CONFIG */
// TODO: MTD_OFS_ISP_CONFIG
#endif /* INCLUDE_MTD_TYPE_RAW1 */


/* used by INCLUDE_MTD_TYPE_FS for filename gen */
#if defined(INCLUDE_MTD_TYPE_RAW2) || defined(INCLUDE_MTD_TYPE_FS)
#define MTD_OFS_CONFIG		(MTD_OFS_MISC + MTD_IDX_CONFIG * MTD_BLOCK_SIZE)
#define MTD_OFS_WIFI		(MTD_OFS_MISC + MTD_IDX_WIFI * MTD_BLOCK_SIZE)
#define MTD_OFS_OTHER		(MTD_OFS_MISC + MTD_IDX_OTHER * MTD_BLOCK_SIZE)
#ifdef INCLUDE_WIFI_DUALBAND
#define MTD_OFS_WIFI_5G		(MTD_OFS_MISC + MTD_IDX_WIFI_5G * MTD_BLOCK_SIZE)
#endif /* INCLUDE_WIFI_DUALBAND */
#ifdef INCLUDE_PON_MTK
#define MTD_OFS_PON			(MTD_OFS_MISC + MTD_IDX_PON * MTD_BLOCK_SIZE)
#endif /* INCLUDE_PON_MTK */
#ifdef INCLUDE_IMPORTANT_CONFIG
#define MTD_OFS_IMCONF		(MTD_OFS_MISC + MTD_IDX_IMCONF * MTD_BLOCK_SIZE)
#endif /* INCLUDE_IMPORTANT_CONFIG */

#ifdef INCLUDE_OPTION66
#define MTD_OFS_ISP_CONFIG	(MTD_OFS_MISC + MTD_IDX_ISP_CONFIG * MTD_BLOCK_SIZE)/* Agile config */
#endif /* INCLUDE_OPTION66 */

#ifdef INCLUDE_DUAL_CONFIG
#define MTD_OFS_CONFIG_BAK	(MTD_OFS_MISC + MTD_BLOCK_SIZE * MTD_IDX_CONFIG_BAK)
#endif

#endif /* INCLUDE_MTD_TYPE_RAW2 || INCLUDE_MTD_TYPE_FS */


/************************************* offsets in other block *************************************/
#ifndef OTHER_OFS
#define OTHER_OFS
enum
{
	OTHER_OFS_MAC				= 0xF100,	/* LAN MAC needs 6 Bytes, uses 0xF100 - 0xF110 */
	OTHER_OFS_OEMID 			= 0xF110,	/* OEMID needs 33 + 8 = 41 Bytes, uses 0xF110 - 0xF140 */ 
	OTHER_OFS_ZONE				= 0xF140,	/* Zone needs 16 + 8 = 24 Bytes, uses 0xF140 - 0xF160 */
	OTHER_OFS_HWID				= 0xF160,	/* HWID needs 33 + 8 = 41 Bytes, uses 0xF160 - 0xF190 */
	
#if defined(INCLUDE_LABEL_MAC)
	OTHER_OFS_LABEL_MAC		 	= 0xF1E0,	/* LABEL_MAC needs 6 bytes, uses 0xF1E0 - 0xF1F0 */
	OTHER_OFS_LABEL_MAC_TYPE 	= 0xF1F0,	/* LABEL_MAC_TYPE needs 4 bytes, uses 0xF1F0 - 0xF200 */
#endif /* INCLUDE_LABEL_MAC */

	OTHER_OFS_PIN				= 0xF200,
	OTHER_OFS_DEVID				= 0xF300,
	OTHER_OFS_RFPI				= 0xF400,	/* for DECT only */
	OTHER_OFS_RXTUN         		= 0xF410,
#ifdef INCLUDE_WRITE_SN
	OTHER_OFS_SN				= 0xF500,	/* for Serial Number */
#endif

#ifdef INCLUDE_WRITE_COUNTRY
	OTHER_OFS_DOMAIN			= 0xF600,	/* for Country Code  */
#endif

#ifdef INCLUDE_WRITE_FUNCTION_CODE
	OTHER_OFS_FUNCTION_CODE		= 0xF700,	/* for Function Code  */
#endif /* INCLUDE_WRITE_FUNCTION_CODE */

#ifdef INCLUDE_WRITE_MNGT_URL
	OTHER_OFS_MNGT_URL			= 0xFA00,	/* for Managemet Server URL,URL is 256 string length */
#endif /* INCLUDE_WRITE_MNGT_URL */

#ifdef INCLUDE_FACTORY_PRE_PAIRED
	OTHER_OFS_FTCONFGROUPID 	= 0xFB00,	/* for Factory pre paired  */
	OTHER_OFS_FTCONFKEY 		= 0xFC00,	/* for Factory pre paired  */
#endif /* INCLUDE_FACTORY_PRE_PAIRED */

#ifdef INCLUDE_WRITE_WIFI_PWD
	OTHER_OFS_WIFI_PWD			= 0xFD00,	/* wifi pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFD00 - 0xFD40 */
	
	#ifdef INCLUDE_WRITE_MULTI_GUEST_PWD
	OTHER_OFS_MULTI1_PWD		= 0xFDC0,	/* wifi pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFDC0 - 0xFE00 */
	OTHER_OFS_MULTI2_PWD		= 0xFE00,	/* wifi pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFE00 - 0xFE40 */
	OTHER_OFS_GUEST_PWD 		= 0xFE40,	/* wifi pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFE40 - 0xFE80 */
	#endif /* INCLUDE_WRITE_MULTI_GUEST_PWD */
#endif /* INCLUDE_WRITE_WIFI_PWD */

#ifdef INCLUDE_WRITE_ADMIN_PWD
	OTHER_OFS_ADMIN_PWD 		= 0xFD40,	/* admin pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFD40 - 0xFD80 */
#endif

#ifdef INCLUDE_WRITE_USER_PWD
	OTHER_OFS_USER_PWD			= 0xFD80,	/* user pwd needs 64 Bytes, give buffer 64 Bytes, uses 0xFD80 - 0xFDC0 */
#endif

#ifdef INCLUDE_LTEWAN
	OTHER_OFS_GOLD		= 0xB200,
#endif /* INCLUDE_LTEWAN */

	OTHER_OFS_DPP_PUBLIC_KEY	= 0xB300,	/* need 120 bytes, use 0xB300 - 0xB500 */
	OTHER_OFS_DPP_PRIVATE_KEY	= 0xB500,	/* need 301 bytes, use 0xB500 - 0xB700 */

	OTHER_OFS_SIGN		= 0xE000, 	/* for devid flash only, mostly for domestic product */
	OTHER_OFS_MICFLAG	= 0xD000,
	OTHER_OFS_GPONSN	= 0xC000,
#if defined(INCLUDE_SPEC_EX220)
	/* nothing, why? */
#else
	OTHER_OFS_ADDHVER	= 0xFF00,
#endif

#ifdef INCLUDE_WRITE_CWMP_PWD
	OTHER_OFS_CWMP_PWD	= 0xB000,	/*	CWMP pwd needs 256 Bytes, give buffer 256 Bytes, uses 0xB000 - 0xB100 */
#endif

	/* ------------------- add above the line -------------------- */
	OTHER_OFS_MAX
};
#endif  /* OTHER_OFS */

#ifdef INCLUDE_IMPORTANT_CONFIG
enum
{
	IMCONF_OFS_CTCLOID = 0xF100,
	IMCONF_OFS_GPONSN  = 0xF200,
	IMCONF_OFS_GPONPWD = 0xF300,
	IMCONF_OFS_EPONMAC = 0xF400,
	IMCONF_OFS_GPONOLTMODE = 0xF500,
	IMCONF_OFS_GPONVENDORID = 0xF600,
	IMCONF_OFS_GPONOLTPROFILE = 0xF700,
	IMCONF_OFS_GPONSNPREFIX = 0xF800,
};
#endif /* INCLUDE_IMPORTANT_CONFIG */


/************************************** type fs mount point ***************************************/
#define FS_PATH_MAX_SIZE	64
#define FS_PATH_FORMAT		"%s/0x%08X"
#define FS_PATH_MISC_RO		"/var/run/misc/misc_ro"
#define FS_PATH_MISC_RW		"/var/run/misc/misc_rw"


/***************************************** for image tag ******************************************/
#define TAG_VERSION_V3		(0x03000003)
#define TAG_VERSION_V4		(0x04000004)

/* PSS1: ENC-PSS */
#define SIG_LEN_PSS1		(256)
/*
SUB_TAG_MAX_TYPE is written directly for backward compatibility,
as new types are added without the need for intermediate software.
*/
#define SUB_TAG_MAX_TYPE	(30)

#define TAG_LEN         512
#define CLOUD_ID_BYTE_LEN	16
#define TOKEN_LEN	20
#define MAGIC_NUM_LEN	20
#define SIG_LEN		128

#ifdef INCLUDE_DUAL_IMAGE
#define OPEN_IMAGE_SLAVE 1
#endif

#define BOOTROM_EXT_FREE_ADDR 0x80020000

/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/
typedef struct
{	
	unsigned int tagVersion;			/* tag version number */   
	unsigned char hardwareId[CLOUD_ID_BYTE_LEN];		/* HWID for cloud */
	unsigned char firmwareId[CLOUD_ID_BYTE_LEN];		/* FWID for cloud */
	unsigned char oemId[CLOUD_ID_BYTE_LEN];			/* OEMID for cloud */
	unsigned int productId;	/* product id */  
	unsigned int productVer;	/* product version */
	unsigned int addHver;		/* Addtional hardware version */
	
	unsigned char imageValidToken[TOKEN_LEN];	/* image validation token - md5 checksum */
	unsigned char magicNum[MAGIC_NUM_LEN];	 	/* magic number */
	
	unsigned int kernelTextAddr; 	/* text section address of kernel */
	unsigned int kernelEntryPoint; /* entry point address of kernel */
	
	unsigned int totalImageLen;	/* the sum of kernelLen+rootfsLen+tagLen */
	
	unsigned int kernelAddress;	/* starting address (offset from the beginning of FILE_TAG) 
									 * of kernel image 
									 */
	unsigned int kernelLen;		/* length of kernel image */
	
	unsigned int rootfsAddress;	/* starting address (offset) of filesystem image */
	unsigned int rootfsLen;		/* length of filesystem image */

	unsigned int bootAddress;		/* starting address (offset) of bootloader image */
	unsigned int bootLen;			/* length of bootloader image */

	unsigned int swRevision;		/* software revision */
	unsigned int platformVer;		/* platform version */
	unsigned int specialVer;

	unsigned int binCrc32;			/* CRC32 for bin(kernel+rootfs) */

	unsigned int reserved1[13];	/* reserved for future */

	unsigned char sig[SIG_LEN];		/* signature for update */
	unsigned char resSig[SIG_LEN];	/* reserved for signature */

	unsigned int reserved2[12];	/* reserved for future */
}IMAGE_TAG;

typedef struct
{
	struct
	{
		unsigned char is_committed;		
		unsigned char is_active;		
		unsigned char is_valid;
	}image[2];
	unsigned char active_flag;
	unsigned char csum;
}IMG_BOOT_INFO;

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           FUNCTIONS                                            */
/**************************************************************************************************/
void boot_set_all_led_on(void);

void boot_set_all_except_power_led_off(void);

#if defined(INCLUDE_WIFI_MTK_MT7992) || defined(INCLUDE_WIFI_MTK_MT7993) || defined(INCLUDE_WIFI_MTK_MT7996)
void boot_reset(void);
#endif /* INCLUDE_WIFI_MTK_MT7992 || defined(INCLUDE_WIFI_MTK_MT7993) || INCLUDE_WIFI_MTK_MT7996 */

/* must set this buf before using TYPE FS function */
void boot_set_block_buf(unsigned char *buf);

#ifdef INCLUDE_DUAL_IMAGE
int boot_get_boot_index(void);
int boot_set_boot_index(int index);
#endif /* INCLUDE_DUAL_IMAGE */

#ifdef INCLUDE_MTD_TYPE_FS
int boot_get_filename(unsigned int start, unsigned int end, char *prefix, char *name);

int boot_read_file(unsigned int start, unsigned int end, 
						char *prefix, unsigned char *buf, unsigned int buflen);
#endif /* INCLUDE_MTD_TYPE_FS */

void boot_get_compressed_kernel(unsigned char *target);

int boot_write_image(unsigned char *image, unsigned int size);

#ifdef INCLUDE_MTD_TYPE_FS
int boot_write_oobimage(unsigned char *image, unsigned int size);

int boot_write_oobimage_partable(unsigned char *image, unsigned int size, 
									unsigned char* partable, unsigned int partable_len);
#endif /* INCLUDE_MTD_TYPE_FS */

#ifdef INCLUDE_UIP_FWUPGRADE
int sdk_gpio_getval(unsigned char gpio);
int check_fw_gpio(void);
int up_file(void);
#endif /* INCLUDE_UIP_FWUPGRADE  */


#endif /* __BOOTLIB_H__ */

