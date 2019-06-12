//#define LOG_NDEBUG 0
#define LOG_TAG "MpiJpegDecoder"
#include <utils/Log.h>

#include <stdlib.h>
#include "mpp_err.h"
#include "Utils.h"
#include "MpiJpegDecoder.h"

// enlarge packet buffer size for large input stream case
#define MPI_DEC_STREAM_SIZE         (SZ_4K)

#define MPP_ALIGN(x, a)         (((x)+(a)-1)&~((a)-1))

MPP_RET MpiJpegDecoder::initDecCtx(DecLoopData **data, DecParam param)
{
    DecLoopData *p = NULL;
    MPP_RET ret = MPP_OK;

    if (!data) {
        ALOGE("invalid input data %p", data);
        return MPP_ERR_NULL_PTR;
    }

    p = (DecLoopData*) malloc(sizeof(DecLoopData));
    if (!p) {
        ALOGE("create MpiDecCtx failed");
        ret = MPP_ERR_MALLOC;
        goto RET;
    }

    p->width = param.input_width;
    p->height = param.input_height;
    p->hor_stride = MPP_ALIGN(param.input_width, 16);
    p->ver_stride = MPP_ALIGN(param.input_height, 16);
    p->output_fmt = param.output_fmt;

    p->fp_input = fopen(param.input_file, "rb");
    if (NULL == p->fp_input) {
        ALOGE("failed to open input file %s", param.input_file);
        ret = MPP_ERR_OPEN_FILE;
        goto RET;
    } else {
        fseek(p->fp_input, 0L, SEEK_END);
        p->file_size = ftell(p->fp_input);
        rewind(p->fp_input);
        ALOGD("input file size %zu", p->file_size);
    }

    if (param.output_type == OUTPUT_TYPE_FILE) {
        p->fp_output = fopen(param.output_file, "w+b");
        if (NULL == p->fp_output) {
            ALOGE("failed to open output file %s", param.output_file);
            ret = MPP_ERR_OPEN_FILE;
            goto RET;
        }
    } else if (param.output_type == OUTPUT_TYPE_MEM_ADDR) {
        p->fp_output = NULL;
        p->output_dst = param.output_dst;
    }

    ret = mpp_buffer_group_get_internal(&p->frm_grp, MPP_BUFFER_TYPE_ION);
    if (ret) {
        ALOGE("failed to get buffer group for input frame ret %d", ret);
        goto RET;
    }

    ret = mpp_buffer_group_get_internal(&p->pkt_grp, MPP_BUFFER_TYPE_ION);
    if (ret) {
        ALOGE("failed to get buffer group for output packet ret %d", ret);
        goto RET;
    }

RET:
    *data = p;
    return ret;
}

MPP_RET MpiJpegDecoder::deinitDecCtx(DecLoopData **data) {
    DecLoopData *p = NULL;

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
        if (p->ctx) {
            mpp_destroy(p->ctx);
            p->ctx = NULL;
        }
        free(p);
        p = NULL;
    }

    return MPP_OK;
}

int MpiJpegDecoder::decode_advanced(DecLoopData *data)
{
    RK_U32 pkt_eos  = 0;
    MPP_RET ret = MPP_OK;
    MppCtx ctx  = data->ctx;
    MppApi *mpi = data->mpi;
    char   *buf = data->buf;
    MppPacket packet = data->packet;
    MppFrame  frame  = data->frame;
    MppTask task = NULL;
    size_t read_size = fread(buf, 1, data->packet_size, data->fp_input);

    if (read_size != data->packet_size || feof(data->fp_input)) {
        ALOGD("found last packet read_size %d, data->packet_size : %d", read_size, data->packet_size);

        // setup eos flag
        data->eos = pkt_eos = 1;
        return ret;
    }

    // reset pos
    mpp_packet_set_pos(packet, buf);
    mpp_packet_set_length(packet, read_size);
    // setup eos flag
    if (pkt_eos)
        mpp_packet_set_eos(packet);

    ret = mpi->poll(ctx, MPP_PORT_INPUT, MPP_POLL_BLOCK);
    if (ret) {
        ALOGE("mpp input poll failed");
        return ret;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_INPUT, &task);  /* input queue */
    if (ret) {
        ALOGE("mpp task input dequeue failed");
        return ret;
    }

    mpp_task_meta_set_packet(task, KEY_INPUT_PACKET, packet);
    mpp_task_meta_set_frame (task, KEY_OUTPUT_FRAME,  frame);

    ret = mpi->enqueue(ctx, MPP_PORT_INPUT, task);  /* input queue */
    if (ret) {
        ALOGE("mpp task input enqueue failed");
        return ret;
    }

    /* poll and wait here */
    ret = mpi->poll(ctx, MPP_PORT_OUTPUT, MPP_POLL_BLOCK);
    if (ret) {
        ALOGE("mpp output poll failed");
        return ret;
    }

    ret = mpi->dequeue(ctx, MPP_PORT_OUTPUT, &task); /* output queue */
    if (ret) {
        ALOGE("mpp task output dequeue failed");
        return ret;
    }

    if (task) {
        MppFrame frame_out = NULL;
        mpp_task_meta_get_frame(task, KEY_OUTPUT_FRAME, &frame_out);

        if (frame) {
            /* write frame to output here */
            if (data->fp_output)
                dump_mpp_frame_to_file(frame, data->fp_output);
            else
                dump_mpp_frame_to_addr(frame, data->output_dst);

            data->frame_count++;
            ALOGD("decoded frame %d", data->frame_count);

            if (mpp_frame_get_eos(frame_out))
                ALOGD("found eos frame");
        }

        /* output queue */
        ret = mpi->enqueue(ctx, MPP_PORT_OUTPUT, task);
        if (ret)
            ALOGE("mpp task output enqueue failed");
    }

    return ret;
}

MPP_RET MpiJpegDecoder::startMppProcess(DecLoopData *data) {
    MPP_RET ret         = MPP_OK;

    // input / output
    MppPacket packet    = NULL;
    MppFrame  frame     = NULL;

    MppParam param      = NULL;
    RK_U32 need_split   = 1;
    // non-block call
    MppPollType timeout = MPP_POLL_NON_BLOCK;

    // resources
    char *buf           = NULL;
    size_t packet_size  = MPI_DEC_STREAM_SIZE;
    MppBuffer pkt_buf   = NULL;
    MppBuffer frm_buf   = NULL;

    ret = mpp_frame_init(&frame); /* output frame */
    if (MPP_OK != ret) {
        ALOGE("mpp_frame_init failed");
        goto MPP_DEC_OUT;
    }

    /*
     * NOTE: For jpeg could have YUV420 and YUV422 the buffer should be
     * larger for output. And the buffer dimension should align to 16.
     * YUV420 buffer is 3/2 times of w*h.
     * YUV422 buffer is 2 times of w*h.
     * So create larger buffer with 2 times w*h.
     */
    ret = mpp_buffer_get(data->frm_grp, &frm_buf, data->hor_stride * data->ver_stride * 4);
    if (ret) {
        ALOGE("failed to get buffer for input frame ret %d", ret);
        goto MPP_DEC_OUT;
    }

    // NOTE: for mjpeg decoding send the whole file
    packet_size = data->file_size;

    ret = mpp_buffer_get(data->pkt_grp, &pkt_buf, packet_size);
    if (ret) {
        ALOGE("failed to get buffer for input frame ret %d", ret);
        goto MPP_DEC_OUT;
    }

    mpp_packet_init_with_buffer(&packet, pkt_buf);
    buf = (char*) mpp_buffer_get_ptr(pkt_buf);

    mpp_frame_set_buffer(frame, frm_buf);

    ALOGD("jpeg_dec_test decoder test start w %d h %d", data->width, data->height);

    // decoder demo
    ret = mpp_create(&data->ctx, &data->mpi);
    if (MPP_OK != ret) {
        ALOGE("mpp_create failed");
        goto MPP_DEC_OUT;
    }

    // NOTE: decoder split mode need to be set before init
    param = &need_split;
    ret = data->mpi->control(data->ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, param);
    if (MPP_OK != ret) {
        ALOGE("mpi->control failed");
        goto MPP_DEC_OUT;
    }

    // NOTE: timeout value please refer to MppPollType definition
    //  0   - non-block call (default)
    // -1   - block call
    // +val - timeout value in ms
    if (timeout) {
        param = &timeout;
        ret = data->mpi->control(data->ctx, MPP_SET_OUTPUT_TIMEOUT, param);
        if (MPP_OK != ret) {
            ALOGE("Failed to set output timeout %d ret %d", timeout, ret);
            goto MPP_DEC_OUT;
        }
    }

    ret = mpp_init(data->ctx, MPP_CTX_DEC, MPP_VIDEO_CodingMJPEG);
    if (MPP_OK != ret) {
        ALOGE("mpp_init failed");
        goto MPP_DEC_OUT;
    }

    data->eos            = 0;
    data->buf            = buf;
    data->packet         = packet;
    data->packet_size    = packet_size;
    data->frame          = frame;
    data->frame_count    = 0;

    /* NOTE: change output format before jpeg decoding */
    if (data->output_fmt < MPP_FMT_BUTT)
        ret = data->mpi->control(data->ctx, MPP_DEC_SET_OUTPUT_FORMAT, &data->output_fmt);

    while (!data->eos) {
        decode_advanced(data);
    }

    ret = data->mpi->reset(data->ctx);
    if (MPP_OK != ret) {
        ALOGE("mpi->reset failed");
        goto MPP_DEC_OUT;
    }

MPP_DEC_OUT:
    if (packet) {
        mpp_packet_deinit(&packet);
        packet = NULL;
    }

    if (frame) {
        mpp_frame_deinit(&frame);
        frame = NULL;
    }

    if (pkt_buf) {
        mpp_buffer_put(pkt_buf);
        pkt_buf = NULL;
    }

    if (frm_buf) {
        mpp_buffer_put(frm_buf);
        frm_buf = NULL;
    }

    if (data->pkt_grp) {
        mpp_buffer_group_put(data->pkt_grp);
        data->pkt_grp = NULL;
    }

    if (data->frm_grp) {
        mpp_buffer_group_put(data->frm_grp);
        data->frm_grp = NULL;
    }

    return ret;
}

bool MpiJpegDecoder::start(DecParam param) {
    MPP_RET ret = MPP_OK;
    DecLoopData *p = NULL;

    ALOGD("mpi_jpeg_dec start");

    ret = initDecCtx(&p, param);
    if (ret) {
        ALOGE("MpiEncCtx init failed");
        goto MPP_TEST_OUT;
    }

    ret = startMppProcess(p);
    if (ret) {
        ALOGE("mpp process run failed ret %d", ret);
        goto MPP_TEST_OUT;
    }

MPP_TEST_OUT:
    deinitDecCtx(&p);

    return ret == MPP_OK ? true : false;
}
