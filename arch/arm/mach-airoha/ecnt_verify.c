
#include <common.h>
#include <command.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/stddef.h>
#include <linux/compat.h>
#include <hash.h>
#include <u-boot/sha256.h>
#include <u-boot/sha512.h>
#include <asm/tc3162.h>
#include "firmware_image_package.h"
#include <linux/libfdt.h>
#define MAX_STRING	30
#define MAX_NUMBER	5
#define HASH_MODE_MASK		(0x1)

#define FIP_MAX_HEADER_SIZE	(0x2000)
#define CONFIG_RAMFS_BASE (0x8b000000)
#define DECRYPTED_FIT_ADDR (0x8b000100)


extern unsigned int get_fip_offset(void);
extern unsigned int image_read_mode;

enum
{
	ENCRYPTED_FW = 1,
	ENCRYPTED_OUTER_HEADER,
};

static struct hash_algo sha_algo[2] = {
	{
		"sha256",
		SHA256_SUM_LEN,
		sha256_csum_wd,
		CHUNKSZ_SHA256,
	},
	{
		"sha512",
		SHA512_SUM_LEN,
		sha512_csum_wd,
		CHUNKSZ_SHA512,
	},
};

unsigned char output[HASH_MAX_DIGEST_SIZE]  __aligned(PAGE_SIZE);

char filename[MAX_NUMBER][MAX_STRING] =
{
	"uboot_filename",
	"kernel_filename",
	"\0"
};

extern unsigned int parse_fip(char **buf, unsigned int *buff_size, uint32_t *offset, uint32_t *size, uint32_t *flag);

static long get_sha_alg(void)
{
	unsigned int r0 = 0, r1 = 0, r2 = 0, r3 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	r0 = 0x82000003;
#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	return (long)res.a0;
#else
	r0 = do_smc(r0, r1, r2, r3);
	return r0;
#endif
}

static int check_image_name(viod)
{
	char *name = NULL, *valid_name = NULL;
	int verify = 0, i = 0;

	name = env_get("filename");

	if (name == NULL)
		return 0;
	while ((i < MAX_NUMBER) && (verify == 0))
	{
		valid_name = env_get(filename[i]);
		if (valid_name != NULL)
		{
			if (!strcmp(name, valid_name))
#if defined(TCSUPPORT_ARM_MULTIBOOT)
			{
				if (i == 0)
					return 2;
				else
					return 1;
			}
#else
				return 1;
#endif
		}
		i++;
	}

	return 0;
}

static long verify_img(char *buf, unsigned int buf_size, int sha_alg, unsigned int offset, unsigned int size, unsigned int flag, unsigned int smc_id)
{
	unsigned int ret = 0, r1 = 0, r2 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	unsigned char *addr = NULL;
	if(smc_id == SIP_VERIFY_BL2)
	{

		r1 = (unsigned int) size;
		r2 = (unsigned int) buf;
	}
	else
	{

		addr = (unsigned char *) ((uint32_t) buf + offset);
		sha_algo[sha_alg].hash_func_ws(addr, size, output, sha_algo[sha_alg].chunk_size);
		r1 = (unsigned int) output;
		r2 = (unsigned int) buf;
	}
	if (buf_size == FIP_MAX_HEADER_SIZE)
		buf_size = offset;

	if(smc_id == SIP_VERIFY_BL2)

	{
		flush_cache(r2, size + buf_size);
	}
	else
	{
		flush_cache(r2, buf_size);
	}
	flush_cache(r1, 4096);

#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(smc_id, r1, r2, buf_size, 0, 0, 0 ,0, &res,0);
	return (long)res.a0;
#else
	ret = do_smc(smc_id, r1, r2, buf_size);
	return ret;
#endif
}

static long decrypt_and_verify_img(char *buf, unsigned int buf_size, int sha_alg, unsigned int offset, unsigned int size, unsigned int flag, unsigned int smc_id)
{
	unsigned int ret = 0, r1 = 0, r2 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	unsigned char *addr = NULL;

	smc_id |= 0x10;


		r1 = (unsigned int) size;
		r2 = (unsigned int) buf;

		flush_cache(r2, size + FIP_MAX_HEADER_SIZE);


	flush_cache(r1, 4096);

#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(smc_id, r1, r2, CONFIG_RAMFS_BASE, 0, 0, 0 ,0, &res,0);
	return (long)res.a0;
#else
	ret = do_smc(smc_id, r1, r2, CONFIG_RAMFS_BASE);
	return ret;
#endif
}

long decrypt_dm_crypt_key(char *buf, unsigned int size)
{
	unsigned int r0 = 0, r1 = 0, r2 = 0, r3 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	r0 = 0x82000206;
	r1 = (unsigned int)buf;
	r2 = size;
#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	return (long)res.a0;
#else
	r0 = do_smc(r0, r1, r2, r3);
	return r0;
#endif
}

int upgrade_verify(char *buf)
{
#if defined(TCSUPPORT_ARM_SECURE_BOOT)
	int verify = 1;
	int sha_alg = 0;
	int ret = 0;
	unsigned int offset = 0, size = 0, flag = 0;
	unsigned int smc_id = 0;
	unsigned int buf_size = FIP_MAX_HEADER_SIZE;

	//verify = check_image_name(); => we will need to fix this when we enable multiboot
	if (verify != 0)
	{
#if defined(TCSUPPORT_ARM_MULTIBOOT)
		if(verify == 2)
			buf += 32;
#endif

		smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);
		/* BL2 image*/
		if (smc_id == SIP_VERIFY_BL2)
			return decrypt_and_verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
		else if (smc_id != 0) /* BL33 image*/
		{
		#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
			// image verification for encrypted firmware
			ret=verify_from_buffer(buf);
			return ret;
		#else
			sha_alg = get_sha_alg();
			sha_alg = (sha_alg & HASH_MODE_MASK);
			ret = verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
			/* check verified img is encrypted*/
			if ((ret == 0) && (flag == ENCRYPTED_OUTER_HEADER))
				return ENCRYPTED_OUTER_HEADER;
			else
				return ret;
                #endif
		}
		else
		{
			/*should not go to here */
			printf("Unexpected image\n");
			return -1;
		}
	}

	return verify;
#else
	return 0;
#endif
}

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
  int verify_from_buffer(char *buf)
  {
      int sha_alg = 0;
      unsigned int offset = 0, size = 0, flag = 0;
      unsigned int smc_id = 0;
      unsigned int buf_size = FIP_MAX_HEADER_SIZE;
      unsigned long retlen = 0;
      char *fit_store_addr = buf + (FIP_MAX_HEADER_SIZE + 0x100);
      char *fip_store_addr = buf;
  
      smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);
                                      
  
  
      /* Check the FIP header to detemine the image has been encrypted, then parse the encryped fw's FIP header*/
      if (flag == ENCRYPTED_OUTER_HEADER)
      {
      
              buf = fit_store_addr;
              smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);
                              
      }
  
      if (smc_id != 0)
      {
              /* read the encryped fw*/
              if(flag == ENCRYPTED_FW)
              {
  
                      buf = fit_store_addr;	
                      /* set image read mode to 1 -> read image content from DRAM*/
                      image_read_mode = 1;
                      return decrypt_and_verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
              }
              else
              {	
                      if(buf!=fip_store_addr){
                              buf = fip_store_addr;
                      }
  
                      sha_alg = get_sha_alg();
                      sha_alg = (sha_alg & HASH_MODE_MASK);
                      return verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
  
              }
      }
  
      return -1;
  }
#endif

int read_flash_and_verify(char *buf, unsigned long addr)
{
#if defined(TCSUPPORT_ARM_SECURE_BOOT)
	int sha_alg = 0;
	unsigned int offset = 0, size = 0, flag = 0;
	unsigned int smc_id = 0;
	unsigned int buf_size = FIP_MAX_HEADER_SIZE;
	unsigned long retlen = 0;

	flash_read(addr, buf_size, &retlen, buf);

	smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);

	/* Check the FIP header to detemine the image has been encrypted, then parse the encryped fw's FIP header*/
	if (flag == ENCRYPTED_OUTER_HEADER)
	{
		flash_read(addr + FIP_MAX_HEADER_SIZE + 0x100, buf_size, &retlen, buf);
		smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);
	}

	if (smc_id != 0)
	{
		/* read the encryped fw*/
		if(flag == ENCRYPTED_FW)
		{
			flash_read(addr + FIP_MAX_HEADER_SIZE + 0x100, (offset+size), &retlen, buf);

			/* set image read mode to 1 -> read image content from DRAM*/
			image_read_mode = 1;
			return decrypt_and_verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
		}
		else
		{
			flash_read(addr, (offset + size), &retlen, buf);
			sha_alg = get_sha_alg();
			sha_alg = (sha_alg & HASH_MODE_MASK);
			return verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);

		}
	}

	return -1;
#else
	return 0;
#endif
}

#ifdef TCSUPPORT_ARM_SECURE_BOOT_FW_ENC
  int boot_verify(char *buf)
  {
  
  	uint32_t image_size =0;
  	uint32_t *prop;
  	int	ramdisk_offset;
  
  
  	if (verify_from_buffer(buf))
  	{
  		printf("Verify image failed!\r\n");
  		return -1;
  	}
  
  	if(image_read_mode==1){
  		printf("Image Verification and Decryption Successful!\r\n");
  	}
  	else{
  		printf("Image verification successful!\r\n");
  	}
  	
  	if (image_read_mode ==1){
#ifndef TCSUPPORT_DM_VERITY
  		//get image size from fdt
  		ramdisk_offset = fdt_path_offset((const void *)(DECRYPTED_FIT_ADDR), "/images/ramdisk");
  
  		prop =fdt_getprop((const void *)(DECRYPTED_FIT_ADDR),ramdisk_offset,"data-offset",NULL);
  		image_size +=be32_to_cpu(*prop);
  		prop =fdt_getprop((const void *)(DECRYPTED_FIT_ADDR),ramdisk_offset,"data-size",NULL);
  		image_size +=be32_to_cpu(*prop);
  		
  		memcpy((unsigned char*)(CONFIG_SYS_LOAD_ADDR), (unsigned long *) (DECRYPTED_FIT_ADDR), image_size);
#else
		memcpy((unsigned char*)(CONFIG_SYS_LOAD_ADDR), (unsigned long *) (DECRYPTED_FIT_ADDR), 0xa00000);
#endif
  	}
  
  	return 0;
  
  }
#endif

static int fip_test_command(struct cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#if defined(TCSUPPORT_ARM_SECURE_BOOT)
	int sha_alg = 0;
	unsigned int offset = 0, size = 0;
	unsigned int smc_id = 0;
	char *buf = (char *) CONFIG_SYS_LOAD_ADDR;
	unsigned int buf_size =  0;

	if (argc >= 2)
		buf = (char *) simple_strtoul(argv[1], NULL, 16);

	smc_id = parse_fip(&buf, &buf_size, &offset, &size, &flag);
	if (smc_id != 0)
	{
		sha_alg = get_sha_alg();
		sha_alg = (sha_alg & HASH_MODE_MASK);
		return verify_img(buf, buf_size, sha_alg, offset, size, flag, smc_id);
	}
#endif
	return 0;
}

U_BOOT_CMD(
		fip_test,   4,      0,      fip_test_command,
		"fip_test - test command\n",
		"fip_test usage:\n"
		"	fip_test CMD ARG1 ARG2\n"
);
