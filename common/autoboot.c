// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */
#include <common.h>
#include <autoboot.h>
#include <bootretry.h>
#include <cli.h>
#include <command.h>
#include <console.h>
#include <env.h>
#include <fdtdec.h>
#include <hash.h>
#include <log.h>
#include <malloc.h>
#include <memalign.h>
#include <menu.h>
#include <post.h>
#include <time.h>
#include <asm/global_data.h>
#include <linux/delay.h>
#include <u-boot/sha256.h>
#include <bootcount.h>
#include <crypt.h>
#include <dm/ofnode.h>

#if CONFIG_IS_ENABLED(UBOOT_ARHT)
#include<airoha/arhtglobal.h>

#include <asm/tc3162.h>
#include <asm/io.h>
#include <common.h>
#include <common.h>
#include <command.h>
#include <blk.h>
#include <image.h>
#include <malloc.h>
#include <linux/ctype.h>
#include <asm/io.h>
#include <linux/libfdt.h>
#include <linux/mtd/mtd.h>
#include <mmc.h>
#include <ecnt_flash.h>
#include <ecnt_image.h>
#include <airoha/trx.h>


#ifdef TCSUPPORT_TCBOOT_1MB_SIZE
#define LOGIN_INFO_OFFSET		(0xfbf80)
#else
#define LOGIN_INFO_OFFSET		(0x7be70)
#endif
#define MEMORY_BASE_ADDRESS		(0x81800000)
#define USERNAME_ADDRESS		(MEMORY_BASE_ADDRESS)
#define PASSWORD_ADDRESS		(USERNAME_ADDRESS + LINE_LEN)
#define LOGIN_INFO_ADDRESS		(PASSWORD_ADDRESS + LINE_LEN)

/* Need to same setting with compile time */
#define PBKDF_ITERATION_TIME	(30000)
#define PBKDF_KEY_LENGTH		(64)
#define PBKDF_HASH_ALGO			(512)

#define LINE_LEN		      128
#define CMD                     0   /*flag for cmd_gets() input cmd*/
#define PWD                     1   /*flag for cmd_gets() input passwd*/
#define USERNAME_PASSWD_LEN	   16   /*length of username or passwd*/
#define NULL    0
int startmulticast = 1; 
int multicastupgrade_started = 0 ;
int multicastupgrade_fail =0;
#endif
DECLARE_GLOBAL_DATA_PTR;
#define DELAY_STOP_STR_MAX_LENGTH 64
#ifndef DEBUG_BOOTKEYS
#define DEBUG_BOOTKEYS 0
#endif
#define debug_bootkeys(fmt, args...)		\
	debug_cond(DEBUG_BOOTKEYS, fmt, ##args)
/* Stored value of bootdelay, used by autoboot_command() */
static int stored_bootdelay;
static int menukey;
#if defined(CONFIG_AUTOBOOT_STOP_STR_CRYPT)
#define AUTOBOOT_STOP_STR_CRYPT	CONFIG_AUTOBOOT_STOP_STR_CRYPT
#else
#define AUTOBOOT_STOP_STR_CRYPT	""
#endif
#if defined(CONFIG_AUTOBOOT_STOP_STR_SHA256)
#define AUTOBOOT_STOP_STR_SHA256	CONFIG_AUTOBOOT_STOP_STR_SHA256
#else
#define AUTOBOOT_STOP_STR_SHA256	""
#endif
#ifdef CONFIG_AUTOBOOT_USE_MENUKEY
#define AUTOBOOT_MENUKEY CONFIG_AUTOBOOT_MENUKEY
#else
#define AUTOBOOT_MENUKEY 0
#endif
/**
 * passwd_abort_crypt() - check for a crypt-style hashed key sequence to abort booting
 *
 * This checks for the user entering a password within a given time.
 *
 * The entered password is hashed via one of the crypt-style hash methods
 * and compared to the pre-defined value from either
 *   the environment variable "bootstopkeycrypt"
 * or
 *   the config value CONFIG_AUTOBOOT_STOP_STR_CRYPT
 *
 * In case the config value CONFIG_AUTOBOOT_NEVER_TIMEOUT has been enabled
 * this function never times out if the user presses the <Enter> key
 * before starting to enter the password.
 *
 * @etime: Timeout value ticks (stop when get_ticks() reachs this)
 * Return: 0 if autoboot should continue, 1 if it should stop
 */
static int passwd_abort_crypt(uint64_t etime)
{
	const char *crypt_env_str = env_get("bootstopkeycrypt");
	char presskey[DELAY_STOP_STR_MAX_LENGTH];
	u_int presskey_len = 0;
	int abort = 0;
	int never_timeout = 0;
	int err;
	if (IS_ENABLED(CONFIG_AUTOBOOT_STOP_STR_ENABLE) && !crypt_env_str)
		crypt_env_str = AUTOBOOT_STOP_STR_CRYPT;
	if (!crypt_env_str)
		return 0;
	/* We expect the stop-string to be newline-terminated */
	do {
		if (tstc()) {
			/* Check for input string overflow */
			if (presskey_len >= sizeof(presskey))
				return 0;
			presskey[presskey_len] = getchar();
			if ((presskey[presskey_len] == '\r') ||
			    (presskey[presskey_len] == '\n')) {
				if (IS_ENABLED(CONFIG_AUTOBOOT_NEVER_TIMEOUT) &&
				    !presskey_len) {
					never_timeout = 1;
					continue;
				}
				presskey[presskey_len] = '\0';
				err = crypt_compare(crypt_env_str, presskey,
						    &abort);
				if (err)
					debug_bootkeys(
						"crypt_compare() failed with: %s\n",
						errno_str(err));
				/* you had one chance */
				break;
			} else {
				presskey_len++;
			}
		}
		udelay(10000);
	} while (never_timeout || get_ticks() <= etime);
	return abort;
}
/*
 * Use a "constant-length" time compare function for this
 * hash compare:
 *
 * https://crackstation.net/hashing-security.htm
 */
static int slow_equals(u8 *a, u8 *b, int len)
{
	int diff = 0;
	int i;
	for (i = 0; i < len; i++)
		diff |= a[i] ^ b[i];
	return diff == 0;
}
/**
 * passwd_abort_sha256() - check for a hashed key sequence to abort booting
 *
 * This checks for the user entering a SHA256 hash within a given time.
 *
 * @etime: Timeout value ticks (stop when get_ticks() reachs this)
 * Return: 0 if autoboot should continue, 1 if it should stop
 */
static int passwd_abort_sha256(uint64_t etime)
{
	const char *sha_env_str = env_get("bootstopkeysha256");
	u8 sha_env[SHA256_SUM_LEN];
	u8 *sha;
	char *presskey;
	char *c;
	const char *algo_name = "sha256";
	u_int presskey_len = 0;
	int abort = 0;
	int size = sizeof(sha);
	int ret;
	if (sha_env_str == NULL)
		sha_env_str = AUTOBOOT_STOP_STR_SHA256;
	presskey = malloc_cache_aligned(DELAY_STOP_STR_MAX_LENGTH);
	c = strstr(sha_env_str, ":");
	if (c && (c - sha_env_str < DELAY_STOP_STR_MAX_LENGTH)) {
		/* preload presskey with salt */
		memcpy(presskey, sha_env_str, c - sha_env_str);
		presskey_len = c - sha_env_str;
		sha_env_str = c + 1;
	}
	/*
	 * Generate the binary value from the environment hash value
	 * so that we can compare this value with the computed hash
	 * from the user input
	 */
	ret = hash_parse_string(algo_name, sha_env_str, sha_env);
	if (ret) {
		printf("Hash %s not supported!\n", algo_name);
		return 0;
	}
	sha = malloc_cache_aligned(SHA256_SUM_LEN);
	size = SHA256_SUM_LEN;
	/*
	 * We don't know how long the stop-string is, so we need to
	 * generate the sha256 hash upon each input character and
	 * compare the value with the one saved in the environment
	 */
	do {
		if (tstc()) {
			/* Check for input string overflow */
			if (presskey_len >= DELAY_STOP_STR_MAX_LENGTH) {
				free(presskey);
				free(sha);
				return 0;
			}
			presskey[presskey_len++] = getchar();
			/* Calculate sha256 upon each new char */
			hash_block(algo_name, (const void *)presskey,
				   presskey_len, sha, &size);
			/* And check if sha matches saved value in env */
			if (slow_equals(sha, sha_env, SHA256_SUM_LEN))
				abort = 1;
		}
		udelay(10000);
	} while (!abort && get_ticks() <= etime);
	free(presskey);
	free(sha);
	return abort;
}
/**
 * passwd_abort_key() - check for a key sequence to aborted booting
 *
 * This checks for the user entering a string within a given time.
 *
 * @etime: Timeout value ticks (stop when get_ticks() reachs this)
 * Return: 0 if autoboot should continue, 1 if it should stop
 */
static int passwd_abort_key(uint64_t etime)
{
	int abort = 0;
	struct {
		char *str;
		u_int len;
		int retry;
	}
	delaykey[] = {
		{ .str = env_get("bootdelaykey"),  .retry = 1 },
		{ .str = env_get("bootstopkey"),   .retry = 0 },
	};
	char presskey[DELAY_STOP_STR_MAX_LENGTH];
	int presskey_len = 0;
	int presskey_max = 0;
	int i;
#  ifdef CONFIG_AUTOBOOT_DELAY_STR
	if (delaykey[0].str == NULL)
		delaykey[0].str = CONFIG_AUTOBOOT_DELAY_STR;
#  endif
#  ifdef CONFIG_AUTOBOOT_STOP_STR
	if (delaykey[1].str == NULL)
		delaykey[1].str = CONFIG_AUTOBOOT_STOP_STR;
#  endif
	for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i++) {
		delaykey[i].len = delaykey[i].str == NULL ?
				    0 : strlen(delaykey[i].str);
		delaykey[i].len = delaykey[i].len > DELAY_STOP_STR_MAX_LENGTH ?
				    DELAY_STOP_STR_MAX_LENGTH : delaykey[i].len;
		presskey_max = presskey_max > delaykey[i].len ?
				    presskey_max : delaykey[i].len;
		debug_bootkeys("%s key:<%s>\n",
			       delaykey[i].retry ? "delay" : "stop",
			       delaykey[i].str ? delaykey[i].str : "NULL");
	}
	/* In order to keep up with incoming data, check timeout only
	 * when catch up.
	 */
	do {
		if (tstc()) {
			if (presskey_len < presskey_max) {
				presskey[presskey_len++] = getchar();
			} else {
				for (i = 0; i < presskey_max - 1; i++)
					presskey[i] = presskey[i + 1];
				presskey[i] = getchar();
			}
		}
		for (i = 0; i < sizeof(delaykey) / sizeof(delaykey[0]); i++) {
			if (delaykey[i].len > 0 &&
			    presskey_len >= delaykey[i].len &&
				memcmp(presskey + presskey_len -
					delaykey[i].len, delaykey[i].str,
					delaykey[i].len) == 0) {
					debug_bootkeys("got %skey\n",
						delaykey[i].retry ? "delay" :
						"stop");
				/* don't retry auto boot */
				if (!delaykey[i].retry)
					bootretry_dont_retry();
				abort = 1;
			}
		}
		udelay(10000);
	} while (!abort && get_ticks() <= etime);
	return abort;
}
/**
 * flush_stdin() - drops all pending characters from stdin
 */
static void flush_stdin(void)
{
	while (tstc())
		(void)getchar();
}
/**
 * fallback_to_sha256() - check whether we should fall back to sha256
 *                        password checking
 *
 * This checks for the environment variable `bootstopusesha256` in case
 * sha256-fallback has been enabled via the config setting
 * `AUTOBOOT_SHA256_FALLBACK`.
 *
 * Return: `false` if we must not fall-back, `true` if plain sha256 should be tried
 */
static bool fallback_to_sha256(void)
{
	if (IS_ENABLED(CONFIG_AUTOBOOT_SHA256_FALLBACK))
		return env_get_yesno("bootstopusesha256") == 1;
	else if (IS_ENABLED(CONFIG_CRYPT_PW))
		return false;
	else
		return true;
}
/***************************************************************************
 * Watch for 'delay' seconds for autoboot stop or autoboot delay string.
 * returns: 0 -  no key string, allow autoboot 1 - got key string, abort
 */
static int abortboot_key_sequence(int bootdelay)
{
	int abort;
	uint64_t etime = endtick(bootdelay);
	if (IS_ENABLED(CONFIG_AUTOBOOT_FLUSH_STDIN))
		flush_stdin();
#  ifdef CONFIG_AUTOBOOT_PROMPT
	/*
	 * CONFIG_AUTOBOOT_PROMPT includes the %d for all boards.
	 * To print the bootdelay value upon bootup.
	 */
	printf(CONFIG_AUTOBOOT_PROMPT, bootdelay);
#  endif
	if (IS_ENABLED(CONFIG_AUTOBOOT_ENCRYPTION)) {
		if (IS_ENABLED(CONFIG_CRYPT_PW) && !fallback_to_sha256())
			abort = passwd_abort_crypt(etime);
		else
			abort = passwd_abort_sha256(etime);
	} else {
		abort = passwd_abort_key(etime);
	}
	if (!abort)
		debug_bootkeys("key timeout\n");
	return abort;
}
#if CONFIG_IS_ENABLED(UBOOT_ARHT)
static void multiupgrade_check()
{	
		if(eth_rx()<0)
		{
			printf("Eth_receive error.\n");
		}
	return;
}
static char *cmd_gets(char *buf, int len, int flag)
{
#define KEY_BS			0x08
#define KEY_CR			0x0D
	int c,i=0;
	char *cp;
	struct udevice *dev;
	cp = buf;
	while ((c = arht_serial_getc(dev)) != KEY_CR)
	{
		if ( c == KEY_BS ) 
		{
			if ( cp != buf ) 
			{
				printf("\b \b");
				cp--;
				i--;
			}
		} 
		else
		{
			if ( buf != NULL ) 
			{
				if ( i <= len-1 )			
				{
					if ( flag == 0x0 )
						arht_serial_putc(dev,c);
					else 
						arht_serial_putc(dev,'*');
					*cp++ = c;
					i++;
				}
			}
		}
	}
	if (buf != NULL)
		*cp = '\0';
	return buf;
}
static int trim(char *buf)
{
	int i,j;
	int flag_i=1, flag_j=1;
	for (i=0,j=strlen(buf)-1; i<=j; i++,j--)
	{
		if (flag_i)
		{	
			if((buf[i] != ' ') && (buf[i] != '\t'))
			flag_i = 0;
		}
		if (flag_j)
		{
			if((buf[j] == ' ') || (buf[j] == '\t'))
				buf[j] = 0;
			else
				flag_j = 0;
		}
		if ((flag_i == 0) && (flag_j == 0))
			break;
	}
	return i;
}

static int pbkdf2_password_setting (unsigned int iter_time, unsigned int key_length, unsigned int hash_algo)
{
	unsigned long r0 = 0, r1 = 0, r2 = 0, r3 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	r0 = 0x82000004;
	r1 = 0x464E4353;			/* SCNF */
	r2 = iter_time;
	r3 = ((hash_algo << 16) | key_length);

#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	return res.a0;
#else
	do_smc(r0, r1, r2, r3);
	return 0;
#endif
}

static int pbkdf2_password_compare (void)
{
	unsigned long r0 = 0, r1 = 0, r2 = 0, r3 = 0;
#ifdef TCSUPPORT_UBOOT_64BIT
	struct arm_smccc_res res;
#endif

	r0 = 0x82000004;
	r1 = 0x504D4350;			/* PCMP */
	r2 = MEMORY_BASE_ADDRESS;
	r3 = LINE_LEN;

#ifdef TCSUPPORT_UBOOT_64BIT
	__arm_smccc_smc(r0, r1, r2, r3, 0, 0, 0 ,0, &res,0);
	return res.a0;
#else
	return do_smc(r0, r1, r2, r3);
#endif
}

static int setup_mtd_device(struct mtd_info **mtd, const char* mtd_dev)
{
	struct mtd_info *mtd_info;

	mtd_probe_devices();

	mtd_info = get_mtd_device_nm(mtd_dev);
	if (IS_ERR_OR_NULL(mtd_info)) {
		printf("MTD device %s not found, ret %ld\n", mtd_dev,
		       PTR_ERR(mtd_info));
		return -1;
	}
	*mtd = mtd_info;

	return 0;
}

int ecnt_abortboot_keyed(int bootdelay)
{
	unsigned char *UserName = (unsigned char *)(USERNAME_ADDRESS);
	unsigned char *Pwd = (unsigned char *)(PASSWORD_ADDRESS);
	unsigned char *Login_info = (unsigned char *)(LOGIN_INFO_ADDRESS);

	int abort = 0;
	int i;
#ifdef TCSUPPORT_AUTOBENCH
	/* always abort login username/passwd */
	return 1;
#endif

	memset (Login_info, 0, LINE_LEN);

	if (is_emmc())
	{
		struct mmc *mmc;
		u32 blk, cnt, n;

		mmc = __init_mmc_device(0, false, MMC_MODES_END);
		if (!mmc) {
			/*
			 * There is nowhere to read the login info from. Fall back to
			 * the standard behaviour and honour the key press instead of
			 * silently continuing the autoboot.
			 */
			printf("No login info available on mmc\n");
			return 1;
		}

		blk = (LOGIN_INFO_OFFSET) / mmc->read_bl_len;
		cnt = (LOGIN_INFO_OFFSET + PBKDF_KEY_LENGTH) / mmc->read_bl_len;
		if(((LOGIN_INFO_OFFSET + PBKDF_KEY_LENGTH) % mmc->read_bl_len) != 0)
		{
			cnt++;
		}
		cnt -= blk;
		n = blk_dread(mmc_get_blk_desc(mmc), blk, cnt, Login_info);

		memmove ((const void *)Login_info, (const void *)(Login_info + LOGIN_INFO_OFFSET - blk*mmc->read_bl_len), PBKDF_KEY_LENGTH);
		if (n != cnt)
		{
			printf("MMC read failed\n");
			return 1;
		}
	}
	else
	{
		struct mtd_info *mtd_bootloader;
		unsigned long ret, retlen;

		ret = setup_mtd_device(&mtd_bootloader, "bootloader");
		if (ret) {
			/*
			 * Only the tclinux layout provides a "bootloader" partition.
			 * Without a credential store there is no password we could
			 * compare against, so behave like plain U-Boot and honour the
			 * key press.
			 */
			printf("No login info available (%s partition not found)\n",
			       "bootloader");
			return 1;
		}

		ret = mtd_read(mtd_bootloader, LOGIN_INFO_OFFSET, PBKDF_KEY_LENGTH, &retlen, Login_info);
		if (ret) {
			printf("Failed to load the login info from %s.\r\n",
			       "bootloader");

			return 1;
		}
	}

    do {
        memset(UserName, 0, LINE_LEN);
        memset(Pwd, 0, LINE_LEN);
        printf("UserName: ");
        cmd_gets(UserName, LINE_LEN, CMD);
        printf("\n");
        printf("Password: ");
        cmd_gets(Pwd, LINE_LEN, PWD);
        i = trim(UserName);
        printf("\n\n");

        pbkdf2_password_setting (PBKDF_ITERATION_TIME, PBKDF_KEY_LENGTH, PBKDF_HASH_ALGO);
        if (pbkdf2_password_compare() == 0)
        {
	        abort = 1;
	        break;
        }
    } while (!abort);	
	return abort;
}
#endif


bool global_multicast_lock = false; 
static int abortboot_single_key(int bootdelay)
{
	int abort = 0;
	unsigned long ts;
#if CONFIG_IS_ENABLED(UBOOT_ARHT)
	/* ethernet is up and may be polled for a multicast upgrade */
	int net_up = 0;
#endif
	printf("Hit any key to stop autoboot: %2d ", bootdelay);
	/*
	 * Check if key already pressed
	 */
	global_multicast_lock = true; 
	 
	if (tstc()) {	/* we got a key press	*/
		getchar();	/* consume input	*/
		puts("\b\b\b 0");
		abort = 1;	/* don't auto boot	*/
	}

#if CONFIG_IS_ENABLED(UBOOT_ARHT)
	/*
	 * The network is only needed to poll for a multicast upgrade, so
	 * bring it up after checking for a key press: ethernet init may
	 * take a while and must neither delay nor skip the abort key, and
	 * it must never be fatal.
	 */
	net_up = (!abort && eth_init() == 0);
	if (!net_up)
		debug("%s: ethernet is not available\n", __func__);
#endif
	debug("getC before bootdelay(%d)?? abort=%d\n", bootdelay, abort);
	while ((!abort)) {
#if CONFIG_IS_ENABLED(UBOOT_ARHT)		
		if (bootdelay >= 0)--bootdelay;
#else
	while ((bootdelay > 0) && (!abort)) {
		--bootdelay;
    }
#endif
		/* delay 1000 ms */
		
#if CONFIG_IS_ENABLED(UBOOT_ARHT) 
		if(!multicastupgrade_started){
			ts = get_timer(0);
		}
		if (net_up)
			multiupgrade_check();
		if(multicastupgrade_started && multicastupgrade_finished) 
			break; 
#else
		ts = get_timer(0);
#endif
		do {
			if (tstc()) {	/* we got a key press	*/
				int key;
				abort  = 1;	/* don't auto boot	*/
				bootdelay = 0;	/* no more delay	*/
				key = getchar();/* consume input	*/
				if (IS_ENABLED(CONFIG_AUTOBOOT_USE_MENUKEY))
					menukey = key;
				break;
			}
#if CONFIG_IS_ENABLED(UBOOT_ARHT) 
			if (bootdelay > 0) udelay(1000);
#else
			udelay(10000);
#endif
		} while (!abort && get_timer(ts) < 1000);
		
#if CONFIG_IS_ENABLED(UBOOT_ARHT) 		
		if (bootdelay > 0)
		{	printf("\b\b\b%2d ", bootdelay);}
		if(multicastupgrade_fail) break;
		if(!multicastupgrade_started && bootdelay < 0) break;
#else
			printf("\b\b\b%2d ", bootdelay);
#endif
	}
	putc('\n');
	
	global_multicast_lock = false; 

#if CONFIG_IS_ENABLED(UBOOT_ARHT)
	if(abort)
	{
		debug("ecnt_abortboot_keyed()\n");
		abort = ecnt_abortboot_keyed(bootdelay);
		debug("ecnt_abortboot_keyed() ret = %d\n", abort);
		return abort;
	}
#endif

	return abort;
}
static int abortboot(int bootdelay)
{
	int abort = 0;
	if (bootdelay >= 0) {
		if (autoboot_keyed())
			abort = abortboot_key_sequence(bootdelay);
		else
			abort = abortboot_single_key(bootdelay);
	}
	if (IS_ENABLED(CONFIG_SILENT_CONSOLE) && abort)
		gd->flags &= ~GD_FLG_SILENT;
	return abort;
}
static void process_fdt_options(void)
{
#ifdef CONFIG_TEXT_BASE
	ulong addr;
	/* Add an env variable to point to a kernel payload, if available */
	addr = ofnode_conf_read_int("kernel-offset", 0);
	if (addr)
		env_set_addr("kernaddr", (void *)(CONFIG_TEXT_BASE + addr));
	/* Add an env variable to point to a root disk, if available */
	addr = ofnode_conf_read_int("rootdisk-offset", 0);
	if (addr)
		env_set_addr("rootaddr", (void *)(CONFIG_TEXT_BASE + addr));
#endif /* CONFIG_TEXT_BASE */
}
const char *bootdelay_process(void)
{
	char *s;
	int bootdelay;
	bootcount_inc();
	s = env_get("bootdelay");
	bootdelay = s ? (int)simple_strtol(s, NULL, 10) : CONFIG_BOOTDELAY;
	/*
	 * Does it really make sense that the devicetree overrides the user
	 * setting? It is possibly helpful for security since the device tree
	 * may be signed whereas the environment is often loaded from storage.
	 */
	if (IS_ENABLED(CONFIG_OF_CONTROL))
		bootdelay = ofnode_conf_read_int("bootdelay", bootdelay);
	debug("### main_loop entered: bootdelay=%d\n\n", bootdelay);
	if (IS_ENABLED(CONFIG_AUTOBOOT_MENU_SHOW))
		bootdelay = menu_show(bootdelay);
	bootretry_init_cmd_timeout();
#ifdef CONFIG_POST
	if (gd->flags & GD_FLG_POSTFAIL) {
		s = env_get("failbootcmd");
	} else
#endif /* CONFIG_POST */
	if (bootcount_error())
		s = env_get("altbootcmd");
	else
		s = env_get("bootcmd");
	if (IS_ENABLED(CONFIG_OF_CONTROL))
		process_fdt_options();
	stored_bootdelay = bootdelay;
	return s;
}
void autoboot_command(const char *s)
{
	debug("### main_loop: bootcmd=\"%s\"\n", s ? s : "<UNDEFINED>");
	if (s && (stored_bootdelay == -2 ||
		 (stored_bootdelay != -1 && !abortboot(stored_bootdelay)))) {
		bool lock;
		int prev;
		lock = autoboot_keyed() &&
			!IS_ENABLED(CONFIG_AUTOBOOT_KEYED_CTRLC);
		if (lock)
			prev = disable_ctrlc(1); /* disable Ctrl-C checking */
		run_command_list(s, -1, 0);
		if (lock)
			disable_ctrlc(prev);	/* restore Ctrl-C checking */
	}
	if (IS_ENABLED(CONFIG_AUTOBOOT_USE_MENUKEY) &&
	    menukey == AUTOBOOT_MENUKEY) {
		s = env_get("menucmd");
		if (s)
			run_command_list(s, -1, 0);
	}
#if CONFIG_IS_ENABLED(UBOOT_ARHT) 
	startmulticast = 0;
#endif
}
