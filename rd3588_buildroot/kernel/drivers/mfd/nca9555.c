/* SPDX-License-Identifier: GPL-2.0-or-later */
/* NCA9555 IO Expander, Key controller core driver.
 *
 * Copyright 2023 RPDZKJ
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/mfd/core.h>
#include <linux/mfd/nca9555.h>

static const struct mfd_cell nca9555_devs[] = {
	{
		.name = "nca9555-gpio",
		.of_compatible = "nca9555-gpio",
	}
};

static int nca9555_i2c_read_reg(struct nca9555_dev *nca9555, u8 reg, u8 *val)
{
	struct i2c_client *i2c = nca9555->i2c_client;
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = i2c->addr;
	xfer[0].flags = 0;
	xfer[0].len = 1;
	xfer[0].buf = &reg;

	/* Read data */
	xfer[1].addr = i2c->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = 1;
	xfer[1].buf = val;

	ret = i2c_transfer(i2c->adapter, xfer, 2);
	if (ret == 2) {
		return 0;
	} else {
		dev_err(&i2c->dev,
			"Failed to read reg 0x%02x, ret is %d\n",
			reg, ret);
		if ( ret >=0)
			return -EIO;
		else
			return ret;
	}
}

static int nca9555_i2c_write_reg(struct nca9555_dev *nca9555, u8 reg, u8 val)
{
	struct i2c_client *i2c = nca9555->i2c_client;
	u8 msg[2];
	int ret;

	msg[0] = reg;
	msg[1] = val;
	ret = i2c_master_send(i2c, msg, 2);
	if (ret == 2) {
		return 0;
	} else {
		dev_err(&i2c->dev,
			"Failed to write reg 0x%02x, ret is %d\n",
			reg, ret);
		if ( ret >=0)
			return -EIO;
		else
			return ret;
	}
}

static int nca9555_i2c_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct nca9555_dev *nca9555;
	u8 reg;
	int ret;

	dev_info(&i2c->dev, "nca9555 probe start \n");

	nca9555 = devm_kzalloc(&i2c->dev, sizeof(struct nca9555_dev),
				GFP_KERNEL);
	if (nca9555 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, nca9555);
	nca9555->dev = &i2c->dev;
	nca9555->i2c_client = i2c;
	nca9555->read_reg = nca9555_i2c_read_reg;
	nca9555->write_reg = nca9555_i2c_write_reg;

	ret = nca9555_i2c_read_reg(nca9555, NCA9555_ID, &reg);
	if (ret)
		return ret;

	dev_info(&i2c->dev, "nca9555 probe success \n");
	return devm_mfd_add_devices(nca9555->dev, PLATFORM_DEVID_AUTO,
				    nca9555_devs, ARRAY_SIZE(nca9555_devs),
				    NULL, 0, NULL);
}

static const struct i2c_device_id nca9555_i2c_id[] = {
	{ "nca9555", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, nca9555_i2c_id);

static const struct of_device_id nca9555_of_match[] = {
	{.compatible = "nca9555", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nca9555_of_match);

static struct i2c_driver nca9555_i2c_driver = {
	.driver = {
		   .name = "nca9555",
		   .of_match_table = of_match_ptr(nca9555_of_match),
	},
	.probe = nca9555_i2c_probe,
	.id_table = nca9555_i2c_id,
};

static int __init nca9555_i2c_init(void)
{
	return i2c_add_driver(&nca9555_i2c_driver);
}
/* init early so consumer devices can complete system boot */
subsys_initcall(nca9555_i2c_init);

static void __exit nca9555_i2c_exit(void)
{
	i2c_del_driver(&nca9555_i2c_driver);
}
module_exit(nca9555_i2c_exit);

MODULE_DESCRIPTION("NCA9555 core driver");
MODULE_AUTHOR("Haibo Chen <haibo.chen@nxp.com>");
MODULE_LICENSE("GPL");
