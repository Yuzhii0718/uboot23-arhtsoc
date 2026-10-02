// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
 * 
 *  Shubham Jain, Shubham.jain@airoha.common.
 *  Zhengping Zhang , zhengping.zhang@airoha.com
*/ 
#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <init.h>
#include <wdt.h>
#include <dm/uclass-internal.h>
int arch_cpu_init(void)
{
	icache_enable();
	return 0;
}
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
