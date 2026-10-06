// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
*/
#ifndef __ECNT_BOOTARGS_H__
#define __ECNT_BOOTARGS_H__

#include <asm/types.h>





extern char get_bootflag(void);
extern int get_partition_info(unsigned int index, uint64_t *val);
extern char get_qdmainit(void);



#endif