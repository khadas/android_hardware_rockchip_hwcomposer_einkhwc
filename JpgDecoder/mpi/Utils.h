#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdio.h>

#include "mpp_err.h"
#include "mpp_frame.h"

void dump_mpp_frame_to_file(MppFrame frame, FILE *fp);

MPP_RET get_file_ptr(const char *file_name, char **buf, size_t *size);
MPP_RET dump_ptr_to_file(char *buf, size_t size, const char *output_file);

MPP_RET crop_yuv_image(RK_U8 *src, RK_U8* dst, RK_U32 src_width, RK_U32 src_height,
                       RK_U32 src_wstride, RK_U32 src_hstride,
                       RK_U32 dst_width, RK_U32 dst_height);

MPP_RET read_yuv_image(RK_U8 *buf, FILE *fp, RK_U32 width, RK_U32 height,
                       RK_U32 hor_stride, RK_U32 ver_stride,
                       MppFrameFormat fmt);
MPP_RET fill_yuv_image(RK_U8 *buf, RK_U32 width, RK_U32 height,
                       RK_U32 hor_stride, RK_U32 ver_stride, MppFrameFormat fmt,
                       RK_U32 frame_count);

#endif //__UTILS_H__
