#ifndef __FLASHHAL_H__
#define __FLASHHAL_H__

#include <asm/tc3162.h>

#include <spi/spi_controller.h>
#include <spi/spi_nand_flash.h>

#ifdef TCSUPPORT_EMMC
#define isEMMC	((VPint(0x1fb000b8) & (1 << 6)) ? 1 : 0)
#define MMC_BLOCK_SIZE	(get_blk_len())
u64 get_emmc_capacity(void);
uint get_blk_len(void);
#endif

struct mtd_info {
	unsigned long offset; /* Offset within the memory */
	unsigned long size;   /* Total size of the MTD */
	unsigned long erasesize;
};

#define KB_SHIFT 10
#define MB_SHIFT 20

#if !defined(BOOTROM_EXT) && !defined(LZMA_IMG)
extern int flash_partial_write(unsigned long to, unsigned long len, const unsigned char *buf);
#endif

extern int flash_init(unsigned long rom_base);
extern int flash_read(unsigned long from,
	unsigned long len, unsigned long *retlen, unsigned char *buf);
extern int flash_write(unsigned long to,
	unsigned long len, unsigned long *retlen, const unsigned char *buf);
extern int flash_erase(unsigned long addr, unsigned long size);
extern int flash_erase_non_block(unsigned long addr, unsigned long size);
extern int FlashBlockDFU(unsigned long dst, unsigned long src, unsigned long len);
extern unsigned long flash_base;
extern unsigned long flash_tclinux_start;
extern u32 reservearea_size;

#if/*TCSUPPORT_COMPILE*/ defined(TCSUPPORT_FH_ENV)
extern unsigned long fh_kernelA_start;
extern unsigned long fh_kernelB_start;
extern unsigned long fh_kernelA_len;
extern unsigned long fh_kernelB_len;

#define FH_ROOTFLAG_ADDR 0x80001fff
#endif/*TCSUPPORT_COMPILE*/

extern unsigned char ReadNandCache(unsigned long index);
extern unsigned char ReadNandByte(unsigned long index);
extern unsigned long ReadNandDWord(unsigned long index);

#ifdef TCSUPPORT_NEW_SPIFLASH
extern unsigned char ReadSPICache(unsigned long index);
extern unsigned char ReadSPIByte(unsigned long index);
extern unsigned long ReadSPIDWord(unsigned long index);
#endif

#if defined(TCSUPPORT_CPU_EN7512)||defined(TCSUPPORT_CPU_EN7521)
#define READ_FLASH_CACHE(i, status)			( IS_NANDFLASH ? SPI_NAND_Flash_Read_Byte(i, status): ReadSPICache(i)  )
#define READ_FLASH_BYTE(i, status)			( IS_NANDFLASH ? SPI_NAND_Flash_Read_Byte(i, status): ReadSPIByte(i)   )
#define READ_FLASH_DWORD(i, status)			( IS_NANDFLASH ? SPI_NAND_Flash_Read_DWord(i, status): ReadSPIDWord(i) )
#else
#define READ_FLASH_CACHE(i, status)			( NANDFLASH_HWTRAP ? ReadNandCache(i) : ReadSPICache(i) )
#define READ_FLASH_BYTE(i, status)			( NANDFLASH_HWTRAP ? ReadNandByte(i) : ReadSPIByte(i) )
#define READ_FLASH_DWORD(i, status)			( NANDFLASH_HWTRAP ? ReadNandDWord(i) : ReadSPIDWord(i) )
#endif


/*#ifdef CONFIG_DUAL_IMAGE*/
#if 0
extern int flash_write_anyaddr(unsigned long to,
	unsigned long len, unsigned long *retlen, const unsigned char *buf);
#endif
#endif /* __FLASHHAL_H__ */
