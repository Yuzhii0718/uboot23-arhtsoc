/*
 * trx.h
 *
 *  Created on: Mar 19, 2009
 *      Author: pork
 */

#ifndef TRX_H_
#define TRX_H_

#if defined(TCSUPPORT_SECURE_BOOT)
#include <boot/sHeader.h>
#endif

struct f_header_t {
	unsigned int bl22_length;
	unsigned int bl23_length;
	unsigned int flash_table_length;
	unsigned int lzma_src;
	unsigned int lzma_des;
	unsigned int lzma_length;
	unsigned int lzma_cmd;
	unsigned int reserved;
};

typedef enum
{
	E_FW_TCLINUX_JAVA_OSGI = 0x01,
	E_FW_TCLINUX_JAVA,    
	E_FW_TCLINUX_OSGI,      
	E_FW_JAVA,              
	E_FW_OSGI,            
	E_FW_SAF,
}fw_optflag;

struct trx_header {
	unsigned int magic;			/* "HDR0" */
	unsigned int header_len;    /*Length of trx header*/
	unsigned int len;			/* Length of file including header */
	unsigned int crc32;			/* 32-bit CRC from flag_version to end of file */
	unsigned char version[32];  /*firmware version number*/
	unsigned char customerversion[32];  /*firmware version number*/
//	unsigned int flag_version;	/* 0:15 flags, 16:31 version */
#if 0
	unsigned int reserved[44];	/* Reserved field of header */
#else
	unsigned int kernel_len;	//kernel length
	unsigned int rootfs_len;	//rootfs length
        unsigned int romfile_len;	//romfile length
	#if 0
	unsigned int reserved[42];  /* Reserved field of header */
	#else
	unsigned char Model[32];
	unsigned int decompAddr;//kernel decompress address
	/*
	CMCC:
	opt_flag = 1, opt_len[0]->tclinux.bin, opt_len[1]->java, opt_len[2]->osgi
	opt_flag = 2, opt_len[0]->tclinux.bin, opt_len[1]->java
	opt_flag = 3, opt_len[0]->tclinux.bin, opt_len[1]->osgi
	opt_flag = 4, opt_len[0]->java
	opt_flag = 5, opt_len[0]->osgi
	CTC:
	opt_flag = 1, opt_len[0]->tclinux.bin, opt_len[1]->saf len, opt_len[2]->saf version, opt_len[3]->test version
	*/
	unsigned int opt_len[10];
	unsigned int opt_flag;
	unsigned int reserved[21]; 

#endif
#if defined(TCSUPPORT_SECURE_BOOT_V2)
	SECURE_HEADER_V2 sHeader;
#elif defined(TCSUPPORT_SECURE_BOOT_V1) || defined(TCSUPPORT_SECURE_BOOT_FLASH_OTP)
	SECURE_HEADER_V1 sHeader;
#elif defined(TCSUPPORT_DUMMY_SECURE_HEADER)
	unsigned char reserved_sheader[2048];
#endif
#endif	
};
#define TRX_MAGIC	0x30524448	/* "HDR0" */
#define TRX_MAGIC1	0x31524448	/* "HDR1" */
#define TRX_MAGIC2	0x32524448	/* "for tclinux" */
#define TRX_MAGIC3		0x33524448	/* "for romfile" */
#define DEFAULT_CRC	0xFFFFFFFF
#endif /* TRX_H_ */
