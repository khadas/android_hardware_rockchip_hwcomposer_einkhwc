//#define LOG_NDEBUG 0
#define LOG_TAG "main"
#include <utils/Log.h>

#include <string.h>
#include "MpiJpegDecoder.h"

int main()
{
    /* --------------- jpeg_enc demo ----------------*/
    // #include "MpiJpegEncoder.h"

    //    MpiJpegEncoder::EncParam param;
    //    strcpy(param.input_file, "/data/raw");
    //    param.input_width = 1920;
    //    param.input_height = 1080;
    //    /*
    //      input picture format support list:
    //         MPP_FMT_YUV420SP
    //         MPP_FMT_YUV422SP
    //         MPP_FMT_RGB565
    //         MPP_FMT_ARGB8888
    //     */
    //    param.input_fmt = MPP_FMT_YUV420SP;
    //    /*
    //      output_type support list:
    //         OUTPUT_TYPE_FILE
    //         OUTPUT_TYPE_MEM_ADDR
    //    */
    //    param.output_type = OUTPUT_TYPE_FILE;
    //    strcpy(param.output_file, "/data/out.jpg");

    //    MpiJpegEncoder encoder;
    //    if (!encoder.start(param))
    //        ALOGE("encode failed");
    /* ----------------------------------------------*/

    /* --------------- jpeg_dec demo ----------------*/
    MpiJpegDecoder::DecParam param;
    strcpy(param.input_file, "/data/test.jpg");
    param.input_width = 332;
    param.input_height = 228;
    /*
      output format support list:
         MPP_FMT_YUV420SP
         MPP_FMT_YUV422SP
         MPP_FMT_RGB565
         MPP_FMT_ARGB8888
    */
    param.output_fmt = MPP_FMT_YUV420SP;
    /*
      output_type support list:
        OUTPUT_TYPE_FILE
        OUTPUT_TYPE_MEM_ADDR
    */
    param.output_type = MpiJpegDecoder::OUTPUT_TYPE_FILE;
    strcpy(param.output_file, "/data/raw");

    MpiJpegDecoder decoder;
    if (!decoder.start(param))
        ALOGE("decode failed");
    /* --------------------------------------------*/

    return 0;
}
