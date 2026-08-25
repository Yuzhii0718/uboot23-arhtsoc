
// SPDX-License-Identifier: GPL-2.0
/*
 * Airoha UART driver
 *
 * Copyright (C) 2023 Airoha Technology Corp.
 * Author: Shubham Jain <shubham.jain@airoha.com>
 */


#include <clk.h>
#include <common.h>
#include <div64.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <errno.h>
#include <log.h>
#include <serial.h>
#include <watchdog.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/types.h>
#include <linux/err.h>
#include <config.h>
/*************************
 * UART Module Registers *
 *************************/
#define	CR_UART_BASE		0x1FBF0000
#define	CR_UART_RBR			(CR_UART_BASE+0x00) 
#define	CR_UART_THR			(CR_UART_BASE+0x00) 
#define	CR_UART_IER			(CR_UART_BASE+0x04)  
#define	CR_UART_IIR			(CR_UART_BASE+0x08) 
#define	CR_UART_FCR			(CR_UART_BASE+0x08) 
#define	CR_UART_LCR			(CR_UART_BASE+0x0c) 
#define	CR_UART_MCR			(CR_UART_BASE+0x10) 
#define	CR_UART_LSR			(CR_UART_BASE+0x14)  
#define	CR_UART_MSR			(CR_UART_BASE+0x18) 
#define	CR_UART_SCR			(CR_UART_BASE+0x1c) 
#define	CR_UART_BRDL		(CR_UART_BASE+0x00)
#define	CR_UART_BRDH		(CR_UART_BASE+0x04)
#define	CR_UART_WORDA		(CR_UART_BASE+0x20)
#define	CR_UART_MISCC		(CR_UART_BASE+0x24)
#define	CR_UART_HWORDA		(CR_UART_BASE+0x28)
#define	CR_UART_XYD			(CR_UART_BASE+0x2c)

#define	UART_BRD_ACCESS		0x80
#define	UART_XYD_Y			65000
#define	UART_UCLK_115200	0
#define	UART_UCLK_57600		1
#define	UART_UCLK_38400		2
#define	UART_UCLK_28800		3
#define	UART_UCLK_19200		4
#define	UART_UCLK_14400		5
#define	UART_UCLK_9600		6
#define	UART_UCLK_4800		7
#define	UART_UCLK_2400		8
#define	UART_UCLK_1200		9
#define	UART_UCLK_600		10
#define	UART_UCLK_300		11
#define	UART_UCLK_110		12
#define	UART_BRDL			0x03
#define	UART_BRDH			0x00
#define	UART_BRDL_20M		0x01
#define	UART_BRDH_20M		0x00
#define	UART_LCR			0x03
#define	UART_FCR			0x0f
#define	UART_WATERMARK		(0x0<<6)
#define	UART_MCR			0x0
#define	UART_MISCC			0x0
#define	UART_IER			0x01

#define	IER_RECEIVED_DATA_INTERRUPT_ENABLE	0x01
#define	IER_THRE_INTERRUPT_ENABLE			0x02
#define	IER_LINE_STATUS_INTERRUPT_ENABLE	0x04

#define	IIR_INDICATOR						readb(CR_UART_IIR)
#define	IIR_RECEIVED_LINE_STATUS			0x06
#define	IIR_RECEIVED_DATA_AVAILABLE			0x04
#define	IIR_RECEIVER_IDLE_TRIGGER			0x0C
#define	IIR_TRANSMITTED_REGISTER_EMPTY		0x02
#define	LSR_INDICATOR						readb(CR_UART_LSR)
#define	LSR_RECEIVED_DATA_READY				0x01
#define	LSR_OVERRUN							0x02
#define	LSR_PARITY_ERROR					0x04
#define	LSR_FRAME_ERROR						0x08
#define	LSR_BREAK							0x10
#define	LSR_THRE							0x20
#define	LSR_THE								0x40
#define	LSR_RFIFO_FLAG						0x80


/* FCR */
#define UART_FCR_FIFOE              (1 << 0)
#define UART_FCR_CLRR               (1 << 1)
#define UART_FCR_1B                 (0 << 6)
#define UART_FCR_4B                 (1 << 6)

/* LCR */
#define UART_LCR_BREAK              (1 << 6)
#define UART_LCR_DLAB               (1 << 7)

#define UART_WLS_5                  (0 << 0)
#define UART_WLS_6                  (1 << 0)
#define UART_WLS_7                  (2 << 0)
#define UART_WLS_8                  (3 << 0)
#define UART_WLS_MASK               (3 << 0)

#define UART_1_STOP                 (0 << 2)
#define UART_2_STOP                 (1 << 2)
#define UART_STOP_MASK              (1 << 2)

#define UART_NONE_PARITY            (0 << 3)
#define UART_ODD_PARITY             (0x1 << 3)
#define UART_EVEN_PARITY            (0x3 << 3)
#define UART_MARK_PARITY            (0x5 << 3)
#define UART_SPACE_PARITY           (0x7 << 3)
#define UART_PARITY_MASK            (0x7 << 3)

/* MCR */
#define UART_MCR_RTS                (1 << 1)
#define UART_MCR_LOOP               (1 << 4)

/* LSR */
#define UART_LSR_DR                 (1 << 0)
#define UART_LSR_OE                 (1 << 1)
#define UART_LSR_PE                 (1 << 2)
#define UART_LSR_FE                 (1 << 3)
#define UART_LSR_BI                 (1 << 4)
#define UART_LSR_THRE               (1 << 5)
#define UART_LSR_TEMT               (1 << 6)
#define UART_LSR_FIFOERR            (1 << 7)

/* MSR */
#define UART_MSR_DCTS               (1 << 0)
#define UART_MSR_CTS                (1 << 4)

#if defined(CONFIG_DM_SERIAL)
/* crystal clock is 20Mhz */
static unsigned long uclk_20M[13]={ // 65000*(b*16*1)/2000000
	59904,		// Baud rate 115200
	29952,		// Baud rate 57600
	19968,		// Baud rate 38400
	14976,		// Baud rate 28800
	9984,		// Baud rate 19200
	7488,		// Baud rate 14400
	4992,		// Baud rate 9600
	2496,		// Baud rate 4800
	1248,		// Baud rate 2400
	624,		// Baud rate 1200
	312,		// Baud rate 600
	156,		// Baud rate 300
	57			// Baud rate 110
};

int arht_serial_getc(struct udevice *dev)	/* returns -1 if no data available */
{
	
	while (!(readl(CR_UART_LSR) & UART_LSR_DR));
	return readl(CR_UART_RBR);
	
}

int arht_serial_putc( struct udevice *dev, char ch)
{
	
	while (!(readl(CR_UART_LSR) & LSR_THRE));
	writel(ch , CR_UART_THR); 
	return 0 ; 
	
}
static int arht_serial_setbrg(struct udevice *dev, int baudrate)
{
	/*stub function*/
	return 0;
}

static int arht_serial_pending(struct udevice *dev, bool input)
{
	if(input)
		return (readl(CR_UART_LSR) & UART_LSR_DR)? 1 : 0;
	else
		return (readl(CR_UART_LSR) & LSR_THRE) ? 0 : 1;
}

static int arht_serial_puts(struct udevice *dev, const char *s , size_t size )
{
	while(*s)	
	{
		if (*s == '\n')
			arht_serial_putc(dev,'\r');
		arht_serial_putc(dev , *s);
		s++;
	}
	return size; 
}


static int arht_serial_probe(struct udevice *dev)
{
	//ECNT
	// Set FIFO controo enable, reset RFIFO, TFIFO, 16550 mode, watermark=0x00 (1 byte)
	writel(UART_FCR|UART_WATERMARK, CR_UART_FCR);

	// Set modem control to 0
	writel(UART_MCR, CR_UART_MCR);

	// Disable IRDA, Disable Power Saving Mode, RTS , CTS flow control
	writel(UART_MISCC, CR_UART_MISCC);

	// Set interrupt Enable to, enable Tx, Rx and Line status
	writel(UART_IER, CR_UART_IER);

	/* access the bardrate divider */
	writel(UART_BRD_ACCESS, CR_UART_LCR);

	writel(((((unsigned int)(uclk_20M[0]))<<16)|UART_XYD_Y), CR_UART_XYD);

	/* Set Baud Rate Divisor to 1*16 */
	writel(UART_BRDL_20M, CR_UART_BRDL);
	writel(UART_BRDH_20M, CR_UART_BRDH);

	/* Set DLAB = 0, clength = 8, stop =1, no parity check	*/
	writel(UART_LCR, CR_UART_LCR);

	return 0;
}

static int arht_serial_of_to_plat(struct udevice *dev)
{	
	fdt_addr_t addr;

	addr = dev_read_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	return 0;
}

static const struct dm_serial_ops arht_serial_ops = {
	 
	.putc = arht_serial_putc,               
	.puts = arht_serial_puts,				
	.getc = arht_serial_getc,  
	.pending = arht_serial_pending,
	.setbrg = arht_serial_setbrg,
};

static const struct udevice_id arht_serial_ids[] = {
	{ .compatible = "airoha,en7523-uart" },
	{ .compatible = "airoha,an7552-uart" },
	{ .compatible = "airoha,an7581-uart" },
	{ .compatible = "airoha,an7583-uart" },
};

U_BOOT_DRIVER(serial_arht) = {
	.name = "serial_arht",
	.id = UCLASS_SERIAL,
	.of_match = arht_serial_ids,
	.of_to_plat = arht_serial_of_to_plat,
	.probe = arht_serial_probe,
	.ops = &arht_serial_ops,
};
#endif
