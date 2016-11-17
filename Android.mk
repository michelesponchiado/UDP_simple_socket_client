# Copyright 2005 The Android Open Source Project
#
# Android.mk for dataSendRcv
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	ASACSOCKET_check.c		\
	crc32.c				\
	main_client.c			

#LOCAL_CFLAGS += -DTS_DEVICE=$(TARGET_TS_DEVICE)



LOCAL_MODULE := ASACZ_UDP_test
LOCAL_MODULE_TAGS := eng

LOCAL_C_INCLUDES := 	$(LOCAL_PATH)/

LOCAL_CFLAGS += -DANDROID

LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_SBIN)
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES += libcutils libc libm
LOCAL_LDLIBS := -lpthread -lrt

include $(BUILD_EXECUTABLE)


