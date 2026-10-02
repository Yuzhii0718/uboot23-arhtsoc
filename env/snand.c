// SPDX-License-Identifier: GPL-2.0-only 
/*
 * Copyright (c) 2024 AIROHA Inc
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
 */ 
#include <common.h>
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

#define OFFSET_INVALID      (~(u32)0)

#define ENV_OFFSET_REDUND   OFFSET_INVALID


DECLARE_GLOBAL_DATA_PTR;


static int setup_mtd_device(struct mtd_info **mtd_env)
{
	struct mtd_info *mtd;
	
	mtd_probe_devices();
	
	mtd = get_mtd_device_nm(CONFIG_SYS_SNAND_ENV_DEV);
	if (IS_ERR_OR_NULL(mtd))
		printf("MTD device %s not found, ret %ld\n", CONFIG_SYS_SNAND_ENV_DEV,
		       PTR_ERR(mtd));
	*mtd_env = mtd;
	
	return 0;
}


static int env_snand_save(void)
{
	u32 saved_size = 0;
	size_t ret_len = 0;
	u32 sect_size = CONFIG_ENV_SECT_SIZE;
    u32 start, end, sect_start, sect_end, sect_num;
	char *saved_buffer = NULL;
	int ret = 1;
	env_t env_new;
	struct mtd_info *mtd_env;
	struct erase_info ei;
	ret = setup_mtd_device(&mtd_env);
    if (ret)
        return ret;
    start = CONFIG_ENV_OFFSET;
    end = start + CONFIG_ENV_SIZE;
    sect_start = sect_size * (start / sect_size);
    sect_end = sect_size * DIV_ROUND_UP(end, sect_size);
    sect_num = (sect_end - sect_start) / sect_size;
    
    saved_size = sect_num * sect_size;
    saved_buffer = malloc(saved_size);
    if (!saved_buffer) {
            goto done;
    }
   
    ret = mtd_read(mtd_env, sect_start, saved_size, &ret_len, saved_buffer);
    if (ret) {
            goto done;
    }
   

	ret = env_export(&env_new);
	if (ret)
		goto done;

    memcpy(saved_buffer + (start - sect_start), &env_new, CONFIG_ENV_SIZE);

	puts("Erasing SPI NAND flash...\r\n");
	
	ei.mtd = mtd_env;
	ei.addr = sect_start;
	ei.len = sect_num * sect_size;
	ret = mtd_erase(mtd_env, &ei);
	if (ret)
		goto done;

	puts("Writing to SPI flash...\r\n");
	ret = mtd_write(mtd_env, sect_start, saved_size, &ret_len, saved_buffer);
	if (ret)
		goto done;
    puts("done\n");

done:
	if (saved_buffer)
		free(saved_buffer);

	return ret;
}

static int env_snand_load(void)
{
	int ret;
	char *buf = NULL;
	size_t ret_len = 0;
	struct mtd_info *mtd_env;
		

	buf = (char *)memalign(ARCH_DMA_MINALIGN, CONFIG_ENV_SIZE);
	if (!buf) {
		env_set_default("malloc() failed", 0);
		return -EIO;
	}

	ret = setup_mtd_device(&mtd_env);
	if (ret)
		return ret;
		
	ret = mtd_read(mtd_env, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE, &ret_len, buf);
	if (ret) {
		env_set_default("spi_flash_read() failed", 0);
		goto out;
	}

	ret = env_import(buf, 0, H_EXTERNAL);
	if (!ret)
		gd->env_valid = ENV_VALID;

out:
	free(buf);

	return ret;
}

static int env_snand_erase(void)
{
	int ret;
	//size_t ret_len = 0;
	//env_t env;
	struct mtd_info *mtd_env;
	ret = setup_mtd_device(&mtd_env);
	if (ret)
		return ret;

done:
	return ret;
}

__weak void *env_snand_get_env_addr(void)
{
	return (void *)CONFIG_ENV_ADDR;

}

/*
 * check if Environment on CONFIG_ENV_ADDR is valid.
 */
static int env_snand_init_addr(void)
{
	env_t *env_ptr = (env_t *)env_snand_get_env_addr();

	if (!env_ptr)
		return -ENOENT;

	if (crc32(0, env_ptr->data, ENV_SIZE) == env_ptr->crc) {
		gd->env_addr = (ulong)&(env_ptr->data);
		gd->env_valid = ENV_VALID;
	} else {
		gd->env_valid = ENV_INVALID;
	}

	return 0;
}


static int env_snand_init(void)
{
	int ret = env_snand_init_addr();
	if (ret != -ENOENT)
		return ret;

	/*
	 * return here -ENOENT, so env_init()
	 * can set the init bit and later if no
	 * other Environment storage is defined
	 * can set the default environment
	 */
	return -ENOENT;
}

U_BOOT_ENV_LOCATION(snand) = {
	.location	= ENVL_SNAND_FLASH,
	ENV_NAME("SNANDFlash")
	.load		= env_snand_load,
	.save		= ENV_SAVE_PTR(env_snand_save),
	.erase		= ENV_ERASE_PTR(env_snand_erase),
	.init		= env_snand_init,
};
