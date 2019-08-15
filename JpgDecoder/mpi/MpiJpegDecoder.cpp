#define LOG_NDEBUG 0
#define LOG_TAG "MpiJpegDecoder"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include <mpp_err.h>
#include <rk_type.h>
#include <sys/time.h>

#include "MpiJpegDecoder.h"
#include "QList.h"
#include "JpegParser.h"
#include "Utils.h"

#define DEBUG_TIMING
//#define OUTPUT_CROP

#define _ALIGN(x, a)            (((x)+(a)-1)&~((a)-1))

typedef struct {
    struct timeval start;
    struct timeval end;
} DebugTimeInfo;

static DebugTimeInfo time_info;

static void time_start_record()
{
#ifdef DEBUG_TIMING
    gettimeofday(&time_info.start, NULL);
#endif
}

static void time_end_record(const char *task)
{
#ifdef DEBUG_TIMING
    gettimeofday(&time_info.end, NULL);
    ALOGD("%s consumes %ld ms", task,
          (time_info.end.tv_sec  - time_info.start.tv_sec)  * 1000 +
          (time_info.end.tv_usec - time_info.start.tv_usec) / 1000);
#endif
}

MpiJpegDecoder::MpiJpegDecoder()     :
    mpp_ctx(NULL),
    mpi(NULL),
    initOK(0),
    mPackets(NULL),
    mFrames(NULL),
    mPacketGroup(NULL),
    mFrameGroup(NULL)
{
    // Output Format set to YUV420SP default
    mOutputFmt = OUT_FORMAT_ARGB;
    mBpp = 4;
}

MpiJpegDecoder::~MpiJpegDecoder()
{
    if (mpp_ctx) {
        mpp_destroy(mpp_ctx);
        mpp_ctx = NULL;
    }

    if (mPackets) {
        delete mPackets;
        mPackets = NULL;
    }
    if (mFrames) {
        delete mFrames;
        mFrames = NULL;
    }
    if (mPacketGroup) {
        mpp_buffer_group_put(mPacketGroup);
        mPacketGroup = NULL;
    }
    if (mFrameGroup) {
        mpp_buffer_group_put(mFrameGroup);
        mFrameGroup = NULL;
    }
}

bool MpiJpegDecoder::prepareDecoder(OutputFormat fmt)
{
    MPP_RET ret         = MPP_OK;

    MppParam param      = NULL;
    uint32_t need_split = 1;
    // non-block call
    MppPollType timeout = MPP_POLL_NON_BLOCK;

    if (initOK)
        return MPP_OK;

    mPackets = new QList((node_destructor)mpp_packet_deinit);
    mFrames = new QList((node_destructor)mpp_frame_deinit);

    /* Input packet buffer group */
    mpp_buffer_group_get_internal(&mPacketGroup, MPP_BUFFER_TYPE_ION);
    mpp_buffer_group_limit_config(mPacketGroup, 0, 5);

    /* Output frame buffer group */
    mpp_buffer_group_get_internal(&mFrameGroup, MPP_BUFFER_TYPE_ION);
    mpp_buffer_group_limit_config(mFrameGroup, 0, 24);

    ret = mpp_create(&mpp_ctx, &mpi);
    if (MPP_OK != ret) {
        ALOGE("failed to create mpp context");
        return false;
    }

    // NOTE: decoder split mode need to be set before init
    param = &need_split;
    ret = mpi->control(mpp_ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, param);
    if (MPP_OK != ret) {
        ALOGE("failed to set mpp split mode");
        return false;
    }

    // NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    if (timeout) {
        param = &timeout;
        ret = mpi->control(mpp_ctx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            ALOGE("failed to set output timeout %d ret %d", timeout, ret);
            return false;
        }
    }

    ret = mpp_init(mpp_ctx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (MPP_OK != ret) {
        ALOGE("failed to init mpp");
        return false;
    }

    mOutputFmt = fmt;
    if (fmt == OutputFormat::OUT_FORMAT_ARGB) {
        mBpp = 4;
    } else {
        mBpp = 1.5;
    }

    /* NOTE: change output format before jpeg decoding */
    if (mOutputFmt < MPP_FMT_BUTT) {
        ret = mpi->control(mpp_ctx, MPP_DEC_SET_OUTPUT_FORMAT, &mOutputFmt);
        if (MPP_OK != ret)
            ALOGE("failed to set output format %d ret %d", mOutputFmt, ret);
    }

    initOK = 1;

    return true;
}

void MpiJpegDecoder::flushBuffer()
{
    mPackets->lock();
    mPackets->flush();
    mPackets->unlock();

    mFrames->lock();
    mFrames->flush();
    mFrames->unlock();

    if (mpi && initOK) {
        mpi->reset(mpp_ctx);
    }
}

void MpiJpegDecoder::setup_output_frame_from_mpp_frame(
        OutputFrame_t *oframe, MppFrame mframe)
{
    MppBuffer buf = mpp_frame_get_buffer(mframe);

    oframe->DisplayWidth = mpp_frame_get_width(mframe);
    oframe->DisplayHeight = mpp_frame_get_height(mframe);
    oframe->FrameWidth = mpp_frame_get_hor_stride(mframe);
    oframe->FrameHeight = mpp_frame_get_ver_stride(mframe);
    oframe->FrameHandler = mframe;

    oframe->ErrorInfo = mpp_frame_get_errinfo(mframe) |
            mpp_frame_get_discard(mframe);

    if (buf) {
        void *ptr = mpp_buffer_get_ptr(buf);
        int32_t fd = mpp_buffer_get_fd(buf);

        oframe->MemVirAddr = (uint8_t*)ptr;
        oframe->MemPhyAddr = fd;

        oframe->OutputSize = oframe->FrameWidth * oframe->FrameHeight * mBpp;
    }
}

MPP_RET MpiJpegDecoder::crop_output_frame_if_neccessary(OutputFrame_t *oframe)
{
    MPP_RET ret = MPP_OK;

#ifdef OUTPUT_CROP
    uint8_t *src_addr, *dst_addr;
    uint32_t src_width, src_height;
    uint32_t src_wstride, src_hstride;
    uint32_t dst_width, dst_height;

    src_addr = dst_addr = oframe->MemVirAddr;
    src_width = _ALIGN(oframe->DisplayWidth, 2);
    src_height = _ALIGN(oframe->DisplayHeight, 2);
    src_wstride = oframe->FrameWidth;
    src_hstride = oframe->FrameHeight;
    dst_width = _ALIGN(src_width, 8);
    dst_height = _ALIGN(src_height, 8);

    if (src_width == src_wstride && src_height == src_hstride)
        return MPP_OK;

    if (NULL == oframe->FrameHandler)
        return MPP_NOK;

    ALOGV("librga: try crop from %dx%d to %dx%d",
          src_width, src_height, dst_width, dst_height);

    ret = crop_yuv_image(src_addr, dst_addr, src_width, src_height,
                         src_wstride, src_hstride, dst_width, dst_height);
    if (MPP_OK == ret) {
        if (src_width != dst_width || src_height != dst_height) {
            oframe->DisplayWidth = dst_width;
            oframe->DisplayHeight = dst_height;
            oframe->FrameWidth = dst_width;
            oframe->FrameHeight = dst_height;
        }

        oframe->OutputSize = oframe->DisplayWidth * oframe->DisplayHeight * mBpp;
    }
#endif

    return ret;
}

MPP_RET MpiJpegDecoder::decode_sendpacket(char *input_buf, size_t buf_len)
{
    MPP_RET ret         = MPP_OK;
    /* input packet and output frame */
    MppPacket pkt       = NULL;
    MppBuffer pkt_buf   = NULL;
    MppFrame frame      = NULL;
    MppBuffer frm_buf   = NULL;

    MppTask task        = NULL;

    uint32_t pic_width, pic_height;
    uint32_t hor_stride, ver_stride;

    if (!initOK)
        return MPP_ERR_VPU_CODEC_INIT;

    if (mPackets->list_size() > 5)
        return MPP_ERR_BUFFER_FULL;

    // NOTE: The size of output frame and input packet depends on JPEG
    // dimens, so we get JPEG dimens from file header first.
    ret = jpeg_parser_get_dimens(input_buf, buf_len, &pic_width, &pic_height);
    if (MPP_OK != ret) {
        ALOGE("failed to get dimens from parser");
        return ret;
    }

    ALOGV("get JPEG dimens: %dx%d", pic_width, pic_height);

    hor_stride = _ALIGN(pic_width, 16);
    ver_stride = _ALIGN(pic_height, 16);

    ret = mpp_buffer_get(mPacketGroup, &pkt_buf, buf_len);
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for input packet ret %d", ret);
        goto SEND_OUT;
    }

    mpp_packet_init_with_buffer(&pkt, pkt_buf);
    mpp_buffer_write(pkt_buf, 0, input_buf, buf_len);

    mPackets->lock();
    mPackets->add_at_tail(&pkt, sizeof(pkt));
    mPackets->unlock();

    ret = mpp_frame_init(&frame); /* output frame */
    if (MPP_OK != ret) {
        ALOGE("failed to init output frame");
        goto SEND_OUT;
    }

    /*
     * NOTE: For jpeg could have YUV420 and ARGB the buffer should be
     * larger for output. And the buffer dimension should align to 16.
     * YUV420 buffer is 3/2 times of w*h.
     * YUV422 buffer is 2 times of w*h.
     * AGRB buffer is 4 times of w*h.
     */
    ret = mpp_buffer_get(mFrameGroup, &frm_buf,
                         hor_stride * ver_stride * (int)(mBpp + 0.5));
    if (MPP_OK != ret) {
        ALOGE("failed to get buffer for output frame ret %d", ret);
        goto SEND_OUT;
    }
    mpp_frame_set_buffer(frame, frm_buf);

    // Start queue input task.
    ret = mpi->poll(mpp_ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll input_task");
        goto SEND_OUT;
    }

    /* input queue */
    ret = mpi->dequeue(mpp_ctx, MPP_PORT_INPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed dequeue to input_task ");
        goto SEND_OUT;
    }

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, pkt);
    mpp_task_meta_set_frame(task, KEY_OUTPUT_FRAME, frame);

    /* input queue */
    ret = mpi->enqueue(mpp_ctx, MPP_PORT_INPUT, task);
    if (MPP_OK != ret)
        ALOGE("failed to enqueue input_task");

SEND_OUT:
    if (pkt_buf) {
        mpp_buffer_put(pkt_buf);
        pkt_buf = NULL;
    }

    if (frm_buf) {
        mpp_buffer_put(frm_buf);
        frm_buf = NULL;
    }

    return ret;
}

MPP_RET MpiJpegDecoder::decode_getoutframe(OutputFrame_t *aFrameOut)
{
    MPP_RET ret      = MPP_OK;
    MppTask task     = NULL;

    if (!initOK)
        return MPP_ERR_VPU_CODEC_INIT;

    /* poll and wait here */
    ret = mpi->poll(mpp_ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (MPP_OK != ret) {
        ALOGE("failed to poll output_task");
        return ret;
    }

    /* output queue */
    ret = mpi->dequeue(mpp_ctx, MPP_PORT_OUTPUT, &task);
    if (MPP_OK != ret) {
        ALOGE("failed to dequeue output_task");
        return ret;
    }

    if (task) {
        MppFrame frame_out = NULL;
        MppPacket packet = NULL;
        mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame_out);

        memset(aFrameOut, 0, sizeof(OutputFrame_t));
        setup_output_frame_from_mpp_frame(aFrameOut, frame_out);

        ret = crop_output_frame_if_neccessary(aFrameOut);
        if (MPP_OK != ret)
            ALOGV("outputFrame crop failed");

        /* output queue */
        ret = mpi->enqueue(mpp_ctx, MPP_PORT_OUTPUT, task);
        if (MPP_OK != ret)
            ALOGE("failed to enqueue output_task");

        mFrames->lock();
        mFrames->add_at_tail(&frame_out, sizeof(frame_out));
        mFrames->unlock();

        mPackets->lock();
        mPackets->del_at_head(&packet, sizeof(packet));
        mpp_packet_deinit(&packet);
        mPackets->unlock();
    }

    return ret;
}

void MpiJpegDecoder::deinitOutputFrame(OutputFrame_t *aframeOut)
{
    MppFrame frame = NULL;

    if (NULL == aframeOut || NULL == aframeOut->FrameHandler)
        return;

    mFrames->lock();
    mFrames->del_at_head(&frame, sizeof(frame));
    if (frame == aframeOut->FrameHandler) {
        mpp_frame_deinit(&frame);
    } else {
        ALOGW("deinit found invaild output frame");
        mFrames->add_at_head(&frame, sizeof(frame));
    }

    mFrames->unlock();
    memset(aframeOut, 0, sizeof(OutputFrame_t));
}

bool MpiJpegDecoder::decodePacket(char* data, size_t size, OutputFrame_t *frameOut)
{
    MPP_RET ret = MPP_OK;
    if (NULL == data) {
        ALOGE("invalid input: data: %p", data);
        return false;
    }

    time_start_record();

    ret = decode_sendpacket(data, size);
    if (MPP_OK != ret) {
        ALOGE("failed to prepare decoder");
        goto DECODE_OUT;
    }

    memset(frameOut, 0, sizeof(OutputFrame_t));
    ret = decode_getoutframe(frameOut);
    if (MPP_OK != ret) {
        ALOGE("failed to get output frame");
        goto DECODE_OUT;
    }

    time_end_record("decode packet");

DECODE_OUT:
    return ret == MPP_OK ? true : false;
}

bool MpiJpegDecoder::decodeFile(const char *input_file, const char *output_file)
{
    MPP_RET ret = MPP_OK;
    char *buf = NULL;
    size_t buf_size = 0;

    OutputFrame_t frameOut;

    ALOGD("mpi_jpeg_dec decodeFile start");

    ret = get_file_ptr(input_file, &buf, &buf_size);
    if (MPP_OK != ret)
        goto DECODE_OUT;

    if (!decodePacket(buf, buf_size, &frameOut)) {
        ALOGE("failed to decode input packet");
        goto DECODE_OUT;
    }

    ALOGD("JPEG decode success get output with dimens %dx%d, fd %d",
          frameOut.FrameWidth, frameOut.FrameHeight, frameOut.MemPhyAddr);

    // Write output frame to destination.
    ret = dump_ptr_to_file((char*)frameOut.MemVirAddr,
                           frameOut.OutputSize, output_file);
    if (MPP_OK != ret)
        ALOGE("failed to dump frame to file");

    deinitOutputFrame(&frameOut);
    flushBuffer();

DECODE_OUT:
    if (buf)
        free(buf);

    return ret == MPP_OK ? true : false;
}
