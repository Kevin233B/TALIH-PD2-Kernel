# TALIH-PD2 TWRP (AOSP minimal manifest)
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/aosp_base.mk)

$(call inherit-product, device/talih/pd2/device.mk)

# 设备标识
PRODUCT_DEVICE := pd2
PRODUCT_NAME := twrp_pd2
PRODUCT_BRAND := TALIH
PRODUCT_MODEL := TALIH-PD2
PRODUCT_MANUFACTURER := TAL
