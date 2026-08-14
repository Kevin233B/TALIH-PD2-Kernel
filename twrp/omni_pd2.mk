# Inherit common Omni (TWRP) config
$(call inherit-product, vendor/omni/config/common.mk)
$(call inherit-product, device/talih/pd2/device.mk)

PRODUCT_NAME := omni_pd2
PRODUCT_DEVICE := pd2
PRODUCT_BRAND := TALIH
PRODUCT_MODEL := TALIH-PD2
PRODUCT_MANUFACTURER := TAL
