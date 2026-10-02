#include <asm/tc3162.h>
#include <asm/io.h>
#include <flashhal.h>
#include <spi/spi_nand_flash.h>
#include <malloc.h>
#include <flash_layout/tc_partition.h>
#include <common.h>

unsigned long flash_base;
extern struct mtd_info mtd;
extern struct ra_nand_chip ra;
unsigned int flashhal_image_read_mode = 0; /* name conflict with ecnt_image.c: */

#ifndef _SPI_NAND_BLOCK_SIZE_512KBYTE
#define _SPI_NAND_BLOCK_SIZE_512KBYTE		0x80000
#endif

#ifdef TCSUPPORT_EMMC
typedef unsigned char uchar;
#include <mmc.h>

extern int mmc_init(struct mmc *mmc);
extern int mmc_block_read(int dev_num, unsigned long blknr, u32 blkcnt, unsigned long *dst);
extern int mmc_block_write(int dev_num, unsigned long blknr, u32 blkcnt, unsigned long *src);
extern int mmc_block_erase(int dev_num, unsigned long blknr_start, unsigned long cnt);
extern int nand_flash_avalable_size;
extern int nandflash_init(int rom_base);
extern int nandflash_read(unsigned long from, unsigned long len, u32 *retlen, unsigned char *buf, SPI_NAND_FLASH_RTN_T *status);
extern int nandflash_write(unsigned long to, unsigned long len, u32 *retlen, unsigned char *buf);
extern int nandflash_erase(unsigned long offset, unsigned long len);
extern int spiflash_init (unsigned long rom_base);
extern int spiflash_erase(unsigned long addr, unsigned long size);
extern int spiflash_read(unsigned long from, unsigned long len, unsigned long *retlen, unsigned char *buf);
extern int spiflash_write(unsigned long to, unsigned long len, unsigned long *retlen, const unsigned char *buf);
extern u64 get_mmc_card_nblks (int dev_num);

struct mmc_data_priv {
    char id;
};

struct mmc arht_mmc;
struct mmc_data_priv arht_mmc_priv;

#define ARHT_DEV_NUM	(0)

#endif

#ifndef TCSUPPORT_EMMC
extern int nandflash_init(int rom_base);
extern int nandflash_read(unsigned long from, unsigned long len, u32 *retlen, unsigned char *buf, SPI_NAND_FLASH_RTN_T *status);
extern int nandflash_write(unsigned long to, unsigned long len, u32 *retlen, unsigned char *buf);
extern int nandflash_erase(unsigned long offset, unsigned long len);
#ifdef TCSUPPORT_NEW_SPIFLASH
extern int spiflash_init (unsigned long rom_base);
extern int spiflash_erase(unsigned long addr, unsigned long size);
extern int spiflash_read(unsigned long from, unsigned long len, unsigned long *retlen, unsigned char *buf);
extern int spiflash_write(unsigned long to, unsigned long len, unsigned long *retlen, const unsigned char *buf);
#endif
#endif

__attribute__((aligned(512))) unsigned char block_buf[_SPI_NAND_BLOCK_SIZE_512KBYTE];

unsigned char *get_block_buf(void)
{	
	return block_buf;
}

#ifdef TCSUPPORT_EMMC
u64 get_emmc_user_capacity(void)
{
	return get_mmc_card_nblks(ARHT_DEV_NUM)*(u64)(arht_mmc.read_bl_len);
}

uint get_blk_len(void)
{
	return arht_mmc.read_bl_len;
}
#endif

#if 0
static void dram_dump(unsigned char *buf, int len)
{
	int i;
	
	printf("\ndump 0x%x: len:0x%x\n", buf, (unsigned int)len);
	for(i=1;i<=len;i++){
		if((i - 1) % 16 == 0) {
			printf("0x%x: ", (unsigned int)&(buf[i-1]));
		}
		printf("%02x ", buf[i-1]);
		if(i % 8 == 0) {
			printf(" ");
		}
		if(i % 16 == 0) {
			printf("\n");
		}
	}
	printf("\n");
}
#else
#ifdef TCSUPPORT_EMMC
static void dram_dump(unsigned char *buf, int len)
{
	return;
}
#endif
#endif


int flash_partial_write(unsigned long to, unsigned long len, const unsigned char *buf)
{
	unsigned long erasesize = 0;
	unsigned long block_start_addr = 0;
	unsigned long block_offset_addr = 0;
	unsigned long retlen = 0;

#ifdef TCSUPPORT_EMMC
	if(isEMMC) {
		erasesize = MMC_BLOCK_SIZE;
	} else 
#endif
    if (IS_NANDFLASH) {
		erasesize = ( 1 << ra.flash->erase_shift );
	}
#ifdef TCSUPPORT_NEW_SPIFLASH
	else if (IS_SPIFLASH) {
		to &= ~(mtd.offset);
		erasesize = mtd.erasesize;
	}
#endif
	else {
		printf("only support spiflash/nandflash\n");
		return -1;
	}

	block_offset_addr = to % erasesize;
	block_start_addr = to - block_offset_addr;

	if (block_offset_addr + len > erasesize) {
		printf("over block boundary\n");
		return -1;
	}

	memset(block_buf, 0, _SPI_NAND_BLOCK_SIZE_512KBYTE);

	if (flash_read(block_start_addr, erasesize, &retlen, block_buf) != 0) {
		printf("read error\n");
		return -1;
	}

	if (flash_erase(block_start_addr, erasesize) != 0) {
		printf("erase error\n");
		return -1;
	}

	memcpy(&block_buf[block_offset_addr], buf, len);

	if (flash_write(block_start_addr, erasesize, &retlen, block_buf) != 0) {
		printf("write error\n");
		return -1;
	}

	return 0;
}

int flash_erase_non_block(unsigned long addr, unsigned long size)
{
	unsigned long start = 0, end = 0, add_head_size = 0, add_tail_size = 0, temp = 0;
	unsigned char *p_head_buf = NULL, *p_tail_buf = NULL;
	int result = -1;
	unsigned int is_EMMC = 0;
	unsigned int erase_size = 0;

#ifdef TCSUPPORT_EMMC
	is_EMMC = (unsigned int)isEMMC;
	erase_size = MMC_BLOCK_SIZE;
#endif

	if (IS_NANDFLASH || is_EMMC) {
		struct SPI_NAND_FLASH_INFO_T flash_info_t;

		if(IS_NANDFLASH) {
			SPI_NAND_Flash_Get_Flash_Info(&flash_info_t);
			erase_size = flash_info_t.erase_size;
		}

		temp = (addr % erase_size);
		
		if (temp != 0)										/* addr isn't block alignment */
		{
			start = addr - temp;							/* record start_addr of first block */
			add_head_size = temp;							/* record redundant size of first block */

			p_head_buf = malloc((size_t) add_head_size);	/* alloc buf and read redundant data of first block */

			if (p_head_buf == NULL)
			{
				goto error;
			}

			if(flash_read(start, add_head_size, &temp, p_head_buf))
			{
				goto error;
			}
		}

		temp = ((addr + size) % erase_size);
		if (temp != 0)										/* end_addr isn't block alignment */
		{
			end = addr + size;								/* record end_addr */
			add_tail_size = erase_size - temp;				/* record redundant size of last block */

			p_tail_buf = malloc((size_t) add_tail_size);	/* alloc buf and read redundant data of last block */

			if (p_tail_buf == NULL)
			{
				goto error;
			}

			if(flash_read(end, add_tail_size, &temp, p_tail_buf))
			{
				goto error;
			}
		}

		if ((add_head_size == 0) && (add_tail_size == 0))
		{
			return flash_erase(addr, size);
		}
		else
		{
			if(flash_erase(start, (size + add_head_size + add_tail_size)))
			{
				goto error;
			}

			if (add_head_size != 0)
			{
				if (flash_write(start, add_head_size, &temp, p_head_buf))
				{
					goto error;
				}
			}

			if (add_tail_size != 0)
			{
				if (flash_write(end, add_tail_size, &temp, p_tail_buf))
				{
					goto error;
				}
			}
		}

	}
	else if (IS_SPIFLASH) {
		return -1;
	}
	else {
		return -1;
	}

	result = 0;

error:
	if (p_head_buf != NULL) {
		free(p_head_buf);
	}

	if (p_tail_buf != NULL) {
		free(p_tail_buf);
	}

	return result;
}

int flash_init(unsigned long rom_base)
{
	int ret = -1;
	
#ifdef TCSUPPORT_EMMC
	if(isEMMC) {
		flash_base = 0x0;
		arht_mmc_priv.id = 0;
		arht_mmc.priv = &arht_mmc_priv;
		ret = mmc_init(&arht_mmc);
		nand_flash_avalable_size = get_emmc_user_capacity();
		reservearea_size = 0x40000;
		return ret;
	} else 
#endif
	if (IS_NANDFLASH) {
		flash_base = 0x0;
		return nandflash_init(flash_base);
	}
#ifdef TCSUPPORT_NEW_SPIFLASH
	else if (IS_SPIFLASH) {
	#ifdef INCLUDE_ECNT_NOR_FLASH_SET_EMPTY_BASE
		flash_base = 0x0;
	#else /* !INCLUDE_ECNT_NOR_FLASH_SET_EMPTY_BASE */
		VPint(0x1fb00038) &= 0xffe0e0e0;
		VPint(0x1fb00038) |= 0x80070F00;
		flash_base = 0x1c000000;
	#endif /* INCLUDE_ECNT_NOR_FLASH_SET_EMPTY_BASE */

		return spiflash_init(flash_base);
	}
#endif
	else {
		return -1;
	}

	return ret;
}

int flash_erase(unsigned long addr, unsigned long size)
{
#ifdef TCSUPPORT_EMMC
	unsigned long blknr_start = 0, blkcnt = 0, retlen = 0;

	if(isEMMC) {
		/* need to align to MMC_BLOCK_SIZE
		 */
		assert(((addr % MMC_BLOCK_SIZE) == 0U));
		blknr_start = addr / MMC_BLOCK_SIZE;
		blkcnt = (addr + size) / MMC_BLOCK_SIZE;
		if(((addr + size) % MMC_BLOCK_SIZE) != 0) {
			blkcnt++;
		}
		blkcnt -= blknr_start;

		//printf("%s %d blknr_start:0x%x, blkcnt:0x%x\n", __func__, __LINE__,  blknr_start, blkcnt);
		retlen = mmc_block_erase(ARHT_DEV_NUM, blknr_start, blkcnt);
		if(retlen != blkcnt) {
			printf("emmc erase fail: retlen:%lu, lba_cnt:%lu.\n", retlen, blkcnt);
			return -1;
		}

		return 0;
	} else 
#endif
	if (IS_NANDFLASH) {
		return nandflash_erase(addr, size);
	}
#ifdef TCSUPPORT_NEW_SPIFLASH
	else if (IS_SPIFLASH) {
		return spiflash_erase(addr, size);
	} 
#endif
	else {
		return -1;
	}
}

int flash_read(unsigned long from,
	unsigned long len, unsigned long *retlen, unsigned char *buf)
{
	SPI_NAND_FLASH_RTN_T status;

#ifdef TCSUPPORT_EMMC
	if(isEMMC) {
		unsigned long remain_len = 0;
		unsigned char *buf_tmp = buf;
		unsigned long nonalignment_from = 0, nonalignment_len = 0, offset = 0, 
			blknr_start = 0, blknr_end = 0, blkcntIdx = 0;
		int read_status;
		
		/* blknr_start is mmc block size alignment
		 * if "from" address is none mmc block size alignment
		 * we need memory shift copy.
		 */
		remain_len = len;
		nonalignment_from = from;
		offset = nonalignment_from % MMC_BLOCK_SIZE;

		//printf("%s %d:from:0x%x, len:0x%x, offset:0x%x\n", __func__, __LINE__, from, len, offset);
		//printf("%s %d:nonalignment_from:0x%x\n", __func__, __LINE__, nonalignment_from);

		if(offset != 0) {
			blknr_start = nonalignment_from / MMC_BLOCK_SIZE;
			//printf("%s %d:blknr_start:0x%x\n", __func__, __LINE__, blknr_start);
			read_status = mmc_block_read(ARHT_DEV_NUM, blknr_start, 1,(long unsigned int *) block_buf);
			if(read_status != 0) {
				printf("%s %d:emmc read fail: read_status:%u, blknr_start:0x%lx.\n", __func__, __LINE__, read_status, blknr_start);
				return -1;
			}
			
			nonalignment_len = (MMC_BLOCK_SIZE - offset);
			if(remain_len < nonalignment_len) {
				nonalignment_len = remain_len;
			}

			//printf("%s %d:nonalignment_len:0x%x\n", __func__, __LINE__, nonalignment_len);
			
			memcpy(buf_tmp, block_buf + offset, nonalignment_len);
			remain_len -= nonalignment_len;
			nonalignment_from += nonalignment_len;
			buf_tmp += nonalignment_len;
		}

		//printf("%s %d:nonalignment_from:0x%x, remain_len:0x%x\n", __func__, __LINE__, nonalignment_from, remain_len);

		if(remain_len && (remain_len >= MMC_BLOCK_SIZE)) {
			blknr_start = nonalignment_from / MMC_BLOCK_SIZE;
			blknr_end = (nonalignment_from + remain_len) / MMC_BLOCK_SIZE;
			//printf("%s %d:blknr_start:0x%x, blknr_end:0x%x\n", __func__, __LINE__, blknr_start, blknr_end);
			for(blkcntIdx = blknr_start; blkcntIdx < blknr_end; blkcntIdx++) {
				//printf("%s %d:blkcntIdx:0x%x\n", __func__, __LINE__, blkcntIdx);
				read_status = mmc_block_read(ARHT_DEV_NUM, blkcntIdx, 1,(long unsigned int *) block_buf);
				if(read_status != 0) {
					printf("%s %d:emmc read fail: read_status:%u, blkcntIdx:0x%lx.\n", __func__, __LINE__, read_status, blkcntIdx);
					return -1;
				}
				memcpy(buf_tmp, block_buf, MMC_BLOCK_SIZE);
				remain_len -= MMC_BLOCK_SIZE;
				nonalignment_from += MMC_BLOCK_SIZE;
				buf_tmp += MMC_BLOCK_SIZE;
			}
		}

		//printf("%s %d:nonalignment_from:0x%x, remain_len:0x%x\n", __func__, __LINE__, nonalignment_from, remain_len);

		if(remain_len) {
			blknr_start = nonalignment_from / MMC_BLOCK_SIZE;
			//printf("%s %d:blknr_start:0x%x\n", __func__, __LINE__, blknr_start);
			read_status = mmc_block_read(ARHT_DEV_NUM, blknr_start, 1,(long unsigned int *) block_buf);
			if(read_status != 0) {
				printf("%s %d:emmc read fail: read_status:%u, blknr_start:0x%lx.\n", __func__, __LINE__, read_status, blknr_start);
				return -1;
			}
			memcpy(buf_tmp, block_buf, remain_len);
			buf_tmp += remain_len;
		}

		dram_dump(buf, len);

		return 0;
	} else 
#endif
	if (IS_NANDFLASH) {
		return nandflash_read(from, len,(u32 *) retlen, buf, &status);
	}
#ifdef TCSUPPORT_NEW_SPIFLASH
	else if (IS_SPIFLASH) {
		return spiflash_read(from, len, retlen, buf);
	}
#endif
	else {
		return -1;
	}
}

int flash_write(unsigned long to,
	unsigned long len, unsigned long *retlen, const unsigned char *buf)
{
#ifdef TCSUPPORT_EMMC
	unsigned long blknr_start = 0;
	u32 blkcnt = 0;
	int blkcnt_write = 0;
	
	if(isEMMC) {
		assert(((to % MMC_BLOCK_SIZE) == 0U));
		
		/* blknr_start is mmc block size alignment
		 */
		blknr_start = to / MMC_BLOCK_SIZE;
		blkcnt = (to + len) / MMC_BLOCK_SIZE;
		if(((to + len) % MMC_BLOCK_SIZE) != 0) {
			blkcnt++;
		}
		blkcnt -= blknr_start;

		//printf("%s %d blknr_start:0x%x, blkcnt:0x%x, buf:0x%p\n", __func__, __LINE__,  blknr_start, blkcnt, buf);
		blkcnt_write = mmc_block_write(ARHT_DEV_NUM, blknr_start, blkcnt, (long unsigned int *)buf);
		if(blkcnt_write != blkcnt) {
			printf("emmc write fail: blkcnt_write:%u, blkcnt:%u.\n", blkcnt_write, blkcnt);
			return -1;
		}

		return 0;
	} else 
#endif
	if (IS_NANDFLASH) {
		return nandflash_write(to, len, (u32 *)retlen, (unsigned char *)buf);
	}
#ifdef TCSUPPORT_NEW_SPIFLASH
	else if (IS_SPIFLASH) {
		return spiflash_write(to, len, retlen, buf);
	}
#endif
	else {
		return -1;
	}
}

int image_read(unsigned long from,
	unsigned long len, unsigned long *retlen, unsigned char *buf)
{
	if (flashhal_image_read_mode == 0)
	{
		if (retlen != NULL)
			return flash_read(from, len, retlen, buf);
		else
			return -1;
	}
	else
	{
		*retlen = len;
		memcpy(buf, (unsigned long *) (CONFIG_RAMFS_BASE + from), len);
		return 0;
	}

}
