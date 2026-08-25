# SPDX-License-Identifier: GPL-2.0+

ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)
nandcore-objs := core.o bbt.o
obj-$(CONFIG_MTD_NAND_CORE) += nandcore.o
obj-$(CONFIG_MTD_RAW_NAND) += raw/
obj-$(CONFIG_MTD_SPI_NAND) += spi/
else
nandcore-objs := core.o bbt.o
obj-$(CONFIG_$(SPL_TPL_)SPI_FLASH_SUPPORT) += nandcore.o
obj-$(CONFIG_$(SPL_TPL_)SPI_FLASH_SUPPORT) += spi/
obj-$(CONFIG_$(SPL_TPL_)NAND_SUPPORT) += raw/
endif
obj-$(CONFIG_AIROHA_BMT) += airoha-bmt.o mtk-bmt.o
