obj-y += bmt.o
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
