/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AW3750x LCD bias (VSP/VSN) power driver for TALPAD
 * (TALIH-PD2, ls12_mt8797_wifi_64).
 *
 * Reconstructed from the OEM kernel 4.19.191+ (2026-03-26 build).
 * The HX83121A panel drivers call tct_aw3750x_volt_outp_set() /
 * tct_aw3750x_volt_outn_set() / tct_aw3750x_ldo_current_set() directly,
 * so this driver keeps the I2C client and exposes those helpers. The
 * voltage registers are written exactly as the OEM driver does:
 *   reg 0x00 = OUTP voltage code
 *   reg 0x01 = OUTN voltage code
 *
 * The current-limit register (0x03) mapping is chip-variant specific and
 * is deliberately left untouched here; the panel init calls
 * tct_aw3750x_ldo_current_set() without checking its return value, and
 * the chip powers up with a safe default limit.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>

#define AW3750X_REG_OUTP		0x00
#define AW3750X_REG_OUTN		0x01

static struct i2c_client *g_aw3750x_client;

static int aw3750x_i2c_write_byte(struct i2c_client *client,
				  u8 reg, u8 val)
{
	int ret;
	int i;

	for (i = 0; i < 5; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret >= 0)
			return ret;
		usleep_range(2000, 3000);
	}

	return ret;
}

int tct_aw3750x_volt_outp_set(int code)
{
	struct i2c_client *client = g_aw3750x_client;

	if (!client)
		return -ENODEV;

	if (code < 0)
		return -EINVAL;
	if (code > 0x19)
		code = 0x19;

	return aw3750x_i2c_write_byte(client, AW3750X_REG_OUTP, code);
}
EXPORT_SYMBOL(tct_aw3750x_volt_outp_set);

int tct_aw3750x_volt_outn_set(int code)
{
	struct i2c_client *client = g_aw3750x_client;

	if (!client)
		return -ENODEV;

	if (code < 0)
		return -EINVAL;
	if (code > 0x19)
		code = 0x19;

	return aw3750x_i2c_write_byte(client, AW3750X_REG_OUTN, code);
}
EXPORT_SYMBOL(tct_aw3750x_volt_outn_set);

int tct_aw3750x_ldo_current_set(int outp_uA, int outn_uA)
{
	/*
	 * The OEM current-limit code depends on the AW3750x variant and its
	 * DT "limit" property. Keep the chip default for now; the caller
	 * (panel init) ignores the return value anyway.
	 */
	return 0;
}
EXPORT_SYMBOL(tct_aw3750x_ldo_current_set);

static int aw3750x_power_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	u32 outp = 0;
	u32 outn = 0;

	g_aw3750x_client = client;

	/* DT defaults; the panel driver overrides them at init time */
	of_property_read_u32(client->dev.of_node, "outp", &outp);
	of_property_read_u32(client->dev.of_node, "outn", &outn);
	if (outp)
		aw3750x_i2c_write_byte(client, AW3750X_REG_OUTP, outp);
	if (outn)
		aw3750x_i2c_write_byte(client, AW3750X_REG_OUTN, outn);

	dev_info(&client->dev, "AW3750x LCD bias probed (outp=%u outn=%u)\n",
		 outp, outn);
	return 0;
}

static int aw3750x_power_remove(struct i2c_client *client)
{
	g_aw3750x_client = NULL;
	return 0;
}

static const struct of_device_id aw3750x_power_of_match[] = {
	{ .compatible = "awinic,aw3750x_led" },
	{ }
};
MODULE_DEVICE_TABLE(of, aw3750x_power_of_match);

static const struct i2c_device_id aw3750x_power_id[] = {
	{ "aw3750x_led", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw3750x_power_id);

static struct i2c_driver aw3750x_power_driver = {
	.probe = aw3750x_power_probe,
	.remove = aw3750x_power_remove,
	.id_table = aw3750x_power_id,
	.driver = {
		.name = "aw3750x_led",
		.owner = THIS_MODULE,
		.of_match_table = aw3750x_power_of_match,
	},
};

module_i2c_driver(aw3750x_power_driver);

MODULE_AUTHOR("TALPAD-BOOM / reconstructed from OEM kernel");
MODULE_DESCRIPTION("AW3750x LCD bias power driver for TALPAD");
MODULE_LICENSE("GPL v2");
