#ifndef MPI_JPEG_ENCODER_H_

#define MPI_JPEG_ENCODER_H_

#include "rk_mpi.h"

#define MAX_FILE_NAME_LENGTH        256

class MpiJpegEncoder {
public:
    MpiJpegEncoder() {}

    typedef struct EncParam {
        // input bitstream file
        char input_file[MAX_FILE_NAME_LENGTH];
        // the width of input picture
        int input_width;
        // the height of input picture
        int input_height;
        /*
          the format of input picture
          support list:
             MPP_FMT_YUV420SP
             MPP_FMT_YUV422SP
             MPP_FMT_RGB565
             MPP_FMT_ARGB8888
         */
        MppFrameFormat input_fmt;
        // output bitstream file
        char output_file[MAX_FILE_NAME_LENGTH];
    } EncParam;

    bool start(EncParam param);

private:
    typedef struct {
        // global flow control flag
        int frm_eos;
        int pkt_eos;
        int frame_count;
        int stream_size;

        // src and dst
        FILE *fp_input;
        FILE *fp_output;

        // base flow context
        MppCtx ctx;
        MppApi *mpi;
        MppEncPrepCfg prep_cfg;
        MppEncRcCfg rc_cfg;
        MppEncCodecCfg codec_cfg;

        // input / output
        MppBuffer frm_buf;
        MppEncSeiMode sei_mode;

        // paramter for resource malloc
        int width;
        int height;
        int hor_stride;
        int ver_stride;
        MppFrameFormat input_fmt;

        // resources
        size_t frame_size;
        /* NOTE: packet buffer may overflow */
        size_t packet_size;

        // rate control runtime parameter
        int gop;
        int fps;
        int bps;
    } MpiEncCtx;

    MPP_RET initMppCtx(MpiEncCtx **data, EncParam param);
    MPP_RET deinitMppCtx(MpiEncCtx **data);
    MPP_RET setupMppCfg(MpiEncCtx *p);
    MPP_RET startMppProcess(MpiEncCtx *p);

};

#endif  // MPI_JPEG_ENCODER_H_
