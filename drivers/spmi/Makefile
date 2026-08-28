# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for kernel SPMI framework.
#
obj-$(CONFIG_SPMI)	+= spmi.o

obj-$(CONFIG_SPMI_MSM_PMIC_ARB)	+= spmi-pmic-arb.o

# MTK PMIF SPMI controller (vendor 增量)：mt6315 等 SPMI PMIC 走此总线。
# spmi-mtk-pmif-core.c 的 <aee.h> 在 mt-plat 头目录下，需额外 -I 路径。
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
obj-$(CONFIG_SPMI_MTK_PMIF)	+= spmi-mtk-pmif.o spmi-mtk-mpu.o
spmi-mtk-pmif-objs := spmi-mtk-pmif-core.o spmi-mtk-debug.o
