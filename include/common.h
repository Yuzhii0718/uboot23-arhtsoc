/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common header file for U-Boot
 *
 * This file still includes quite a few headers that should be included
 * individually as needed. Patches to remove things are welcome.
 *
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#ifndef __COMMON_H_
#define __COMMON_H_	1

#ifndef __ASSEMBLY__		/* put C only stuff in this section */
#include <config.h>
#include <errno.h>
#include <time.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <stdarg.h>
#include <stdio.h>
#include <linux/kernel.h>
#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <vsprintf.h>
#endif	/* __ASSEMBLY__ */

#include <blk.h>

/* Pull in stuff for the build system */
#ifdef DO_DEPS_ONLY
# include <env_internal.h>
#endif

char *env_get(const char *name);
int gpt_verify(struct blk_desc *blk_dev_desc, const char *str_part);
int gpt_default(struct blk_desc *blk_dev_desc, const char *str_part);

#ifdef INCLUDE_UIP_FWUPGRADE

typedef struct _BUFFER_ELEM_ BUFFER_ELEM;

struct _BUFFER_ELEM_
{
	int				tx_idx;
    unsigned char	*pbuf;
    BUFFER_ELEM		*next;
};

typedef struct _VALID_BUFFER_STRUCT_ VALID_BUFFER_STRUCT;

struct _VALID_BUFFER_STRUCT_
{
    BUFFER_ELEM		*head;
    BUFFER_ELEM		*tail;
};
#endif /* INCLUDE_UIP_FWUPGRADE */

#endif	/* __COMMON_H_ */
