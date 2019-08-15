# Introduction

HwJpeg 库用于支持 Rockchip 平台 JPEG 硬编解码，是平台 MPP（Media Process Platform）
库 JPEG 编解码逻辑的封装。其中，MpiJpegEncoder 类封装了硬编码相关操作，MpiJpegDecoder
类封装了硬解码相关操作，用于支持图片或 MJPEG 码流解码。

工程包含主要目录：
- mpi：HwJpeg 库实现代码
- test：HwJpeg 测试实例

> 工程代码使用 mk 文件组织，在 Android SDK 环境下直接编译使用即可。

# MpiJpegDecoder

MpiJpegDecoder 类是平台 JPEG 硬编码的封装，支持输入 JPG 图片及 MJPEG 码流，同时支持同步及
异步解码方式。

- decodePacket(char* data, size_t size, OutputFrame_t *aframeOut);
- decodeFile(const char *input_file, const char *output_file);

decodePacket & decodeFile 为同步解码方式，同步解码方式使用简单，阻塞等待解码输出，
OutputFrame_t 为解码输出封装，包含输出帧宽、高、物理地址、虚拟地址等信息。

- decode_sendpacket(char* input_buf, size_t buf_len);
- decode_getoutframe(OutputFrame_t *aframeOut);

decode_sendpacket & decode_getoutframe 用于配合实现异步解码输出，应用端处理开启两个线程，一
个线程送输入 decode_sendpacket，另一个线程异步取输出 decode_getoutframe。

**Note:**
1. HwJpeg 解码默认输出 RAW NV12 数据
2. 平台硬解码器只处理对齐过的 buffer，因此 MpiJpegDecoder 输出的 YUV buffer 经过 16 位对齐。
如果原始宽高非 16 位对齐，直接显示可能出现底部绿边等问题，MpiJpegDecoder 实现代码中 OUTPUT_CROP
宏用于实现 OutFrame 的裁剪操作，可手动开启，也可以外部获取 OutFrame 句柄再进行相应的裁剪。
3. OutFrame buffer 在解码库内部循环使用，在解码显示完成之后使用 deinitOutputFrame 释放内存

解码使用示例：

```
MpiJpegDecoder decoder;
MpiJpegDecoder::OutputFrame_t frameOut;

ret = decoder.prepareDecoder();
if (!ret) {
	ALOGE("failed to prepare JPEG decoder");
	goto DECODE_OUT;
}

memset(&frameOut, 0, sizeof(frameOut));
ret = decoder.decodePacket(buf, size, &frameOut);
if (!ret) {
	ALOGE("failed to decode packet");
	goto DECODE_OUT;
}

decoder.deinitOutputFrame(&frameOut);
decoder.flushBuffer();
```

# MpiJpegEncoder

## 编码使用示例：
```
#include "MpiJpegEncoder.h"

MpiJpegEncoder::EncParam param;
strcpy(param.input_file, "/data/raw");
param.input_width = 1920;
param.input_height = 1080;
/*
  input picture format support list:
	MPP_FMT_YUV420SP
	MPP_FMT_YUV422SP
	MPP_FMT_RGB565
	MPP_FMT_ARGB8888
*/
param.input_fmt = MPP_FMT_YUV420SP;
/*
  output_type support list:
	 OUTPUT_TYPE_FILE
	 OUTPUT_TYPE_MEM_ADDR
*/
param.output_type = OUTPUT_TYPE_FILE;
strcpy(param.output_file, "/data/out.jpg");

MpiJpegEncoder encoder;
if (!encoder.start(param))
	ALOGE("encode failed");
```
