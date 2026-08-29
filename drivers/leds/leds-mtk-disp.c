// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <leds-mtk.h>
#include "../drivers/gpu/drm/mediatek/mediatek_v2/mtk_panel_ext.h"

/*
 * gate-IC 背光弱桩：ls12 面板是 HX83121A DSI 面板，背光走 mtkfb_set_backlight_level
 * (led_disp_set)，不经过 gate IC；这两个 gate-IC 函数只在 gate-IC 面板的 led_i2c_set
 * 路径用到。5.10 GKI 的 vmlinux 以 -shared -Bsymbolic --no-undefined 链接，弱【未定义】
 * 函数会生成 .got.plt/.plt 触发链接 ASSERT（Unexpected GOT/PLT entries detected）。
 * 这里提供弱【定义】空桩（对标 5.10 fork 的 mtk_drm_panel_common.h #else 分支），
 * 让引用本地绑定、return 0 无害，避免引入 rt4831a 等 gate-IC 驱动的连带依赖。
 */
__attribute__ ((weak)) int mtk_drm_gateic_set_backlight(unsigned int level, char func)
{
	return 0;
}
__attribute__ ((weak)) int _gate_ic_backlight_set(unsigned int brightness)
{
	return 0;
}

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME " %s(%d) :" fmt, __func__, __LINE__

struct mt_leds_disp {
	int num_leds;
	struct mt_led_data leds[];
};

static int led_disp_create_fwnode(struct device *dev, struct mt_leds_disp *priv)
{
	struct fwnode_handle *fwnode;
	struct mt_led_data *led_data;
	int ret = 0;

	pr_info("create fwnode begain +++");

	device_for_each_child_node(dev, fwnode) {
		led_data = &priv->leds[priv->num_leds];
		ret = mt_leds_parse_dt(led_data, fwnode);
		if (ret < 0) {
			fwnode_handle_put(fwnode);
			return -EINVAL;
		}
		led_data->mtk_hw_brightness_set = of_device_get_match_data(dev);
		ret = mt_leds_classdev_register(dev, led_data);
		if (ret < 0) {
			dev_notice(dev, "failed to register led for %s: %d\n",
				led_data->conf.cdev.name, ret);
			return ret;
		}
		priv->num_leds++;

		pr_info("parse led: %s, num: %d", led_data->conf.cdev.name, priv->num_leds);
	}

	return 0;
}

static int led_disp_probe(struct platform_device *pdev)
{
	struct mt_leds_disp *priv;
	int ret = 0;
	int count;

	pr_info("probe begain +++");

	count = device_get_child_node_count(&pdev->dev);

	if (!count) {
		ret = -EINVAL;
		goto err;
	}

	priv = devm_kzalloc(&pdev->dev, struct_size(priv, leds, count),
			    GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto err;
	}

	ret = led_disp_create_fwnode(&pdev->dev, priv);

	if (ret < 0)
		goto err;

	platform_set_drvdata(pdev, priv);

	pr_info("probe end +++");

	return 0;
err:
	pr_notice("Failed to probe: %d, %d!\n", ret, count);
	return ret;

}

static void __maybe_unused led_disp_shutdown(struct platform_device *pdev)
{
	int i;
	struct mt_leds_disp *m_leds = dev_get_platdata(&pdev->dev);

	pr_info("Turn off backlight\n");

	for (i = 0; m_leds && i < m_leds->num_leds; i++) {
		if (!&(m_leds->leds[i]))
			continue;

		m_leds->leds[i].mtk_hw_brightness_set(&m_leds->leds[i], 0);
		mt_leds_call_notifier(LED_STATUS_SHUTDOWN, &(m_leds->leds[i].conf));
	}
}

static int __maybe_unused led_disp_set(struct mt_led_data *mdev,
		       int brightness)
{
	pr_debug("set brightness %d", brightness);
	return mtkfb_set_backlight_level(brightness);
}

static int __maybe_unused led_i2c_set(struct mt_led_data *mdev,
		       int brightness)
{
	int version = mtk_drm_get_lcm_version();

	pr_debug("set brightness %d, version:%d", brightness, version);
	if (version == MTK_COMMON_LCM_DRV)
		return mtk_drm_gateic_set_backlight(brightness, 2);
	else if (version == MTK_LEGACY_LCM_DRV)
		return _gate_ic_backlight_set(brightness);

	pr_notice("%s,gate ic is not ready yet\n", __func__);
	return 0;
}

static int __maybe_unused led_set_virtual(struct mt_led_data *mdev,
		       int brightness)
{
	pr_debug("set brightness %d return, no need", brightness);
	return 0;
}

static const struct of_device_id of_disp_leds_match[] = {
	{ .compatible = "mediatek,disp-leds", .data = (int *)led_disp_set},
	{ .compatible = "mediatek,i2c-leds", .data = (int *)led_i2c_set},
	{ .compatible = "mediatek,mtk-leds", .data = (int *)led_set_virtual},
	{},
};
MODULE_DEVICE_TABLE(of, of_disp_leds_match);

static struct platform_driver led_disp_driver = {
	.probe		= led_disp_probe,
	.driver		= {
		.name	= "mtk_leds_disp",
		.of_match_table = of_disp_leds_match,
	},
	.shutdown = led_disp_shutdown,
};

module_platform_driver(led_disp_driver);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Disp Backlight Driver");
MODULE_LICENSE("GPL");


