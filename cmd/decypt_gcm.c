// SPDX-License-Identifier: GPL-2.0+
/*
 * Implements the 'bd' command to show board information
 *
 * (C) Copyright 2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */
#include <common.h>
#include <command.h>
#include <uboot_aes.h>
#include <linux/arm-smccc.h>
#include <malloc.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <mapmem.h>
#include "linux/libfdt.h"

/*
---------------------------------------------------------------------------------------
	reference from u-boot-2023/aes.c
*/


static int do_decrypt(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[]){
	uint32_t key_addr, iv_addr, src_addr, dst_addr, len;
	uint8_t *key_ptr, *iv_ptr, *src_ptr, *dst_ptr;
	u8 key_exp[AES256_EXPAND_KEY_LENGTH];
	u32 aes_blocks, key_len;
	int enc, decrypt_res;

#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif	

	#if(1)
	/* u-boot way */
	mbedtls_gcm_self_test(1);
	#endif

	key_addr = hextoul(argv[1], NULL);
	printf("debug decrypt base=0x%x\n", key_addr);
	len = hextoul(argv[2], NULL);
	printf("debug len=%d\n", len);

	#if(0)
	decrypt_res = enc_file_read(key_addr, len);
	printf("decryption result=%d\n", decrypt_res);
	if(decrypt_res){
		printf("\033[31;1m fail\033[0m\n")	;
	}	
	#endif


	#if(0)
	/* atf way */

	unsigned long r0 = 0, r1 = 0, r2 = 0, r3 = 0;


	if (argc < 4)
		return CMD_RET_USAGE;

	r0 = 0x82000209;

#if(0)
	r1 = *((unsigned int *) argv[1]);
	r2 = simple_strtoul(argv[2], NULL, 16);
	r3 = simple_strtoul(argv[3], NULL, 16);
#endif

	r1=0x8a800000;
	r2=0x6bd24;
	r3=0;

	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	printf("smc reuslt-->%d\n", res.a0);
	#endif 

	return;
}

static char aes_help_text[] =
	"[.128,.192,.256] enc key iv src dst len - Encrypt block of data $len bytes long\n"
	"                             at address $src using a key at address\n"
	"                             $key with initialization vector at address\n"
	"                             $iv. Store the result at address $dst.\n"
	"                             The $len size must be multiple of 16 bytes.\n"
	"                             The $key and $iv must be 16 bytes long.\n"
	"aes [.128,.192,.256] dec key iv src dst len - Decrypt block of data $len bytes long\n"
	"                             at address $src using a key at address\n"
	"                             $key with initialization vector at address\n"
	"                             $iv. Store the result at address $dst.\n"
	"                             The $len size must be multiple of 16 bytes.\n"
	"                             The $key and $iv must be 16 bytes long.";
	
U_BOOT_CMD(
	decrypt,   4,      0,      do_decrypt,
	"AES 128/192/256 CBC encryption",
	aes_help_text
);


/*
---------------------------------------------------------------------------------------
	reference from u-boot-airoha/ecnt_bootargs.c
*/

#define FIT_IMAGES_PATH		"/images"
#define FIT_CONFS_PATH		"/configurations"

/* hash/signature/key node */
#define FIT_HASH_NODENAME	"hash"
#define FIT_ALGO_PROP		"algo"
#define FIT_VALUE_PROP		"value"
#define FIT_IGNORE_PROP		"uboot-ignore"
#define FIT_SIG_NODENAME	"signature"
#define FIT_KEY_REQUIRED	"required"
#define FIT_KEY_HINT		"key-name-hint"

/* cipher node */
#define FIT_CIPHER_NODENAME	"cipher"
#define FIT_ALGO_PROP		"algo"

/* image node */
#define FIT_DATA_PROP		"data"
#define FIT_DATA_POSITION_PROP	"data-position"
#define FIT_DATA_OFFSET_PROP	"data-offset"
#define FIT_DATA_SIZE_PROP	"data-size"
#define FIT_TIMESTAMP_PROP	"timestamp"
#define FIT_DESC_PROP		"description"
#define FIT_ARCH_PROP		"arch"
#define FIT_TYPE_PROP		"type"
#define FIT_OS_PROP		"os"
#define FIT_COMP_PROP		"compression"
#define FIT_ENTRY_PROP		"entry"
#define FIT_LOAD_PROP		"load"

/* configuration node */
#define FIT_KERNEL_PROP		"kernel"
#define FIT_FILESYSTEM_PROP	"filesystem"
#define FIT_RAMDISK_PROP	"ramdisk"
#define FIT_FDT_PROP		"fdt"
#define FIT_LOADABLE_PROP	"loadables"
#define FIT_DEFAULT_PROP	"default"
#define FIT_SETUP_PROP		"setup"
#define FIT_FPGA_PROP		"fpga"
#define FIT_FIRMWARE_PROP	"firmware"
#define FIT_STANDALONE_PROP	"standalone"

static void do_get_kernel_key() {

	uint8_t* data = (uint8_t*)CONFIG_SYS_LOAD_ADDR;
	uint32_t data_len = 0;



	/*access FIT data from DRAM*/
	uint32_t size, image_pos, image_len;
	const uint32_t *image_offset_be, *image_len_be, *image_pos_be;
	int node, images;
	const char *image_name, *image_type, *image_description;
	int image_name_len, image_type_len, image_description_len;
	const void *fit = (const void*)CONFIG_SYS_LOAD_ADDR;

	size = fdt_totalsize(fit);
	images = fdt_path_offset(fit, FIT_IMAGES_PATH);

	fdt_for_each_subnode(node, fit, images) {
		image_name = fdt_get_name(fit, node, &image_name_len);
		image_type = fdt_getprop(fit, node, FIT_TYPE_PROP, &image_type_len);
		image_offset_be = fdt_getprop(fit, node, FIT_DATA_OFFSET_PROP, NULL);
		image_pos_be = fdt_getprop(fit, node, FIT_DATA_POSITION_PROP, NULL);
		image_len_be = fdt_getprop(fit, node, FIT_DATA_SIZE_PROP, NULL);
		if (!image_name || !image_type || !image_len_be)
			continue;

		image_len = be32_to_cpu(*image_len_be);
		if (!image_len)
			continue;

		if (image_offset_be)
			image_pos = be32_to_cpu(*image_offset_be) + size;
		else if (image_pos_be)
			image_pos = be32_to_cpu(*image_pos_be);
		else
			continue;

		image_description = fdt_getprop(fit, node, FIT_DESC_PROP, &image_description_len);

		if(strcmp(image_name, "rootfs_env") == 0)
		{
			printf("\033[32;1m find rootfs_env! \n \033[0m");
			#if(0)
			data = (uint8_t*)(image_pos + CONFIG_SYS_LOAD_ADDR);
			data_len = image_len;
			break;
			#endif
		}
		
		if(strcmp(image_name, "kernel_key") == 0)
		{
			printf("\033[32;1m find kernel_key! \n \033[0m");
		}		
	}
	
	return ;
}


U_BOOT_CMD(
	get_kernel_key,   4,      0,      do_get_kernel_key,
	"testing fdt for accessing my mode",
	" N/A "
);


