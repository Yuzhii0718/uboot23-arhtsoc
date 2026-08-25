/*
 * (C) Copyright 2000-2009
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ECNT_IMAGE_H
#define _ECNT_IMAGE_H

#include <linux/ctype.h>

struct tclinux_imginfo {
	u32 kernel_off;
	u32 kernel_size;
	u32 rootfs_off;
	u32 rootfs_size;
	u32 tclinux_size;
};

int mtd_bootflag_read(int *boot);
int mtd_bootflag_write(u_char *buf);
int mmc_bootflag_write(struct mmc *mmcptr, u_char *bootflag);
int mmc_bootflag_read(struct mmc *mmcptr, int *bootflag);
int get_tclinux_imginfo(struct tclinux_imginfo *info);

#endif

