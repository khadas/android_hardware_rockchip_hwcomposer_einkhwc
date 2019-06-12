#ifndef MPI_JPEG_DECODER_H_

#define MPI_JPEG_DECODER_H_

#include "rk_mpi.h"

#define MAX_FILE_NAME_LENGTH        256

class MpiJpegDecoder {
public:
    MpiJpegDecoder() {}

    typedef struct DecParam {
        // input bitstream file
        char input_file[MAX_FILE_NAME_LENGTH];
        // the width of input picture
        int input_width;
        // the height of input picture
        int input_height;
        /*
          the format of output picture
          support list:
             MPP_FMT_YUV420SP
             MPP_FMT_YUV422SP
             MPP_FMT_RGB565
             MPP_FMT_ARGB8888
         */
        MppFrameFormat output_fmt;
        // output bitstream file
        char output_file[MAX_FILE_NAME_LENGTH];
    } DecParam;

    bool start(DecParam param);

private:
    typedef struct {
        MppCtx          ctx;
        MppApi          *mpi;

        int             width;
        int             height;
        int             hor_stride;
        int             ver_stride;
        size_t          file_size;
        MppFrameFormat  output_fmt;

        /* end of stream flag when set quit the loop */
        RK_U32          eos;

        /* buffer for stream data reading */
        char            *buf;

        /* input and output */
        MppBufferGroup  frm_grp;
        MppBufferGroup  pkt_grp;
        MppPacket       packet;
        size_t          packet_size;
        MppFrame        frame;

        FILE            *fp_input;
        FILE            *fp_output;
        FILE            *fp_config;
        RK_S32          frame_count;
    } DecLoopData;

    MPP_RET initDecCtx(DecLoopData **data, DecParam param);
    MPP_RET deinitDecCtx(DecLoopData **data);
    int decode_advanced(DecLoopData *data);
    MPP_RET startMppProcess(DecLoopData *data);

};

#endif  // MPI_JPEG_DECODER_H_
