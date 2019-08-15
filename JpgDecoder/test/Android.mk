LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CPPFLAGS := \
	-std=c++11 \
	-fno-threadsafe-statics \

LOCAL_SRC_FILES := \
	jpeg_dec_test.cpp \

LOCAL_SHARED_LIBRARIES := \
	liblog \
	libhwjpeg \

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../inc/mpp_inc/ \
	$(LOCAL_PATH)/../inc \

LOCAL_MULTILIB := 32
LOCAL_MODULE := jpegd_test

include $(BUILD_EXECUTABLE)
