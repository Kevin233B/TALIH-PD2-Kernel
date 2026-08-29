// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/* lk 传给内核的 atag,videolfb 解析（lcm 在位/vram/fb_base/fps）。
 * 原实现位于 mtk_drm_fbdev.c（被 CONFIG_DRM_FBDEV_EMULATION 门控编译），
 * 但 mtk_drm_crtc.c / mtk_drm_drv.c / mtk_disp_recovery.c 无条件引用这三个
 * 符号——GKI 硬门禁要求 FBDEV 关闭（struct fb_info 门控字段会漂移冻结 KMI
 * CRC，见 ls12_gki.fragment 注释）时链接报 undefined。这三个函数均为纯
 * device-tree 解析、零 fbdev API 依赖，故拆出为无条件编译的独立对象。
 */
#include <linux/io.h>
#include <linux/of.h>

#include "mtk_drm_drv.h"
#include "mtk_log.h"

bool mtk_drm_lcm_is_connect(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb", (int *)&size);
		if (videolfb_tag)
			return videolfb_tag->islcmfound;

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}

	return false;
}

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps)
{
#ifdef CONFIG_MTK_DISP_NO_LK
		return -1;
#else
	struct device_node *chosen_node;

	*fps = 6000;
	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb", (int *)&size);
		if (videolfb_tag) {
			*vramsize = videolfb_tag->vram;
			*fb_base = videolfb_tag->fb_base;
			*fps = videolfb_tag->fps;
			if (*fps == 0)
				*fps = 6000;
			return 0;
		}

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
		goto found;
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}
	return -1;

found:
	DDPINFO("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)*fb_base);
	DDPINFO("[DT][videolfb] vram       = 0x%x (%d)\n", *vramsize,
		*vramsize);
	DDPINFO("[DT][videolfb] fps	   = %d\n", *fps);

	return 0;
#endif
}

int free_fb_buf(void)
{
	unsigned long va_start = 0;
	unsigned long va_end = 0;
	phys_addr_t fb_base;
	unsigned int vramsize, fps;

	_parse_tag_videolfb(&vramsize, &fb_base, &fps);

	if (!fb_base) {
		DDPINFO("%s:get fb pa error\n", __func__);
		return -1;
	}

	va_start = (unsigned long)__va(fb_base);
	va_end = (unsigned long)__va(fb_base + (unsigned long)vramsize);
	if (va_start)
		//free_reserved_area((void *)va_start,
		//		   (void *)va_end, 0xff, "fbmem");
		;
	else
		DDPINFO("%s:va invalid\n", __func__);

	return 0;
}
