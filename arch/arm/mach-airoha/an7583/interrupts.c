/*
 * (C) Copyright 2000-2009
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/interrupts.h>
#include <asm/arch/arm-gic-v3.h>
#include <asm/arch/arm-gic.h>

/* Initialize to non-zero so that we avoid clearing of bss */
void (*interrupt_vectors[MAX_INT_NUM])(void) = {INVERSE_NULL} ;
extern void gic_dic_set_enable (unsigned int intrID);
extern void gic_dic_set_config (unsigned int intrID,
	unsigned char level0edge1);

extern void gic_dist_init(void);
extern void gic_cpu_init(void);
extern void gic_dic_clear_active (unsigned int intr);

struct gic_chip_data {
	unsigned long		*dist_base;
	unsigned long		redist_regions;
	struct rdists		rdists;
	unsigned long long		redist_stride;
	unsigned long			nr_redist_regions;
	unsigned int		irq_nr;
};
struct gic_chip_data gic_data;

static void null_func(void)
{
	printf("Dummy ISR function \n");
	
}

static void init_handler_irq(void)
{
	int i =0 ;
	for (i = 0 ; i < MAX_INT_NUM;i++)
	{
		interrupt_vectors[i] = null_func;
	}

}
int arch_interrupt_init (void)
{
	void *dist_base;
	u64 redist_stride;
	u32 nr_redist_regions;
	u32 typer;
	u32 reg;
	int gic_irqs;
	int ret = 0;
	
	dist_base = (void*)GICD_BASE;
	reg = readl(dist_base + GICD_PIDR2) & GIC_PIDR2_ARCH_MASK;
	if (reg != GIC_PIDR2_ARCH_GICv3 && reg != GIC_PIDR2_ARCH_GICv4) {
		printf("no distributor detected, giving up\n"	);
		return -1;
	}
	nr_redist_regions = 1;
	redist_stride = 0;

	
	gic_data.dist_base = (long unsigned int *)GICD_BASE;
	gic_data.redist_regions = GICR_CTLR;
	gic_data.nr_redist_regions = nr_redist_regions;
	gic_data.redist_stride = redist_stride;
	

	/*
	 * Find out how many interrupts are supported.
	 * The GIC only supports up to 1020 interrupt sources (SGI+PPI+SPI)
	 */
	typer = readl(GICD_BASE + GICD_TYPER);
	gic_data.rdists.id_bits = GICD_TYPER_ID_BITS(typer);
	gic_irqs = GICD_TYPER_IRQS(typer);
	if (gic_irqs > 1020)
		gic_irqs = 1020;
	gic_data.irq_nr = gic_irqs;

	init_handler_irq();
	gic_dist_init();
	gic_cpu_init();

	
	return ret;
}



int irq_register (unsigned int irq_num, void (*fxn)(void), unsigned char level0edge1)
{
    interrupt_vectors[irq_num] = fxn;
	
	gic_dic_set_config (irq_num, level0edge1);
	/* always goto CPU0 for now */
	gic_dic_set_enable (irq_num);
	return 0;
}


