# TALIH-PD2 TWRP device
# 无需 vendor blobs（TWRP 独立 recovery ramdisk，直接使用预编译内核 + dtb）
# TWRP-in-boot: 交付物为 boot 镜像（boot-twrp.img）

# aosp_base 会带 health HAL（安装到 /vendor 路径），会与 recovery root 的
# vendor 符号链接冲突导致 ramdisk 打包失败；TWRP 直接读 sysfs 电量，不需要它
PRODUCT_PACKAGES := $(filter-out \
    android.hardware.health@2.1-service \
    android.hardware.health@2.1-impl \
    android.hardware.health@2.0-service \
    android.hardware.health@2.0-impl,$(PRODUCT_PACKAGES))
