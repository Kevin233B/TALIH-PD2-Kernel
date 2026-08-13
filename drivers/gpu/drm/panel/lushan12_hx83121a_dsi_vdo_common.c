/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TALPAD (TALIH-PD2, ls12_mt8797_wifi_64) HX83121A CDOT CSOT WQXGA DSI VDO
 * panel driver, reconstructed from the OEM kernel 4.19.191+ (2026-03-26).
 *
 * Two variants share this implementation:
 *   - without_vcom: lushan12,hx83121a_cdot_csot_without_vcom_wqxga_dsi_vdo
 *   - with_vcom:    lushan12,hx83121a_cdot_csot_wqxga_dsi_vdo
 * The only differences are the compatible/driver name and one extra DSI
 * command (0xb6 0x88 0x88 0x03) sent by the with_vcom variant.
 */
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#include "lushan12_hx83121a_dsi_vdo_init.h"
#include "aw3750x_power.h"

/*
 * TCT touch resume hook: the OEM panel driver calls the Himax touch resume
 * after the panel reset/power sequence when the touch driver is ready, so
 * the TDDI stays in sync after LCM power-on.
 */
extern int himax_common_resume(struct device *dev);
extern int himax_common_suspend(struct device *dev);
extern struct device *tct_get_touch_dev(void);
extern int tct_get_panel_resume_flag(void);
extern int tct_get_gesture_en(void);

#ifndef PANEL_DRIVER_NAME
#error "PANEL_DRIVER_NAME must be defined by the including file"
#endif
#ifndef PANEL_COMPATIBLE
#error "PANEL_COMPATIBLE must be defined by the including file"
#endif

#ifndef PANEL_HAS_EXTRA_VCOM_CMD
#define PANEL_HAS_EXTRA_VCOM_CMD 0
#endif

#define PANEL_WIDTH		1600
#define PANEL_HEIGHT		2560
#define PHYSICAL_WIDTH_UM	166244
#define PHYSICAL_HEIGHT_UM	265958

#define HSA			20
#define HBP			40
#define HFP			60
#define VSA			4
#define VBP			18

/* 60 Hz mode (clock/(htotal*vtotal) = 579110000/(1720*5828) = 57.8Hz) */
#define MODE_0_FPS		60
#define MODE_0_VFP		3246
#define PANEL_CLOCK_60HZ	579110

/* 120 Hz mode (568972000/(1720*2914) = 113.5Hz, 半帧时序) */
#define MODE_1_FPS		120
#define MODE_1_VFP		332
#define PANEL_CLOCK_120HZ	568972

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	int esd_te_master;
	int esd_te_slave;
	bool prepared;
	bool enabled;
	bool slept;
	int error;
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static int lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	const u8 *cmd = data;

	/*
	 * 官核/2024 基驱动一致的分派规则：标准 DCS 命令（首字节 < 0xB0，
	 * 如 0x11/0x29/0x51/0x53/0x35）必须用 DCS 包类型
	 * （0x05/0x15/0x39）；厂商扩展命令（首字节 >= 0xB0）用 generic
	 * 包类型（0x03/0x04/0x29）。此前全部走 generic，Sleep Out /
	 * Display On 以 0x03 包发送会被 HX83121A 忽略，导致面板未真正
	 * 退出 sleep/未开启显示，表现为开机花屏。
	 */
	if (cmd && len && cmd[0] < 0xb0)
		return mipi_dsi_dcs_write_buffer(dsi, data, len);

	return mipi_dsi_generic_write(dsi, data, len);
}

static void lcm_panel_init(struct lcm *ctx)
{
	const struct hx83121a_init_cmd *cmd;
	int i, ret;

	/*
	 * 官核 2026 lcm_prepare 反汇编上电时序（__const_udelay 换算：
	 * 0x8312b0=8.6ms、0x147aeb8=21.5ms、0x418958=4.3ms，
	 * 复位后 141*4.3ms≈606ms 稳定窗口）。
	 * 此前沿用 2024 基驱动的 5ms 级短时序，冷启动可能不满足
	 * HX83121A 电源/复位建立时间，导致初始化不完整。
	 */
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
	else
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	mdelay(9);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0,
					     GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos))
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
	mdelay(21);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1,
					     GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg))
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio))
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));

	/* AW3750x LCD bias: 5.6 V positive/negative, 150 mA */
	tct_aw3750x_ldo_current_set(150000, 150000);
	tct_aw3750x_volt_outp_set(0x10);
	tct_aw3750x_volt_outn_set(0x10);
	mdelay(4);

	if (!IS_ERR(ctx->bias_pos)) {
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
	mdelay(21);

	if (!IS_ERR(ctx->bias_neg)) {
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	}
	mdelay(4);

	if (!IS_ERR(ctx->reset_gpio)) {
		gpiod_set_value(ctx->reset_gpio, 1);
		mdelay(21);
		gpiod_set_value(ctx->reset_gpio, 0);
		mdelay(21);
		gpiod_set_value(ctx->reset_gpio, 1);
		mdelay(21);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	}
	mdelay(606);

	if (tct_get_panel_resume_flag() && tct_get_touch_dev()) {
		himax_common_resume(tct_get_touch_dev());
		mdelay(9);
	}

	ctx->error = 0;
	for (i = 0; i < 7; i++) {
		cmd = &hx83121a_cdot_csot_init_seq[i];
		ret = lcm_dcs_write(ctx, cmd->data, cmd->len);
		if (ret < 0) {
			ctx->error = ret;
			dev_err(ctx->dev, "%s: init cmd %d failed: %d\n",
				__func__, i, ret);
			goto init_wait;
		}
	}

#if PANEL_HAS_EXTRA_VCOM_CMD
	cmd = &hx83121a_cdot_csot_with_vcom_extra_seq[0];
	ret = lcm_dcs_write(ctx, cmd->data, cmd->len);
	if (ret < 0) {
		ctx->error = ret;
		dev_err(ctx->dev, "%s: with_vcom extra cmd failed: %d\n",
			__func__, ret);
		goto init_wait;
	}
#endif

	for (; i < HX83121A_CDOT_CSOT_INIT_SEQ_LEN; i++) {
		cmd = &hx83121a_cdot_csot_init_seq[i];
		ret = lcm_dcs_write(ctx, cmd->data, cmd->len);
		if (ret < 0) {
			ctx->error = ret;
			dev_err(ctx->dev, "%s: init cmd %d failed: %d\n",
				__func__, i, ret);
			goto init_wait;
		}
	}

	ret = lcm_dcs_write(ctx, hx83121a_cdot_csot_sleep_out_seq,
			    HX83121A_CDOT_CSOT_SLEEP_OUT_SEQ_LEN);
	if (ret < 0) {
		ctx->error = ret;
		dev_err(ctx->dev, "%s: sleep out failed: %d\n",
			__func__, ret);
	}

init_wait:
	/*
	 * 官核 lcm_prepare 出口反汇编（0x867cd88）：无论前面命令是否
	 * 失败都等待 120*4.295ms≈515ms；error>=0 才发 0x29；再等
	 * 50*4.295ms≈215ms。失败时 0x11 可能未发出，官核同样只等
	 * 515ms 后跳过 0x29。
	 */
	mdelay(515);

	if (ctx->error >= 0) {
		ret = lcm_dcs_write(ctx, hx83121a_cdot_csot_display_on_seq,
				    HX83121A_CDOT_CSOT_DISPLAY_ON_SEQ_LEN);
		if (ret < 0) {
			ctx->error = ret;
			dev_err(ctx->dev, "%s: display on failed: %d\n",
				__func__, ret);
		}
	}
	mdelay(215);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	u8 cmd;

	pr_info("%s()+ %d,%d,%d\n", __func__, ctx->prepared, ctx->enabled,
		ctx->error);
	if (!ctx->prepared)
		return 0;

	if (ctx->error >= 0) {
		cmd = 0x28; /* display off */
		lcm_dcs_write(ctx, &cmd, 1);
		/* 官核 lcm_unprepare：0x28 后 1*4.3ms、0x10 后 2*4.3ms */
		mdelay(4);
	}

	if (ctx->error >= 0) {
		cmd = 0x10; /* sleep in */
		lcm_dcs_write(ctx, &cmd, 1);
		mdelay(9);
	}

	ctx->error = 0;
	ctx->prepared = false;

	/*
	 * TCT: keep the panel powered when single-click wake is enabled so
	 * the touch IC can still wake the system (same as the OEM kernel).
	 */
	if (tct_get_gesture_en()) {
		pr_info("lcm_unprepare tct_singleclkick_wakeup_en is enable, "
			"skip power down\n");
		/* 面板仅睡眠(未下电), 唤醒时走轻量 resume 免重跑完整 init */
		ctx->slept = true;
		return 0;
	}
	ctx->slept = false;

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	/* 官核 lcm_unprepare 下电步进为 21.5ms（0x147aeb8），保证放电完成 */
	mdelay(21);

	ctx->bias_neg = devm_gpiod_get_index(ctx->dev, "bias", 1,
					     GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	gpiod_set_value(ctx->bias_neg, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_neg);
	mdelay(21);

	ctx->bias_pos = devm_gpiod_get_index(ctx->dev, "bias", 0,
					     GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	gpiod_set_value(ctx->bias_pos, 0);
	devm_gpiod_put(ctx->dev, ctx->bias_pos);
	mdelay(21);

	if (tct_get_panel_resume_flag() && tct_get_touch_dev()) {
		himax_common_suspend(tct_get_touch_dev());
		/* 官核 suspend 后 7*4.3ms≈30ms，等待触摸 IC 稳定 */
		mdelay(30);
	}

	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	/*
	 * 轻量 resume: 面板在 single-click wake 时仅睡眠未下电,
	 * 只需 sleep out + display on, 免重跑 reset/606ms/完整 init,
	 * 消除电源键点亮的高延迟.
	 */
	if (ctx->slept) {
		ret = lcm_dcs_write(ctx, hx83121a_cdot_csot_sleep_out_seq,
				    HX83121A_CDOT_CSOT_SLEEP_OUT_SEQ_LEN);
		if (ret < 0)
			dev_err(ctx->dev, "%s: sleep out failed: %d\n",
				__func__, ret);
		ret = lcm_dcs_write(ctx, hx83121a_cdot_csot_display_on_seq,
				    HX83121A_CDOT_CSOT_DISPLAY_ON_SEQ_LEN);
		if (ret < 0)
			dev_err(ctx->dev, "%s: display on failed: %d\n",
				__func__, ret);
		mdelay(30);
		ctx->slept = false;
		ctx->prepared = true;
		return 0;
	}

	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;
	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock		= PANEL_CLOCK_60HZ,
	.hdisplay	= PANEL_WIDTH,
	.hsync_start	= PANEL_WIDTH + HFP,
	.hsync_end	= PANEL_WIDTH + HFP + HSA,
	.htotal		= PANEL_WIDTH + HFP + HSA + HBP,
	.vdisplay	= PANEL_HEIGHT,
	.vsync_start	= PANEL_HEIGHT + MODE_0_VFP,
	.vsync_end	= PANEL_HEIGHT + MODE_0_VFP + VSA,
	.vtotal		= PANEL_HEIGHT + MODE_0_VFP + VSA + VBP,
	.vrefresh	= MODE_0_FPS,
};

static const struct drm_display_mode performance_mode = {
	.clock		= PANEL_CLOCK_120HZ,
	.hdisplay	= PANEL_WIDTH,
	.hsync_start	= PANEL_WIDTH + HFP,
	.hsync_end	= PANEL_WIDTH + HFP + HSA,
	.htotal		= PANEL_WIDTH + HFP + HSA + HBP,
	.vdisplay	= PANEL_HEIGHT,
	.vsync_start	= PANEL_HEIGHT + MODE_1_VFP,
	.vsync_end	= PANEL_HEIGHT + MODE_1_VFP + VSA,
	.vtotal		= PANEL_HEIGHT + MODE_1_VFP + VSA + VBP,
	.vrefresh	= MODE_1_FPS,
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_120hz;

	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->dev, "failed to add mode %dx%d@%d\n",
			PANEL_WIDTH, PANEL_HEIGHT, MODE_0_FPS);
		return -ENOMEM;
	}
	mode->vrefresh = MODE_0_FPS;
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	mode_120hz = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode_120hz) {
		dev_err(panel->dev, "failed to add mode %dx%d@%d\n",
			PANEL_WIDTH, PANEL_HEIGHT, MODE_1_FPS);
		return -ENOMEM;
	}
	mode_120hz->vrefresh = MODE_1_FPS;
	mode_120hz->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_set_name(mode_120hz);
	drm_mode_probed_add(connector, mode_120hz);

	connector->display_info.width_mm = 166;
	connector->display_info.height_mm = 266;
	return 2;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

#if defined(CONFIG_MTK_PANEL_EXT)
/*
 * 官核 ext_params（0x97d61d8=mode0、0x97d6a00=mode1）反汇编核对：
 * 官核 mtk_dsi_default_rate（0x8648080）读 params+4=pll_clk、
 * +8=data_rate，实测 pll_clk=485、data_rate=970（此前误写 1/485，
 * DSI 时钟减半会导致带宽不足）；dyn.switch_en=1、dyn.data_rate=970、
 * dyn.vfp=3246/332；dyn_fps.switch_en=1、vact_timing_fps=60/120、
 * dfps_cmd_table 3 项（B9 83 12 1A / E2 10|E2 00 / B9 00 00 00）。
 * 60→120Hz 切换时 mtk_dsi_send_switch_cmd 发送该表，120Hz 档发
 * E2 00；缺失则面板保持 60Hz 配置，切换后花屏。
 */
#define HX83121A_DSC_PARAMS \
	.dsc_params = { \
		.enable = 1, \
		.bdg_dsc_enable = 0, \
		.ver = 17, \
		.slice_mode = 0, \
		.rgb_swap = 0, \
		.dsc_cfg = 34, \
		.rct_on = 1, \
		.bit_per_channel = 8, \
		.dsc_line_buf_depth = 9, \
		.bp_enable = 1, \
		.bit_per_pixel = 128, \
		.pic_height = PANEL_HEIGHT, \
		.pic_width = 800, \
		.slice_height = 20, \
		.slice_width = 800, \
		.chunk_size = 800, \
		.xmit_delay = 512, \
		.dec_delay = 657, \
		.scale_value = 32, \
		.increment_interval = 583, \
		.decrement_interval = 11, \
		.line_bpg_offset = 12, \
		.nfl_bpg_offset = 1294, \
		.slice_bpg_offset = 872, \
		.initial_offset = 6144, \
		.final_offset = 4320, \
		.flatness_minqp = 3, \
		.flatness_maxqp = 12, \
		.rc_model_size = 8192, \
		.rc_edge_factor = 6, \
		.rc_quant_incr_limit0 = 11, \
		.rc_quant_incr_limit1 = 11, \
		.rc_tgt_offset_hi = 3, \
		.rc_tgt_offset_lo = 3, \
		.rc_buf_thresh[0] = 14, \
		.rc_buf_thresh[1] = 28, \
		.rc_buf_thresh[2] = 42, \
		.rc_buf_thresh[3] = 56, \
		.rc_buf_thresh[4] = 70, \
		.rc_buf_thresh[5] = 84, \
		.rc_buf_thresh[6] = 98, \
		.rc_buf_thresh[7] = 105, \
		.rc_buf_thresh[8] = 112, \
		.rc_buf_thresh[9] = 119, \
		.rc_buf_thresh[10] = 121, \
		.rc_buf_thresh[11] = 123, \
		.rc_buf_thresh[12] = 125, \
		.rc_buf_thresh[13] = 126, \
		.rc_range_parameters[0] = { 0, 4, 2 }, \
		.rc_range_parameters[1] = { 0, 4, 0 }, \
		.rc_range_parameters[2] = { 1, 5, 0 }, \
		.rc_range_parameters[3] = { 1, 6, -2 }, \
		.rc_range_parameters[4] = { 3, 7, -4 }, \
		.rc_range_parameters[5] = { 3, 7, -6 }, \
		.rc_range_parameters[6] = { 3, 7, -8 }, \
		.rc_range_parameters[7] = { 3, 8, -8 }, \
		.rc_range_parameters[8] = { 3, 9, -8 }, \
		.rc_range_parameters[9] = { 3, 10, -10 }, \
		.rc_range_parameters[10] = { 5, 11, -10 }, \
		.rc_range_parameters[11] = { 5, 12, -12 }, \
		.rc_range_parameters[12] = { 5, 13, -12 }, \
		.rc_range_parameters[13] = { 7, 13, -12 }, \
		.rc_range_parameters[14] = { 13, 13, -12 }, \
	}

#define HX83121A_DYN_FPS_CMDS(rate_cmd) \
	.dfps_cmd_table = { \
		[0] = { .src_fps = 0, .cmd_num = 4, \
			.para_list = { 0xb9, 0x83, 0x12, 0x1a } }, \
		[1] = { .src_fps = 0, .cmd_num = 2, \
			.para_list = { 0xe2, rate_cmd } }, \
		[2] = { .src_fps = 0, .cmd_num = 4, \
			.para_list = { 0xb9, 0x00, 0x00, 0x00 } }, \
	}

static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = 485,
	.data_rate = 970,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.physical_width_um = PHYSICAL_WIDTH_UM,
	.physical_height_um = PHYSICAL_HEIGHT_UM,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dyn = {
		.switch_en = 1,
		.data_rate = 970,
		.vfp = MODE_0_VFP,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_0_FPS,
		HX83121A_DYN_FPS_CMDS(0x10),
	},
	HX83121A_DSC_PARAMS,
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 485,
	.data_rate = 970,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.physical_width_um = PHYSICAL_WIDTH_UM,
	.physical_height_um = PHYSICAL_HEIGHT_UM,
	.output_mode = MTK_PANEL_DUAL_PORT,
	.lcm_cmd_if = MTK_PANEL_DUAL_PORT,
	.dyn = {
		.switch_en = 1,
		.data_rate = 970,
		.vfp = MODE_1_VFP,
	},
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_1_FPS,
		HX83121A_DYN_FPS_CMDS(0x00),
	},
	HX83121A_DSC_PARAMS,
};

static int mtk_panel_ext_param_set(struct drm_panel *panel,
				   unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);

	if (mode > 1)
		return 1;

	/* 官核 mtk_panel_ext_param_set：mode0/1 切换两套 params */
	if (mode == 0)
		ext->params = &ext_params_60hz;
	else
		ext->params = &ext_params_120hz;
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
};
#endif

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *backlight;
	struct lcm *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	ctx->esd_te_master = -1;
	ctx->esd_te_slave = -1;

	/*
	 * 官核 lcm_probe 反汇编确认（offset 984/992）：
	 * lanes = 4, mode_flags = 0xE05；format 默认 MIPI_DSI_FMT_RGB888。
	 * 缺少这些配置会导致 DSI 以 0 lane/错误模式运行，表现为开机花屏。
	 */
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			| MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			| MIPI_DSI_CLOCK_NON_CONTINUOUS;

	mipi_dsi_set_drvdata(dsi, ctx);

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_err(dev, "%s: cannot get bias_pos %ld\n",
			__func__, PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_err(dev, "%s: cannot get bias_neg %ld\n",
			__func__, PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	/* ESD TE pins, requested as inputs for future ESD support */
	ctx->esd_te_master = of_get_named_gpio(dev->of_node,
					       "tct_esd_te_master", 0);
	if (gpio_is_valid(ctx->esd_te_master))
		gpio_request_one(ctx->esd_te_master, GPIOF_IN,
				 "lcm_esd_te_master");
	ctx->esd_te_slave = of_get_named_gpio(dev->of_node,
					      "tct_esd_te_slave", 0);
	if (gpio_is_valid(ctx->esd_te_slave))
		gpio_request_one(ctx->esd_te_slave, GPIOF_IN,
				 "lcm_esd_te_slave");

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	ret = drm_panel_add(&ctx->panel);
	if (ret < 0) {
		dev_err(dev, "%s: drm_panel_add failed: %d\n", __func__, ret);
		return ret;
	}

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "%s: mipi_dsi_attach failed: %d\n", __func__, ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs,
				   &ctx->panel);
	if (ret < 0) {
		dev_err(dev, "%s: mtk_panel_ext_create failed: %d\n",
			__func__, ret);
		return ret;
	}
#endif

	pr_info("%s: %s probed\n", __func__, PANEL_DRIVER_NAME);
	return 0;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = PANEL_COMPATIBLE },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = PANEL_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("TALPAD-BOOM / reconstructed from OEM kernel");
MODULE_DESCRIPTION("Lushan12 HX83121A CDOT CSOT WQXGA DSI VDO panel driver");
MODULE_LICENSE("GPL v2");
