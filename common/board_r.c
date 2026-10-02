// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2011 The Chromium OS Authors.
 * (C) Copyright 2002-2006
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 */

#include <common.h>
#include <api.h>
#include <bootstage.h>
#include <cpu_func.h>
#include <cyclic.h>
#include <display_options.h>
#include <exports.h>
#ifdef CONFIG_MTD_NOR_FLASH
#include <flash.h>
#endif
#include <hang.h>
#include <image.h>
#include <irq_func.h>
#include <log.h>
#include <net.h>
#include <asm/cache.h>
#include <asm/global_data.h>
#include <u-boot/crc.h>
#include <binman.h>
#include <command.h>
#include <console.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdtdec.h>
#include <ide.h>
#include <init.h>
#include <initcall.h>
#include <kgdb.h>
#include <irq_func.h>
#include <malloc.h>
#include <mapmem.h>
#include <miiphy.h>
#include <mmc.h>
#include <mux.h>
#include <nand.h>
#include <of_live.h>
#include <onenand_uboot.h>
#include <pvblock.h>
#include <scsi.h>
#include <serial.h>
#include <status_led.h>
#include <stdio_dev.h>
#include <timer.h>
#include <trace.h>
#include <watchdog.h>
#include <xen.h>
#include <asm/sections.h>
#include <dm/root.h>
#include <dm/ofnode.h>
#include <linux/compiler.h>
#include <linux/err.h>
#include <efi_loader.h>
#include <wdt.h>
#include <asm-generic/gpio.h>
#include <efi_loader.h>
#include <relocate.h>

#ifdef TCSUPPORT_BOARD_SELECT
#include <linux/string.h>
#include <airoha/airoha_board.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

ulong monitor_flash_len;

extern bool is_emmc(void);
__weak int board_flash_wp_on(void)
{
	/*
	 * Most flashes can't be detected when write protection is enabled,
	 * so provide a way to let U-Boot gracefully ignore write protected
	 * devices.
	 */
	return 0;
}

__weak int cpu_secondary_init_r(void)
{
	return 0;
}

static int initr_trace(void)
{
#ifdef CONFIG_TRACE
	trace_init(gd->trace_buff, CONFIG_TRACE_BUFFER_SIZE);
#endif

	return 0;
}

static int initr_reloc(void)
{
	/* tell others: relocation done */
	gd->flags |= GD_FLG_RELOC | GD_FLG_FULL_MALLOC_INIT;

	return 0;
}

#if defined(CONFIG_ARM) || defined(CONFIG_RISCV)
/*
 * Some of these functions are needed purely because the functions they
 * call return void. If we change them to return 0, these stubs can go away.
 */
static int initr_caches(void)
{
	/* Enable caches */
	enable_caches();
	return 0;
}
#endif

__weak int fixup_cpu(void)
{
	return 0;
}

static int initr_reloc_global_data(void)
{
#ifdef __ARM__
	monitor_flash_len = _end - __image_copy_start;
#elif defined(CONFIG_RISCV)
	monitor_flash_len = (ulong)&_end - (ulong)&_start;
#elif !defined(CONFIG_SANDBOX) && !defined(CONFIG_NIOS2)
	monitor_flash_len = (ulong)&__init_end - gd->relocaddr;
#endif
#if defined(CONFIG_MPC85xx) || defined(CONFIG_MPC86xx)
	/*
	 * The gd->cpu pointer is set to an address in flash before relocation.
	 * We need to update it to point to the same CPU entry in RAM.
	 * TODO: why not just add gd->reloc_ofs?
	 */
	gd->arch.cpu += gd->relocaddr - CONFIG_SYS_MONITOR_BASE;

	/*
	 * If we didn't know the cpu mask & # cores, we can save them of
	 * now rather than 'computing' them constantly
	 */
	fixup_cpu();
#endif
#ifdef CONFIG_SYS_RELOC_GD_ENV_ADDR
	/*
	 * Relocate the early env_addr pointer unless we know it is not inside
	 * the binary. Some systems need this and for the rest, it doesn't hurt.
	 */
	gd->env_addr += gd->reloc_off;
#endif
	/*
	 * The fdt_blob needs to be moved to new relocation address
	 * incase of FDT blob is embedded with in image
	 */
	if (IS_ENABLED(CONFIG_OF_EMBED) && IS_ENABLED(CONFIG_NEEDS_MANUAL_RELOC))
		gd->fdt_blob += gd->reloc_off;

#ifdef CONFIG_EFI_LOADER
	/*
	 * On the ARM architecture gd is mapped to a fixed register (r9 or x18).
	 * As this register may be overwritten by an EFI payload we save it here
	 * and restore it on every callback entered.
	 */
	efi_save_gd();

	efi_runtime_relocate(gd->relocaddr, NULL);
#endif

	return 0;
}

__weak int arch_initr_trap(void)
{
	return 0;
}

#if defined(CONFIG_SYS_INIT_RAM_LOCK) && defined(CONFIG_E500)
static int initr_unlock_ram_in_cache(void)
{
	unlock_ram_in_cache();	/* it's time to unlock D-cache in e500 */
	return 0;
}
#endif

static int initr_barrier(void)
{
#ifdef CONFIG_PPC
	/* TODO: Can we not use dmb() macros for this? */
	asm("sync ; isync");
#endif
	return 0;
}

static int initr_malloc(void)
{
	ulong malloc_start;

#if CONFIG_VAL(SYS_MALLOC_F_LEN)
	debug("Pre-reloc malloc() used %#lx bytes (%ld KB)\n", gd->malloc_ptr,
	      gd->malloc_ptr / 1024);
#endif
	/* The malloc area is immediately below the monitor copy in DRAM */
	/*
	 * This value MUST match the value of gd->start_addr_sp in board_f.c:
	 * reserve_noncached().
	 */
	malloc_start = gd->relocaddr - TOTAL_MALLOC_LEN;
	mem_malloc_init((ulong)map_sysmem(malloc_start, TOTAL_MALLOC_LEN),
			TOTAL_MALLOC_LEN);
	return 0;
}

static int initr_of_live(void)
{
	if (CONFIG_IS_ENABLED(OF_LIVE)) {
		int ret;

		bootstage_start(BOOTSTAGE_ID_ACCUM_OF_LIVE, "of_live");
		ret = of_live_build(gd->fdt_blob,
				    (struct device_node **)gd_of_root_ptr());
		bootstage_accum(BOOTSTAGE_ID_ACCUM_OF_LIVE);
		if (ret)
			return ret;
	}

	return 0;
}

#ifdef CONFIG_DM
static int initr_dm(void)
{
	int ret;

	oftree_reset();

	/* Save the pre-reloc driver model and start a new one */
	gd->dm_root_f = gd->dm_root;
	gd->dm_root = NULL;
#ifdef CONFIG_TIMER
	gd->timer = NULL;
#endif
	bootstage_start(BOOTSTAGE_ID_ACCUM_DM_R, "dm_r");
	ret = dm_init_and_scan(false);
	bootstage_accum(BOOTSTAGE_ID_ACCUM_DM_R);
	if (ret)
		return ret;

	return 0;
}
#endif

static int initr_dm_devices(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_TIMER_EARLY)) {
		ret = dm_timer_init();
		if (ret)
			return ret;
	}

	if (IS_ENABLED(CONFIG_MULTIPLEXER)) {
		/*
		 * Initialize the multiplexer controls to their default state.
		 * This must be done early as other drivers may unknowingly
		 * rely on it.
		 */
		ret = dm_mux_init();
		if (ret)
			return ret;
	}

	return 0;
}

static int initr_bootstage(void)
{
	bootstage_mark_name(BOOTSTAGE_ID_START_UBOOT_R, "board_init_r");

	return 0;
}

__weak int power_init_board(void)
{
	return 0;
}

static int initr_announce(void)
{
	debug("Now running in RAM - U-Boot at: %08lx\n", gd->relocaddr);
	return 0;
}

#ifdef CONFIG_NEEDS_MANUAL_RELOC
static int initr_manual_reloc_cmdtable(void)
{
	fixup_cmdtable(ll_entry_start(struct cmd_tbl, cmd),
		       ll_entry_count(struct cmd_tbl, cmd));
	return 0;
}
#endif

static int initr_binman(void)
{
	int ret;

	if (!CONFIG_IS_ENABLED(BINMAN_FDT))
		return 0;

	ret = binman_init();
	if (ret)
		printf("binman_init failed:%d\n", ret);

	return ret;
}

#if defined(CONFIG_MTD_NOR_FLASH)
__weak int is_flash_available(void)
{
	return 1;
}

static int initr_flash(void)
{
	ulong flash_size = 0;
	struct bd_info *bd = gd->bd;

	if (!is_flash_available())
		return 0;

	puts("Flash: ");

	if (board_flash_wp_on())
		printf("Uninitialized - Write Protect On\n");
	else
		flash_size = flash_init();

	print_size(flash_size, "");
#ifdef CONFIG_SYS_FLASH_CHECKSUM
	/*
	 * Compute and print flash CRC if flashchecksum is set to 'y'
	 *
	 * NOTE: Maybe we should add some schedule()? XXX
	 */
	if (env_get_yesno("flashchecksum") == 1) {
		const uchar *flash_base = (const uchar *)CFG_SYS_FLASH_BASE;

		printf("  CRC: %08X", crc32(0,
					    flash_base,
					    flash_size));
	}
#endif /* CONFIG_SYS_FLASH_CHECKSUM */
	putc('\n');

	/* update start of FLASH memory    */
#ifdef CFG_SYS_FLASH_BASE
	bd->bi_flashstart = CFG_SYS_FLASH_BASE;
#endif
	/* size of FLASH memory (final value) */
	bd->bi_flashsize = flash_size;

#if defined(CONFIG_SYS_UPDATE_FLASH_SIZE)
	/* Make a update of the Memctrl. */
	update_flash_size(flash_size);
#endif

#if defined(CONFIG_OXC) || defined(CONFIG_RMU)
	/* flash mapped at end of memory map */
	bd->bi_flashoffset = CONFIG_TEXT_BASE + flash_size;
#elif CONFIG_SYS_MONITOR_BASE == CFG_SYS_FLASH_BASE
	bd->bi_flashoffset = monitor_flash_len;	/* reserved area for monitor */
#endif
	return 0;
}
#endif

#ifdef CONFIG_CMD_NAND
/* go init the NAND */
static int initr_nand(void)
{
	puts("NAND:  ");
	nand_init();
	printf("%lu MiB\n", nand_size() / 1024);
	return 0;
}
#endif

#if defined(CONFIG_CMD_ONENAND)
/* go init the NAND */
static int initr_onenand(void)
{
	puts("NAND:  ");
	onenand_init();
	return 0;
}
#endif

#ifdef CONFIG_MMC
static int initr_mmc(void)
{
	puts("MMC:   ");
	mmc_initialize(gd->bd);
	return 0;
}
#endif

#ifdef CONFIG_PVBLOCK
static int initr_pvblock(void)
{
	puts("PVBLOCK: ");
	pvblock_init();
	return 0;
}
#endif

/*
 * Tell if it's OK to load the environment early in boot.
 *
 * If CONFIG_OF_CONTROL is defined, we'll check with the FDT to see
 * if this is OK (defaulting to saying it's OK).
 *
 * NOTE: Loading the environment early can be a bad idea if security is
 *       important, since no verification is done on the environment.
 *
 * Return: 0 if environment should not be loaded, !=0 if it is ok to load
 */
static int should_load_env(void)
{
	if (IS_ENABLED(CONFIG_OF_CONTROL))
		return ofnode_conf_read_int("load-environment", 1);

	if (IS_ENABLED(CONFIG_DELAY_ENVIRONMENT))
		return 0;

	return 1;
}

static int initr_env(void)
{
	/* initialize environment */
	if (should_load_env())
		env_relocate();
	else
		env_set_default(NULL, 0);

	env_import_fdt();

	if (IS_ENABLED(CONFIG_OF_CONTROL))
		env_set_hex("fdtcontroladdr",
			    (unsigned long)map_to_sysmem(gd->fdt_blob));

	#if (IS_ENABLED(CONFIG_SAVE_PREV_BL_INITRAMFS_START_ADDR) || \
						IS_ENABLED(CONFIG_SAVE_PREV_BL_FDT_ADDR))
		save_prev_bl_data();
	#endif

	/* Initialize from environment */
	image_load_addr = env_get_ulong("loadaddr", 16, image_load_addr);

	return 0;
}

static int initr_gpt(void)
{
	char *gptenv = NULL;
	struct blk_desc *blk_dev_desc = NULL;

	if(!is_emmc() || (blk_dev_desc = blk_get_dev("mmc", 0)) == NULL) 
	{
		printf("%s: mmc dev 0 NOT available\n",__func__);
		return 0;
	}

	/*1.try to repair*/
	gpt_repair_headers(blk_dev_desc);
	
	/*2.verify with new partitions env*/
	gptenv = env_get("partitions");
	if(0 == gpt_verify(blk_dev_desc, gptenv))
		return 0;
	
	printf("gpt info verify failed\n");
	/*3.generate new gpt using new partitions env*/
	if(gpt_default(blk_dev_desc, gptenv))
		printf("gpt info write failed\n");
	
	return 0;
}

static int initr_gpio(void)
{
	int ret;
	char *rst_gpio = NULL;
	char *tmp = NULL;
	char *token = NULL;
	unsigned int gpio;
	
	rst_gpio = env_get("rst_gpio");
	if(rst_gpio == NULL)
		return 0;
	
	tmp = (char *)malloc(strlen(rst_gpio) + 1);	
	if(tmp == NULL){
		return -1;
	}
	strncpy(tmp, rst_gpio, strlen(rst_gpio));
	
	token = strtok(tmp,",");
	
	while(token != NULL) {
		if(strcmp(token, "-1") == 0)
		{
			token = strtok(NULL,",");
			continue;
		}
			
		ret = gpio_lookup_name(token, NULL, NULL, &gpio);
		if (ret) {
			printf("initr_gpio: GPIO '%s' not found\n", token);
			token = strtok(NULL, ",");
			continue;
		}

		token = strtok(NULL, ",");
		ret = gpio_request(gpio, "cmd_gpio");
		if (ret && ret != -EBUSY) {
			printf("initr_gpio: requesting gpio %u failed\n",gpio);
			continue;
		}
		gpio_direction_output(gpio, 0);
		ret = gpio_get_value(gpio);
		printf("initr_gpio: value of gpio %u is %d\n", gpio,ret);

		udelay(200000);
	
		gpio_direction_output(gpio, 1);
		ret = gpio_get_value(gpio);
		printf("initr_gpio: value of gpio %u is %d\n", gpio,ret);

		gpio_free(gpio);
	}
	free(tmp);
	return 0;
}

#ifdef CONFIG_SYS_MALLOC_BOOTPARAMS
static int initr_malloc_bootparams(void)
{
	gd->bd->bi_boot_params = (ulong)malloc(CONFIG_SYS_BOOTPARAMS_LEN);
	if (!gd->bd->bi_boot_params) {
		puts("WARNING: Cannot allocate space for boot parameters\n");
		return -ENOMEM;
	}
	return 0;
}
#endif

#if defined(CONFIG_LED_STATUS)
static int initr_status_led(void)
{
#if defined(CONFIG_LED_STATUS_BOOT)
	status_led_set(CONFIG_LED_STATUS_BOOT, CONFIG_LED_STATUS_BLINKING);
#else
	status_led_init();
#endif
	return 0;
}
#endif

#if defined(CONFIG_SCSI) && !defined(CONFIG_DM_SCSI)
static int initr_scsi(void)
{
	puts("SCSI:  ");
	scsi_init();
	puts("\n");

	return 0;
}
#endif


#ifdef TCSUPPORT_BOARD_SELECT
static void get_board_id(RFB_ID_t* rfb_id)
{
	char efuse_read_cmd[30];
	uint8_t id1_remark,id2_remark;
	char value[256]={0};
	uint32_t efuse_value;

	snprintf(efuse_read_cmd, sizeof(efuse_read_cmd), "efuse READ %p 1", (void*)value);

	run_command(efuse_read_cmd, 0);

	efuse_value=((uint32_t*) value)[0];

	id1_remark=(efuse_value>>ID1_REMARK_LSB)&ID1_REMARK_MASK;
	id2_remark=(efuse_value>>ID2_REMARK_LSB)&ID2_REMARK_MASK;
	
	if(1==id1_remark){													/*check wether  id1_remark is 1 or not*/
		rfb_id->id1=(efuse_value>>REMARKD_ID1_LSB)&REMARKD_ID1_MASK;   /*if it is 1 then return remarkd_id1*/			
	}else{			
		rfb_id->id1=(efuse_value>>ID1_LSB)&ID1_MASK;					/*otherwise return id1*/
	}

	if(1==id2_remark){												/*check wether  id2_remark is 1 or not*/
		rfb_id->id2=(efuse_value>>REMARKD_ID2_LSB)&REMARKD_ID2_MASK; /*if it is 1 then return remarkd_id2*/			
	}else{
		rfb_id->id2=(efuse_value>>ID2_LSB)&ID2_MASK;				/*otherwise return id2*/
	}
}

/*set rfb_id1/id2 into uboot env*/
static void init_rfb_no(void)
{
	char buf[30];
	char rfb_id1[ID1_LEN+1], rfb_id2[ID2_LEN+1];
	RFB_ID_t rfb_id;
	char *val;
	char input_str[30];
	char *find_target;
	char *pos;
	char boot_str_buf[128];
	
	get_board_id(&rfb_id);

	snprintf(rfb_id1, sizeof(rfb_id1), "%x", rfb_id.id1);
	printf("%s=%s\n", BOOTARGS_RFB_ID1_STR, rfb_id1);
	env_set(BOOTARGS_RFB_ID1_STR, rfb_id1); 

	snprintf(rfb_id2, sizeof(rfb_id2), "%02x", rfb_id.id2);
	printf("%s=%s\n", BOOTARGS_RFB_ID2_STR, rfb_id2);
	env_set(BOOTARGS_RFB_ID2_STR, rfb_id2); 

	snprintf(buf, sizeof(buf), "rfb_%s%s", rfb_id1, rfb_id2);
	if(NULL==env_get(buf)){
		env_set("rfb_no", "rfb_defult"); 
	}else{
		env_set("rfb_no", buf);
	}
	/*printf("rfb_no=%s\n", env_get("rfb_no")); */

	snprintf(buf, sizeof(buf), "%s", env_get(env_get("rfb_no")));
	printf("rfb_cfg=%s\n", buf); 
	env_set("rfb_cfg", buf);

	val = env_get("bootcmd"); /*get bootcmd env param*/
	/*Check if 'val' is valid and 'rfb_id.id1' is greater than 0*/
	if(val && (rfb_id.id1 > 0)) {
		memset(input_str, 0, sizeof(input_str));
		memset(boot_str_buf, 0, sizeof(boot_str_buf));
		find_target = "conf";
		if(strstr(val, find_target) == NULL){ /*find the target = "conf" in bootcmd env param*/
			find_target = "0x81800000";
			pos = strstr(val, find_target); /*find the target = "0x81800000" in bootcmd env param, and get the position the header of the target*/
			if(pos){
				snprintf(input_str, sizeof(input_str), "%s%d;", "#conf-", (rfb_id.id1)); /*Construct input string as "#conf-<id>;"*/
			} else{
				find_target = "bootm";
				pos = strstr(val, find_target); /*find the target = "bootm" in bootcmd env param, and get the position the header of the target*/
				if(pos){
					snprintf(input_str, sizeof(input_str), " %s%d;", "$loadaddr#conf-", (rfb_id.id1)); /*Construct input string as " $loadaddr#conf-<id>;"*/
				}else{ /*cannot find the target = "bootm", position = NULL*/
					snprintf(input_str, sizeof(input_str), "%s%d;", ";bootm $loadaddr#conf-", (rfb_id.id1)); /*Construct input string as ";bootm $loadaddr#conf-<id>;"*/
					snprintf(boot_str_buf, sizeof(boot_str_buf), "%s%s", val, input_str); /*Concatenate bootcmd environment parameter and input string*/
					env_set("bootcmd", boot_str_buf);
					return;
				}
			}
			/*e.g. current bootcmd env param, bootcmd=xxxx;bootm oooo*/
			strncpy(boot_str_buf, val, (pos - val) + strlen(find_target)); /*Extract part of 'val' preceding the matched target, e.g. xxxx;bootm*/
			strcpy(boot_str_buf + (pos - val) + strlen(find_target), input_str); /*Append input string to extracted part, e.g. xxxx;bootm $loadaddr#conf-3;*/
			strcpy(boot_str_buf + (pos - val) + strlen(find_target)+strlen(input_str), pos + strlen(find_target)); /*Append remainder of 'val' starting from the end of the matched target, e.g. xxxx;bootm $loadaddr#conf-3;oooo*/

			env_set("bootcmd", boot_str_buf);
		}
	}
}

static int init_serdes_env(void)
{
	char* serdes_bootargs[NUM_SERDES_ARGS] = {BOOTARGS_PON, BOOTARGS_ETH, BOOTARGS_WIFI1, BOOTARGS_WIFI2, BOOTARGS_USB1};
	int i=0;
	char buf[30];
	char *token_str, *rfb_cfg;
	const char* delim=",";

	snprintf(buf, sizeof(buf), "%s", env_get("rfb_cfg"));
	rfb_cfg=buf;

	for(i=0;i<NUM_SERDES_ARGS;i++){
		token_str=strsep(&rfb_cfg, delim);
		if(NULL==token_str){
			return -1;
		}
		printf("%s=%s\n",serdes_bootargs[i],token_str);
		env_set(serdes_bootargs[i], token_str); 
	}
	return 0;

}

static int init_arht_rfb(void)
{
	int ret = 0;

	/*initialize rfb env*/
	init_rfb_no();
	
	/*initialize serdes env*/
	ret=init_serdes_env();

	return ret;
}
#endif


#ifdef CONFIG_CMD_NET
static int initr_net(void)
{
	puts("Net:   ");
	eth_initialize();
#if defined(CONFIG_RESET_PHY_R)
	debug("Reset Ethernet PHY\n");
	reset_phy();
#endif
	return 0;
}
#endif

#ifdef CONFIG_POST
static int initr_post(void)
{
	post_run(NULL, POST_RAM | post_bootmode_get(0));
	return 0;
}
#endif

#if defined(CONFIG_IDE) && !defined(CONFIG_BLK)
static int initr_ide(void)
{
	puts("IDE:   ");
#if defined(CONFIG_START_IDE)
	if (board_start_ide())
		ide_init();
#else
	ide_init();
#endif
	return 0;
}
#endif

#if defined(CFG_PRAM)
/*
 * Export available size of memory for Linux, taking into account the
 * protected RAM at top of memory
 */
int initr_mem(void)
{
	ulong pram = 0;
	char memsz[32];

	pram = env_get_ulong("pram", 10, CFG_PRAM);
	sprintf(memsz, "%ldk", (long int)((gd->ram_size / 1024) - pram));
	env_set("mem", memsz);

	return 0;
}
#endif

static int dm_announce(void)
{
	int device_count;
	int uclass_count;

	if (IS_ENABLED(CONFIG_DM)) {
		dm_get_stats(&device_count, &uclass_count);
		printf("Core:  %d devices, %d uclasses", device_count,
		       uclass_count);
		if (CONFIG_IS_ENABLED(OF_REAL))
			printf(", devicetree: %s", fdtdec_get_srcname());
		printf("\n");
		if (IS_ENABLED(CONFIG_OF_HAS_PRIOR_STAGE) &&
		    (gd->fdt_src == FDTSRC_SEPARATE ||
		     gd->fdt_src == FDTSRC_EMBED)) {
			printf("Warning: Unexpected devicetree source (not from a prior stage)");
			printf("Warning: U-Boot may not function properly\n");
		}
	}

	return 0;
}

static int run_main_loop(void)
{
#ifdef CONFIG_SANDBOX
	sandbox_main_loop_init();
#endif

	event_notify_null(EVT_MAIN_LOOP);

	/* main_loop() can return to retry autoboot, if so just run it again */
	for (;;)
		main_loop();
	return 0;
}

/*
 * We hope to remove most of the driver-related init and do it if/when
 * the driver is later used.
 *
 * TODO: perhaps reset the watchdog in the initcall function after each call?
 */
static init_fnc_t init_sequence_r[] = {
	initr_trace,
	initr_reloc,
	event_init,
	/* TODO: could x86/PPC have this also perhaps? */
#if defined(CONFIG_ARM) || defined(CONFIG_RISCV)
	initr_caches,
	/* Note: For Freescale LS2 SoCs, new MMU table is created in DDR.
	 *	 A temporary mapping of IFC high region is since removed,
	 *	 so environmental variables in NOR flash is not available
	 *	 until board_init() is called below to remap IFC to high
	 *	 region.
	 */
#endif
	initr_reloc_global_data,
#if IS_ENABLED(CONFIG_NEEDS_MANUAL_RELOC) && CONFIG_IS_ENABLED(EVENT)
	event_manual_reloc,
#endif
#if defined(CONFIG_SYS_INIT_RAM_LOCK) && defined(CONFIG_E500)
	initr_unlock_ram_in_cache,
#endif
	initr_barrier,
	initr_malloc,
	log_init,
	initr_bootstage,	/* Needs malloc() but has its own timer */
#if defined(CONFIG_CONSOLE_RECORD)
	console_record_init,
#endif
#ifdef CONFIG_SYS_NONCACHED_MEMORY
	noncached_init,
#endif
	initr_of_live,
#ifdef CONFIG_DM
	initr_dm,
#endif
#ifdef CONFIG_ADDR_MAP
	init_addr_map,
#endif
#if defined(CONFIG_ARM) || defined(CONFIG_RISCV) || defined(CONFIG_SANDBOX)
	board_init,	/* Setup chipselects */
#endif
	/*
	 * TODO: printing of the clock inforamtion of the board is now
	 * implemented as part of bdinfo command. Currently only support for
	 * davinci SOC's is added. Remove this check once all the board
	 * implement this.
	 */
#ifdef CONFIG_CLOCKS
	set_cpu_clk_info, /* Setup clock information */
#endif
#ifdef CONFIG_EFI_LOADER
	efi_memory_init,
#endif
	initr_binman,
#ifdef CONFIG_FSP_VERSION2
	arch_fsp_init_r,
#endif
	initr_dm_devices,
	stdio_init_tables,
	serial_initialize,
	initr_announce,
	dm_announce,
#if CONFIG_IS_ENABLED(WDT)
	initr_watchdog,
#endif
	INIT_FUNC_WATCHDOG_RESET
#if defined(CONFIG_NEEDS_MANUAL_RELOC) && defined(CONFIG_BLOCK_CACHE)
	blkcache_init,
#endif
#ifdef CONFIG_NEEDS_MANUAL_RELOC
	initr_manual_reloc_cmdtable,
#endif
	arch_initr_trap,
#if defined(CONFIG_BOARD_EARLY_INIT_R)
	board_early_init_r,
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_POST
	post_output_backlog,
#endif
	INIT_FUNC_WATCHDOG_RESET
#if defined(CONFIG_PCI_INIT_R) && defined(CONFIG_SYS_EARLY_PCI_INIT)
	/*
	 * Do early PCI configuration _before_ the flash gets initialised,
	 * because PCU resources are crucial for flash access on some boards.
	 */
	pci_init,
#endif
#ifdef CONFIG_ARCH_EARLY_INIT_R
	arch_early_init_r,
#endif
	power_init_board,
#ifdef CONFIG_MTD_NOR_FLASH
	initr_flash,
#endif
	INIT_FUNC_WATCHDOG_RESET
#if defined(CONFIG_PPC) || defined(CONFIG_M68K) || defined(CONFIG_X86)
	/* initialize higher level parts of CPU like time base and timers */
	cpu_init_r,
#endif
#ifdef CONFIG_EFI_LOADER
	efi_init_early,
#endif
#ifdef CONFIG_CMD_NAND
	initr_nand,
#endif
#ifdef CONFIG_CMD_ONENAND
	initr_onenand,
#endif
#ifdef CONFIG_MMC
	initr_mmc,
#endif
#ifdef CONFIG_XEN
	xen_init,
#endif
#ifdef CONFIG_PVBLOCK
	initr_pvblock,
#endif
	initr_env,
#ifdef CONFIG_MMC
	initr_gpt,
#endif
	initr_gpio,
#ifdef CONFIG_SYS_MALLOC_BOOTPARAMS
	initr_malloc_bootparams,
#endif
	INIT_FUNC_WATCHDOG_RESET
	cpu_secondary_init_r,
#if defined(CONFIG_ID_EEPROM)
	mac_read_from_eeprom,
#endif
	INIT_FUNC_WATCHDOG_RESET
#if defined(CONFIG_PCI_INIT_R) && !defined(CONFIG_SYS_EARLY_PCI_INIT)
	/*
	 * Do pci configuration
	 */
	pci_init,
#endif
	stdio_add_devices,
	jumptable_init,
#ifdef CONFIG_API
	api_init,
#endif
	console_init_r,		/* fully init console as a device */
#ifdef CONFIG_DISPLAY_BOARDINFO_LATE
	console_announce_r,
	show_board_info,
#endif
#ifdef CONFIG_ARCH_MISC_INIT
	arch_misc_init,		/* miscellaneous arch-dependent init */
#endif
#ifdef CONFIG_MISC_INIT_R
	misc_init_r,		/* miscellaneous platform-dependent init */
#endif
	INIT_FUNC_WATCHDOG_RESET
#ifdef CONFIG_CMD_KGDB
	kgdb_init,
#endif
	interrupt_init,
#if defined(CONFIG_MICROBLAZE) || defined(CONFIG_M68K)
	timer_init,		/* initialize timer */
#endif
#if defined(CONFIG_LED_STATUS)
	initr_status_led,
#endif
	/* PPC has a udelay(20) here dating from 2002. Why? */
#ifdef CONFIG_BOARD_LATE_INIT
	board_late_init,
#endif
#if defined(CONFIG_SCSI) && !defined(CONFIG_DM_SCSI)
	INIT_FUNC_WATCHDOG_RESET
	initr_scsi,
#endif
#ifdef CONFIG_BITBANGMII
	bb_miiphy_init,
#endif
#ifdef CONFIG_PCI_ENDPOINT
	pci_ep_init,
#endif
#ifdef TCSUPPORT_BOARD_SELECT
	init_arht_rfb,
#endif
#ifdef CONFIG_CMD_NET
	INIT_FUNC_WATCHDOG_RESET
	initr_net,
#endif
#ifdef CONFIG_POST
	initr_post,
#endif
#if defined(CONFIG_IDE) && !defined(CONFIG_BLK)
	initr_ide,
#endif
#ifdef CONFIG_LAST_STAGE_INIT
	INIT_FUNC_WATCHDOG_RESET
	/*
	 * Some parts can be only initialized if all others (like
	 * Interrupts) are up and running (i.e. the PC-style ISA
	 * keyboard).
	 */
	last_stage_init,
#endif
#if defined(CFG_PRAM)
	initr_mem,
#endif
	run_main_loop,
};

void board_init_r(gd_t *new_gd, ulong dest_addr)
{
	/*
	 * The pre-relocation drivers may be using memory that has now gone
	 * away. Mark serial as unavailable - this will fall back to the debug
	 * UART if available.
	 *
	 * Do the same with log drivers since the memory may not be available.
	 */
	gd->flags &= ~(GD_FLG_SERIAL_READY | GD_FLG_LOG_READY);

	/*
	 * Set up the new global data pointer. So far only x86 does this
	 * here.
	 * TODO(sjg@chromium.org): Consider doing this for all archs, or
	 * dropping the new_gd parameter.
	 */
	if (CONFIG_IS_ENABLED(X86_64) && !IS_ENABLED(CONFIG_EFI_APP))
		arch_setup_gd(new_gd);

#if !defined(CONFIG_X86) && !defined(CONFIG_ARM) && !defined(CONFIG_ARM64)
	gd = new_gd;
#endif
	gd->flags &= ~GD_FLG_LOG_READY;

	if (IS_ENABLED(CONFIG_NEEDS_MANUAL_RELOC)) {
		for (int i = 0; i < ARRAY_SIZE(init_sequence_r); i++)
			MANUAL_RELOC(init_sequence_r[i]);
	}

	if (initcall_run_list(init_sequence_r))
		hang();

	/* NOTREACHED - run_main_loop() does not return */
	hang();
}
