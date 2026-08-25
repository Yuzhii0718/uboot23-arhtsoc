# SPDX-License-Identifier: GPL-2.0+
#
# Airoha FIP image builder
#
# This Makefile bundles BL2/BL31 prebuilt binaries (located in per-SoC
# subdirectories) together with the just-built u-boot.bin into FIP
# (Firmware Image Package) images using fiptool.
#
# Kconfig-driven behaviour:
#   CONFIG_AIROHA_BUILD_FIP       — modern FIP build (AN7563/AN7581/AN7583)
#   CONFIG_AIROHA_FIP_LEGACY      — single-FIP layout: BL2+BL31+U-Boot,
#                                   1024-aligned (default y on AN7563)
#   CONFIG_AIROHA_BUILD_TCBOOT    — tcboot build (all Airoha platforms;
#                                   fixed 512 KiB, see airoha_pack_boot.sh)
#   CONFIG_AIROHA_BUILD_CHAINLOADER     — chainloader build (AN7581)
#   CONFIG_AIROHA_CHAINLOADER_DIR       — chainloader board selection
#
# Output files (placed in U-Boot top-level directory), named to match the
# OpenWrt artifacts:
#   bl31-uboot.fip   — main FIP image (split layout only: BL31+U-Boot)
#   preloader.bin    — BL2 FIP image (split layout or COPY_BL2)
#   bl2-bl31-uboot.bin — legacy single-FIP flash image: 2 KiB zero prefix +
#                        internal FIP @0x800, padded to 512 KiB (FIP_LEGACY)
#   tcboot.bin   — 512 KiB tcboot image (TCBOOT build; BL1 + the same
#                        internal FIP as bl2-bl31-uboot.bin)
#   <board>-chainload-uboot.itb — OpenWrt-style chainload FIT (OPENWRT method)

-include ../../include/config/auto.conf
-include ../../include/autoconf.mk

HOSTCC      ?= cc
# SoC name for locating prebuilt blob directories.  CONFIG_AIROHA_SOC_NAME
# carries the actual SoC name (en7523, an7581, ...); fall back to
# CONFIG_SYS_SOC (always "airoha" for this family) for robustness.
SOC          := $(subst ",,$(CONFIG_AIROHA_SOC_NAME))
ifeq ($(SOC),)
SOC          := $(subst ",,$(CONFIG_SYS_SOC))
endif

# Prebuilt blobs live under tools/build_airoha/{legacy,openwrt}/<variant>_default/.
# The variant defaults to the SoC (e.g. en7523 -> legacy/en7523_default) and can
# be overridden per board via CONFIG_AIROHA_FIP_BLOBS_DIR (e.g. set it to
# "en7562_fiberhome_sr1041f_default").  Strip quotes first so "" (empty string
# in Kconfig) is treated as unset.
_FIP_BLOBS_DIR_RAW := $(subst ",,$(CONFIG_AIROHA_FIP_BLOBS_DIR))
ifneq ($(_FIP_BLOBS_DIR_RAW),)
  SOC_VARIANT := $(_FIP_BLOBS_DIR_RAW)
else
  SOC_VARIANT := $(SOC)_default
endif

# Pick the blob directory: openwrt/<variant> when CONFIG_AIROHA_BUILD_FIP
# is enabled, otherwise the SDK legacy path legacy/<variant>.
ifeq ($(CONFIG_AIROHA_BUILD_FIP),y)
  BLOBS_DIR := openwrt/$(SOC_VARIANT)
else
  BLOBS_DIR := legacy/$(SOC_VARIANT)
endif
FIPTOOL      := ../fiptool
UBOOT_BIN    := ../../u-boot.bin
OUT_DIR      := ../..
PACK_SH      := ../airoha_pack_boot.sh

BL1_BIN      := $(BLOBS_DIR)/bl1.bin
BL2_BIN      := $(BLOBS_DIR)/bl2.bin
BL31_LZMA    := $(BLOBS_DIR)/bl31.lzma
CERT_BIN     := $(BLOBS_DIR)/certificates.bin
KEY_AREA_BIN := $(BLOBS_DIR)/key_area.bin

BL2_FIP      := $(OUT_DIR)/preloader.bin
FIP_IMAGE    := $(OUT_DIR)/bl31-uboot.fip
TCBOOT_IMAGE := $(OUT_DIR)/tcboot.bin
_TCBOOT_FIP  := $(OUT_DIR)/_tcboot.fip

BOOTEXT_PY    := ../airoha_pack_bootext.py
BOOTEXT_IMAGE := $(OUT_DIR)/bootext.ram
CERT_EXTRACT_PY := ../airoha_extract_certificates.py
BOOTEXT_CERT_CNT := $(subst ",,$(CONFIG_AIROHA_BOOTEXT_CERT_COUNT))

FIP_LEGACY   := $(CONFIG_AIROHA_FIP_LEGACY)
BUILD_TCBOOT := $(CONFIG_AIROHA_BUILD_TCBOOT)

# ---- LZMA tool detection ------------------------------------------------
# Prefer the older lzma (LZMA SDK) encoder over xz.
LZMA_E := $(shell \
	if command -v lzma >/dev/null 2>&1; then echo "lzma -c"; \
	elif command -v xz >/dev/null 2>&1; then echo "xz --format=lzma --stdout"; fi \
)

# ---- BL31 / U-Boot path setup -------------------------------------------
# BL31 is always pre-compressed (bl31.lzma); u-boot.bin is always
# LZMA-compressed to u-boot.bin.lzma before packing.
UBOOT_OUT := $(UBOOT_BIN).lzma
UBOOT_TC_ARG := --uboot-lzma $(UBOOT_OUT)

# ---------------------------------------------------------------------------
# ATF metadata printout tools
# ---------------------------------------------------------------------------
_PRELOADER_INFO_PY := ../airoha_info_preloader.py
_BL31_INFO_PY      := ../airoha_info_bl31.py

.PHONY: _print_bl_info
_print_bl_info: $(BL2_BIN)
	@if [ -f "$(_PRELOADER_INFO_PY)" ] && [ -f "$(BL2_BIN)" ]; then \
		echo ""; \
		python3 "$(_PRELOADER_INFO_PY)" "$(BL2_BIN)" --summary 2>/dev/null; \
	fi
	@if [ -f "$(_BL31_INFO_PY)" ] && [ -f "$(BL31_LZMA)" ]; then \
		echo ""; \
		python3 "$(_BL31_INFO_PY)" "$(BL31_LZMA)" --summary 2>/dev/null; \
	fi
	@echo ""

# ---------------------------------------------------------------------------
# Build rules
# ---------------------------------------------------------------------------

.DEFAULT_GOAL := all

# Build fiptool from sources
$(FIPTOOL):
	@echo "  [FIP]    Building fiptool (host compiler)..."
	$(MAKE) -C ../fiptool-src CC=$(HOSTCC) ARCH_AIROHA=$(CONFIG_ARCH_AIROHA)

# ----- LZMA compression targets -------------------------------------------
# u-boot.bin is built by the main U-Boot Makefile, not by us.
$(UBOOT_BIN).lzma:
	@test -f $(UBOOT_BIN) || { echo "ERROR: $(UBOOT_BIN) not found (build u-boot.bin first)"; exit 1; }
	@echo "  [LZMA]   Compressing u-boot.bin -> u-boot.bin.lzma..."
	$(LZMA_E) $(UBOOT_BIN) > $@
	@python3 -c "import struct,os; p='$@'; s='$(UBOOT_BIN)'; d=open(p,'rb').read(); n=struct.pack('<Q',os.path.getsize(s)); open(p,'wb').write(d[:5]+n+d[13:]) if d[5:13]!=b'\xff'*8 and d[5:13]!=n else None"

ifeq ($(CONFIG_AIROHA_BUILD_FIP),y)
all: $(FIPTOOL)
all: $(BL2_FIP)

# ----- preloader.bin (BL2 FIP) -------------------------------------------
ifeq ($(FIP_LEGACY),y)
$(BL2_FIP): $(BL2_BIN) $(FIPTOOL)
	@echo "  [FIP]    Creating preloader.bin (BL2-only, 1024-byte aligned)..."
	$(FIPTOOL) create --align 1024 --tb-fw $(BL2_BIN) $@
else
$(BL2_FIP): $(BL2_BIN) $(FIPTOOL)
	@echo "  [FIP]    Creating preloader.bin (BL2-only, split layout)..."
	$(FIPTOOL) create --tb-fw $(BL2_BIN) $@
endif

# ----- bl31-uboot.fip (split layout only) -------------------------------------
# FIP-legacy does not build bl31-uboot.fip: it carries exactly the same
# BL2+BL31+U-Boot FIP, built once as $(_TCBOOT_FIP) below and shared
# with the tcboot image.  The two layouts only differ in the prefix
# (real BL1 header vs 2 KiB zero prefix), added by airoha_pack_boot.sh.
ifeq ($(FIP_LEGACY),)
all: $(FIP_IMAGE)
$(FIP_IMAGE): $(BL31_LZMA) $(UBOOT_OUT) $(FIPTOOL) _print_bl_info
	@echo "  [FIP]    Creating bl31-uboot.fip (split layout: BL31+U-Boot)..."
	$(FIPTOOL) create --soc-fw $(BL31_LZMA) --nt-fw $(UBOOT_OUT) $@
endif

# ----- bl2-bl31-uboot.bin (legacy single-FIP flash image) ----------------------
# AN7563-class BootROM layout (matches OpenWrt's bl2-bl31-uboot.bin):
# 2 KiB zero prefix + shared FIP @0x800, padded to 512 KiB.
# See tools/airoha_pack_boot.sh for the layout details.
ifeq ($(FIP_LEGACY),y)
FIP_LEGACY_IMAGE := $(OUT_DIR)/bl2-bl31-uboot.bin
all: $(FIP_LEGACY_IMAGE)
$(FIP_LEGACY_IMAGE): $(_TCBOOT_FIP) $(PACK_SH)
	@echo "  [FIP]    Packing bl2-bl31-uboot.bin (2 KiB zero prefix + shared FIP, 512 KiB)..."
	bash $(PACK_SH) --fip $(_TCBOOT_FIP) -o $@
endif

endif  # CONFIG_AIROHA_BUILD_FIP

# ----- shared internal FIP (BL2 + LZMA BL31 + LZMA U-Boot, 1024-aligned) -------
# The tcboot and FIP-legacy layouts differ only in the BL1 prefix (real
# BL1 header vs 2 KiB zero prefix); the internal FIP is identical, so it
# is built once here and shared by both.  tools/airoha_pack_boot.sh adds
# the prefix and pads the image to 512 KiB.
$(_TCBOOT_FIP): $(BL2_BIN) $(BL31_LZMA) $(UBOOT_OUT) $(FIPTOOL) _print_bl_info
	@echo "  [FIP]    Creating internal shared FIP (BL2+BL31+U-Boot, 1024-byte aligned)..."
	$(FIPTOOL) create --align 1024 \
		--tb-fw $(BL2_BIN) \
		--soc-fw $(BL31_LZMA) \
		--nt-fw $(UBOOT_OUT) $@

# ----- tcboot (airoha_pack_boot.sh: BL1 + shared FIP, fixed 512 KiB) -----
ifeq ($(BUILD_TCBOOT),y)
all: $(TCBOOT_IMAGE)
$(TCBOOT_IMAGE): $(BL1_BIN) $(_TCBOOT_FIP)
	@echo "  [TCBOOT] Packing tcboot.bin (BL1 + FIP, 512 KiB)..."
	bash $(PACK_SH) --bl1 $(BL1_BIN) --fip $(_TCBOOT_FIP) -o $@
endif

# ----- bootext.ram (airoha_pack_bootext.py) -------------------------------
ifeq ($(CONFIG_AIROHA_BOOTEXT_RAM),y)
all: $(BOOTEXT_IMAGE)

ifeq ($(CONFIG_AIROHA_BOOTEXT_EMBED_CERTS),y)
_BOOTEXT_CERT_TMP := $(BLOBS_DIR)/.bootext_cert_tmp

$(_BOOTEXT_CERT_TMP): $(CERT_BIN)
	@mkdir -p $@
	@echo "  [CERT]   Extracting $(BOOTEXT_CERT_CNT) TF-A certificate(s) from certificates.bin..."
	python3 $(CERT_EXTRACT_PY) $(CERT_BIN) -o $@ --expected $(BOOTEXT_CERT_CNT)

$(BOOTEXT_IMAGE): $(BL2_BIN) $(_BOOTEXT_CERT_TMP)
	@echo "  [BOOTEXT] Packing bootext.ram (BL2 + $(BOOTEXT_CERT_CNT) embedded TF-A cert(s), no CRC)..."
	@if [ "$(BOOTEXT_CERT_CNT)" = "1" ]; then \
		python3 $(BOOTEXT_PY) pack $(BL2_BIN) \
			--cert $(_BOOTEXT_CERT_TMP)/cert_01.der --no-crc -o $@; \
	else \
		CERTS=""; \
		for f in $$(ls $(_BOOTEXT_CERT_TMP)/*.der 2>/dev/null); do \
			CERTS="$$CERTS --cert $$f"; \
		done; \
		python3 $(BOOTEXT_PY) pack $(BL2_BIN) $$CERTS --no-crc -o $@; \
	fi
	@rm -rf $(_BOOTEXT_CERT_TMP)
else
$(BOOTEXT_IMAGE): $(BL2_BIN)
	@echo "  [BOOTEXT] Packing bootext.ram (bare BL2, no embedded certificates)..."
	python3 $(BOOTEXT_PY) pack $(BL2_BIN) -o $@
endif
endif

# ----- chainloader ----------------------------------------------------------
#   OpenWrt-style chainload FIT:
#     FIT with u-boot.bin.lzma (or u-boot.bin) + u-boot.dtb, generated via
#     tools/airoha_pack_chainloader_openwrt.sh (mkits.sh + mkimage).
#     Output: <board>-chainload-uboot.itb.

ifeq ($(CONFIG_AIROHA_BUILD_CHAINLOADER),y)
CHAINLOADER_SOC := $(SOC)

_CHAINLOADER_BOARD_RAW := $(subst ",,$(CONFIG_AIROHA_CHAINLOADER_BOARD))
ifneq ($(_CHAINLOADER_BOARD_RAW),)
CHAINLOADER_BOARD := $(_CHAINLOADER_BOARD_RAW)
else
$(error CONFIG_AIROHA_CHAINLOADER_BOARD is not set; set it to the board name.")
endif

CHAINLOADER_ITB    := $(OUT_DIR)/$(CHAINLOADER_BOARD)-chainload-uboot.itb
CHAINLOADER_SCRIPT := ../airoha_pack_chainloader_openwrt.sh
MKITS_SH           := ../mkits.sh
MKIMAGE_BIN        := ../../tools/mkimage
UBOOT_DTB          := $(OUT_DIR)/u-boot.dtb

all: $(CHAINLOADER_ITB)

$(CHAINLOADER_ITB): $(UBOOT_BIN) $(UBOOT_OUT) $(UBOOT_DTB) $(CHAINLOADER_SCRIPT) $(MKITS_SH) $(MKIMAGE_BIN)
	@echo "  [CHAINLOADER] Packing $(CHAINLOADER_BOARD) chainloader (OpenWrt method: mkits.sh + mkimage)..."
	bash $(CHAINLOADER_SCRIPT) \
		--soc $(CHAINLOADER_SOC) --board $(CHAINLOADER_BOARD) \
		--payload $(UBOOT_BIN) --payload-lzma $(UBOOT_OUT) \
		--dtb $(UBOOT_DTB) --output-dir $(OUT_DIR) \
		--mkimage ../../tools/mkimage --mkits ../mkits.sh

endif  # CONFIG_AIROHA_BUILD_CHAINLOADER

# ---------------------------------------------------------------------------
clean:
	rm -f $(BL2_FIP) $(FIP_IMAGE) $(TCBOOT_IMAGE) $(_TCBOOT_FIP) $(BOOTEXT_IMAGE) $(UBOOT_BIN).lzma \
		$(OUT_DIR)/bl2-bl31-uboot.bin \
		$(OUT_DIR)/*-chainload-uboot.itb \
	rm -rf $(OUT_DIR)/.chainload-fit-*

.PHONY: all clean
