/* SPDX-License-Identifier: GPL-2.0 */
/*
 * aw883xx_ext.c   aw883xx i2c driver glue
 *
 * Copyright (c) 2023 MediaTek Inc.
 *
 * Registers aw883xx (AWINIC Smart PA) as an I2C driver so the codec binds
 * to the "awinic,aw883xx_smartpa" DT node. The core driver lives in
 * aw883xx.c and exposes aw883xx_i2c_probe()/aw883xx_i2c_remove().
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include "aw883xx_ext.h"

#define AW883XX_I2C_NAME "aw883xx_smartpa"

static int aw883xx_ext_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	return aw883xx_i2c_probe(i2c, id);
}

static int aw883xx_ext_remove(struct i2c_client *i2c)
{
	return aw883xx_i2c_remove(i2c);
}

static const struct i2c_device_id aw883xx_ext_i2c_id[] = {
	{ AW883XX_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw883xx_ext_i2c_id);

static const struct of_device_id aw883xx_ext_dt_match[] = {
	{ .compatible = "awinic,aw883xx_smartpa" },
	{ }
};
MODULE_DEVICE_TABLE(of, aw883xx_ext_dt_match);

static struct i2c_driver aw883xx_ext_driver = {
	.driver = {
		.name = AW883XX_I2C_NAME,
		.of_match_table = aw883xx_ext_dt_match,
	},
	.probe = aw883xx_ext_probe,
	.remove = aw883xx_ext_remove,
	.id_table = aw883xx_ext_i2c_id,
};

module_i2c_driver(aw883xx_ext_driver);

MODULE_DESCRIPTION("ASoC AW883XX Smart PA I2C driver");
MODULE_LICENSE("GPL v2");
