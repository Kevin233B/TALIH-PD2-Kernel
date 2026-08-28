
# Copyright (C) 2016 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# gpufreq 平台头 mtk_gpufreq_v1.h 靠 -DCONFIG_GPU_MT68xx 做平台分支，
# 该宏仅在 drivers/gpu/mediatek/Makefile 注入，覆盖不到本 misc 目录，
# 导致 mt_gpufreq_* 声明缺失(-Wimplicit-function-declaration)。
# 这里按 CONFIG_MTK_GPU_VERSION 推导平台并同步注入。
ifneq ($(CONFIG_MTK_GPU_VERSION),"")
MTK_PLATFORM := $(word 4,$(subst ",,$(CONFIG_MTK_GPU_VERSION)))
ifeq ($(MTK_PLATFORM),mt6893)
ccflags-y += -DCONFIG_GPU_MT6893
endif
endif

ccflags-y += -I$(srctree)/drivers/gpu/mediatek/gpufreq/
ccflags-y += -I$(srctree)/include/linux/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eemgpu/common/inc/

obj-${CONFIG_MTK_PTPOD_GPU} += mtk_eem.o
mtk_eem-y += \
	mtk_eemgpu.o \
	mtk_eemgpu_platform.o \
	mtk_eemgpu_internal.o
