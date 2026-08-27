# SPDX-License-Identifier: GPL-2.0

ifeq ($(CONFIG_MTK_GCOV_KERNEL),y)
GCOV_PROFILE := y
endif
DSP_VERSION := hifi3_v2

# adsp include path
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/audio_ipi/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/adsp/include

# common driver
obj-$(CONFIG_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o
snd-soc-mtk-common-objs += mtk-afe-platform-driver.o mtk-afe-fe-dai.o mtk-mmap-ion.o
snd-soc-mtk-common-$(CONFIG_MTK_ECCCI_DRIVER) += mtk-usip.o
snd-soc-mtk-common-$(CONFIG_SND_SOC_MTK_SRAM) += mtk-sram-manager.o
snd-soc-mtk-common-$(CONFIG_SND_SOC_MTK_SMART_PHONE) += mtk-sp-pcm-ops.o mtk-afe-debug.o

# functions for external modules
obj-y += mtk-afe-external.o

# speaker dynamic detect
# 内建机器驱动 mt6885/mt6853/mt6895/mt6833 及 audio_dsp 无条件引用
# mtk_spk_get_type/mtk_spk_get_i2s_*_type/mtk_spk_update_info（4.19 main 里本文件
# 是并入 snd-soc-mtk-sp-common 内建的），若走 obj-m 则 vmlinux 链接 undefined。
obj-y += mtk-sp-spk-amp.o

# BT CVSD driver
obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o
