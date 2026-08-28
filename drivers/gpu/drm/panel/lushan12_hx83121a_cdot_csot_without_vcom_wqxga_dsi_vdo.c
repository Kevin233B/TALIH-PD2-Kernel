/* SPDX-License-Identifier: GPL-2.0 */
/*
 * TALPAD (TALIH-PD2, ls12_mt8797_wifi_64) HX83121A CSOT panel,
 * WITHOUT_VCOM variant. Implementation is shared with the with_vcom
 * variant in lushan12_hx83121a_dsi_vdo_common.c.
 */
#define PANEL_DRIVER_NAME	"lushan12_hx83121a_cdot_csot_without_vcom_wqxga_dsi_vdo_lcm_drv"
#define PANEL_COMPATIBLE	"lushan12,hx83121a_cdot_csot_without_vcom_wqxga_dsi_vdo"
#define PANEL_HAS_EXTRA_VCOM_CMD	0

#include "lushan12_hx83121a_dsi_vdo_common.c"
