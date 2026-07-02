// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * GPIO driver for NCA9555 MFD
 *
 * Copyright 2023 RPDZKJ
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mfd/nca9555.h>
#include <linux/gpio/driver.h>

#define NCA9555_GPIO_MAX 16

struct nca9555_gpio_dev {
	struct device *parent;
	struct gpio_chip gpio_chip;
	struct mutex lock;
	u8 dat_out[2];
	u8 dir[2];
};

static int nca9555_gpio_reg_read(struct nca9555_gpio_dev *nca9555_gpio, u8 reg, u8 *val)
{
	struct nca9555_dev *nca9555;
	nca9555 = dev_get_drvdata(nca9555_gpio->parent);

	return nca9555->read_reg(nca9555, reg, val);
}

static int nca9555_gpio_reg_write(struct nca9555_gpio_dev *nca9555_gpio, u8 reg, u8 val)
{
	struct nca9555_dev *nca9555;
	nca9555 = dev_get_drvdata(nca9555_gpio->parent);

	return nca9555->write_reg(nca9555, reg, val);
}

static int nca9555_gpio_get_value(struct gpio_chip *chip, unsigned int off)
{
	struct nca9555_gpio_dev *nca9555_gpio;
	unsigned int bank, bit, ret;
	u8 val;

	nca9555_gpio = gpiochip_get_data(chip);
	bank = NCA9555_BANK(off);
	bit = NCA9555_BIT(off);

	mutex_lock(&nca9555_gpio->lock);
	/* There are dedicated registers for GPIO IN/OUT. */
	if (nca9555_gpio->dir[bank] & bit)
        ret=nca9555_gpio_reg_read(nca9555_gpio, GPIO_PORT0_INPUT_REG + bank, &val);
	else	
		val = nca9555_gpio->dat_out[bank];
	mutex_unlock(&nca9555_gpio->lock);

	return !!(val & bit);
}

static void nca9555_gpio_set_value(struct gpio_chip *chip, unsigned int off, int val)
{
	struct nca9555_gpio_dev *nca9555_gpio;
	unsigned int bank, bit;

	nca9555_gpio = gpiochip_get_data(chip);
	bank = NCA9555_BANK(off);
	bit = NCA9555_BIT(off);

	mutex_lock(&nca9555_gpio->lock);
	if (val)
		nca9555_gpio->dat_out[bank] |= bit;
	else
		nca9555_gpio->dat_out[bank] &= ~bit;

	nca9555_gpio_reg_write(nca9555_gpio, GPIO_PORT0_OUTPUT_REG + bank,
			       nca9555_gpio->dat_out[bank]);
	mutex_unlock(&nca9555_gpio->lock);
}

static int nca9555_gpio_direction_input(struct gpio_chip *chip, unsigned int off)
{
	struct nca9555_gpio_dev *nca9555_gpio;
	unsigned int bank, bit;
	int ret;

	nca9555_gpio = gpiochip_get_data(chip);
	bank = NCA9555_BANK(off);
	bit = NCA9555_BIT(off);

	mutex_lock(&nca9555_gpio->lock);
	nca9555_gpio->dir[bank] |= bit;
	ret = nca9555_gpio_reg_write(nca9555_gpio, GPIO_PORT0_CONFIG_REG + bank,
				     nca9555_gpio->dir[bank]);
	mutex_unlock(&nca9555_gpio->lock);
	return ret;
}

static int nca9555_gpio_direction_output(struct gpio_chip *chip, unsigned int off, int val)
{
	struct nca9555_gpio_dev *nca9555_gpio;
	unsigned int bank, bit;
	int ret;

	nca9555_gpio = gpiochip_get_data(chip);

	bank = NCA9555_BANK(off);
	bit = NCA9555_BIT(off);

	mutex_lock(&nca9555_gpio->lock);
	nca9555_gpio->dir[bank] &= ~bit;

	if (val)
		nca9555_gpio->dat_out[bank] |= bit;
	else
		nca9555_gpio->dat_out[bank] &= ~bit;
	ret = nca9555_gpio_reg_write(nca9555_gpio, GPIO_PORT0_OUTPUT_REG + bank,
				     nca9555_gpio->dat_out[bank]);
	ret |= nca9555_gpio_reg_write(nca9555_gpio, GPIO_PORT0_CONFIG_REG + bank,
				      nca9555_gpio->dir[bank]);

	mutex_unlock(&nca9555_gpio->lock);

	return ret;
}

static int nca9555_gpio_probe(struct platform_device *pdev)
{
	struct nca9555_gpio_dev *nca9555_gpio;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct gpio_chip *gc;
	int i;

	dev_info(dev, "gpio-nca9555 probe start \n");

	nca9555_gpio = devm_kzalloc(&pdev->dev, sizeof(struct nca9555_gpio_dev), GFP_KERNEL);
	if (nca9555_gpio == NULL)
		return -ENOMEM;

	nca9555_gpio->parent = pdev->dev.parent;

	gc = &nca9555_gpio->gpio_chip;
	gc->of_node = np;
	gc->parent = dev;
	gc->direction_input  = nca9555_gpio_direction_input;
	gc->direction_output = nca9555_gpio_direction_output;
	gc->get = nca9555_gpio_get_value;
	gc->set = nca9555_gpio_set_value;
	gc->can_sleep = true;

	gc->base = -1;
	gc->ngpio = NCA9555_GPIO_MAX;
	gc->label = pdev->name;
	gc->owner = THIS_MODULE;

	mutex_init(&nca9555_gpio->lock);

	for (i = 0; i < 2; i++) {
		u8 *dat_out, *dir;
		dat_out = nca9555_gpio->dat_out;
		dir = nca9555_gpio->dir;
		nca9555_gpio_reg_read(nca9555_gpio,
				      GPIO_PORT0_OUTPUT_REG + i, dat_out + i);
		nca9555_gpio_reg_read(nca9555_gpio,
				      GPIO_PORT0_CONFIG_REG + i, dir + i);
	}
	
	dev_info(dev, "gpio-nca9555 probe succes \n");
	return devm_gpiochip_add_data(&pdev->dev, &nca9555_gpio->gpio_chip, nca9555_gpio);
}

static const struct of_device_id nca9555_of_match[] = {
	{.compatible = "nca9555-gpio", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nca9555_of_match);

static struct platform_driver nca9555_gpio_driver = {
	.driver	= {
		.name	= "nca9555-gpio",
		.of_match_table = nca9555_of_match,
	},
	.probe		= nca9555_gpio_probe,
};

module_platform_driver(nca9555_gpio_driver);

MODULE_AUTHOR("Haibo Chen <haibo.chen@nxp.com>");
MODULE_DESCRIPTION("GPIO NCA9555 Driver");
MODULE_LICENSE("GPL");
