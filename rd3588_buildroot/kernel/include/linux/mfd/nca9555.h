/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Analog Devices NCA9555 I/O Expander, keypad controller,
 * PWM contorller.
 *
 * Copyright 2022 NXP
 */

#ifndef __NCA9555_H_
#define __NCA9555_H_


#define GPIO_PORT0 0
#define GPIO_PORT1 1

#define GPIO_PORT0_INPUT_REG     0x00
#define GPIO_PORT1_INPUT_REG     0x01

#define GPIO_PORT0_OUTPUT_REG    0x02
#define GPIO_PORT1_OUTPUT_REG    0x03

#define GPIO_PORT0_POLAR_REG     0x04
#define GPIO_PORT1_POLAR_REG     0x05

#define GPIO_PORT0_CONFIG_REG    0x06
#define GPIO_PORT1_CONFIG_REG    0x07

#define NCA9555_ID			0x00

#define NCA9555_BANK(offs)		((offs) >= 8)
#define NCA9555_BIT(offs)		(offs >= 8 ? \
					1u << (offs - 8) : 1u << (offs))

struct nca9555_dev {
	struct device *dev;
	struct i2c_client *i2c_client;

	int (*read_reg)(struct nca9555_dev *nca9555, u8 reg, u8 *val);
	int (*write_reg)(struct nca9555_dev *nca9555, u8 reg, u8 val);
};

#endif
