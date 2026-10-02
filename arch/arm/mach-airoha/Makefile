# SPDX-License-Identifier:	GPL-2.0
obj-y	+= cpu.o
ifeq ($(CONFIG_SPL_BUILD)$(CONFIG_TPL_BUILD),)
obj-y	+= ecnt_flash.o
obj-y	+= ecnt_image.o
obj-y   += ecnt_verify.o
obj-y   += fiptool.o
obj-y	+= ecnt_bootargs.o
else
obj-y	+= spl.o
endif

ifneq ($(CONFIG_CMD_INIC),)
obj-y	+= pt.o
obj-y	+= host_bp1.o
obj-y	+= host_bp2.o
obj-y	+= host_service_manager.o
obj-y	+= mdio_api.o
endif

obj-$(CONFIG_TARGET_EN7523) += en7523/
obj-$(CONFIG_TARGET_AN7581) += an7581/
obj-$(CONFIG_TARGET_AN7552) += an7552/
obj-$(CONFIG_TARGET_AN7583) += an7583/
