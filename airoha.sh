#!/bin/bash
#===============================================================================
# airoha.sh - Universal Airoha U-Boot build script
#
# Usage: SOC=<soc> BOARD=<board> [OPTIONS] ./airoha.sh
#
# Airoha U-Boot standalone build script.
# FIP image creation (BL2/BL31/U-Boot packing) is handled by U-Boot's
# build system (make all).  This script only manages toolchain detection,
# build invocation, and output file collection.
#
# Required:
#   SOC=<soc>       Target SoC: en7523 | en7529 | en7562 | an7563 | an7581 | an7583
#   BOARD=<board>   Target board name (matches defconfig: ${SOC}_${BOARD}_defconfig)
#
# Options:
#   TOOLCHAIN=<prefix>  Cross-compiler prefix (default: auto-detect by SOC)
#   JOBS=<n>            Parallel make jobs (default: nproc)
#   STAGING_DIR=<path>  Passed to make (default: empty)
#
# Toolchain auto-detection:
#   en7523 / en7529 / en7562 / an7563  ->  arm-linux-gnueabi-        (ARMv7, 32-bit)
#   an7581 / an7583                    ->  aarch64-linux-gnu-        (AArch64, 64-bit)
#
# Examples:
#   SOC=en7523  BOARD=evb                ./airoha.sh
#   SOC=an7563  BOARD=xiaomi_be5000      ./airoha.sh
#   SOC=an7581  BOARD=evb                ./airoha.sh
#   SOC=an7581  BOARD=w1700k             ./airoha.sh
#   SOC=an7583  BOARD=evb                ./airoha.sh
#
#   # Override toolchain:
#   SOC=an7581 BOARD=evb TOOLCHAIN=aarch64-linux-musl- ./airoha.sh
#===============================================================================

set -e

export PATH="$(printf '%s' "$PATH" | tr ':' '\n' | grep -vE 'safe-bin|/vendor/shim' | paste -sd:)"
unset -f rm 2>/dev/null || true
unset BASH_ENV 2>/dev/null || true

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()    { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()    { echo -e "${YELLOW}[WARN]${NC}  $*"; }
error()   { echo -e "${RED}[ERROR]${NC} $*"; }
step()    { echo -e "\n${BLUE}=== $* ===${NC}"; }
die()     { error "$*"; exit 1; }

#------------------------------------------------------------------------------
# --help / -h: print usage extracted from the header comment block
#------------------------------------------------------------------------------
usage() {
	sed -n '2,/^[^#]/p' "$0" | grep -E '^#( |$)' | sed 's/^# \?//'
	exit 0
}

case "${1:-}" in
	--help|-h|help)
		usage
		;;
esac

# ---------------------------------------------------------------------------
# Extract U-Boot version from Makefile
# ---------------------------------------------------------------------------
UBOOT_VERSION=$(awk -F'= ' '
    /^VERSION =/      {v=$2}
    /^PATCHLEVEL =/   {p=$2}
    /^SUBLEVEL =/     {s=$2}
    /^EXTRAVERSION =/ {e=$2}
    /^NAME =/         {n=$2}
    END {
        ver = v "." p
        if (s != "") ver = ver "." s
        ver = ver e "-" n
        print ver
    }' Makefile)

#------------------------------------------------------------------------------
# SOC and BOARD parameter check
#------------------------------------------------------------------------------
SOC="${SOC,,}" # Transform to lowercase

if [ -z "${SOC}" ] || [ -z "${BOARD}" ]; then
	error "SOC and BOARD environment variables must be specified."
	echo "Usage: SOC=<soc> BOARD=<board> [OPTIONS] $0"
	echo "Try '$0 --help' for more information."
	exit 1
fi

case "${SOC}" in
	en7523|en7529|en7562|an7552|an7563)
		DEFAULT_TOOLCHAIN="arm-linux-gnueabi-"
		;;
	an7551|an7581|an7553|an7583)
		DEFAULT_TOOLCHAIN="aarch64-linux-gnu-"
		;;
	*)
		error "Not supported SOC: ${SOC}"
		echo "Supported: en7523, en7529, en7562, an7552, an7563, an7551, an7581, an7553, an7583"
		exit 1
		;;
esac

SOC_UPPER="${SOC^^}"

#------------------------------------------------------------------------------
# Path and Toolchain Configuration
#------------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
UBOOT_DIR="${SCRIPT_DIR}"
OUTPUT_DIR="${UBOOT_DIR}/output"
TOOLCHAIN="${TOOLCHAIN:-${DEFAULT_TOOLCHAIN}}"
DEFCONFIG="${SOC}_${BOARD}_defconfig"
DEFCONFIG_PATH="${UBOOT_DIR}/configs/${DEFCONFIG}"
OUTPUT_PREFIX="${SOC}_${BOARD}"

if [ -z "${JOBS}" ]; then
	if command -v nproc &>/dev/null; then
		JOBS=$(nproc)
	else
		JOBS=1
	fi
fi

# Helpers
# Files actually copied by the current build; print_summary only lists these.
COPIED_FILES=()

copy_with_md5() {
	local file=$1
	local dest=$2
	local label=$3
	local md5
	md5=$(md5sum "$file" | awk '{print $1}')
	cp -f "$file" "$dest"
	COPIED_FILES+=("${dest}")
	info "${label} (md5: ${md5}) -> ${dest}"
}

config_enabled() {
	grep -q "^$1=y$" "${UBOOT_DIR}/.config"
}

get_bootext_prefix() {
	local blobs_dir embed_certs prefix
	blobs_dir=$(sed -n 's/^CONFIG_AIROHA_FIP_BLOBS_DIR="\(.*\)"$/\1/p' "${UBOOT_DIR}/.config")
	embed_certs=$(sed -n 's/^CONFIG_AIROHA_BOOTEXT_EMBED_CERTS=\(.*\)$/\1/p' "${UBOOT_DIR}/.config")
	prefix="${blobs_dir:-${SOC}}"
	# strip the "_default" suffix so output names stay <soc>-bootext.ram
	prefix="${prefix%_default}"
	if [ "${embed_certs}" = "y" ]; then
		prefix="${prefix}-cert"
	fi
	echo "${prefix}"
}

#------------------------------------------------------------------------------
# Environment Check
#------------------------------------------------------------------------------
check_environment() {
	step "Environment Check [SOC: ${SOC_UPPER}] [BOARD: ${BOARD}]"

	# --- Python 3 ---
	if ! command -v python3 &>/dev/null; then
		error "Python 3 is not installed."
		error "Please install: sudo apt install -y python3"
		exit 1
	fi
	info "Python3: $(python3 --version 2>&1)"

	# --- Cross Toolchain ---
	if ! command -v "${TOOLCHAIN}gcc" &>/dev/null; then
		error "Cross toolchain not found: ${TOOLCHAIN}gcc"
		error "Please install the appropriate toolchain or set TOOLCHAIN=<prefix>"
		exit 1
	fi
	info "Toolchain: $(${TOOLCHAIN}gcc --version | head -1)"

	# --- Defconfig ---
	if [ ! -f "${DEFCONFIG_PATH}" ]; then
		error "Defconfig not found: ${DEFCONFIG_PATH}"
		exit 1
	fi
	info "Defconfig: ${DEFCONFIG}"

	mkdir -p "${OUTPUT_DIR}"
	info "Environment Check passed"
}

#------------------------------------------------------------------------------
# Configure U-Boot
#------------------------------------------------------------------------------
configure_uboot() {
	step "Configure U-Boot"

	cd "${UBOOT_DIR}"

	cp -f "${DEFCONFIG_PATH}" "${UBOOT_DIR}/.config"
	make olddefconfig

	info "U-Boot configured"
}

#------------------------------------------------------------------------------
# Detect Build Features
#------------------------------------------------------------------------------
detect_build_features() {
	step "Detect Build Features"

	SOC_FAMILY=$(sed -n 's/^CONFIG_SYS_SOC="\(.*\)"$/\1/p' "${UBOOT_DIR}/.config")
	[ -n "${SOC_FAMILY}" ] || SOC_FAMILY="${SOC}"

	if config_enabled CONFIG_AIROHA_BUILD_FIP; then
		AIROHA_BUILD_FIP="y"
	else
		AIROHA_BUILD_FIP="n"
	fi

	if config_enabled CONFIG_AIROHA_BUILD_TCBOOT; then
		AIROHA_BUILD_TCBOOT="y"
	else
		AIROHA_BUILD_TCBOOT="n"
	fi

	if config_enabled CONFIG_AIROHA_BUILD_CHAINLOADER; then
		AIROHA_BUILD_CHAINLOADER="y"
	else
		AIROHA_BUILD_CHAINLOADER="n"
	fi

	if config_enabled CONFIG_AIROHA_BOOTEXT_RAM; then
		AIROHA_BOOTEXT_RAM="y"
	else
		AIROHA_BOOTEXT_RAM="n"
	fi

    if [ "${AIROHA_BUILD_FIP}" = "y" ]; then
        BUILD_TYPE="FIP"
    elif [ "${AIROHA_BUILD_TCBOOT}" = "y" ]; then
        BUILD_TYPE="TCBOOT"
	elif [ "${AIROHA_BUILD_CHAINLOADER}" = "y" ]; then
		BUILD_TYPE="CHAINLOADER"
    else
        BUILD_TYPE="None (legacy)"
    fi

	echo "SOC:                  ${SOC}"
	echo "BOARD:                ${BOARD}"
	echo "Defconfig:            ${DEFCONFIG}"
	echo "Toolchain:            ${TOOLCHAIN}"
	echo "Build Type:           ${BUILD_TYPE}"
}

#------------------------------------------------------------------------------
# Build U-Boot
#------------------------------------------------------------------------------
build_uboot() {
	step "Build U-Boot [${SOC_UPPER}]"

	cd "${UBOOT_DIR}"

	make clean
	make CROSS_COMPILE="${TOOLCHAIN}" STAGING_DIR="${STAGING_DIR:-}" -j "${JOBS}" all

	if [ ! -f "${UBOOT_DIR}/u-boot.bin" ]; then
		error "U-Boot build failed: u-boot.bin not generated"
		exit 1
	fi
	info "U-Boot build done: $(stat -c%s "${UBOOT_DIR}/u-boot.bin") bytes"
}

#------------------------------------------------------------------------------
# Copy Output Files
#------------------------------------------------------------------------------
copy_outputs() {
	step "Copy Output Files"

	cd "${UBOOT_DIR}"
	mkdir -p "${OUTPUT_DIR}"

	if [ "${AIROHA_BUILD_FIP}" = "y" ]; then
		if config_enabled CONFIG_AIROHA_FIP_LEGACY; then
			# FIP-legacy (AN7563): bl31-uboot.fip is the intermediate FIP
			# embedded in bl2-bl31-uboot.bin, so only the packed BootROM
			# flash image is copied out (preloader.bin below is the BL2).
			[ -f "${UBOOT_DIR}/bl2-bl31-uboot.bin" ] || \
				die "FIP legacy enabled, but bl2-bl31-uboot.bin was not generated."
			copy_with_md5 "${UBOOT_DIR}/bl2-bl31-uboot.bin" \
				"${OUTPUT_DIR}/${OUTPUT_PREFIX}-bl2-bl31-uboot.bin" \
				"bl2-bl31-uboot.bin"
		else
			# Split FIP (AN7581/AN7583): BL31 + U-Boot pair in
			# bl31-uboot.fip (OpenWrt artifact name).
			[ -f "${UBOOT_DIR}/bl31-uboot.fip" ] || die "FIP build enabled, but bl31-uboot.fip was not generated."
			copy_with_md5 "${UBOOT_DIR}/bl31-uboot.fip" \
				"${OUTPUT_DIR}/${OUTPUT_PREFIX}-bl31-uboot.fip" \
				"bl31-uboot.fip"
		fi

		# preloader.bin is the standalone BL2 FIP (OpenWrt artifact name).
		[ -f "${UBOOT_DIR}/preloader.bin" ] || die "FIP COPY_BL2 enabled, but preloader.bin was not generated."
		copy_with_md5 "${UBOOT_DIR}/preloader.bin" \
			"${OUTPUT_DIR}/${OUTPUT_PREFIX}-preloader.bin" \
			"preloader.bin"
	elif [ "${AIROHA_BUILD_TCBOOT}" = "y" ]; then
		# Non-FIP + TCBOOT: tcboot.bin handles boot, skip u-boot.bin.lzma copy
		:
	else
		# Non-FIP build: prefer u-boot.bin.lzma if make produced it
		if [ -f "${UBOOT_DIR}/u-boot.bin.lzma" ]; then
			copy_with_md5 "${UBOOT_DIR}/u-boot.bin.lzma" \
				"${OUTPUT_DIR}/${OUTPUT_PREFIX}-u-boot.bin.lzma" \
				"u-boot.bin.lzma"
		else
			copy_with_md5 "${UBOOT_DIR}/u-boot.bin" \
				"${OUTPUT_DIR}/${OUTPUT_PREFIX}-u-boot.bin" \
				"u-boot.bin"
		fi
	fi

	# TCBOOT is independent of the FIP build mode (all Airoha platforms
	# support it, and it can be enabled together with AIROHA_BUILD_FIP).
	if [ "${AIROHA_BUILD_TCBOOT}" = "y" ]; then
		[ -f "${UBOOT_DIR}/tcboot.bin" ] || die "TCBOOT enabled, but tcboot.bin was not generated."
		copy_with_md5 "${UBOOT_DIR}/tcboot.bin" \
			"${OUTPUT_DIR}/${OUTPUT_PREFIX}-tcboot.bin" \
			"tcboot.bin"
	fi

	if [ "${AIROHA_BOOTEXT_RAM}" = "y" ]; then
		[ -f "${UBOOT_DIR}/bootext.ram" ] || die "BOOTEXT enabled, but bootext.ram was not generated."
		local bootext_prefix
		bootext_prefix=$(get_bootext_prefix)
		copy_with_md5 "${UBOOT_DIR}/bootext.ram" \
			"${OUTPUT_DIR}/${bootext_prefix}-bootext.ram" \
			"bootext.ram"
	fi

	if [ "${AIROHA_BUILD_CHAINLOADER}" = "y" ]; then
		local chainloader_dir chainloader_board
		chainloader_board=$(sed -n 's/^CONFIG_AIROHA_CHAINLOADER_BOARD="\(.*\)"$/\1/p' "${UBOOT_DIR}/.config")
		[ -n "${chainloader_board}" ] || \
			die "CONFIG_AIROHA_CHAINLOADER_BOARD is not set."
		[ -f "${UBOOT_DIR}/${chainloader_board}-chainload-uboot.itb" ] || \
			die "CHAINLOADER enabled, but ${chainloader_board}-chainload-uboot.itb was not generated."
		copy_with_md5 "${UBOOT_DIR}/${chainloader_board}-chainload-uboot.itb" \
			"${OUTPUT_DIR}/${OUTPUT_PREFIX}-chainload-uboot.itb" \
			"chainload-uboot.itb"
	fi
}

#------------------------------------------------------------------------------
# Print Summary
#------------------------------------------------------------------------------
print_summary() {
	echo ""
	echo "==========================================================================="
	echo -e "  ${GREEN}${SOC_UPPER} ${BOARD} U-Boot build completed!${NC}"
	echo "==========================================================================="
	echo ""
	echo "  Output directory: ${OUTPUT_DIR}/"
	echo ""

	if [ ${#COPIED_FILES[@]} -gt 0 ]; then
		local f base size
		for f in "${COPIED_FILES[@]}"; do
			[ -f "$f" ] || continue
			base=$(basename "$f")
			size=$(stat -c%s "$f")
			printf "    %-40s  %10s bytes\n" "${base}" "${size}"
		done
	fi

	echo ""
	echo "==========================================================================="
}

#------------------------------------------------------------------------------
# Main
#------------------------------------------------------------------------------
main() {
	echo ""
	echo "==========================================================================="
	echo "	Airoha U-Boot ${UBOOT_VERSION} Build Script"
	echo "  Build for ${SOC_UPPER} ${BOARD}"
	echo "  Source:   ${UBOOT_DIR}"
	echo "  Defconfig: ${DEFCONFIG}"
	echo "  Output:   ${OUTPUT_DIR}"
	echo "==========================================================================="

	check_environment
	configure_uboot
	detect_build_features
	build_uboot
	copy_outputs

	print_summary
}

main "$@"
