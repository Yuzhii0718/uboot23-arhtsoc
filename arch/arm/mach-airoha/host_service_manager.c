#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <common.h>
#include <net.h>
#include <linux/libfdt.h>
#include <ecnt_flash.h>
#include <ecnt_image.h>
#include <image.h>
#include <linux/mii.h> 
#include <linux/delay.h>
#include <linux/io.h>

unsigned char mac_addr[6] = {0x00,0xAA,0xBB,0x75,0x17,0x00};
void start_sending_tclinux(void);
bool check_tclinux_sending_state(void);
void end_of_sending_tclinux(void);
void fdt_load_init(void);
uint32_t *get_ddr_cal_addr(void);
uint32_t get_ddr_cal_len(void);
uint32_t *get_inic_addr(void);
uint32_t get_inic_len(void);
uint32_t *get_tclinux_7517_addr(void);
uint32_t get_tclinux_7517_len(void);
void client_initialization(int gpio, int restart);
void miiStationWrite(uint32_t enetPhyAddr, uint32_t phyReg, uint32_t miiData);
uint32_t miiStationRead(uint32_t enetPhyAddr, uint32_t phyReg);
int host_bp1(int gpio, unsigned int phy_addr);
int host_bp2(unsigned int phy_addr, unsigned int start_addr);

enum {
	IDLE,
	SENDING,
};

struct bin_file
{
	char* name;
	uint32_t start_addr;
	uint32_t len;
};

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

#define INIC_BIN_NAME "iNIC_7517"
#define DDR_CAL_NAME "ddr_cal_7517"
#define TCLINUX_7517_NAME "tclinux_7517"

static int sending_status = IDLE;
struct bin_file ddr_cal, inic, tclinux_7517;

uint32_t *get_ddr_cal_addr(void)
{
    return (uint32_t *)((uint8_t *)CONFIG_SYS_LOAD_ADDR+ddr_cal.start_addr);
}
uint32_t get_ddr_cal_len(void)
{
    return ddr_cal.len;
}
uint32_t *get_inic_addr(void)
{
    return (uint32_t *)((uint8_t *)CONFIG_SYS_LOAD_ADDR+inic.start_addr);
}
uint32_t get_inic_len()
{
    return inic.len;
}
uint32_t *get_tclinux_7517_addr(void)
{
    return (uint32_t *)((uint8_t *)CONFIG_SYS_LOAD_ADDR+tclinux_7517.start_addr);
}
uint32_t get_tclinux_7517_len(void)
{
    return tclinux_7517.len;
}

void fdt_load_init(void)
{
	/*access FIT data from DRAM*/

    uint32_t size, image_pos, image_len;
    const uint32_t *image_offset_be, *image_len_be, *image_pos_be;
    int node, images;
    const char *image_name, *image_type, *image_description;
	int image_name_len, image_type_len, image_description_len;
	const void *fit = (const void*)CONFIG_SYS_LOAD_ADDR;

    size = fdt_totalsize(fit);
	images = fdt_path_offset(fit, FIT_IMAGES_PATH);

	printf("[hsm] %s(), size = %d\n", __FUNCTION__, size);

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

		//printk(KERN_DEBUG "FIT: %16s sub-image 0x%08x - 0x%08x '%s' %s%s%s\n",
		printf("FIT: %16s sub-image 0x%08x - 0x%08x '%s' %s%s%s\n",
			image_type, image_pos, image_pos + image_len, image_name,
			image_description?"(":"", image_description?:"", image_description?") ":"");

		if(strcmp(image_name, DDR_CAL_NAME) == 0)
		{
			ddr_cal.name = (char *)image_name;
			ddr_cal.start_addr = image_pos;
			ddr_cal.len = image_len;
			printf("[hsm] store ddr_cal info, name:%s, addr:%x, len:%x\n",
				ddr_cal.name, ddr_cal.start_addr, ddr_cal.len);
		}
		else if (strcmp(image_name, INIC_BIN_NAME) == 0)
		{
			inic.name = (char *)image_name;
			inic.start_addr = image_pos;
			inic.len = image_len;
			printf("[hsm] store inic info, name:%s, addr:%x, len:%x\n",
				inic.name, inic.start_addr, inic.len);

		}
		else if (strcmp(image_name, TCLINUX_7517_NAME) == 0)
		{
			tclinux_7517.name = (char *)image_name;
			tclinux_7517.start_addr = image_pos;
			tclinux_7517.len = image_len;
			printf("[hsm] store tclinux info, name:%s addr:%x len:%x\n",
				tclinux_7517.name, tclinux_7517.start_addr, tclinux_7517.len);
		}
	}
}
void start_sending_tclinux(void)
{
	printf("[hsm] start sending tclinux\n");
    sending_status = SENDING;
}

bool check_tclinux_sending_state(void)
{
    if(sending_status == SENDING)
    	return true;
    else
    	return false;
}

void end_of_sending_tclinux(void)
{
	printf("[hsm] stop sending tclinux\n");
    sending_status = IDLE;
}

void etherwan_init(void)
{
	printf("[hsm] %s\n", __FUNCTION__);
	if(eth_init()<0)
	{
		printf("[hsm] eth_init fail...\n");
	}
	if(eth_initialize()<0)
	{
		printf("[hsm] eth_initialize fail...\n");
	}
}

static char inf_buffer[5000*1000];
static int EN8801_cl22_read (int phy_id, int phy_register)
{
	return miiStationRead (phy_id, phy_register);
}

static int EN8801_cl22_write (int phy_id, int phy_register, int write_data)
{
	miiStationWrite (phy_id, phy_register, write_data);

	return 0;
}

/* EN8801 PBUS write function */
static int EN8801_pbus_write(int pbus_reg, unsigned long pbus_data)
{
    int ret = 0;

    ret = EN8801_cl22_write(0x1E, 0x1F, (pbus_reg >> 6));
    ret = EN8801_cl22_write(0x1E, ((pbus_reg >> 2) & 0xf), (pbus_data & 0xFFFF));
    ret = EN8801_cl22_write(0x1E, 0x10, (pbus_data >> 16));
    return ret;
}

/* EN8801 PBUS read function */
static unsigned long EN8801_pbus_read( int pbus_reg)
{
    unsigned long pbus_data;
    unsigned int pbus_data_low, pbus_data_high;

    EN8801_cl22_write(0x1E, 0x1F, (pbus_reg >> 6));
    pbus_data_low = EN8801_cl22_read(0x1E, ((pbus_reg >> 2) & 0xf));
    pbus_data_high = EN8801_cl22_read(0x1E, 0x10);
    pbus_data = (pbus_data_high << 16) + pbus_data_low;
    return pbus_data;
}

void host_service_manager_main(void)
{
	int ethrx = 0;
	int ret = 0;
	/*gpio is reset pin*/
#ifdef TCSUPPORT_CPU_AN7583
	int gpio = 30;
#else
	int gpio = 45;
#endif
	unsigned int value = 0, status = 0;
	unsigned int mdio_phy_addr = 24;
	unsigned int inic_start_addr = 0x1000000;

	printf("[hsm] %s start\n", __FUNCTION__);
	printf("[hsm] gpio=%d mdio_phy_addr=%d inix_start_addr=%x\n", gpio, mdio_phy_addr, inic_start_addr);
	printf("[hsm] mac_addr = %02x:%02x:%02x:%02x:%02x:%02x\n",mac_addr[0],mac_addr[1],mac_addr[2],mac_addr[3],mac_addr[4],mac_addr[5]);

    /*load fw from FDT*/
    fdt_load_init();

	/*etherwan init...*/
	etherwan_init();

	/* Disable EN7517 */
	client_initialization(gpio, 0);

	/* Get EN8801 status */
	value = EN8801_pbus_read(0x18e0);
	status = value & 0xf;

	/* One time setting for EN8801 */
	if (4 == status)
	{
		value = EN8801_pbus_read(0x1900);
		//printf("Before 0x1900 0x%x\n", value);
		ret = EN8801_pbus_write(0x1900, 0x101009f);
		value = EN8801_pbus_read(0x1900);
		//printf("After 0x1900 0x%x\n", value);

		value = EN8801_pbus_read(0x19a8);
		//printf("Before 0x19a8 0x%x\n", value);
		ret = EN8801_pbus_write(0x19a8, (value & ~(1<<16)));
		//value = EN8801_pbus_read(0x19a8);
		//printf("After 0x19a8 0x%x\n", value);

		mdelay(100);

		value = EN8801_cl22_read(0x1D, MII_CTRL1000);
		//printf("Before MII_CTRL1000 = 0x%x value = 0x%x\n", MII_CTRL1000, value);
		value &= ~((1<<11) | (1<<12));
		ret = EN8801_cl22_write(0x1D, MII_CTRL1000, value);
		value = EN8801_cl22_read(0x1D, MII_CTRL1000);
		//printf("After MII_CTRL1000 = 0x%x value = 0x%x\n", MII_CTRL1000, value);

		value = EN8801_cl22_read(0x1D, MII_BMCR);
		//printf("Before MII_BMCR = 0x%x value = 0x%x BMCR_ANRESTART = 0x%x\n", MII_BMCR, value, BMCR_ANRESTART);
		value |= BMCR_ANRESTART;
		ret = EN8801_cl22_write(0x1D, MII_BMCR, value);
		value = EN8801_cl22_read(0x1D, MII_BMCR);
		//printf("After MII_BMCR = 0x%x value = 0x%x\n", MII_BMCR, value);
	}

    printf("[hsm] start boot phase 1\n");
	ret = host_bp1(gpio, mdio_phy_addr);
	if (ret!=0)
	{
		printf("[hsm] host_bp1 fail ...\n");
		return;
	}

	printf("[hsm] start boot phase 2\n");
	ret = host_bp2(mdio_phy_addr, inic_start_addr);

	if (ret!= 0)
	{
		printf("[hsm] host_bp2 fail ...\n");
		return;
	}

#if TCSUPPORT_CPU_AN7583
	uint8_t status_7517 = 0;
	void __iomem * io_base;
	unsigned int rg1, rg2;

	printf("set rg\n");
	io_base = ioremap(0x1fa08000, sizeof(uint32_t));
	iowrite32(0x77fe2830, io_base);

	rg1 = 0;
	rg2 = 0;
	while(status_7517 < 100)
	{
		mdelay(100);
		io_base = ioremap(0x1fa80b04, sizeof(uint32_t));
		rg1 = ioread32(io_base);
		io_base = ioremap(0x1fa8e290, sizeof(uint32_t));
		rg2 = ioread32(io_base);
		printf("rg 0x1fa80b04 = %x\n", rg1);
		printf("rg 0x1fa8e290 = %x\n", rg2);

		if((rg1&0x33) != 0x33)
		{
			printf("rg 0x1fa80b04 status error\n");
		}
		if((rg2&0x1000000) != 0x1000000)
		{
			printf("rg 0x1fa8e290 status error\n");
		}
		if(((rg1&0x33) == 0x33) && ((rg2&0x1000000) == 0x1000000))
		{
			printf("status correct continue...\n");
			break;
		}
		status_7517++;
		if(status_7517 == 100)
		{
			printf("timeout!!!  finish hsm ....\n");
			return;
		}
	}
#elif TCSUPPORT_CPU_AN7581
	/* Check eth port 4 status, because 7551 use lan port 4 as ethwan port
	connect to 7517...*/
	value = miiStationRead (12, 1);
	while ((value & (1<<2)) == 0)
	{
		printf ("Wait connect, phy = 12 reg = 01 value = 0x%x\n", value);
		printf ("phy = 29, reg 01, value = 0x%x\n", miiStationRead (29, 1));
		mdelay(100);
		value = miiStationRead (12, 1);
	}
#else
	printf("not 7581 not 7583...\n");
#endif
    start_sending_tclinux();
    /* dslam data send begin */
	struct DSL_data { 
		uint8_t magic[10];
		uint8_t vendor_id[50];
		uint8_t version[50];
		uint8_t ser_no[50];
	} dsl_data = { 
		.magic = "DSL_DATA00",
		.vendor_id =  { /* default value */
			0x26, 0x00, 0x54, 0x43,
			0x54, 0x4e, 0x00, 0x00	       
		},
	};
	char *t; 	

	t = env_get("dsl_system_vendor_id");
	if ( t != NULL && strlen(t) < 50) 
		memcpy(dsl_data.vendor_id, t , strlen(t));

	t = env_get("dsl_version"); 
	if ( t != NULL && strlen(t) < 50)  
		memcpy(dsl_data.version, t, strlen(t)); 

	t = env_get("dsl_ser_no"); 
	if ( t != NULL && strlen(t) < 50)  
		memcpy(dsl_data.ser_no , t, strlen(t));


	printf("dsl values\nvendor_id: %s\nversion: %s\nser_no: %s\n",
		dsl_data.vendor_id, 
		dsl_data.version, 
		dsl_data.ser_no
	);
	extern uint8_t *tclinux_7517_ptr;
	extern uint32_t tclinux_7517_len;

	memcpy(inf_buffer , tclinux_7517_ptr, tclinux_7517_len  );
	memcpy(inf_buffer + tclinux_7517_len ,  &dsl_data , sizeof(struct DSL_data));
	tclinux_7517_len += sizeof(struct DSL_data);
	tclinux_7517_ptr = inf_buffer; 


	while(check_tclinux_sending_state())
	{
		ethrx = eth_rx();
		if(ethrx < 0)
		{
			printf("[hsm] eth_rx %d\n", ethrx);
			break;
		}
	}
}
