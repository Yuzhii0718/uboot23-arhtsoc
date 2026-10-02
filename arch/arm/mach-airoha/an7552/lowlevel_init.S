/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 AIROHA Inc
 */
#include <linux/linkage.h>
#include <asm/proc-armv/ptrace.h>
#define WAIT_CODE_SRAM_BASE	0x0010ff00
#define SLAVE_JUMP_REG		0x10202034
#define SLAVE1_MAGIC_REG	0x10202038
#define SLAVE1_MAGIC_NUM	0x534c4131
#define GIC_CPU_BASE		0x10320000
ENTRY(lowlevel_init)
	/* Return to U-Boot via saved link register */
	mov	pc, lr
ENDPROC(lowlevel_init)
/* This function will be copied into SRAM */
ENTRY(slave_cpu_wait)
	wfi
	ldr	r2, [r0]
	cmp	r2, r1
	bne	slave_cpu_wait
	movw	r0, #:lower16:SLAVE_JUMP_REG
	movt	r0, #:upper16:SLAVE_JUMP_REG
	ldr	r1, [r0]
	mov	pc, r1
ENDPROC(slave_cpu_wait)