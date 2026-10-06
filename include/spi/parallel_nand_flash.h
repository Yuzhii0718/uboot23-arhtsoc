// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
*/

/*======================================================================================
 * MODULE NAME: 
 * FILE NAME: 
 * DATE: 
 * VERSION: 1.00
 * PURPOSE: 
 * NOTES:
 *
 * AUTHOR : 
 *
 * FUNCTIONS  
 *
 * DEPENDENCIES
 *
 * * $History: $
 * MODIFICTION HISTORY:
 *======================================================================================
 */

#ifndef __PARALLEL_NAND_FLASH_H__
#define __PARALLEL_NAND_FLASH_H__

/* INCLUDE FILE DECLARATIONS --------------------------------------------------------- */
#include <linux/types.h>
#include <spi/spi_nand_flash.h>



/* MACRO DECLARATIONS ---------------------------------------------------------------- */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)

#else
//extern void * memcpy4(void * dest, const void *src, size_t count);
//#define memcpy	memcpy4
#endif

/* TYPE DECLARATIONS ----------------------------------------------------------------- */


/* EXPORTED SUBPROGRAM SPECIFICATION ------------------------------------------------- */

#endif /* ifndef __PARALLEL_NAND_FLASH_H__ */
/* End of [parallel_nand_flash.h] package */

