/*
 * Copyright (c) 2014-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FIRMWARE_IMAGE_PACKAGE_H
#define FIRMWARE_IMAGE_PACKAGE_H

#include <linux/types.h>

#include "uuid.h"

/* This is used as a signature to validate the blob header */
#define TOC_HEADER_NAME	0xAA640001

#define SIP_VERIFY_BL2			0x82000201
#define SIP_VERIFY_BL33			0x82000205

/* ToC Entry UUIDs */
#define UUID_TRUSTED_BOOT_FIRMWARE_BL2 \
	{{0x5f, 0xf9, 0xec, 0x0b}, {0x4d, 0x22}, {0x3e, 0x4d}, 0xa5, 0x44, {0xc3, 0x9d, 0x81, 0xc7, 0x3f, 0x0a} }
#define UUID_NON_TRUSTED_FIRMWARE_BL33 \
	{{0xd6,  0xd0, 0xee, 0xa7}, {0xfc, 0xea}, {0xd5, 0x4b}, 0x97, 0x82, {0x99, 0x34, 0xf2, 0x34, 0xb6, 0xe4} }

typedef struct fip_toc_header {
	uint32_t	name;
	uint32_t	serial_number;
	uint64_t	flags;
} fip_toc_header_t;

typedef struct fip_toc_entry {
	uuid_t		uuid;
	uint64_t	offset_address;
	uint64_t	size;
	uint64_t	flags;
} fip_toc_entry_t;

#endif /* FIRMWARE_IMAGE_PACKAGE_H */
