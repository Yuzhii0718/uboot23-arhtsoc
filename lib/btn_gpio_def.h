/*  Copyright(c) 2009-2024 TP-Link Systems Inc.	All rights reserved.
 *
 * file		btn_gpio_def.h
 * brief	Front panel button GPIO table used by check_fw_gpio() in
 *		lib/bootlib.c.
 *
 * warning	Board specific: "id" is the four character name bootlib.c looks
 *		up, "gpio" is the SoC GPIO number, "reverse" is the unlatch
 *		level (0 = pressed means low, 1 = pressed means high).
 */

#ifndef __BTN_GPIO_DEF_H__
#define __BTN_GPIO_DEF_H__

#include <linux/types.h>

/* Keep in sync with UIP_RESET_KEY_GPIO in common/autoboot.c. */
#define BOOT_BTN_NAME_RST	"RST"

typedef struct _BOOT_BTN_DEF
{
	char	id[8];
	u8	gpio;
	u8	reverse;
} BOOT_BTN_DEF;

static BOOT_BTN_DEF boot_btn_def[] =
{
	/* Generic Airoha reset key: GPIO 0, active low. */
	{ BOOT_BTN_NAME_RST, 0, 0 },
};

#endif /* __BTN_GPIO_DEF_H__ */
