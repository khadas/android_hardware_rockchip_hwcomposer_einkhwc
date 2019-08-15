#ifndef __MPI_JPEG_DECODER_H__
#define __MPI_JPEG_DECODER_H__

#include "rk_mpi.h"

class QList;

class MpiJpegDecoder {
public:
    MpiJpegDecoder();
    ~MpiJpegDecoder();

    typedef struct {
        /*
         * NOTE: Since the output frame buffer send to vpu is aligned, it is
         * neccessary to crop output with JPEG image dimens.
         *
         * vpu frame buffer width: FrameWidth
         * actual image buffer width: DisplayWidth
         */
        uint32_t   FrameWidth;         // buffer horizontal stride
        uint32_t   FrameHeight;        // buffer vertical stride
        uint32_t   DisplayWidth;       // valid width for display
        uint32_t   DisplayHeight;      // valid height for display

        uint32_t   ErrorInfo;          // error information
        uint32_t   OutputSize;

        uint8_t   *MemVirAddr;
        uint32_t   MemPhyAddr;

        void      *FrameHandler;       // MppFrame handler
    } OutputFrame_t;

    typedef enum {
        OUT_FORMAT_ARGB      =  1,
        OUT_FORMAT_YUV420SP  =  5,
    } OutputFormat;

    bool prepareDecoder(OutputFormat fmt);
    void flushBuffer();

    /**
     * Output frame buffers within limits, so release frame buffer if one
     * frame has been display successful.
     */
    void deinitOutputFrame(OutputFrame_t *aframeOut);

    MPP_RET decode_sendpacket(char* input_buf, size_t buf_len);
    MPP_RET decode_getoutframe(OutputFrame_t *aframeOut);

    bool decodePacket(char* data, size_t size, OutputFrame_t *aframeOut);
    bool decodeFile(const char *input_file, const char *output_file);

private:
    MppCtx          mpp_ctx;
    MppApi          *mpi;

    int             initOK;

    // bit per pixel
    float           mBpp;
    int             mOutputFmt;

    QList           *mPackets;
    QList           *mFrames;

    /*
     * packet buffer group
     *      - packets in I/O, can be ion buffer or normal buffer
     * frame buffer group
     *      - frames in I/O, normally should be a ion buffer group
     */
    MppBufferGroup  mPacketGroup;
    MppBufferGroup  mFrameGroup;

    void setup_output_frame_from_mpp_frame(OutputFrame_t *oframe, MppFrame mframe);
    MPP_RET crop_output_frame_if_neccessary(OutputFrame_t *oframe);
};

#endif  // __MPI_JPEG_DECODER_H__
