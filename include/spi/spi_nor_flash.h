// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 AIROHA Inc
*/

#ifndef __SPI_NOR_FLASH_H__
#define __SPI_NOR_FLASH_H__

void spi_nor_read(unsigned char *data, unsigned long addr, int len);
unsigned char spi_nor_read_byte(unsigned long addr);

#endif
/* End of [spi_nor_flash.h] package */

