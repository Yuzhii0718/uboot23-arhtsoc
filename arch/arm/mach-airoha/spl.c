
#include <common.h>
#include <asm/spl.h>
#include <asm/io.h>
#include <asm/spl.h>
//#include <asm/arch/typedefs.h>
#include <asm/arch/en7523.h>
//#include <asm/arch/ecnt_timer.h>
//#include <xyzModem.h>
//#include <flashhal.h>
#include <asm/global_data.h>


DECLARE_GLOBAL_DATA_PTR;
#define ATF_BOOT_ARG_ADDR               (CONFIG_SYS_SDRAM_BASE + SZ_8K)
#define BL31_BASE			            (ATF_BOOT_ARG_ADDR + SZ_4K)
#define BL33_BASE			            (CONFIG_TPL_TEXT_BASE)
#define RVBADDRESS_CPU0					(0x1EFBE038)

#define XMODEM_BUFFER_SIZE				(SZ_128)

u32 bl31_base_addr = BL31_BASE;
u32 rst_vector_base_addr = RVBADDRESS_CPU0;

extern int ecnt_uart_init (void);
#ifdef TEST_BLOCK_CNT_IN_SRAM
extern void dma_block_cnt_test(void);
#endif
//#define TEST_RBUS_TIMEOUT
#define RBUS_TIMEOUT_BASE	(0x1fa00000)
#define TIMEOUT_STS0		(RBUS_TIMEOUT_BASE + 0xd0)
#define TIMEOUT_STS1		(RBUS_TIMEOUT_BASE + 0xd4)

#ifdef TEST_RBUS_TIMEOUT
#if 1 //FPGA CLK
#define DRAM_CONTROLLER_CLK 0x4000000
#else //ASIC CLK
#define DRAM_CONTROLLER_CLK 0x19000000
#endif
#define DRAM_TEST_ADDR		(0x84000000)
#define FORCE_TIMEOUT		(RBUS_TIMEOUT_BASE + 0xbc)
#define TIMEOUT_CFG0		(RBUS_TIMEOUT_BASE + 0xd8)
#define TIMEOUT_CFG1		(RBUS_TIMEOUT_BASE + 0xdc)
#define TIMEOUT_CFG2		(RBUS_TIMEOUT_BASE + 0xe0)
#endif

void board_init_f(ulong dummy)
{
#if defined(CONFIG_TPL_BUILD)

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	/* Set global data pointer. */

	arch_cpu_init();


	int ret;

	ret = spl_early_init();
	if (ret) {
		debug("spl_early_init() failed: %d\n", ret);
		hang();
	}
	preloader_console_init();

#endif
}

#define HWTRAP_MASK				(0xf)
#define HWTRAP_EMMC_MODE		(0xe)
#define HWTRAP_DEC_MASK			(0x7f)
#define HWTRAP_NAND_MODE		(0x7)
#define HWTRAP_NAND_MODE2   	(0xf)
#define EN7523_HWTRAP_CONF	(0x1FB000B4)

struct legacy_img_hdr *spl_get_load_buffer(ssize_t offset, size_t size)
{
	return (0x8a000000 + offset);
}

u32 spl_boot_device(void)
{
	u32 boot_device = BOOT_DEVICE_NONE;

	u32 hwtrap_cfg = (readw(EN7523_HWTRAP_CONF) & HWTRAP_MASK);

	printf("hwtrap_cfg = %x\n", hwtrap_cfg);
	
	if(hwtrap_cfg == HWTRAP_EMMC_MODE)
	{
		printf("BOOT_DEVICE_MMC1 has chose\n");
		return BOOT_DEVICE_MMC1;
	}
	else
	{
		return BOOT_DEVICE_SPI;
	}

}
