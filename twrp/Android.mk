LOCAL_PATH := $(call my-dir)

ifeq ($(TARGET_DEVICE),pd2)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
