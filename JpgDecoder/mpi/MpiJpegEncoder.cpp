//#define LOG_NDEBUG 0
#define LOG_TAG "MpiJpegEncoder"
#include <utils/Log.h>

#include <stdlib.h>
#include "mpp_err.h"
#include "Utils.h"
#include "MpiJpegEncoder.h"

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

MPP_RET MpiJpegEncoder::initMppCtx(MpiEncCtx **data, EncParam param) {
    MpiEncCtx *p = NULL;
    MPP_RET ret = MPP_OK;

    if (!data) {
        ALOGE("invalid input data %p", data);
        return MPP_ERR_NULL_PTR;
    }

    p = (MpiEncCtx*) malloc(sizeof(MpiEncCtx));
    if (!p) {
        ALOGE("create MpiEncCtx failed");
        ret = MPP_ERR_MALLOC;
        goto RET;
    }

    // setup required parameter
    p->width        = param.input_width;
    p->height       = param.input_height;
    p->hor_stride   = MPP_ALIGN(param.input_width, 16);
    p->ver_stride   = MPP_ALIGN(param.input_height, 16);
    p->input_fmt    = param.input_fmt;

    p->fp_input = fopen(param.input_file, "rb");
    if (NULL == p->fp_input) {
        ALOGE("failed to open input file %s", param.input_file);
        ret = MPP_ERR_OPEN_FILE;
    }

    p->fp_output = fopen(param.output_file, "w+b");
    if (NULL == p->fp_output) {
        ALOGE("failed to open output file %s", param.output_file);
        ret = MPP_ERR_OPEN_FILE;
    }

    // update resource parameter
    if (p->input_fmt <= MPP_FMT_YUV420SP_VU)
        p->frame_size = p->hor_stride * p->ver_stride * 3 / 2;
    else if (p->input_fmt <= MPP_FMT_YUV422_UYVY) {
        // NOTE: yuyv and uyvy need to double stride
        p->hor_stride *= 2;
        p->frame_size = p->hor_stride * p->ver_stride;
    } else
        p->frame_size = p->hor_stride * p->ver_stride * 4;
    p->packet_size  = p->width * p->height;

RET:
    *data = p;
    return ret;
}

MPP_RET MpiJpegEncoder::deinitMppCtx(MpiEncCtx **data) {
    MpiEncCtx *p = NULL;

    if (!data) {
        ALOGE("invalid input data %p", data);
        return MPP_ERR_NULL_PTR;
    }

    p = *data;
    if (p) {
        if (p->fp_input) {
            fclose(p->fp_input);
            p->fp_input = NULL;
        }
        if (p->fp_output) {
            fclose(p->fp_output);
            p->fp_output = NULL;
        }
        free(p);
        p = NULL;
    }

    return MPP_OK;
}

MPP_RET MpiJpegEncoder::setupMppCfg(MpiEncCtx *p) {
    MPP_RET ret;
    MppApi *mpi;
    MppCtx ctx;
    MppEncCodecCfg *codec_cfg;
    MppEncPrepCfg *prep_cfg;
    MppEncRcCfg *rc_cfg;

    if (NULL == p)
        return MPP_ERR_NULL_PTR;

    mpi = p->mpi;
    ctx = p->ctx;
    codec_cfg = &p->codec_cfg;
    prep_cfg = &p->prep_cfg;
    rc_cfg = &p->rc_cfg;

    /* setup default parameter */
    p->fps = 30;
    p->gop = 60;
    p->bps = p->width * p->height / 8 * p->fps;

    prep_cfg->change        = MPP_ENC_PREP_CFG_CHANGE_INPUT |
            MPP_ENC_PREP_CFG_CHANGE_ROTATION |
            MPP_ENC_PREP_CFG_CHANGE_FORMAT;
    prep_cfg->width         = p->width;
    prep_cfg->height        = p->height;
    prep_cfg->hor_stride    = p->hor_stride;
    prep_cfg->ver_stride    = p->ver_stride;
    prep_cfg->format        = p->input_fmt;
    prep_cfg->rotation      = MPP_ENC_ROT_0;
    ret = mpi->control(ctx, MPP_ENC_SET_PREP_CFG, prep_cfg);
    if (ret) {
        ALOGE("mpi control enc set prep cfg failed ret %d", ret);
        goto RET;
    }

    rc_cfg->change  = MPP_ENC_RC_CFG_CHANGE_ALL;
    rc_cfg->rc_mode = MPP_ENC_RC_MODE_CBR;
    rc_cfg->quality = MPP_ENC_RC_QUALITY_MEDIUM;

    if (rc_cfg->rc_mode == MPP_ENC_RC_MODE_CBR) {
        /* constant bitrate has very small bps range of 1/16 bps */
        rc_cfg->bps_target   = p->bps;
        rc_cfg->bps_max      = p->bps * 17 / 16;
        rc_cfg->bps_min      = p->bps * 15 / 16;
    } else if (rc_cfg->rc_mode ==  MPP_ENC_RC_MODE_VBR) {
        if (rc_cfg->quality == MPP_ENC_RC_QUALITY_CQP) {
            /* constant QP does not have bps */
            rc_cfg->bps_target   = -1;
            rc_cfg->bps_max      = -1;
            rc_cfg->bps_min      = -1;
        } else {
            /* variable bitrate has large bps range */
            rc_cfg->bps_target   = p->bps;
            rc_cfg->bps_max      = p->bps * 17 / 16;
            rc_cfg->bps_min      = p->bps * 1 / 16;
        }
    }

    /* fix input / output frame rate */
    rc_cfg->fps_in_flex      = 0;
    rc_cfg->fps_in_num       = p->fps;
    rc_cfg->fps_in_denorm    = 1;
    rc_cfg->fps_out_flex     = 0;
    rc_cfg->fps_out_num      = p->fps;
    rc_cfg->fps_out_denorm   = 1;

    rc_cfg->gop              = p->gop;
    rc_cfg->skip_cnt         = 0;

    ALOGD("mpi_jpeg_enc bps %d fps %d gop %d",
          rc_cfg->bps_target, rc_cfg->fps_out_num, rc_cfg->gop);

    ret = mpi->control(ctx, MPP_ENC_SET_RC_CFG, rc_cfg);
    if (ret) {
        ALOGE("mpi control enc set rc cfg failed ret %d", ret);
        goto RET;
    }

    codec_cfg->coding = MPP_VIDEO_CodingMJPEG;
    codec_cfg->jpeg.change  = MPP_ENC_JPEG_CFG_CHANGE_QP;
    codec_cfg->jpeg.quant   = 10;

    ret = mpi->control(ctx, MPP_ENC_SET_CODEC_CFG, codec_cfg);
    if (ret) {
        ALOGE("mpi control enc set codec cfg failed ret %d", ret);
        goto RET;
    }

    /* optional */
    p->sei_mode = MPP_ENC_SEI_MODE_ONE_FRAME;
    ret = mpi->control(ctx, MPP_ENC_SET_SEI_CFG, &p->sei_mode);
    if (ret) {
        ALOGE("mpi control enc set sei cfg failed ret %d", ret);
        goto RET;
    }

RET:
    return ret;
}

MPP_RET MpiJpegEncoder::startMppProcess(MpiEncCtx *p) {
    MPP_RET ret = MPP_OK;
    MppApi *mpi;
    MppCtx ctx;

    if (NULL == p)
        return MPP_ERR_NULL_PTR;

    mpi = p->mpi;
    ctx = p->ctx;

    while (!p->pkt_eos) {
        MppFrame frame = NULL;
        MppPacket packet = NULL;
        void *buf = mpp_buffer_get_ptr(p->frm_buf);

        if (p->fp_input) {
            ret = read_yuv_image((RK_U8*)buf, p->fp_input, p->width, p->height,
                                 p->hor_stride, p->ver_stride, p->input_fmt);
            if (ret == MPP_NOK || feof(p->fp_input)) {
                ALOGE("found last frame. feof %d", feof(p->fp_input));
                p->frm_eos = 1;
            } else if (ret == MPP_ERR_VALUE)
                goto RET;
        } else {
            ret == MPP_ERR_VALUE;
            goto RET;
        }

        ret = mpp_frame_init(&frame);
        if (ret) {
            ALOGE("mpp_frame_init failed");
            goto RET;
        }

        mpp_frame_set_width(frame, p->width);
        mpp_frame_set_height(frame, p->height);
        mpp_frame_set_hor_stride(frame, p->hor_stride);
        mpp_frame_set_ver_stride(frame, p->ver_stride);
        mpp_frame_set_fmt(frame, p->input_fmt);
        mpp_frame_set_buffer(frame, p->frm_buf);
        mpp_frame_set_eos(frame, p->frm_eos);

        ret = mpi->encode_put_frame(ctx, frame);
        if (ret) {
            ALOGE("mpp encode put frame failed");
            goto RET;
        }

        ret = mpi->encode_get_packet(ctx, &packet);
        if (ret) {
            ALOGE("mpp encode get packet failed");
            goto RET;
        }

        if (packet) {
            // write packet to file here
            void *ptr  = mpp_packet_get_pos(packet);
            size_t len = mpp_packet_get_length(packet);

            p->pkt_eos = mpp_packet_get_eos(packet);

            if (p->fp_output)
                fwrite(ptr, 1, len, p->fp_output);
            mpp_packet_deinit(&packet);

            ALOGD("encoded frame %d size %d", p->frame_count, len);
            p->stream_size += len;
            p->frame_count++;

            if (p->pkt_eos) {
                ALOGD("found last packet");
                //mpp_assert(p->frm_eos);
            }
        }

        if (p->frm_eos && p->pkt_eos)
            break;
    }

RET:
    return ret;
}

bool MpiJpegEncoder::start(EncParam param) {
    MPP_RET ret = MPP_OK;
    MpiEncCtx *p = NULL;

    ALOGD("mpi_jpeg_enc start");

    ret = initMppCtx(&p, param);
    if (ret) {
        ALOGE("MpiEncCtx init failed");
        goto MPP_TEST_OUT;
    }

    ret = mpp_buffer_get(NULL, &p->frm_buf, p->frame_size);
    if (ret) {
        ALOGE("failed to get buffer for input frame ret %d", ret);
        goto MPP_TEST_OUT;
    }

    ALOGD("mpi_jpeg_enc test start w %d h %d", p->width, p->height);

    // encoder demo
    ret = mpp_create(&p->ctx, &p->mpi);
    if (ret) {
        ALOGE("mpp_create failed ret %d", ret);
        goto MPP_TEST_OUT;
    }

    ret = mpp_init(p->ctx, MPP_CTX_ENC, MPP_VIDEO_CodingMJPEG);
    if (ret) {
        ALOGE("mpp_init failed ret %d", ret);
        goto MPP_TEST_OUT;
    }

    ret = setupMppCfg(p);
    if (ret) {
        ALOGE("mpp setup failed ret %d", ret);
        goto MPP_TEST_OUT;
    }

    ret = startMppProcess(p);
    if (ret) {
        ALOGE("test mpp run failed ret %d", ret);
        goto MPP_TEST_OUT;
    }

    ret = p->mpi->reset(p->ctx);
    if (ret) {
        ALOGE("mpi->reset failed");
        goto MPP_TEST_OUT;
    }

MPP_TEST_OUT:
    if (p->ctx) {
        mpp_destroy(p->ctx);
        p->ctx = NULL;
    }

    if (p->frm_buf) {
        mpp_buffer_put(p->frm_buf);
        p->frm_buf = NULL;
    }

    if (MPP_OK == ret)
        ALOGE("mpi_enc_test success total frame %d bps %lld",
              p->frame_count, (RK_U64)((p->stream_size * 8 * p->fps) / p->frame_count));
    else
        ALOGE("mpi_enc_test failed ret %d", ret);

    deinitMppCtx(&p);

    return ret == MPP_OK ? true : false;
}
