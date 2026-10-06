# BMT implements the I/O mapping helpers used by the guarded BMT paths in
# spi_nand_flash.c; it references en7512_nand_* which only exist when the
# legacy BMT is enabled (see LEGACY_NAND_BMT in include/spi/spi_nand_flash.h).
# Keyed off the same Kconfig switch as the new SPI-NAND stack so the two can
# never disagree.
obj-$(CONFIG_AIROHA_BMT) += bmt.o
obj-y += flashhal.o
obj-y += newspiflash.o
obj-y += spi_nand_flash_table.o
obj-y += spi_nand_flash.o
obj-y += spi_controller.o
obj-y += spi_ecc.o
obj-y += spi_nfi.o

# Parallel (raw) NAND support is a per-platform option: the chip tables and the
# NFI macros it needs are all behind TCSUPPORT_PARALLEL_NAND in include/spi/.
ifneq ($(TCSUPPORT_PARALLEL_NAND),)
obj-y += parallel_nand_flash.o
obj-y += parallel_nand_flash_table.o
endif
