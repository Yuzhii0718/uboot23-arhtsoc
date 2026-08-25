/*(C) Copyright 2023 Airoha Technology Corp.
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 
#include <clk.h>
#include <dm.h>
#include <spi.h>
#include <spi-mem.h>
#include <watchdog.h>
#include <linux/mtd/nand.h> 
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#define LOG_CATEGORY UCLASS_SPI
#include <common.h>
DECLARE_GLOBAL_DATA_PTR;
#define K1_TO_PHY(x)		(((uint32)x) & 0x1fffffff)
#define SPI_DEFAULT_SPEED_HZ 100000
#define _SPI_CONTROLLER_REGS_BASE                   0x1FA10000
#define _SPI_CONTROLLER_REGS_READ_MODE              0x0000
#define _SPI_CONTROLLER_REGS_READ_IDLE_EN           0x0004
#define _SPI_CONTROLLER_REGS_SIDLY                  0x0008
#define _SPI_CONTROLLER_REGS_CSHEXT                 0x000C
#define _SPI_CONTROLLER_REGS_CSLEXT                 0x0010
#define _SPI_CONTROLLER_REGS_MTX_MODE_TOG           0x0014
#define _SPI_CONTROLLER_REGS_RDCTL_FSM              0x0018
#define _SPI_CONTROLLER_REGS_MACMUX_SEL             0x001C
#define _SPI_CONTROLLER_REGS_MANUAL_EN              0x0020
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_EMPTY    0x0024
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WDATA    0x0028
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_FULL     0x002C
#define _SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WR       0x0030
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL      0x0034
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_WDATA     0x0038
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_EMPTY     0x003C
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RD        0x0040
#define _SPI_CONTROLLER_REGS_MANUAL_DFIFO_RDATA     0x0044
#define _SPI_CONTROLLER_REGS_DUMMY                  0x0080
#define _SPI_CONTROLLER_REGS_PROBE_SEL              0x0088
#define _SPI_CONTROLLER_REGS_INTERRUPT              0x0090
#define _SPI_CONTROLLER_REGS_INTERRUPT_EN           0x0094
#define _SPI_CONTROLLER_REGS_SI_CK_SEL              0x009C
#define _SPI_CONTROLLER_REGS_SW_CFGNANDADDR_VAL     0x010C
#define _SPI_CONTROLLER_REGS_SW_CFGNANDADDR_EN      0x0110
#define _SPI_CONTROLLER_REGS_SFC_STRAP              0x0114
#define _SPI_CONTROLLER_REGS_NFI2SPI_EN             0x0130
#define _SPI_NFI_REGS_BASE                          0x1FA11000
#define _SPI_NFI_REGS_CNFG                          0x0000
#define _SPI_NFI_REGS_PAGEFMT                       0x0004
#define _SPI_NFI_REGS_CON                           0x0008
#define _SPI_NFI_REGS_INTR_EN                       0x0010
#define _SPI_NFI_REGS_INTR                          0x0014
#define _SPI_NFI_REGS_CMD                           0x0020
#define _SPI_NFI_REGS_STA                           0x0060
#define _SPI_NFI_REGS_FIFOSTA                       0x0064
#define _SPI_NFI_REGS_STRADDR                       0x0080
#define _SPI_NFI_REGS_FDM0L                         0x00A0
#define _SPI_NFI_REGS_FDM0M                         0x00A4
#define _SPI_NFI_REGS_FDM7L                         0x00D8
#define _SPI_NFI_REGS_FDM7M                         0x00DC
#define _SPI_NFI_REGS_FIFODATA0                     0x0190
#define _SPI_NFI_REGS_FIFODATA1                     0x0194
#define _SPI_NFI_REGS_FIFODATA2                     0x0198
#define _SPI_NFI_REGS_FIFODATA3                     0x019C
#define _SPI_NFI_REGS_MASTERSTA                     0x0224
#define _SPI_NFI_REGS_SECCUS_SIZE                   0x022C
#define _SPI_NFI_REGS_RD_CTL2                       0x0510
#define _SPI_NFI_REGS_RD_CTL3                       0x0514
#define _SPI_NFI_REGS_PG_CTL1                       0x0524
#define _SPI_NFI_REGS_PG_CTL2                       0x0528
#define _SPI_NFI_REGS_NOR_PROG_ADDR                 0x052C
#define _SPI_NFI_REGS_NOR_RD_ADDR                   0x0534
#define _SPI_NFI_REGS_SNF_MISC_CTL                  0x0538
#define _SPI_NFI_REGS_SNF_MISC_CTL2                 0x053C
#define _SPI_NFI_REGS_SNF_STA_CTL1                  0x0550
#define _SPI_NFI_REGS_SNF_STA_CTL2                  0x0554
#define _SPI_NFI_REGS_SNF_NFI_CNFG                  0x055C
/* SPI NAND Protocol OP */
#define _SPI_NAND_OP_GET_FEATURE                    0x0F    /* Get Feature */
#define _SPI_NAND_OP_SET_FEATURE                    0x1F    /* Set Feature */
#define _SPI_NAND_OP_PAGE_READ                      0x13    /* Load page data into cache of SPI NAND chip */
#define _SPI_NAND_OP_READ_FROM_CACHE_SINGLE         0x03    /* Read data from cache of SPI NAND chip, single speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_SINGLE_FAST    0x0B    /* Read data from cache of SPI NAND chip, single speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_DUAL           0x3B    /* Read data from cache of SPI NAND chip, dual speed*/
#define _SPI_NAND_OP_READ_FROM_CACHE_QUAD           0x6B    /* Read data from cache of SPI NAND chip, quad speed*/
#define _SPI_NAND_OP_WRITE_ENABLE                   0x06    /* Enable write data to  SPI NAND chip */
#define _SPI_NAND_OP_WRITE_DISABLE                  0x04    /* Reseting the Write Enable Latch (WEL) */
#define _SPI_NAND_OP_PROGRAM_LOAD_SINGLE            0x02    /* Write data into cache of SPI NAND chip with cache reset, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_QUAD              0x32    /* Write data into cache of SPI NAND chip with cache reset, quad speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDOM_SINGLE     0x84    /* Write data into cache of SPI NAND chip, single speed */
#define _SPI_NAND_OP_PROGRAM_LOAD_RAMDON_QUAD       0x34    /* Write data into cache of SPI NAND chip, quad speed */
#define _SPI_NAND_OP_PROGRAM_EXECUTE                0x10    /* Write data from cache into SPI NAND chip */
#define _SPI_NAND_OP_READ_ID                        0x9F    /* Read Manufacture ID and Device ID */
#define _SPI_NAND_OP_BLOCK_ERASE                    0xD8    /* Erase Block */
#define _SPI_NAND_OP_RESET                          0xFF    /* Reset */
#define _SPI_NAND_OP_DIE_SELECT                     0xC2    /* Die Select */

#define VPint   *(volatile unsigned int *)
#define	writeReg(data, reg)	(VPint(reg) = data)
#define	readReg(reg)		(VPint(reg))
#define WRITE_SPI_REG(a,b) writeReg(b, priv->spi_base + a)
#define READ_SPI_REG(a) readReg(priv->spi_base + a)
#define WRITE_NFI_REG(a,b) writeReg(b, priv->nfi_base + a)
#define READ_NFI_REG(a) readReg(priv->nfi_base + a)

#define WRITE_NFI_REG_WITH_MASK(a,b,c) WRITE_NFI_REG(a, (READ_NFI_REG(a) & ~(b)) | (c))
#define _SPI_NAND_SEMAPHORE_LOCK()                  spin_lock_irqsave(&spinandLock, spinand_spinlock_flags) /* Disable interrupt */
#define _SPI_NAND_SEMAPHORE_UNLOCK()                spin_unlock_irqrestore(&spinandLock, spinand_spinlock_flags)    /* Enable interrupt  */

#define _SPI_NAND_PAGE_SIZE                         (2048)
#define _SPI_NAND_OOB_SIZE                          (64)
#define _SPI_NAND_CACHE_SIZE                        (_SPI_NAND_PAGE_SIZE+_SPI_NAND_OOB_SIZE)


struct nand_device *airoha_nand_ref; 
bool nand_setup = false; 

typedef enum{
	SPI_CONTROLLER_MODE_AUTO=0,
	SPI_CONTROLLER_MODE_MANUAL,
	SPI_CONTROLLER_MODE_DMA,
	SPI_CONTROLLER_MODE_NO
} SPI_CONTROLLER_MODE_T;

typedef enum{
	SPI_CONTROLLER_CHIP_SELECT_HIGH=0,
	SPI_CONTROLLER_CHIP_SELECT_LOW,
} SPI_CONTROLLER_CHIP_SELECT_T;


struct airoha_nfi_conf {
	size_t page_size;
	size_t oob_size;
	size_t sec_size;
	unsigned char sec_num;
    unsigned char spare_size;
    unsigned char dummy_byte_num;
	unsigned long current_page_num;
};


struct arht_spim_priv {
	void __iomem *spi_base;
	void __iomem *nfi_base;
	struct udevice *dev;
	unsigned char *rx_buf;
    unsigned char *tx_buf;
	struct airoha_nfi_conf nfi_cfg;
	u8 *buf;
	dma_addr_t rx_dma_addr;
    dma_addr_t tx_dma_addr;
	size_t buf_len;
	bool autofmt;
};

static int airoha_spi_set_opfifo(struct arht_spim_priv *priv, u8 op_cmd, u32 op_len)
{
    WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WDATA, ((((op_cmd) & 0x1f) << 0x9) | ((op_len) & 0x1ff)));
    while(READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_OPFIFO_FULL));
    WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_OPFIFO_WR, 0x1);
    while(!READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_OPFIFO_EMPTY));
    return 0;
}

static void airoha_spi_set_cs(struct arht_spim_priv *priv, SPI_CONTROLLER_CHIP_SELECT_T cs)
{
    airoha_spi_set_opfifo(priv, cs, 1);
}

static int airoha_spi_write_data_fifo(struct arht_spim_priv *priv, u8 *ptr_data, u32 data_len)
{
    u32 idx;
    for(idx = 0; idx < data_len; idx++)
    {
        /* 1. Wait until dfifo is not full */
        while(READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL));
        /* 2. Write data to register DFIFO_WDATA */
        WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_WDATA, ((*(ptr_data+idx)) & 0xff));
        /* 3. Wait until dfifo is not full */
        while(READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_FULL));
    }
    return 0;
}

static int airoha_spi_read_data_fifo(struct arht_spim_priv *priv, unsigned char *ptr_rtn_data, unsigned int data_len)
{	
    unsigned int idx;
    for(idx = 0; idx < data_len; idx++)
    {
        /* 1. wait until dfifo is not empty */
        while(READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_EMPTY));
        /* 2. read from dfifo to register DFIFO_RDATA */
        *(ptr_rtn_data+idx) = (READ_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_RDATA)) & 0xff;
        /* 3. enable register DFIFO_RD to read next byte */
        WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_DFIFO_RD, 0x1);
    }
    return 0;
}

static int airoha_spi_write_one_byte_with_cmd(struct arht_spim_priv *priv, unsigned char cmd, unsigned char *ptr_data)
{
    airoha_spi_set_opfifo(priv, cmd, 1);
    airoha_spi_write_data_fifo(priv, ptr_data, 1);
    return 0;
}

static int airoha_spi_write_nbytes_with_cmd(struct arht_spim_priv *priv, unsigned char cmd, unsigned char *ptr_data, unsigned int len)
{
    unsigned int data_len, remain_len, finish_len;
    remain_len = len;
    finish_len = 0;
    
    while (remain_len > 0)
    {
        data_len = (remain_len > 0x1ff) ? 0x1ff : remain_len;
        airoha_spi_set_opfifo(priv, cmd, data_len);
        airoha_spi_write_data_fifo(priv, ptr_data + finish_len, data_len);
        remain_len -= data_len;
        finish_len += data_len;
    }
    return 0;
}

static int airoha_spi_read_nbytes(struct arht_spim_priv *priv, unsigned char *ptr_rtn_data, unsigned int len)
{
    unsigned int data_len, remain_len, finish_len;
    remain_len = len;
    finish_len = 0;
    
    while (remain_len > 0)
    {
        data_len = (remain_len > 0x1ff) ? 0x1ff : remain_len;
        airoha_spi_set_opfifo(priv, 0xc, data_len);
        airoha_spi_read_data_fifo(priv, ptr_rtn_data + finish_len, data_len);
        remain_len -= data_len;
        finish_len += data_len;
    }

    return 0;
}

static int airoha_snand_nfi_init(struct arht_spim_priv *priv)
{
    /* switch to SNFI mode */
    WRITE_NFI_REG(_SPI_NFI_REGS_SNF_NFI_CNFG, 0x1);
    /* Enable DMA */
    WRITE_NFI_REG(_SPI_NFI_REGS_INTR_EN, READ_NFI_REG(_SPI_NFI_REGS_INTR_EN) | 0x0040);
    return 0;
}

static int airoha_snand_nfi_reset(struct arht_spim_priv *priv)
{
    WRITE_NFI_REG(_SPI_NFI_REGS_CON, 0x3);
    return 0;
}

static int airoha_snand_nfi_config(struct arht_spim_priv *priv)
{
    /* Disable AutoFDM */
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, READ_NFI_REG(_SPI_NFI_REGS_CNFG) & ~(0x0200));
    /* HW ECC Disable */
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, READ_NFI_REG(_SPI_NFI_REGS_CNFG) & ~(0x0100));
    /* Enable DMA Burst */
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, READ_NFI_REG(_SPI_NFI_REGS_CNFG) | (0x0004));
    /* page format */
    WRITE_NFI_REG(_SPI_NFI_REGS_PAGEFMT, 0x1);
    /* sec num */
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_CON, 0xF000, priv->nfi_cfg.sec_num << 12);
    /* enable cust sec size */
    WRITE_NFI_REG(_SPI_NFI_REGS_SECCUS_SIZE, READ_NFI_REG(_SPI_NFI_REGS_SECCUS_SIZE) | (0x00010000));
    /* set cust sec size */
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_SECCUS_SIZE, 0x00001FFF, ((priv->nfi_cfg.page_size + priv->nfi_cfg.oob_size) / priv->nfi_cfg.sec_num) << 0);
    return 0;
}

static bool airoha_snand_is_page_ops(const struct spi_mem_op *op)
{	
    if (op->addr.nbytes != 2)
        return false;

    if (op->addr.buswidth != 1 && op->addr.buswidth != 2 &&
        op->addr.buswidth != 4)
        return false;

    if (op->data.dir == SPI_MEM_DATA_IN) {

        if (op->dummy.nbytes * BITS_PER_BYTE / op->dummy.buswidth >
            0xf)
            return false;

        if ((op->addr.buswidth == 4 || op->addr.buswidth == 1) &&
            op->data.buswidth == 4)
            return true;

        if ((op->addr.buswidth == 2 || op->addr.buswidth == 1) &&
            op->data.buswidth == 2)
            return true;
			
        if (op->addr.buswidth == 1 && op->data.buswidth == 1)
            return true;
    } else if (op->data.dir == SPI_MEM_DATA_OUT) {

        if (op->dummy.nbytes)
            return false;

        if (op->addr.buswidth == 1 && op->data.buswidth == 4)
            return true;
 
        if (op->addr.buswidth == 1 && op->data.buswidth == 1)
            return true;
    }
    return false;
}

static int airoha_snand_adjust_op_size(struct spi_slave *slave, struct spi_mem_op *op)
{
    struct udevice *bus = slave->dev->parent;
	struct arht_spim_priv *priv = dev_get_priv(bus);
	if (!nand_setup) 
		return 0; 
    if(airoha_snand_is_page_ops(op))
    {
        size_t l;
        if(priv->autofmt)
            return 0;
        l = priv->nfi_cfg.sec_size + priv->nfi_cfg.spare_size;
        l *= priv->nfi_cfg.sec_num;
        if(op->data.nbytes > l)
            op->data.nbytes = l;
    }
    else
    {
        size_t hl = 1 + op->addr.nbytes + op->dummy.nbytes;
        if(hl >= 0xa0)
            return -EOPNOTSUPP;
        if(op->data.nbytes > 0xa0 - hl)
            op->data.nbytes = 0xa0 - hl;
    }
    return 0; 
}

static bool airoha_snand_supports_op(struct spi_slave *slave, const struct spi_mem_op *op)
{	
    if (op->cmd.buswidth != 1)
	{
        return false;
	}
    if (airoha_snand_is_page_ops(op))
	{
        return true;
	}
    return ((op->addr.nbytes == 0 || op->addr.buswidth == 1) &&
        (op->dummy.nbytes == 0 || op->dummy.buswidth == 1) &&
        (op->data.nbytes == 0 || op->data.buswidth == 1));
}

static int airoha_snand_dirmap_create(struct spi_slave *slave)
{
    struct udevice *bus = slave->dev->parent;
	struct arht_spim_priv *priv = dev_get_priv(bus);

    if(!priv->buf)
        return -EINVAL;
   
    return 0;
}

static int airoha_spim_set_mode(struct arht_spim_priv *priv, SPI_CONTROLLER_MODE_T mode)
{
	switch(mode)
    {
        case SPI_CONTROLLER_MODE_AUTO:
        {
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_READ_IDLE_EN, 0x0);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MTX_MODE_TOG, 0x0);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_EN, 0x0);
            
            break;
        }
        case SPI_CONTROLLER_MODE_MANUAL:
        {
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_NFI2SPI_EN, 0x0);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_READ_IDLE_EN, 0x0);
            while(READ_SPI_REG(_SPI_CONTROLLER_REGS_RDCTL_FSM));
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MTX_MODE_TOG, 0x9);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_EN, 0x1);
            break;
        }
        case SPI_CONTROLLER_MODE_DMA:
        {
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_NFI2SPI_EN, 0x1);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MTX_MODE_TOG, 0x0);
            WRITE_SPI_REG(_SPI_CONTROLLER_REGS_MANUAL_EN, 0x0);
            break;
        }
        default:
        {
            break;
        }
    }
	
    WRITE_SPI_REG(_SPI_CONTROLLER_REGS_DUMMY, priv->nfi_cfg.dummy_byte_num);
    return 0;
}
static int _airoha_spim_set_mode(struct udevice *dev, SPI_CONTROLLER_MODE_T mode)
{	
	struct arht_spim_priv *priv = dev_get_priv(dev); 
	airoha_spim_set_mode(priv, mode);
	return 0;
}

static ssize_t airoha_snand_dirmap_read(struct spi_slave *slave, struct spi_mem_op *op)
{	
    struct udevice *bus = slave->dev->parent;
	struct arht_spim_priv *priv = dev_get_priv(bus);
    unsigned int rd_mode, val;
    int ret;
    char *buf = op->data.buf.in;
    size_t len = op->data.nbytes;
    u64 offs = 0;

    if(op->cmd.opcode == _SPI_NAND_OP_READ_FROM_CACHE_SINGLE || op->cmd.opcode == _SPI_NAND_OP_READ_FROM_CACHE_SINGLE_FAST)
    {
        rd_mode = 0x0;
    }
    else if(op->cmd.opcode == _SPI_NAND_OP_READ_FROM_CACHE_DUAL)
    {
        rd_mode = 0x1;
    }
    else if(op->cmd.opcode == _SPI_NAND_OP_READ_FROM_CACHE_QUAD)
    {
        rd_mode = 0x2;
    }
    else
    {
        rd_mode = 0x0;
    }

    /* force the dual mode */
    rd_mode = 0x1; 
    op->cmd.opcode =  0x3b;

    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_DMA);
    airoha_snand_nfi_reset(priv);
    airoha_snand_nfi_config(priv);
    invalidate_dcache_range(priv->rx_buf, priv->rx_buf + priv->buf_len);

    mb();
    // set dma addr
    WRITE_NFI_REG(_SPI_NFI_REGS_STRADDR, priv->rx_dma_addr);
    // set cust sec size
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_SNF_MISC_CTL2, 0x1fff, (priv->nfi_cfg.sec_size * priv->nfi_cfg.sec_num) << 0);
    // set read command
    WRITE_NFI_REG(_SPI_NFI_REGS_RD_CTL2, op->cmd.opcode);
    // set read mode
    WRITE_NFI_REG(_SPI_NFI_REGS_SNF_MISC_CTL, rd_mode << 16);
    // set read addr
    WRITE_NFI_REG(_SPI_NFI_REGS_RD_CTL3, 0x0);
    // set nfi read
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_CNFG, 0x7000, (6 << 12));
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, READ_NFI_REG(_SPI_NFI_REGS_CNFG) | 0x0002);
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, READ_NFI_REG(_SPI_NFI_REGS_CNFG) | 0x0001);
    WRITE_NFI_REG(_SPI_NFI_REGS_CMD, 0x00);
    // trigger dma start read
    WRITE_NFI_REG(_SPI_NFI_REGS_CON, READ_NFI_REG(_SPI_NFI_REGS_CON) & ~(0x0100));
    WRITE_NFI_REG(_SPI_NFI_REGS_CON, READ_NFI_REG(_SPI_NFI_REGS_CON) | (0x0100));

    ret = readl_poll_timeout(priv->nfi_base + _SPI_NFI_REGS_SNF_STA_CTL1, val, (val & 0x02000000) != 0, 1000000);
    if(ret)
    {
        printf("[Error] Read DMA : Check READ FROM CACHE Done Timeout ! \n");
        return -1;
    }
    WRITE_NFI_REG(_SPI_NFI_REGS_SNF_STA_CTL1, READ_NFI_REG(_SPI_NFI_REGS_SNF_STA_CTL1) | 0x02000000);
    ret = readl_poll_timeout(priv->nfi_base + _SPI_NFI_REGS_INTR, val, (val & 0x0040) != 0, 1000000);
    if(ret)
    {
        printf("[Error] Read DMA : Check AHB Done Timeout ! \n");
        return -1;
    }
    udelay(1);
    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_MANUAL);
    memcpy(buf, priv->rx_buf + offs, len);
    
    return 0;
}

static ssize_t airoha_snand_dirmap_write(struct spi_slave *slave, struct spi_mem_op *op)
{
	struct udevice *bus = slave->dev->parent;
	struct arht_spim_priv *priv = dev_get_priv(bus);
    unsigned int wr_mode;
    int ret;
    unsigned int val;
    
    char *buf = op->data.buf.out;
    size_t len = op->data.nbytes;

    if(op->cmd.opcode == 0x32 || op->cmd.opcode == 0x34)
    {
        wr_mode = 0x2;
    }
    else
    {
        wr_mode = 0x0;
    }

    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_MANUAL);
    memcpy(priv->tx_buf, buf, len);
    flush_dcache_range(priv->buf_len, priv->buf_len + priv->buf_len);
    mb();
    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_DMA);
    airoha_snand_nfi_reset(priv);
    airoha_snand_nfi_config(priv);
    WRITE_NFI_REG(_SPI_NFI_REGS_STRADDR, priv->tx_dma_addr);
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_SNF_MISC_CTL2, 0x1fff0000, ((priv->nfi_cfg.sec_size * priv->nfi_cfg.sec_num) << 16));
    WRITE_NFI_REG(_SPI_NFI_REGS_PG_CTL1, (op->cmd.opcode << 8));
    WRITE_NFI_REG(_SPI_NFI_REGS_SNF_MISC_CTL, (wr_mode << 16));
    WRITE_NFI_REG(_SPI_NFI_REGS_PG_CTL2, 0x0);
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, (READ_NFI_REG(_SPI_NFI_REGS_CNFG) & ~(0x0002)));
    WRITE_NFI_REG_WITH_MASK(_SPI_NFI_REGS_CNFG, 0x7000, (3 << 12));
    WRITE_NFI_REG(_SPI_NFI_REGS_CNFG, (READ_NFI_REG(_SPI_NFI_REGS_CNFG) | (0x0001)));
    WRITE_NFI_REG(_SPI_NFI_REGS_CMD, 0x80);
    WRITE_NFI_REG(_SPI_NFI_REGS_CON, (READ_NFI_REG(_SPI_NFI_REGS_CON) & ~(0x0200)));
    WRITE_NFI_REG(_SPI_NFI_REGS_CON, (READ_NFI_REG(_SPI_NFI_REGS_CON) | (0x0200)));
    udelay(1);
    ret = readl_poll_timeout(priv->nfi_base + _SPI_NFI_REGS_INTR, val, (val & 0x0040) != 0, 1000000);
    if(ret)
    {
        printk("[Error] Write DMA : Check LOAD TO CACHE Done Timeout ! \n");
        return -1;
    }
    
    ret = readl_poll_timeout(priv->nfi_base + _SPI_NFI_REGS_SNF_STA_CTL1, val, (val & 0x04000000) != 0, 1000000);
    if(ret)
    {
        printk("[Error] Read DMA : Check AHB Done Timeout ! \n");
        return -1;
    }
    WRITE_NFI_REG(_SPI_NFI_REGS_SNF_STA_CTL1, READ_NFI_REG(_SPI_NFI_REGS_SNF_STA_CTL1) | 0x04000000);
    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_MANUAL);

    return 0;
}

static int airoha_snand_op_transfer(struct arht_spim_priv *priv, const struct spi_mem_op *op)
{
    u32 idx;

    /* Switch To Manual Mode */
    airoha_spim_set_mode(priv, SPI_CONTROLLER_MODE_MANUAL);

    airoha_spi_set_cs(priv, SPI_CONTROLLER_CHIP_SELECT_LOW);

    // opcode part
    airoha_spi_write_one_byte_with_cmd(priv, 0x8, &op->cmd.opcode);

    // addr part
    for(idx = 0; idx < op->addr.nbytes; idx++)
    {
        unsigned char addr_data = (op->addr.val >> ((op->addr.nbytes - idx - 1) * 8)) & 0xff;
        if(op->cmd.opcode == _SPI_NAND_OP_GET_FEATURE)
        {
            airoha_spi_write_one_byte_with_cmd(priv, 0x11, &addr_data);
        }
        else
        {
            airoha_spi_write_one_byte_with_cmd(priv, 0x8, &addr_data);
        }
    }

    // dummy part
    for(idx = 0; idx < op->dummy.nbytes; idx++)
    {
        unsigned char data = 0xff;
        airoha_spi_write_one_byte_with_cmd(priv, 0x8, &data);
    }

    // data part
    if(op->data.dir == SPI_MEM_DATA_IN)
    {
        airoha_spi_read_nbytes(priv, op->data.buf.in, op->data.nbytes);
    }
    else
    {
        airoha_spi_write_nbytes_with_cmd(priv, 0x8, op->data.buf.out, op->data.nbytes);
    }

    airoha_spi_set_cs(priv, SPI_CONTROLLER_CHIP_SELECT_HIGH);
	
    return 0;
}

static int airoha_snand_exec_op(struct spi_slave *slave, const struct spi_mem_op *op)
{
	struct udevice *bus = slave->dev->parent;
	struct arht_spim_priv *as = dev_get_priv(bus);
   
	if (!nand_setup && airoha_nand_ref != NULL ) { 
		
		int pagesize = airoha_nand_ref->memorg.pagesize; 
		int oobsize = airoha_nand_ref->memorg.oobsize; 
		
		int sec_num , sec_size ; 
		if(pagesize == 2 * 1024 ) {
			sec_num = 4; 
		} else if ( pagesize == 4 * 1024 ) { 
			sec_num = 8;
		} else { 
			sec_num = 1; 
		}
		
		sec_size = ( pagesize + oobsize ) / sec_num ;
		as->nfi_cfg.sec_num = sec_num;
		as->nfi_cfg.sec_size = sec_size;
		as->nfi_cfg.page_size = (((sec_size * sec_num) / 1024) * 1024);
		as->nfi_cfg.oob_size = ((sec_size * sec_num) % 1024);
		as->nfi_cfg.spare_size = 16; 
		as->nfi_cfg.dummy_byte_num = 0;
		as->nfi_cfg.current_page_num  = 0;
		
	    airoha_snand_nfi_init(as);
	    airoha_snand_nfi_config(as);
		
		nand_setup = true; 
	}
	
    if (airoha_snand_is_page_ops(op)) {
        /* exec DMA page read/write */
        if (op->data.dir == SPI_MEM_DATA_IN) {
            /* exec DMA page read */
            return airoha_snand_dirmap_read(slave, op);
        } else {
            /* exec DMA page write */
            return airoha_snand_op_transfer(as, op);
        }
    } else {
        return airoha_snand_op_transfer(as, op);
    }

}

static int airoha_spim_set_speed(struct udevice *dev, uint speed)
{ 
    // we already have the maximum setting from ATF
    // so we should not change here.

	// u32 dividend = 500;
	// u32 clock_factor = ( dividend / ( 50 * 2)) ;
	// u32 val = readl(0x1fa201c4);
	// val &= 0xffff0000;
	// writel(val , 0x1fa201c4);
	// val |= (((clock_factor) << 8) | 1 );
	// writel(val , 0x1fa201c4);
	return 0;
}


static int airoha_snand_setup(struct arht_spim_priv *as)
{
	int ret ;

    /* prepare buffer */
    as->buf_len = _SPI_NAND_CACHE_SIZE;
    as->rx_buf = (unsigned char *)kmalloc(as->buf_len, GFP_KERNEL);
    if(!as->rx_buf)
    {
        printk("[airoha_snand_setup] rx_buf allocate failed\n");
        return -ENOMEM;
    }
    as->rx_dma_addr = dma_map_single((void *)as->rx_buf, as->buf_len, DMA_FROM_DEVICE);
    ret = dma_mapping_error(as->dev, as->rx_dma_addr);
    if(ret)
    {
        printk("[airoha_snand_setup] rx_dma_addr mapping error\n");
        return -EINVAL;
    }
    as->tx_buf = (unsigned char *)kmalloc(as->buf_len, GFP_KERNEL);
    if(!as->tx_buf)
    {
        printk("[airoha_snand_setup] tx_buf allocate failed\n");
        return -ENOMEM;
    }
    as->tx_dma_addr = dma_map_single((void *)as->tx_buf, as->buf_len, DMA_TO_DEVICE);
    ret = dma_mapping_error(as->dev, as->tx_dma_addr);
    if(ret)
    {
        printk("[airoha_snand_setup] tx_dma_addr mapping error\n");
        return -EINVAL;
    }

    return 0;
}

static const struct spi_controller_mem_ops airoha_spim_mem_ops;

static int airoha_spim_probe (struct udevice *dev) 
{	
	struct arht_spim_priv *as = dev_get_priv(dev); 
	as->spi_base = (void __iomem *) devfdt_get_addr_index(dev , 0 ); 
	if (!as->spi_base) {
		return -EINVAL;
    } else {
    	printf(" spi_base is set : %p\n", as->spi_base);
    }
    as->nfi_base = (void __iomem *) devfdt_get_addr_index(dev, 1); 
	if (!as->nfi_base) {
		return -EINVAL;
    } else {
	    printf(" nfi_base is set : %p\n" , as->nfi_base); 
    }

    airoha_snand_setup(as);
	
	return 0 ; 
}

static const struct spi_controller_mem_ops airoha_spim_mem_ops = {
	.adjust_op_size = airoha_snand_adjust_op_size,
	.supports_op = airoha_snand_supports_op,
	.exec_op = airoha_snand_exec_op,
	.dirmap_create = airoha_snand_dirmap_create,
	.dirmap_read = airoha_snand_dirmap_read,
	.dirmap_write = airoha_snand_dirmap_write,
	
};

static const struct dm_spi_ops airoha_spim_ops = {
	.set_speed = airoha_spim_set_speed,
	.set_mode = _airoha_spim_set_mode,
	.mem_ops = &airoha_spim_mem_ops,
};

static const struct udevice_id airoha_spim_ids[] = {
	{ .compatible = "airoha,en7523-spi" },
	{ .compatible = "airoha,an7552-spi" },
	{ .compatible = "airoha,an7581-spi" },
	{ .compatible = "airoha,an7583-spi" },
};


U_BOOT_DRIVER(airoha_spim) = {
	.name = "airoha_spim",
	.id = UCLASS_SPI,
	.of_match = airoha_spim_ids,
	.ops = &airoha_spim_ops,
	.priv_auto = sizeof(struct arht_spim_priv),
	.probe = airoha_spim_probe,
};  
