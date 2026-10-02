#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <common.h>
#include "ecnt_image.h"
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/io.h>
#include <linux/delay.h>

#define UNHANDLE_FAIL -1
#define SOCKET_FAIL -2
#define PT_SUCCESS 0
#define CRC_FAIL -3

#define FE_SRAM_BASE 0x1fa30000
#define STATUS_REG	0x1fb00f08
#define MSG_CHANNEL_TX	0x1fb00f00
#define MSG_CHANNEL_RX	0x1fb00f04
#define CRC_DISABLE		0x1fb00f0c
#define IMAGE_LEN 0x1fb00f10


#define MSG_READY 0x57414445
#define MSG_READY_INIC 0x5244494E
#define MSG_IMAGE_DONE 0x444f4E45
#define MSG_IMAGE_DONE_INIC 0x494E444E
#define MSG_SGMII2RGMII_MODE 0x53475247
#define MSG_CRC_FAIL 0x4352464c
#define MSG_CRC_SUCCESS 0x46574F4B
#define MSG_NO_CRC	0x4E4F4352
#define MSG_FINISH 0x66697368

#define BP1_TIMEOUT 10

extern int mdio_msg_send(unsigned int phy_addr, unsigned int addr, unsigned int data);
extern int mdio_msg_rcv(unsigned int phy_addr, unsigned int addr);
extern uint32_t *get_ddr_cal_addr(void);
extern uint32_t get_ddr_cal_len(void);

int mdio_msg_handler_bp1 (unsigned int phy_addr)
{
	unsigned int fe_addr = FE_SRAM_BASE;
	unsigned int data = 0;
	unsigned int file_len = 0;
	unsigned int rcv_result = 0;
	uint32_t *ddr_cal_ptr = NULL;
	uint32_t ddr_cal_len = 0;
	int time = 0;
	
	int ret = UNHANDLE_FAIL;

	/*temp 7517 crc disable */
	mdio_msg_send(phy_addr, CRC_DISABLE, 1);

	ddr_cal_ptr = get_ddr_cal_addr();
	ddr_cal_len = get_ddr_cal_len();

	while(time<BP1_TIMEOUT)
	{
		rcv_result = mdio_msg_rcv(phy_addr, MSG_CHANNEL_TX);
		if (rcv_result == MSG_READY)
		{
			printf("[hsm] mdio_msg_handler_bp1 MSG_READY \n");
			break;
		}
		printf("[hsm] mdio_msg_handler_bp1 not ready\n");
		mdelay(100);
		time++;
		if(time == BP1_TIMEOUT)
		{
			printf("[hsm] timeout!! please check mdio\n");
			return ret;
		}
	}
	printf("[hsm] start writing ddr_cal.bin to client!(7517)\n");
	printf("[hsm] ddr_cal.bin dump ...  start = 0x%8p, length = %u\n", ddr_cal_ptr, ddr_cal_len);
	do
	{
		data = *ddr_cal_ptr;
		ddr_cal_ptr += 1;
		mdio_msg_send(phy_addr, fe_addr, data);
		fe_addr +=4;
		file_len += 4;
	}while(file_len < ddr_cal_len);

	mdio_msg_send(phy_addr,IMAGE_LEN,file_len);

	printf("[hsm] write fw finished ! \n");

	mdio_msg_send(phy_addr, MSG_CHANNEL_RX, MSG_IMAGE_DONE);
	while(1)
	{
		rcv_result = mdio_msg_rcv(phy_addr, MSG_CHANNEL_TX);
		if ((rcv_result == MSG_CRC_SUCCESS) ||(rcv_result == MSG_NO_CRC))
		{
			ret = PT_SUCCESS;
			break;
		}
		else if(rcv_result == MSG_CRC_FAIL)
		{
			ret = CRC_FAIL;
			printf("CRC Check failed !");
			break;
		}
		else
		{
			printf("[hsm] mdio_msg_handler_bp1 rcv_result2 = %x\n", rcv_result);
			continue;
		}
	}

	/* notify client that host has received result*/
	mdio_msg_send(phy_addr, MSG_CHANNEL_RX, MSG_FINISH);

	printf("[hsm] client boot into ddr_cal!\n");

	return ret;
}

void EN7517_pll_setting(unsigned int phy_addr)
{
	unsigned int data = 0;
	data= mdio_msg_rcv(phy_addr, 0x1fb0009c);
	printf("[hsm] config EN7517 pll setting %d, data = %d\n", phy_addr,data);
	if ((data & 0x1) == 0)
	{
		return;
	}
	else
	{
		data = mdio_msg_rcv(phy_addr, 0x1fb0005c);
		if (data == 0x1)
		{
			data = mdio_msg_rcv(phy_addr, 0x1fa20174);
			if(data & (1 << 16)) {
				/* FE PLL */
				mdio_msg_send(phy_addr, 0x1fa20190, 0x12);
				mdio_msg_send(phy_addr, 0x1fa201f8, 0x05001f08);
				mdio_msg_send(phy_addr, 0x1fa201f8, 0x05001f0a);
				mdio_msg_send(phy_addr, 0x1fa201f8, 0x05001f0e);
				mdio_msg_send(phy_addr, 0x1fa20208, 0x03000000);
				mdio_msg_send(phy_addr, 0x1fa20204, 0x402);

				/* CPU PLL */
				//mdio_msg_send(phy_addr, 0x1fa201dc, 0x04101f0a);
				//mdio_msg_send(phy_addr, 0x1fa201dc, 0x04101f0e);
				//mdio_msg_send(phy_addr, 0x1fa201ec, 0x03000000);
				//mdio_msg_send(phy_addr, 0x1fa201e8, 0x402);
			} else {
				/* FE PLL */
				mdio_msg_send(phy_addr, 0x1fa20190, 0x12);
				mdio_msg_send(phy_addr, 0x1fa2020c, 0x04000000);
				mdio_msg_send(phy_addr, 0x1fa20204, 0x402);
			}
		}

	}
}

void client_initialization(int gpio, int restart)
{
	/*set gpio to initialize client*/
	printf("%s: reset Client with gpio %d\n",__FUNCTION__, gpio);

	uint32_t value = 0;

#define _gpioctrl  0x1fbf0200
#define _gpioctrl1 0x1fbf0220
#define _gpioctrl2 0x1fbf0260
#define _gpioctrl3 0x1fbf0264

#define _gpiooe    0x1fbf0214
#define _gpiooe1   0x1fbf0278

#define _gpiodata  0x1fbf0204
#define _gpiodata1 0x1fbf0270

	uint32_t ctrlArray[4] = {_gpioctrl, _gpioctrl1, _gpioctrl2, _gpioctrl3};
	uint32_t openArray[4] = {_gpiooe  , _gpiooe1  };
	uint32_t dataArray[2] = {_gpiodata, _gpiodata1};

	uint32_t ctrlSet = (uint32_t)gpio / 16;
	uint32_t ctrlOffset = ((uint32_t)gpio % 16) << 1;
	uint32_t dataSet = (uint32_t)gpio / 32;
	uint32_t dataOffset = (uint32_t)gpio % 32;

	uint32_t ctrlAddr = ctrlArray[ctrlSet];
	uint32_t openAddr = openArray[dataSet];
	uint32_t dataAddr = dataArray[dataSet];

	void __iomem * io_base;

	/* 1. set gpio output mode*/
	io_base = ioremap(ctrlAddr, sizeof(uint32_t));
	/* read GPIOCTRL */
	value = ioread32(io_base);
	/* set output mode */
	value &= ~(0x03 << ctrlOffset);
	value |=  (0x01 << ctrlOffset);
	/* write GPIOCTRL */
	iowrite32(value, io_base);
	printf("[hsm] gpioctr%d(%d)=0x%08X\n", ctrlSet, ctrlOffset, value);

	/* 2. set gpio output enable*/
	io_base = ioremap(openAddr, sizeof(uint32_t));
	/* read GPIOOE */
	value = ioread32(io_base);
	/* set output mode */
	value |=  (0x01 << dataOffset);
	/* write GPIOOE */
	iowrite32(value, io_base);
	printf("[hsm] gpiooe%d(%d)=0x%08X\n", dataSet, dataOffset, value);

	/* 3. set gpio low*/
	io_base = ioremap(dataAddr, sizeof(uint32_t));
	/* read GPIODATA */
	value = ioread32(io_base);
	/* set gpio low */
	value &= ~(0x01 << dataOffset);
	/* write GPIODATA */
	iowrite32(value, io_base);
	printf("[hsm] gpiodata%d(%d)=0x%08X\n", dataSet, dataOffset, value);

	if (restart)
	{
		/* 4. set gpio high*/
		/* read GPIODATA */
		value = ioread32(io_base);
		/* set gpio high */
		value |=  (0x01 << dataOffset);
		/* write GPIODATA */
		iowrite32(value, io_base);
		printf("[hsm] gpiodata%d(%d)=0x%08X\n", dataSet, dataOffset, value);
	}
}

int host_bp1(int gpio, unsigned int phy_addr)
{
	int ret = -1;
	client_initialization(gpio, 1);
	EN7517_pll_setting(phy_addr);
	ret = mdio_msg_handler_bp1(phy_addr);
	return ret;
}
