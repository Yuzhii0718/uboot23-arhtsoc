/*
 * (C) Copyright 2000-2009
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <asm/io.h>
#include <airoha/trx.h>
#include "linux/libfdt.h"
#include <ecnt_image.h>
#include <command.h>
#include <env.h>
#include "asm/tc3162.h"
#include "cpu_func.h"
#include <stdlib.h>
#include <hexdump.h>

#define BOOTARGS_STR_MAX_LEN      2048

#ifdef TCSUPPORT_BOARD_SELECT
#include <linux/string.h>
#endif

#ifdef TCSUPPORT_DM_VERITY
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
#endif

/*attention when add the mi.conf, because the size maybe overflow*/
char *mi_conf[] = {"sdram_conf",
					"vendor_name",
					"product_name",
					"ethaddr",
					"snmp_sysobjid",
					"country_code",
					"ether_gpio",
					"power_gpio",
					"username",
					"password",
					"dsl_gpio",
					"internet_gpio",
					"multi_upgrade_gpio",
					"onu_type",
					"qdma_init",
					"root",
					"console",
					"bootflag",
					"serdes_sel",
					"iommu",
					"mem",
					"swiotlb",
					"dram_limit",
					"iommu.passthrough",
#if defined (CONFIG_TARGET_AN7581) || defined (CONFIG_TARGET_AN7583)
#ifdef TCSUPPORT_BOARD_SELECT
					"srdsPortCnt",
					"srdsParmCntPerPort",
					"srdsIfNameAllComp",
					"srdsPortNameAllComp",
					"srdsEtherTypeAllComp",
					"srdsEtherPhyTypeAllComp",
					"srdsPortCombo",
					"rfb_cfg",
					"rfb_no",
					"rfb_id1",
					"rfb_id2",
#else
					"serdes_pon",
					"serdes_ethernet",
					"serdes_wifi1",
					"serdes_wifi2",
					"serdes_usb1",
					"serdes_usb2",
#endif
					"board_args",
					"8811phy_addr",
					"arht_low_power",
#endif
					"sn_test",
					"tclinux_info",
					"dm-mod.create",
					"hybrid"};

static void set_bootflag_env(unsigned int bootflag) {
    if(bootflag == 0) {
        env_set("bootflag", "0");
    } else {
        env_set("bootflag", "1");
    }
}

#ifdef TCSUPPORT_DM_VERITY
//#define AIROHA_DM_DEBUG
static void set_dm_verity_env() {

	uint8_t* data = (uint8_t*)CONFIG_SYS_LOAD_ADDR;
	uint32_t data_len = 0;
	uint32_t i = 0;
	char key[256], value[256];
	char* rootfs_partition_name = env_get("rootfs_partition_name");
	char data_size_in_sector[256];
	char hash_type[256];
	char data_block_size[256];
	char hash_block_size[256];
	char data_blocks[256];
	char data_blocks_plus_one[256];
	char hash_algorithm[256];
	char root_hash[256];
	char salt[256];
	char dm_verity_str[1024];
	char dm_mod_create_str[BOOTARGS_STR_MAX_LEN] = {0};
	char *bootargs;
#ifdef TCSUPPORT_DM_CRYPT
	char crypted_data_size_in_sector[256];
	char crypted_cipher_algo[256];
	char crypted_rootfs_key[256]; // the key length can be modified
	char crypted_iv_offset[256];
	char crypted_payload_offset[256];
	char dm_crypt_str[1024];
#endif

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
			data = (uint8_t*)(image_pos + CONFIG_SYS_LOAD_ADDR);
			data_len = image_len;
			break;
		}
	}

	/* now we can extract dm-verity arguments from fit subimage through data & data_len */
	while(i < data_len) {
		uint32_t key_len = 0;
		uint32_t value_len = 0;
		char value_str[256];

		while(i < data_len && data[i] != '=') {
			key[key_len++] = data[i++];
		}
		key[key_len] = '\0';
		i++;
		while(i < data_len && data[i] != '\n') {
			value[value_len++] = data[i++];
		}
		value[value_len] = '\0';
		i++;

		//env_set(key, value);
		if(!strncmp(key, "DATA_SIZE", strlen("DATA_SIZE"))) {
			sprintf(data_size_in_sector, "%ld", (simple_strtoul(value, NULL, 0) / 512));
		}
		else if(!strncmp(key, "HASH_TYPE", strlen("HASH_TYPE"))) {
			memcpy(hash_type, value, (value_len + 1));
		}
		else if(!strncmp(key, "DATA_BLOCK_SIZE", strlen("DATA_BLOCK_SIZE"))) {
			memcpy(data_block_size, value, (value_len + 1));
		}
		else if(!strncmp(key, "HASH_BLOCK_SIZE", strlen("HASH_BLOCK_SIZE"))) {
			memcpy(hash_block_size, value, (value_len + 1));
		}
		else if(!strncmp(key, "DATA_BLOCKS", strlen("DATA_BLOCKS"))) {
			memcpy(data_blocks, value, (value_len + 1));
			sprintf(data_blocks_plus_one, "%ld", (simple_strtoul(value, NULL, 0) + 1));
		}
		else if(!strncmp(key, "HASH_ALGORITHM", strlen("HASH_ALGORITHM"))) {
			memcpy(hash_algorithm, value, (value_len + 1));
		}
		else if(!strncmp(key, "ROOT_HASH", strlen("ROOT_HASH"))) {
			memcpy(root_hash, value, (value_len + 1));
		}
		else if(!strncmp(key, "SALT", strlen("SALT"))) {
			memcpy(salt, value, (value_len + 1));
		}
#ifdef TCSUPPORT_DM_CRYPT
		else if(!strncmp(key, "CRYPTED_DATA_SIZE_IN_SECTOR", strlen("CRYPTED_DATA_SIZE_IN_SECTOR"))) {
			memcpy(crypted_data_size_in_sector, value, (value_len + 1));
		}
		else if(!strncmp(key, "CRYPTED_CIPHER_ALGO", strlen("CRYPTED_CIPHER_ALGO"))) {
			memcpy(crypted_cipher_algo, value, (value_len + 1));
		}
		else if(!strncmp(key, "CRYPTED_ROOTFS_KEY", strlen("CRYPTED_ROOTFS_KEY"))) {
			// copy to specific memory offset to decrypt
			hex2bin((char *)0x1e800000, value, ((value_len + 1) / 2));
			if(decrypt_dm_crypt_key((char *)0x1e800000, ((value_len + 1) / 2)) != 0) {
				printf("dm-crypt key decrypt failed!!!\n");
				return -1;
			}
			bin2hex(crypted_rootfs_key, (char *)0x1e800000, 32);
		}
		else if(!strncmp(key, "CRYPTED_IV_OFFSET", strlen("CRYPTED_IV_OFFSET"))) {
			memcpy(crypted_iv_offset, value, (value_len + 1));
		}
		else if(!strncmp(key, "CRYPTED_PAYLOAD_OFFSET", strlen("CRYPTED_PAYLOAD_OFFSET"))) {
			memcpy(crypted_payload_offset, value, (value_len + 1));
		}
#endif
		memset(key, 0, sizeof(key));
		memset(value, 0, sizeof(value));
	}

	/* collect all arguments and combine them to a dm-mod.create command */
	snprintf(dm_mod_create_str, sizeof(dm_mod_create_str), "dm-mod.waitfor=\"PARTLABEL=%s\" dm-mod.create=\"", rootfs_partition_name);
	snprintf(dm_verity_str, sizeof(dm_verity_str), "dm-verity,,,ro,0 %s verity %s PARTLABEL=%s PARTLABEL=%s %s %s %s %s %s %s %s", data_size_in_sector, hash_type, rootfs_partition_name, rootfs_partition_name, data_block_size, hash_block_size, data_blocks, data_blocks_plus_one, hash_algorithm, root_hash, salt);
	strncat(dm_mod_create_str, dm_verity_str, BOOTARGS_STR_MAX_LEN - 1);
#ifdef TCSUPPORT_DM_CRYPT
	snprintf(dm_crypt_str, sizeof(dm_crypt_str), "; dm-crypt,,,ro,0 %s crypt %s %s %s /dev/dm-0 %s", crypted_data_size_in_sector, crypted_cipher_algo, crypted_rootfs_key, crypted_iv_offset, crypted_payload_offset);
	strncat(dm_mod_create_str, dm_crypt_str, BOOTARGS_STR_MAX_LEN - 1);
#endif
	strncat(dm_mod_create_str, "\" ", BOOTARGS_STR_MAX_LEN - 1);

#ifdef AIROHA_DM_DEBUG
	// DM-verity
	printf("\n=== DM-verity args ===\n");
	printf("data_size_in_sector: %s\n", data_size_in_sector);
	printf("hash_type: %s\n", hash_type);
	printf("data_block_size: %s\n", data_block_size);
	printf("hash_block_size: %s\n", hash_block_size);
	printf("data_blocks: %s\n", data_blocks);
	printf("data_blocks_plus_one: %s\n", data_blocks_plus_one);
	printf("hash_algorithm: %s\n", hash_algorithm);
	printf("root_hash: %s\n", root_hash);
	printf("salt: %s\n", salt);
	// DM-crypt
#ifdef TCSUPPORT_DM_CRYPT
	printf("\n=== DM-crypt args ===\n");
	printf("crypted_data_size_in_sector: %s\n", crypted_data_size_in_sector);
	printf("crypted_cipher_algo: %s\n", crypted_cipher_algo);
	printf("crypted_rootfs_key: %s\n", crypted_rootfs_key);
	printf("crypted_iv_offset: %s\n", crypted_iv_offset);
	printf("crypted_payload_offset: %s\n", crypted_payload_offset);
#endif
	// The string pass to kernel
	printf("\n=== Summary ===\n");
	printf("dm-verity command: %s\n", dm_verity_str);
#ifdef TCSUPPORT_DM_CRYPT
	printf("dm-crypt command: %s\n", dm_crypt_str);
#endif
	printf("dm_mod_create_str: %s\n", dm_mod_create_str);
	printf("\n=== End ===\n");
#endif

	/* set the bootargs */
	bootargs = env_get("bootargs");
	if(bootargs) {
		strncat(dm_mod_create_str, bootargs, BOOTARGS_STR_MAX_LEN - 1);
	}
	env_set("bootargs", dm_mod_create_str);

	env_set("root", "/dev/dm-0 ro rootwait");
#ifdef TCSUPPORT_DM_CRYPT
	env_set("root", "/dev/dm-1 ro rootwait");
#endif
}
#endif

unsigned long long ecnt_memparse(const char *ptr, char **retptr, unsigned int blocksize)
{
	char *endptr;	/* local pointer to end of parsed string */

	unsigned long long ret = simple_strtoull(ptr, &endptr, 0);

	switch (*endptr) {
	case 'G':
	case 'g':
		ret <<= 10;
	case 'M':
	case 'm':
		ret <<= 10;
	case 'K':
	case 'k':
		ret <<= 10;
		endptr++;
		break;
	case 'B':
	case 'b':
		ret *=blocksize;
		endptr++;
		break;
	default:
		break;
	}

	if (retptr)
		*retptr = endptr;

	return ret;
}

static inline void __coverity_taint_getenv_check(char *s)
{
	(void)s;
}

static void check_dram_limit(void)
{
	char *val;
	unsigned long long size = 0;
	unsigned long long cal_size = 0;

	cal_size =  ((u64)GET_DRAM_SIZE * (u64)SZ_1M);

	val = env_get("dram_limit");
	if(val) {
		__coverity_taint_getenv_check(val);
		size = ecnt_memparse(val, 0, 0);

		if (cal_size >= size)
		{
			SET_DRAM_SIZE(size >> 20);
			printf("dram size is limited to %s\n", val);
		}
		else
		{
			printf("ERROR: dram limit value is exceed the calibrated dram size !! Use calibrated dram size as default.\n");
		}
	}
	else 
	{
		/* does not need limit the dram size, do nothing*/
	}
}

#ifdef TCSUPPORT_BOARD_SELECT
static int chk_srds_parm(char *rfb_str, int srdsPortCnt, int srdsParmCntPerPort)
{
	char* delim=",";
	char *token_str, *cur_str=rfb_str; 
	int i=0;

	for(i=0;i<srdsPortCnt;i++){
		token_str=strsep(&cur_str, delim);
		if(NULL==token_str){
			return -1;
		}
		if(strlen(token_str) != srdsParmCntPerPort) {
				return -1;
		}
	}

	return 0;
}
#endif

static int parse_env_config(char *var)
{
	int i;
	int n_mi_conf;
	char *val;
	unsigned int totalLen = 0;

#ifdef TCSUPPORT_BOARD_SELECT
	int srdsPortCnt;
	int srdsParmCntPerPort;
	int rfbParmCnt = 0;
	char buf[30];
#endif

	n_mi_conf = (sizeof(mi_conf) / sizeof(const char *));
	totalLen = strlen(var);

#ifdef TCSUPPORT_BOARD_SELECT
	val = env_get("srdsPortCnt");
	srdsPortCnt = simple_strtol(val, NULL, 10);
	val = env_get("srdsParmCntPerPort");
	srdsParmCntPerPort = simple_strtol(val, NULL, 10);

	if(srdsPortCnt < 0 || srdsParmCntPerPort < 0) {
		printf("The rfb_%s srdsPortCnt: %d or srdsParmCntPerPort: %d error\n", env_get("rfb_no"), srdsPortCnt, srdsParmCntPerPort);
		return -1;
	}

	snprintf(buf, sizeof(buf), "%s", env_get("rfb_cfg"));
	val = buf;

	if(-1==chk_srds_parm(val, srdsPortCnt, srdsParmCntPerPort)){
		return -1;
	}

	val = env_get("rfbParmCnt");
	rfbParmCnt = simple_strtol(val, NULL, 10);
	if(srdsPortCnt > rfbParmCnt) {
		printf("[Warning]%s Cnt is %d, the rfbParmCnt is %d\n", buf, srdsPortCnt, rfbParmCnt);
	}
#endif

	for(i = 0; i < n_mi_conf; i++) {
		val = env_get(mi_conf[i]);
#if DBG_MI_CONF
		printf("=== totalLen:%d ===\n", totalLen);
		printf("parse %s=%s\n", mi_conf[i], val);
#endif
		if(val) {
			totalLen += (strlen(mi_conf[i]) + strlen("=")
						+ strlen(val) + strlen(" "));
			if(totalLen >= (BOOTARGS_STR_MAX_LEN - 1)) {
				printf("bootargs len:%d more than %d ===\n", totalLen, BOOTARGS_STR_MAX_LEN);
				return -1;
			}

			
			strncat(var, mi_conf[i], BOOTARGS_STR_MAX_LEN - 1);
			strncat(var, "=", BOOTARGS_STR_MAX_LEN - 1);
			strncat(var, val, BOOTARGS_STR_MAX_LEN - 1);
			strncat(var, " ", BOOTARGS_STR_MAX_LEN - 1);
		}
	}
	
	return 0;
}

static int set_tclinux_img_env(struct tclinux_imginfo *info) {
    char info_str[86 * 2] = ""; /* 86 = 16 * 5 + 5 char ("," * 4 + " ") */
    char tclinux_size_str[16] = {0};
    char kernel_off_str[16] = {0};
    char kernel_size_str[16] = {0};
    char rootfs_off_str[16] = {0};
    char rootfs_size_str[16] = {0};

    if(info) {
        sprintf(tclinux_size_str, "0x%x", info->tclinux_size);
        sprintf(kernel_off_str, "0x%x", info->kernel_off);
        sprintf(kernel_size_str, "0x%x", info->kernel_size);
        sprintf(rootfs_off_str, "0x%x", info->rootfs_off);
        sprintf(rootfs_size_str, "0x%x", info->rootfs_size);
        strncat(info_str, tclinux_size_str, sizeof(info_str) - 1);
        strncat(info_str, ",", sizeof(info_str) - 1);
        strncat(info_str, kernel_off_str, sizeof(info_str) - 1);
        strncat(info_str, ",", sizeof(info_str) - 1);
        strncat(info_str, kernel_size_str, sizeof(info_str) - 1);
        strncat(info_str, ",", sizeof(info_str) - 1);
        strncat(info_str, rootfs_off_str, sizeof(info_str) - 1);
        strncat(info_str, ",", sizeof(info_str) - 1);
        strncat(info_str, rootfs_size_str, sizeof(info_str) - 1);
        // no need to support dual-image
        strncat(info_str, ",0x0,0x0,0x0,0x0,0x0", sizeof(info_str) - 1);
    }
    env_set("tclinux_info", info_str);

    return 0;
}

int bootargs_init(unsigned int bootflag)
{
    char var[BOOTARGS_STR_MAX_LEN] = {0};
    char *bootargs;

    struct tclinux_imginfo info;

    if(get_tclinux_imginfo(&info)) {
        return -1;
    }
    if(set_tclinux_img_env(&info)) {
        return -1;
    }

    set_bootflag_env(bootflag);
#ifdef TCSUPPORT_DM_VERITY
    set_dm_verity_env();
#endif
    
    if(parse_env_config(var)) {
        return -1;
    }
    check_dram_limit();

    bootargs = env_get("bootargs");
    if(bootargs) {
        strncat(var, bootargs, BOOTARGS_STR_MAX_LEN - 1);
    }
    
    env_set("bootargs", var);

    return 0;
}

static int do_env_bootargs(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
    return bootargs_init(0);
}

U_BOOT_CMD(
    bargsenv,
    1,
    0,
    do_env_bootargs,
    "auto-create bootargs env",
    ""
)
