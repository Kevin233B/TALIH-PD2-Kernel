// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>


#define FSA4480_I2C_NAME	"fsa4480-driver"

#define HL5280_DEVICE_REG_VALUE 0x49

#define FSA4480_DEVICE_ID  0x00
#define FSA4480_SWITCH_SETTINGS 0x04
#define FSA4480_SWITCH_CONTROL  0x05
#define FSA4480_SWITCH_STATUS0  0x06
#define FSA4480_SWITCH_STATUS1  0x07
#define FSA4480_SLOW_L          0x08
#define FSA4480_SLOW_R          0x09
#define FSA4480_SLOW_MIC        0x0A
#define FSA4480_SLOW_SENSE      0x0B
#define FSA4480_SLOW_GND        0x0C
#define FSA4480_DELAY_L_R       0x0D
#define FSA4480_DELAY_L_MIC     0x0E
#define FSA4480_DELAY_L_SENSE   0x0F
#define FSA4480_DELAY_L_AGND    0x10
#define FSA4480_FUN_EN          0x12
#define FSA4480_JACK_STATUS     0x17
#define FSA4480_RESET           0x1E
#define FSA4480_CURRENT_SOURCE_SETTING 0x1F

#undef dev_dbg
#define dev_dbg dev_info


enum switch_vendor {
    FSA4480 = 0,
    HL5280
};

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct work_struct usbc_analog_work;
	struct mutex notification_lock;
	unsigned int hs_det_pin;
	//added for mic
	unsigned int mic_det_pin;
	enum switch_vendor vendor;
	bool plug_state;
};

static struct fsa4480_priv *fsa_priv;

struct fsa4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FSA4480_CURRENT_SOURCE_SETTING,
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
	{FSA4480_DELAY_L_MIC, 0x00},
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x09},
	{FSA4480_SWITCH_SETTINGS, 0x98},
};

static void fsa4480_dump_reg(struct fsa4480_priv *fsa_priv)
{
    int switch_status = 0;
    int i = 0x04;
    printk("hly %s line = %d",__func__,__LINE__);
    if (!fsa_priv->regmap) {
		printk("%s: regmap invalid\n", __func__);
		return;
	}
    while(i <= 0x1f)
    {
        regmap_read(fsa_priv->regmap, i, &switch_status);
		printk("%s: aa reg[0x%x]=0x%x.\n", __func__, i, switch_status);
		i++;
    }		
}

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}
    printk("hly %s line = %d",__func__,__LINE__);
	//regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x80);
	
	/* FSA4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
}
//added by lanying.he 
void fsa4480_usbc_event_changed(int plug_state)
{

	if(plug_state)
	{
		printk("%s: audio plug in\n", __func__);		
			fsa_priv->plug_state = true;
			pm_stay_awake(fsa_priv->dev);
			schedule_work(&fsa_priv->usbc_analog_work);
	}
	else
	{
					/* AUDIO plug out */
			printk("%s: audio plug out\n", __func__);
			fsa_priv->plug_state = false;
			pm_stay_awake(fsa_priv->dev);
			schedule_work(&fsa_priv->usbc_analog_work);
	}

}
EXPORT_SYMBOL(fsa4480_usbc_event_changed);

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	int rc = 0;
	struct device *dev;
	//unsigned int switch_status = 0;
	unsigned int jack_status = 0;
	int state; //added for mic detect

    //struct pinctrl_state *hs_det_gpio;
    //struct pinctrl *pinctrl1;
//    int ret = 0;

	if (!fsa_priv)
		return -EINVAL;
	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	mutex_lock(&fsa_priv->notification_lock);

	dev_info(dev, "%s: plug_state %d\n", __func__, fsa_priv->plug_state);
	if (fsa_priv->plug_state) {
		/* activate switches */
		//regmap_write(fsa_priv->regmap, 0x19, 0x0);
		//regmap_write(fsa_priv->regmap, 0x1c, 0x20);
		//regmap_write(fsa_priv->regmap, 0x1d, 0xff);
		//regmap_write(fsa_priv->regmap, 0x1f, 0x07);
//Begein added by lanying.he for mic detect
		if (gpio_is_valid(fsa_priv->mic_det_pin)) {
			state = gpio_get_value(fsa_priv->mic_det_pin);
			dev_info(dev, "%s: before mic_det_pin state = %d.\n", __func__, state);

			gpio_direction_output(fsa_priv->mic_det_pin, 0);
			state = gpio_get_value(fsa_priv->mic_det_pin);
			dev_info(dev, "%s: after mic_det_pin state = %d.\n", __func__, state);
		}
//End added by lanying.he for mic detect		
		fsa4480_usbc_update_settings(fsa_priv, 0x07, 0x9F);
        regmap_write(fsa_priv->regmap, FSA4480_CURRENT_SOURCE_SETTING, 0x07);
		usleep_range(1000, 1005);
		printk("hly %s line = %d",__func__,__LINE__);
        //fsa4480_dump_reg(fsa_priv);
		//regmap_write(fsa_priv->regmap, FSA4480_FUN_EN, 0x45);
		regmap_write(fsa_priv->regmap, FSA4480_FUN_EN, 0x1);
		usleep_range(10000, 10005);
		dev_info(dev, "%s: set reg[0x%x] done.\n", __func__, FSA4480_FUN_EN);

		regmap_read(fsa_priv->regmap, FSA4480_JACK_STATUS, &jack_status);
		dev_info(dev, "%s: reg[0x%x]=0x%x.\n", __func__, FSA4480_JACK_STATUS, jack_status);

        /* Hefeng.Wu@RM.MM.AudioDriver.HeadsetDet, 2020/06/12,
         * if detect fail under 700uA, use 100uA detect again */
        if (1 == jack_status) {
            dev_info(dev, "%s: use 100uA detect again\n", __func__);
            fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);
            regmap_write(fsa_priv->regmap, FSA4480_CURRENT_SOURCE_SETTING, 0x01);
            usleep_range(1000, 1005);

            regmap_write(fsa_priv->regmap, FSA4480_FUN_EN, 0x45);
            usleep_range(10000, 10005);
            regmap_read(fsa_priv->regmap, FSA4480_JACK_STATUS, &jack_status);
            dev_info(dev, "%s: reg[0x%x]=0x%x.\n", __func__, FSA4480_JACK_STATUS, jack_status);
        }

		if (jack_status & 0x2) {
			//for 3 pole, mic switch to SBU2
			dev_info(dev, "%s: set mic to sbu2 for 3 pole.\n", __func__);
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9F);
			usleep_range(4000, 4005);
		}
		printk("hly %s line = %d",__func__,__LINE__);
        fsa4480_dump_reg(fsa_priv);
	} else {

		/* deactivate switches */
	//begin add by wenhaodeng for task  LUSHAN-102 on 20230803
		//fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0xF8);
	//begin add by wenhaodeng for task  LUSHAN-102 on 20230803
		printk("hly %s line = %d",__func__,__LINE__);
		fsa4480_dump_reg(fsa_priv);
//Begein added by lanying.he for mic detect
		if (gpio_is_valid(fsa_priv->mic_det_pin)) {
			state = gpio_get_value(fsa_priv->mic_det_pin);
			dev_info(dev, "%s: before mic_det_pin state = %d.\n", __func__, state);

			gpio_direction_output(fsa_priv->mic_det_pin, 1);
			state = gpio_get_value(fsa_priv->mic_det_pin);
		    dev_info(dev, "%s: after mic_det_pin state = %d.\n", __func__, state);
		}
//End added by lanying.he for mic detect		
	}

	mutex_unlock(&fsa_priv->notification_lock);
	return rc;
}

static int fsa4480_parse_dt(struct fsa4480_priv *fsa_priv,struct device *dev)
{

    struct device_node *dNode = dev->of_node;
    int ret = 0;
	
    if (dNode == NULL)
        return -ENODEV;

    if (!fsa_priv) {
		pr_err("%s: fsa_priv is NULL\n", __func__);
		return -ENOMEM;
	}

	fsa_priv->hs_det_pin = of_get_named_gpio(dNode, "hs-det-gpio", 0);
	if (!gpio_is_valid(fsa_priv->hs_det_pin)) {
	    pr_warn("%s: hs-det-gpio in dt node is missing\n", __func__);
	    return -ENODEV;
	}
	ret = gpio_request(fsa_priv->hs_det_pin, "fsa4480_hs_det");
	if (ret) {
		pr_warn("%s: hs-det-gpio request fail\n", __func__);
		return ret;
	}
	gpio_direction_output(fsa_priv->hs_det_pin, 0);
	//Begin added for mic detect
	fsa_priv->mic_det_pin = of_get_named_gpio(dNode, "mic-gpio", 0);
	if (!gpio_is_valid(fsa_priv->mic_det_pin)) {
	    pr_warn("%s: hs-mic-gpio in dt node is missing\n", __func__);
	    return -ENODEV;
	}
	ret = gpio_request(fsa_priv->mic_det_pin, "fsa4480_mic_det");
	if (ret) {
		pr_warn("%s: hs-mic-gpio request fail\n", __func__);
		return ret;
	}
	gpio_direction_output(fsa_priv->mic_det_pin, 1);	
    //End added for mic detect
	
	return ret;

}

static void fsa4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct fsa4480_priv *fsa_priv =
		container_of(work, struct fsa4480_priv, usbc_analog_work);

	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	fsa4480_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
}

static void fsa4480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

static int fsa4480_probe(struct i2c_client *i2c,
			 const struct i2c_device_id *id)
{
	//struct fsa4480_priv *fsa_priv;
	int rc = -EINVAL;
	unsigned int reg_value = 0;

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	fsa_priv->dev = &i2c->dev;

    fsa4480_parse_dt(fsa_priv, &i2c->dev);

	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_data;
	}

	fsa4480_update_reg_defaults(fsa_priv->regmap);

	regmap_read(fsa_priv->regmap, FSA4480_DEVICE_ID, &reg_value);
	dev_err(fsa_priv->dev, "%s: device id reg value: 0x%x\n", __func__, reg_value);
	if (HL5280_DEVICE_REG_VALUE == reg_value) {
		dev_err(fsa_priv->dev, "%s: switch chip is HL5280\n", __func__);
		fsa_priv->vendor = HL5280;
	} else {
		dev_err(fsa_priv->dev, "%s: switch chip is FSA4480\n", __func__);
		fsa_priv->vendor = FSA4480;
	}

	fsa_priv->plug_state = false;


	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

//begin add by wenhaodeng for task  LUSHAN-102 on 20230803
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0xF8);
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0x18);
//end add by wenhaodeng for task  LUSHAN-102 on 20230803
	return 0;

err_data:
	if (gpio_is_valid(fsa_priv->hs_det_pin)) {
		gpio_free(fsa_priv->hs_det_pin);
	}
	devm_kfree(&i2c->dev, fsa_priv);
	return rc;
}

static int fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);

	return 0;
}

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{
		.compatible = "mediatek,fsa4480-i2c",
	},
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
};

static int __init fsa4480_init(void)
{
	int rc;

	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
//late_initcall_sync(fsa4480_init);
module_init(fsa4480_init);

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL v2");
