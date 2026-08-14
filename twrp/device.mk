# TALIH-PD2 TWRP device
# 无需 vendor blobs（TWRP 独立 recovery ramdisk，直接使用预编译内核 + dtb）
# TWRP-in-boot: 交付物为 boot 镜像（boot-twrp.img）

# 带 health 的模块（HAL/service/vintf manifest）会安装到 /vendor 路径，
# 与 recovery root 的 vendor 符号链接冲突导致 ramdisk 打包失败；
# TWRP 直接读 sysfs 电量，不需要 health HAL，全部剔除
PRODUCT_PACKAGES := $(filter-out %health%,$(PRODUCT_PACKAGES))
