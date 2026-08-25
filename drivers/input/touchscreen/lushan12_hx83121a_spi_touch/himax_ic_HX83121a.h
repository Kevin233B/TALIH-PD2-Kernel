/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83121A chipset
 *
 *  Copyright (C) 2022 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>


#define HX_83121A_SERIES_PWON		"HX83121A"

#define HX83121A_DATA_ADC_NUM 1280 /* 640 * 2 */

#define HX83121A_ADDR_RAWOUT_SEL     0x100072ec

// #define HX83121A_ADDR_TCON_ON_RST 0x80020004

#define HX83121A_REG_ICID 0x900000d0
#define HX83121A_ISRAM_SZ 131072
#define HX83121A_DSRAM_SZ 131072
#define HX83121A_FLASH_SIZE 261120

#define HX83121A_PARA_AHB_INC4       0x12
