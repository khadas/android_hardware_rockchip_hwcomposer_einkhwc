//#define LOG_NDEBUG 0
#define LOG_TAG "Utils"
#include <utils/Log.h>

#include <string.h>
#include <errno.h>
#include <drmrga.h>
#include <RgaApi.h>
#include "mpp_mem.h"
#include "Utils.h"

static int rga_init = 0;

void dump_mpp_frame_to_file(MppFrame frame, FILE *fp)
{
    RK_U32 width    = 0;
    RK_U32 height   = 0;
    RK_U32 h_stride = 0;
    RK_U32 v_stride = 0;
    MppFrameFormat fmt  = MPP_FMT_YUV420SP;
    MppBuffer buffer    = NULL;
    RK_U8 *base = NULL;

    if (NULL == fp || NULL == frame)
        return;

    width    = mpp_frame_get_width(frame);
    height   = mpp_frame_get_height(frame);
    h_stride = mpp_frame_get_hor_stride(frame);
    v_stride = mpp_frame_get_ver_stride(frame);
    fmt      = mpp_frame_get_fmt(frame);
    buffer   = mpp_frame_get_buffer(frame);

    if (NULL == buffer)
        return;

    base = (RK_U8 *)mpp_buffer_get_ptr(buffer);

    switch (fmt) {
    case MPP_FMT_YUV422SP : {
        /* YUV422SP -> YUV422P for better display */
        RK_U32 i, j;
        RK_U8 *base_y = base;
        RK_U8 *base_c = base + h_stride * v_stride;
        RK_U8 *tmp = mpp_malloc(RK_U8, h_stride * height * 2);
        RK_U8 *tmp_u = tmp;
        RK_U8 *tmp_v = tmp + width * height / 2;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width, fp);

        for (i = 0; i < height; i++, base_c += h_stride) {
            for (j = 0; j < width / 2; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width / 2;
            tmp_v += width / 2;
        }

        fwrite(tmp, 1, width * height, fp);
        mpp_free(tmp);
    } break;
    case MPP_FMT_YUV420SP : {
        RK_U32 i;
        RK_U8 *base_y = base;
        RK_U8 *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride) {
            fwrite(base_c, 1, width, fp);
        }
    } break;
    case MPP_FMT_YUV420P : {
        RK_U32 i;
        RK_U8 *base_y = base;
        RK_U8 *base_c = base + h_stride * v_stride;

        for (i = 0; i < height; i++, base_y += h_stride) {
            fwrite(base_y, 1, width, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, fp);
        }
        for (i = 0; i < height / 2; i++, base_c += h_stride / 2) {
            fwrite(base_c, 1, width / 2, fp);
        }
    } break;
    case MPP_FMT_YUV444SP : {
        /* YUV444SP -> YUV444P for better display */
        RK_U32 i, j;
        RK_U8 *base_y = base;
        RK_U8 *base_c = base + h_stride * v_stride;
        RK_U8 *tmp = mpp_malloc(RK_U8, h_stride * height * 2);
        RK_U8 *tmp_u = tmp;
        RK_U8 *tmp_v = tmp + width * height;

        for (i = 0; i < height; i++, base_y += h_stride)
            fwrite(base_y, 1, width, fp);

        for (i = 0; i < height; i++, base_c += h_stride * 2) {
            for (j = 0; j < width; j++) {
                tmp_u[j] = base_c[2 * j + 0];
                tmp_v[j] = base_c[2 * j + 1];
            }
            tmp_u += width;
            tmp_v += width;
        }

        fwrite(tmp, 1, width * height * 2, fp);
        mpp_free(tmp);
    } break;
    default : {
        ALOGE("not supported format %d", fmt);
    } break;
    }
}

MPP_RET get_file_ptr(const char *file_name, char **buf, size_t *size)
{
    FILE *fp = NULL;
    size_t file_size = 0;

    fp = fopen(file_name, "rb");
    if (NULL == fp) {
        ALOGE("failed to open file %s - %s", file_name, strerror(errno));
        return MPP_NOK;
    }

    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    rewind(fp);

    *buf = (char*)malloc(file_size);
    if (NULL == *buf) {
        ALOGE("failed to malloc buffer - file %s", file_name);
        fclose(fp);
        return MPP_NOK;
    }

    fread(*buf, 1, file_size, fp);
    *size = file_size;
    fclose(fp);

    return MPP_OK;
}

MPP_RET dump_ptr_to_file(char *buf, size_t size, const char *output_file)
{
    FILE *fp = NULL;

    fp = fopen(output_file, "w+b");
    if (NULL == fp) {
        ALOGE("failed to open file %s - %s", output_file, strerror(errno));
        return MPP_NOK;
    }

    fwrite(buf, 1, size, fp);
    fflush(fp);
    fclose(fp);

    return MPP_OK;
}

MPP_RET crop_yuv_image(RK_U8 *src, RK_U8 *dst, RK_U32 src_width, RK_U32 src_height,
                       RK_U32 src_wstride, RK_U32 src_hstride,
                       RK_U32 dst_width, RK_U32 dst_height)
{
    RK_U32 ret = 0;
    void *rga_ctx = NULL;
    RK_U32 srcFormat, dstFormat;
    rga_info_t rgasrc, rgadst;

    if (!rga_init) {
        RgaInit(&rga_ctx);
        if (NULL == rga_ctx) {
            ALOGW("failed to init rga ctx");
            return MPP_NOK;
        } else {
            ALOGD("init rga ctx done");
            rga_init = 1;
        }
    }

    srcFormat = dstFormat = HAL_PIXEL_FORMAT_YCrCb_NV12;

    memset(&rgasrc, 0, sizeof(rga_info_t));
    rgasrc.fd = -1;
    rgasrc.mmuFlag = 1;
    rgasrc.virAddr = src;

    memset(&rgadst, 0, sizeof(rga_info_t));
    rgadst.fd = -1;
    rgadst.mmuFlag = 1;
    rgadst.virAddr = dst;

    rga_set_rect(&rgasrc.rect, 0, 0, src_width, src_height,
                 src_wstride, src_hstride, srcFormat);
    rga_set_rect(&rgadst.rect, 0, 0, dst_width, dst_height,
                 dst_width, dst_height, srcFormat);

    ret = RgaBlit(&rgasrc, &rgadst, NULL);
    if (ret) {
        ALOGE("failed to rga blit ret %d", ret);
        return MPP_NOK;
    }

    return MPP_OK;
}

MPP_RET read_yuv_image(RK_U8 *buf, FILE *fp, RK_U32 width, RK_U32 height,
                       RK_U32 hor_stride, RK_U32 ver_stride, MppFrameFormat fmt)
{
    MPP_RET ret = MPP_OK;
    RK_U32 read_size;
    RK_U32 row = 0;
    RK_U8 *buf_y = buf;
    RK_U8 *buf_u = buf_y + hor_stride * ver_stride; // NOTE: diff from gen_yuv_image
    RK_U8 *buf_v = buf_u + hor_stride * ver_stride / 4; // NOTE: diff from gen_yuv_image

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        for (row = 0; row < height; row++) {
            read_size = fread(buf_y + row * hor_stride, 1, width, fp);
            if (read_size != width) {
                ALOGE("read ori yuv file luma failed");
                ret  = MPP_NOK;
                goto err;
            }
        }

        for (row = 0; row < height / 2; row++) {
            read_size = fread(buf_u + row * hor_stride, 1, width, fp);
            if (read_size != width) {
                ALOGE("read ori yuv file cb failed");
                ret  = MPP_NOK;
                goto err;
            }
        }
    } break;
    case MPP_FMT_YUV420P : {
        for (row = 0; row < height; row++) {
            read_size = fread(buf_y + row * hor_stride, 1, width, fp);
            if (read_size != width) {
                ALOGE("read ori yuv file luma failed");
                ret  = MPP_NOK;
                goto err;
            }
        }

        for (row = 0; row < height / 2; row++) {
            read_size = fread(buf_u + row * hor_stride / 2, 1, width / 2, fp);
            if (read_size != width / 2) {
                ALOGE("read ori yuv file cb failed");
                ret  = MPP_NOK;
                goto err;
            }
        }

        for (row = 0; row < height / 2; row++) {
            read_size = fread(buf_v + row * hor_stride / 2, 1, width / 2, fp);
            if (read_size != width / 2) {
                ALOGE("read ori yuv file cr failed");
                ret  = MPP_NOK;
                goto err;
            }
        }
    } break;
    case MPP_FMT_ARGB8888 : {
        for (row = 0; row < height; row++) {
            read_size = fread(buf_y + row * hor_stride * 4, 1, width * 4, fp);
        }
    } break;
    case MPP_FMT_YUV422_YUYV :
    case MPP_FMT_YUV422_UYVY : {
        for (row = 0; row < height; row++) {
            read_size = fread(buf_y + row * hor_stride, 1, width * 2, fp);
        }
    } break;
    default : {
        ALOGE("read image do not support fmt %d", fmt);
        ret = MPP_ERR_VALUE;
    } break;
    }

err:
    return ret;
}

MPP_RET fill_yuv_image(RK_U8 *buf, RK_U32 width, RK_U32 height,
                       RK_U32 hor_stride, RK_U32 ver_stride, MppFrameFormat fmt,
                       RK_U32 frame_count)
{
    MPP_RET ret = MPP_OK;
    RK_U8 *buf_y = buf;
    RK_U8 *buf_c = buf + hor_stride * ver_stride;
    RK_U32 x, y;

    switch (fmt) {
    case MPP_FMT_YUV420SP : {
        RK_U8 *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width; x++) {
                p[x] = x + y + frame_count * 3;
            }
        }

        p = buf_c;
        for (y = 0; y < height / 2; y++, p += hor_stride) {
            for (x = 0; x < width / 2; x++) {
                p[x * 2 + 0] = 128 + y + frame_count * 2;
                p[x * 2 + 1] = 64  + x + frame_count * 5;
            }
        }
    } break;
    case MPP_FMT_YUV420P : {
        RK_U8 *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width; x++) {
                p[x] = x + y + frame_count * 3;
            }
        }

        p = buf_c;
        for (y = 0; y < height / 2; y++, p += hor_stride / 2) {
            for (x = 0; x < width / 2; x++) {
                p[x] = 128 + y + frame_count * 2;
            }
        }

        p = buf_c + hor_stride * ver_stride / 4;
        for (y = 0; y < height / 2; y++, p += hor_stride / 2) {
            for (x = 0; x < width / 2; x++) {
                p[x] = 64 + x + frame_count * 5;
            }
        }
    } break;
    case MPP_FMT_YUV422_UYVY : {
        RK_U8 *p = buf_y;

        for (y = 0; y < height; y++, p += hor_stride) {
            for (x = 0; x < width / 2; x++) {
                p[x * 4 + 1] = x * 2 + 0 + y + frame_count * 3;
                p[x * 4 + 3] = x * 2 + 1 + y + frame_count * 3;
                p[x * 4 + 0] = 128 + y + frame_count * 2;
                p[x * 4 + 2] = 64  + x + frame_count * 5;
            }
        }
    } break;
    default : {
        ALOGE("filling function do not support type %d", fmt);
        ret = MPP_NOK;
    } break;
    }
    return ret;
}
