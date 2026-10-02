/*
 * (C) Copyright 2000-2009
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */



#include <common.h>
#include <asm/io.h>
#include <asm/arch/arm-gic-v3.h>
#include <asm/arch/arm-gic.h>
#include <asm/arch/interrupts.h>
#include <linux/delay.h>

#define MPIDR_LEVEL_BITS 8
#define MPIDR_LEVEL_MASK ((1 << MPIDR_LEVEL_BITS) - 1)

#define MPIDR_AFFINITY_LEVEL(mpidr, level) \
	((mpidr >> (MPIDR_LEVEL_BITS * level)) & MPIDR_LEVEL_MASK)

#undef SZ_64K
#define SZ_64K 0x10000

#define DEFAULT_PMR_VALUE	0xf0

struct gic_chip_data {
	unsigned long		*dist_base;
	unsigned long		redist_regions;
	struct rdists		rdists;
	unsigned long long		redist_stride;
	unsigned long			nr_redist_regions;
	unsigned int		irq_nr;
};
extern struct gic_chip_data gic_data;
extern void udelay(unsigned long usec);



#define gic_data_rdist()		(gic_data.rdists.__cpu0)
#define gic_data_rdist_rd_base()	(gic_data_rdist().rd_base)
#define gic_data_rdist_sgi_base()	(gic_data_rdist_rd_base() + SZ_64K)

static inline bool gic_enable_sre(void)
{
	u32 val;

	val = gic_read_sre();
	if (val & ICC_SRE_EL1_SRE)
		return true;

	val |= ICC_SRE_EL1_SRE;
	gic_write_sre(val);
	val = gic_read_sre();

	return !!(val & ICC_SRE_EL1_SRE);
}

void gic_dic_clear_enable_all_intr (void)
{
	unsigned int i = 0;

	for (i = 0; i < (MAX_INT_NUM/32); i++)
	{
		/* disable SGI and PPI */
		writel(0xFFFFFFFF, (GICD_BASE+GICD_ICENABLER + (i<<2)));
	}
}

void gic_dic_clear_pending_all_intr (void)
{
	unsigned int i = 0;
	
	for (i = 0; i < (MAX_INT_NUM/32); i++)
	{

	/* disable SGI and PPI pending interrupt */
	writel(0xFFFFFFFF, (GICD_BASE+GICD_ICPENDR + (i<<2)));

	}
}

void gic_dic_set_enable (unsigned int intr)
{
	unsigned int div = intr >> 5;
	unsigned int mod = intr & 0x1f;
	setbits_le32((GICD_BASE+ GICD_ISENABLER+ (div<<2)), (1 << mod));
}

void gic_dic_set_icenable (unsigned int intr)
{
	unsigned int div = intr >> 5;
	unsigned int mod = intr & 0x1f;
	setbits_le32((GICD_BASE+ GICD_ICENABLER+ (div<<2)), (1 << mod));
}


void gic_dic_set_config (unsigned int intr,
	unsigned char level0edge1)
{
	unsigned int div = intr >> 4;
	unsigned int mod = intr & 0xf;
	clrbits_le32((GICD_BASE+GICD_ICFGR + (div<<2)),
		(0x3 << (2 * mod)));
	if (level0edge1 == 1)
		setbits_le32((GICD_BASE+GICD_ICFGR + (div<<2)),
			(0x2 << (2 * mod)));
}

void gic_dic_clear_active (unsigned int intr)
{
	unsigned int div = intr >> 5;
	unsigned int mod = intr & 0x1f;
	setbits_le32((GICD_BASE+GICD_ICACTIVER + (div<<2)),
		(1 << mod));
}

static void gic_enable_redist(bool enable)
{
	void *rbase;
	u32 count = 1000000;	/* 1s! */
	u32 val;

	rbase = gic_data_rdist_rd_base();

	val = readl(rbase + GICR_WAKER);
	if (enable)
		/* Wake up this CPU redistributor */
		val &= ~GICR_WAKER_ProcessorSleep;
	else
		val |= GICR_WAKER_ProcessorSleep;
	writel(val, rbase + GICR_WAKER);

	if (!enable) {		/* Check that GICR_WAKER is writeable */
		val = readl(rbase + GICR_WAKER);
		if (!(val & GICR_WAKER_ProcessorSleep))
			return;	/* No PM support in this redistributor */
	}

	while (--count) {
		val = readl(rbase + GICR_WAKER);
		if (enable ^ (val & GICR_WAKER_ChildrenAsleep))
			break;
		barrier();
		udelay(1);
	};
	if (!count)
		printf("redistributor failed to %s...\n",
				   enable ? "wakeup" : "sleep");
}

/* Wait for completion of a distributor change */
static void gic_do_wait_for_rwp(void *base)
{
	u32 count = 1000000;	/* 1s! */

	while (readl(base + GICD_CTLR) & GICD_CTLR_RWP) {
		count--;
		if (!count) {
			printf("RWP timeout, gone fishing\n");
			return;
		}
		barrier();
		udelay(1);
	};
}

static void gic_dist_wait_for_rwp(void)
{
	gic_do_wait_for_rwp(gic_data.dist_base);
}

static u64 gic_mpidr_to_affinity(unsigned long mpidr)
{
	u64 aff;

	aff = ((u64)MPIDR_AFFINITY_LEVEL(mpidr, 3) << 32 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8  |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	return aff;
}

static void gic_dist_config(void *base, int gic_irqs,void (*sync_access)(void))
{
	unsigned int i;

	/*
	 * Set all global interrupts to be level triggered, active low.
	 */
	for (i = 32; i < gic_irqs; i += 16)
		writel(GICD_INT_ACTLOW_LVLTRIG,
					base + GIC_DIST_CONFIG + i / 4);

	/*
	 * Set priority on all global interrupts.
	 */
	for (i = 32; i < gic_irqs; i += 4)
		writel(GICD_INT_DEF_PRI_X4, base + GIC_DIST_PRI + i);

	/*
	 * Deactivate and disable all SPIs. Leave the PPI and SGIs
	 * alone as they are in the redistributor registers on GICv3.
	 */
	for (i = 32; i < gic_irqs; i += 32) {
		writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ACTIVE_CLEAR + i / 8);
		writel(GICD_INT_EN_CLR_X32,
			       base + GIC_DIST_ENABLE_CLEAR + i / 8);
	}
	if (sync_access)
			sync_access();

}
void gic_dist_init(void)
{
	unsigned int i;
	//unsigned long value = 0 ;
	u64 affinity;
	void *base = gic_data.dist_base;

	/* Disable the distributor */
	writel(0, base + GICD_CTLR);
	gic_dist_wait_for_rwp();

	/*
	 * Configure SPIs as non-secure Group-1. This will only matter
	 * if the GIC only has a single security state. This will not
	 * do the right thing if the kernel is running in secure mode,
	 * but that's not the intended use case anyway.
	 */
	for (i = 32; i < gic_data.irq_nr; i += 32)
		writel(~0, base + GICD_IGROUPR + i / 8);

	gic_dist_config(base, gic_data.irq_nr,gic_dist_wait_for_rwp);

	/* Enable distributor with ARE, Group1 */
	writel(GICD_CTLR_ARE_NS | GICD_CTLR_ENABLE_G1A | GICD_CTLR_ENABLE_G1,
		       base + GICD_CTLR);

	/*
	 * Set all global interrupts to the boot CPU only. ARE must be
	 * enabled.
	 */
	affinity = gic_mpidr_to_affinity(0);
	for (i = 32; i < gic_data.irq_nr; i++)
		gic_write_irouter(affinity, base + GICD_IROUTER + i * 8);
}

static void gic_cpu_config(void *base, void (*sync_access)(void))
{
	int i;

	/*
	 * Deal with the banked PPI and SGI interrupts - disable all
	 * PPI interrupts, ensure all SGI interrupts are enabled.
	 * Make sure everything is deactivated.
	 */
	writel(GICD_INT_EN_CLR_X32, base + GIC_DIST_ACTIVE_CLEAR);
	writel(GICD_INT_EN_CLR_PPI, base + GIC_DIST_ENABLE_CLEAR);
	writel(GICD_INT_EN_SET_SGI, base + GIC_DIST_ENABLE_SET);

	/*
	 * Set priority on PPI and SGI interrupts
	 */
	for (i = 0; i < 32; i += 4)
		writel(GICD_INT_DEF_PRI_X4,
					base + GIC_DIST_PRI + i * 4 / 4);
	if (sync_access)
			 sync_access();

}


static int gic_populate_rdist(void)
{
	unsigned long mpidr = 0;
	u64 typer;
	u32 aff;
	int i;

	/*
	 * Convert affinity to a 32bit value that can be matched to
	 * GICR_TYPER bits [63:32].
	 */
	aff = (MPIDR_AFFINITY_LEVEL(mpidr, 3) << 24 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 2) << 16 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 1) << 8 |
	       MPIDR_AFFINITY_LEVEL(mpidr, 0));

	for (i = 0; i < gic_data.nr_redist_regions; i++) {
		void *ptr = (void*)gic_data.redist_regions;
		u32 reg;

		reg = readl(ptr + GICR_PIDR2) & GIC_PIDR2_ARCH_MASK;
		if (reg != GIC_PIDR2_ARCH_GICv3 &&
		    reg != GIC_PIDR2_ARCH_GICv4) { /* We're in trouble... */
			printf("No redistributor present @%p\n", ptr);
			break;
		}

		do {
			typer = gic_read_typer(ptr + GICR_TYPER);
			if ((typer >> 32) == aff) {
				u64 offset = (uintptr_t)ptr - (uintptr_t)gic_data.redist_regions;
				gic_data_rdist_rd_base() = ptr;
				gic_data_rdist().phys_base = gic_data.redist_regions+ offset;
				printf("CPU0: found redistributor %lx region %d:%pa\n",
					mpidr, i, &gic_data_rdist().phys_base);
				return 0;
			}

			if (gic_data.redist_stride) {
				ptr += gic_data.redist_stride;
			} else {
				ptr += SZ_64K * 2; /* Skip RD_base + SGI_base */
				if (typer & GICR_TYPER_VLPIS)
					ptr += SZ_64K * 2; /* Skip VLPI_base + reserved page */
			}
		} while (!(typer & GICR_TYPER_LAST));
	}

	/* We couldn't even deal with ourselves... */

	return -1;
}
static void gic_cpu_sys_reg_init(void)
{
	/*
	 * Need to check that the SRE bit has actually been set. If
	 * not, it means that SRE is disabled at EL2. We're going to
	 * die painfully, and there is nothing we can do about it.
	 *
	 * Kindly inform the luser.
	 */
	if (!gic_enable_sre())
		printf("GIC: unable to set SRE (disabled at EL2), panic ahead\n");

	/* Set priority mask register */
	gic_write_pmr(DEFAULT_PMR_VALUE);

	if (1) {
		/* EOI drops priority only (mode 1) */
		gic_write_ctlr(ICC_CTLR_EL1_EOImode_drop);
	} else {
		/* EOI deactivates interrupt too (mode 0) */
		gic_write_ctlr(ICC_CTLR_EL1_EOImode_drop_dir);
	}

	/* ... and let's hit the road... */
	gic_write_grpen1(1);
}


void gic_cpu_init(void)
{
	void *rbase;

	/* Register ourselves with the rest of the world */
	if (gic_populate_rdist())
		return;

	gic_enable_redist(true);

	rbase = gic_data_rdist_sgi_base();

	/* Configure SGIs/PPIs as non-secure Group-1 */
	writel(~0, rbase + GICR_IGROUPR0);

	gic_cpu_config(rbase,gic_dist_wait_for_rwp);

	/* initialise system registers */
	gic_cpu_sys_reg_init();
}

