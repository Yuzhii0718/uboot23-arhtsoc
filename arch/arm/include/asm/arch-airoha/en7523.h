/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 
#ifndef EN7523_H
#define EN7523_H

/*=======================================================================*/
/* Constant Definitions													 */
/*=======================================================================*/

#define IO_PHYS				(0x1F000000)
#define IO_SIZE				(0x01000000)


/*=======================================================================*/
/* Register Bases														 */
/*=======================================================================*/

#define SPI_BASE		(IO_PHYS + 0xA10000)  
#define CHIP_CSR_BASE		(IO_PHYS + 0xA20000)
#define DRAMC_BASE		(IO_PHYS + 0xB20000)
#define UART1_BASE		(IO_PHYS + 0xBF0000)
#define TIMER_BASE		(IO_PHYS + 0xBF0100)
#define GPIO_BASE		(IO_PHYS + 0xBF0200)
#define UART2_BASE		(IO_PHYS + 0xBF0300)
#define CPU_TIMER1_BASE		(IO_PHYS + 0xBF0400)
#define CPU_TIMER2_BASE		(IO_PHYS + 0xBE0000)
#define UART3_BASE		(IO_PHYS + 0xBE1000)
#define UART4_BASE		(IO_PHYS + 0xBF0600)
#define UART5_BASE		(IO_PHYS + 0xBF0700)
#define I2C1_BASE		(IO_PHYS + 0xBF8000)
#define I2C2_BASE		(IO_PHYS + 0xBF8100)

#ifdef TCSUPPORT_EMMC
#define MSDC0_BASE 0x1fa0e000
#define MSDC0_TOP_BASE 0x1fa0c000
#endif

/*=======================================================================*/
/* NAND Control															 */
/*=======================================================================*/
#define NAND_PAGE_SIZE					(2048)	// (Bytes)
#define NAND_BLOCK_BLKS					(64)	// 64 nand pages = 128KB
#define NAND_PAGE_SHIFT					(9)
#define NAND_LARGE_PAGE					(11)	// large page
#define NAND_SMALL_PAGE					(9)		// small page
#define NAND_BUS_WIDTH_8				(8)
#define NAND_BUS_WIDTH_16				(16)
#define NAND_FDM_SIZE					(8)
#define NAND_ECC_SW						(0)
#define NAND_ECC_HW						(1)

#define NFI_MAX_FDM_SIZE				(8)
#define NFI_MAX_FDM_SEC_NUM				(8)
#define NFI_MAX_LOCK_CHANNEL			(16)

#define ECC_MAX_CORRECTABLE_BITS		(12)
#define ECC_MAX_PARITY_SIZE				(20)	/* in bytes */

#define ECC_ERR_LOCATION_MASK			(0x1FFF)
#define ECC_ERR_LOCATION_SHIFT			(16)

#define NAND_FFBUF_SIZE					(2048+64)

/*=======================================================================*/
/* SW Reset Vector														 */
/*=======================================================================*/

#endif
