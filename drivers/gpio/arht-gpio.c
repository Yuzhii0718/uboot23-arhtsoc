// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2021-2023 Airoha Inc.
/*
 * GPIO implementation based on Airoha SDK
 *
 * Author: Shubham Jain <shubham.jain@airoha.com>
 * 	   Zhengping zhang <zhengping.zhang@airoha.com> 
 *
 */

#include <linux/types.h>
#include <linux/io.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/bitops.h>

/**
 * airoha_gpio_ctrl - Airoha GPIO driver data
 * @gc: Associated gpio_chip instance.
 * @data: The data register.
 * @dir0: The direction register for the lower 16 pins.
 * @dir1: The direction register for the higher 16 pins.
 * @output: The output enable register.
 */

struct airoha_gpio_priv {
	void __iomem *data;
	void __iomem *dir[2];
	void __iomem *output;
};

static int arht_gpio_get_value(struct udevice *dev, unsigned offset)
{
	struct airoha_gpio_priv *priv = dev_get_priv(dev);

	return !!(readl(priv->data) & BIT(offset));
}

static int arht_gpio_set_value(struct udevice *dev, unsigned offset,
				  int value)
{
	struct airoha_gpio_priv *priv = dev_get_priv(dev);

	if (value)
		setbits_32(priv->data, BIT(offset));
	else
		clrbits_32(priv->data, BIT(offset));

	return 0;
}

static int airoha_dir_set(struct udevice *dev, unsigned int gpio,
			  int val, int out)
{
	struct airoha_gpio_priv *priv = dev_get_priv(dev);
	u32 dir = ioread32(priv->dir[gpio >> 4]);				// gpio >> 4 is equivalent to gpio / 16 and is more computationally efficient
	u32 output = ioread32(priv->output);
	u32 mask = BIT((gpio & 0xf) << 1);						// (gpio & 0xf) << 1  is equivalent to (gpio % 16) * 2 and is more computationally efficient

	if (out) {
		dir |= mask;
		output |= BIT(gpio);
	} else {
		dir &= ~mask;
		output &= ~BIT(gpio);
	}
		
	iowrite32(dir, priv->dir[gpio >> 4]); 				// gpio >> 4 is equivalent to gpio / 16 and is more computationally efficient
	
	if (out){
		arht_gpio_set_value(dev,gpio,val);
	}
	
	iowrite32(output, priv->output);
	
	return 0;
}

static int arht_gpio_direction_output(struct udevice *dev, unsigned int gpio,
					  int value)
{
	
	return airoha_dir_set(dev, gpio, value, 1);
}

static int arht_gpio_direction_input(struct udevice *dev, unsigned int gpio)
{
	return airoha_dir_set(dev, gpio, 0, 0);
}

static int arht_gpio_get_function(struct udevice *dev, unsigned int gpio)
{
	struct airoha_gpio_priv *priv = dev_get_priv(dev);
	u32 dir = ioread32(priv->dir[gpio >> 4]);					// gpio >> 4 is equivalent to gpio / 16 and is more computationally efficient
	u32 mask = BIT((gpio & 0xf) << 1);							// (gpio & 0xf) << 1  is equivalent to (gpio % 16) * 2 and is more computationally efficient

	return (dir & mask) ? 0 : 1;
}

static const struct dm_gpio_ops gpio_arht_ops = {
	.direction_input	= arht_gpio_direction_input,
	.direction_output	= arht_gpio_direction_output,
	.get_value		    = arht_gpio_get_value,
	.set_value		    = arht_gpio_set_value,
	.get_function		= arht_gpio_get_function,
};

static int airoha_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct airoha_gpio_priv *priv = dev_get_priv(dev);
	
	priv->data = dev_remap_addr_index(dev, 0);
	if (!priv->data)
		return -EINVAL;

	priv->dir[0] = dev_remap_addr_index(dev, 1);
	if (!priv->dir[0])
		return -EINVAL;

	priv->dir[1] = dev_remap_addr_index(dev, 2);
	if (!priv->dir[1])
		return -EINVAL;
	
	priv->output = dev_remap_addr_index(dev, 3);
	if (!priv->output)
		return -EINVAL;	
	
	uc_priv->gpio_count = dev_read_u32_default(dev, "ngpios", 32);
	uc_priv->bank_name = dev->name;
	
	return 0;
}


static const struct udevice_id arht_gpio_ids[] = {
	{ .compatible = "airoha,en7523-gpio" },
	{ .compatible = "airoha,an7552-gpio" },
	{ .compatible = "airoha,an7581-gpio" },
	{ .compatible = "airoha,an7583-gpio" },
	{}
};

U_BOOT_DRIVER(airoha_gpio) = {
	.name	= "airoha_gpio",
	.id	= UCLASS_GPIO,
	.of_match = arht_gpio_ids,
	.ops	= &gpio_arht_ops,
	.priv_auto = sizeof(struct airoha_gpio_priv),
	.probe	= airoha_gpio_probe,
};

