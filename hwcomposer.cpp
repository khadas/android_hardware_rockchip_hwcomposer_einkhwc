/*
 * Copyright (C) 2018 Fuzhou Rockchip Electronics Co.Ltd.
 *
 * Modification based on code covered by the Apache License, Version 2.0 (the "License").
 * You may not use this software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS TO YOU ON AN "AS IS" BASIS
 * AND ANY AND ALL WARRANTIES AND REPRESENTATIONS WITH RESPECT TO SUCH SOFTWARE, WHETHER EXPRESS,
 * IMPLIED, STATUTORY OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF TITLE,
 * NON-INFRINGEMENT, MERCHANTABILITY, SATISFACTROY QUALITY, ACCURACY OR FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.
 *
 * IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#define LOG_TAG "hwcomposer-drm"

// #define ENABLE_DEBUG_LOG
//#include <log/custom_log.h>
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkImageInfo.h"
#include "SkStream.h"
#include "SkImage.h"
#include "SkEncodedImageFormat.h"
#include "SkImageEncoder.h"
#include "SkCodec.h"
#include "SkData.h"

#include "drmhwcomposer.h"
#include "einkcompositorworker.h"

#include <stdlib.h>

#include <cinttypes>
#include <map>
#include <vector>
#include <sstream>
#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <sw_sync.h>
#include <sync/sync.h>
#include <utils/Trace.h>
#include <drm_fourcc.h>
#if RK_DRM_GRALLOC
#include "gralloc_drm_handle.h"
#endif
#include <linux/fb.h>

#include "hwc_util.h"
#include "hwc_rockchip.h"
#include "vsyncworker.h"
#include <android/configuration.h>


//open header
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//map header
#include <map>

#include <sys/inotify.h>
#include <sys/prctl.h>
#include <sys/epoll.h>

//gui
#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/GraphicBufferMapper.h>

//Image jpg decoder
#include "MpiJpegDecoder.h"
#include "libcfa/libcfa.h"

#define UM_PER_INCH 25400

namespace android {
#ifndef ANDROID_EINK_COMPOSITOR_WORKER_H_

#define EPD_NULL            (-1)
#define EPD_AUTO            (0)
#define EPD_FULL            (1)
#define EPD_A2              (2)
#define EPD_PART            (3)
#define EPD_FULL_DITHER     (4)
#define EPD_RESET           (5)
#define EPD_BLACK_WHITE     (6)
#define EPD_BG            (7)
#define EPD_BLOCK           (8)
#define EPD_FULL_WIN        (9)
#define EPD_OED_PART		(10)
#define EPD_DIRECT_PART     (11)
#define EPD_DIRECT_A2       (12)
#define EPD_STANDBY			(13)
#define EPD_POWEROFF        (14)
#define EPD_NOPOWER        (15)
#define EPD_AUTO_BG        (16)
#define EPD_UNBLOCK        (17)
#define EPD_PART_GL16     (18)
#define EPD_PART_GLR16   (19)
#define EPD_PART_GLD16   (20)
#define EPD_FULL_GL16      (21)
#define EPD_FULL_GLR16    (22)
#define EPD_FULL_GLD16    (23)

/*android use struct*/
struct ebc_buf_info{
  int offset;
  int epd_mode;
  int height;
  int width;
  int vir_height;
  int vir_width;
  int fb_width;
  int fb_height;
  int color_panel;
  int win_x1;
  int win_y1;
  int win_x2;
  int win_y2;
  int rotate;
  int width_mm;
  int height_mm;
}__packed;
struct win_coordinate{
	int x1;
	int x2;
	int y1;
	int y2;
};


#define USE_RGA 1

#define GET_EBC_BUFFER 0x7000
#define SET_EBC_SEND_BUFFER 0x7001
#define GET_EBC_BUFFER_INFO 0x7003
#define SET_EBC_NOT_FULL_NUM 0x7006

#endif


#define POWEROFF_IMAGE_PATH_USER "/data/misc/poweroff.jpg"
#define NOPOWER_IMAGE_PATH_USER "/data/misc/nopower.jpg"
#define STANDBY_IMAGE_PATH_USER "/data/misc/standby.jpg"
#define STANDBY_NOPOWER_PATH_USER "/data/misc/standby_nopower.jpg"
#define STANDBY_CHARGE_PATH_USER "/data/misc/standby_charge.jpg"

#define POWEROFF_IMAGE_PATH_DEFAULT "/vendor/media/poweroff.jpg"
#define NOPOWER_IMAGE_PATH_DEFAULT "/vendor/media/nopower.jpg"
#define STANDBY_IMAGE_PATH_DEFAULT "/vendor/media/standby.jpg"
#define STANDBY_NOPOWER_PATH_DEFAULT "/vendor/media/standby_nopower.jpg"
#define STANDBY_CHARGE_PATH_DEFAULT "/vendor/media/standby_charge.jpg"

int gPixel_format = 24;


int ebc_fd = -1;
void *ebc_buffer_base = NULL;
struct ebc_buf_info ebc_buf_info;

static int gLastEpdMode = EPD_PART;
static int gCurrentEpdMode = EPD_PART;
static int gResetEpdMode = EPD_PART;
static Region gLastA2Region;
static Region gSavedUpdateRegion;

static bool gFirst = true;
static bool gPoweroff =false;
static int gPowerMode = 0;

static Mutex mEinkModeLock;

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index);

extern "C" {
    void neon_rgb888_to_gray256ARM_32(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_rgb888_to_gray256ARM_16(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_rgb888_to_gray16ARM_32(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_rgb888_to_gray16ARM_16(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}

extern "C" {
    void neon_bgr888_to_gray16ARM_32(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_bgr888_to_gray16ARM_16(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);
}


extern "C" {
    void neon_gray16_to_gray2ARM(uint8_t * dest,int w,int h);
}
extern "C" {
    void neon_rgb256_to_gray16DITHER(int  *src, int *dst, short int *res0,  short int*res1, int w);
}
extern "C" {
    void neon_gray256_to_gray16ARM_16(unsigned int * dest,unsigned int *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_gray256_to_gray16ARM_32(unsigned int * dest,unsigned int *  src,int h,int w,int vir_w);
}
extern "C" {
    void neon_gray256_to_gray256(int * dest,int *  src,int h,int w);
}


#if 1 //RGA_POLICY
int hwc_set_epd(DrmRgaBuffer &rgaBuffer,hwc_layer_1_t &fb_target,Region &A2Region,Region &updateRegion,Region &AutoRegion);
int hwc_rgba888_to_gray256(hwc_drm_display_t *hd, hwc_layer_1_t &fb_target);
void hwc_free_buffer(hwc_drm_display_t *hd);
#endif


#if SKIP_BOOT
static unsigned int g_boot_cnt = 0;
#endif
static unsigned int g_boot_gles_cnt = 0;
static unsigned int g_extern_gles_cnt = 0;
static bool g_bSkipExtern = false;

#ifdef USE_HWC2
static bool g_hasHotplug = false;
#endif

static bool g_bSkipCurFrame = false;
//#if RK_INVALID_REFRESH
hwc_context_t* g_ctx = NULL;
//#endif

static int gama[256];
static int MAX_GAMMA_LEVEL = 80;
static int DEFAULT_GRAY_WHITE_COUNT = 16;
static int DEFAULT_GRAY_BLACK_COUNT = 16;
static int last_gamma_level = 0;
static int DEFAULT_GAMMA_LEVEL = 14;

/* add by xy
处理gamma table灰阶映射表参数，根据像素点的rgb值转换成0-255值，然后对应gamma table对应灰阶，初始值是16个灰阶平均分配（16*16），
0x00表示纯黑，0x0f表示纯白，总共16个灰阶值；
*/
static void init_gamma_table(int gamma_level){
    if(gamma_level < 0 || gamma_level > MAX_GAMMA_LEVEL)
        return;

    ALOGD("init_gamma_table...  gamma_level= %d",gamma_level);
    int currentGammaLevel = gamma_level;
    last_gamma_level = currentGammaLevel;//记录最新gamma值

    //纯黑点越多显示效果比较好，纯白点越多效果越不好，所以根据currentGammaLevel纯白和纯黑的变化不一样，纯黑×2，纯白/2

    int mWhiteCount;//gammaTable中纯白灰阶个数
    int mBlackCount;//gammaTable中纯黑灰阶个数
    if(currentGammaLevel < MAX_GAMMA_LEVEL){
        mWhiteCount = DEFAULT_GRAY_WHITE_COUNT + currentGammaLevel / 2;
        mBlackCount = DEFAULT_GRAY_BLACK_COUNT  + currentGammaLevel * 2 ;
    }else{//最大对比度时，将对比度特殊化设置成黑白两色
        mWhiteCount = 100;
        mBlackCount = 156;
    }

    int mChangeMultiple = (256 - mBlackCount - mWhiteCount)/14;//除掉纯白纯黑其他灰阶平均分配个数
    int whiteIndex = 256 - mWhiteCount;
    int remainder = (256 - mBlackCount - mWhiteCount) % 14;
    int tempremainder = remainder;
    for (int i = 0; i < 256; i++) {
        if(i < mBlackCount){
            gama[i] = 0;
        }else if(i > whiteIndex){
            gama[i] = 15;
        }else {
            if(remainder > 0){ //处理平均误差,平均分配到每一个灰阶上
                int gray = (i - mBlackCount + mChangeMultiple + 1) / (mChangeMultiple + 1);
                gama[i] = gray;
                if((i - mBlackCount + mChangeMultiple + 1) % (mChangeMultiple + 1) * 2 == 0)
                    remainder--;
            }else {
                int gray = (i - mBlackCount - tempremainder  + mChangeMultiple) / mChangeMultiple;
                gama[i] = gray;
            }
        }
    }
}

struct hwc_context_t {
  // map of display:hwc_drm_display_t
  typedef std::map<int, hwc_drm_display_t> DisplayMap;

  ~hwc_context_t() {
  }

  hwc_composer_device_1_t device;
  hwc_procs_t const *procs = NULL;

  DisplayMap displays;
  const gralloc_module_t *gralloc;
  EinkCompositorWorker eink_compositor_worker;
  VSyncWorker primary_vsync_worker;
  VSyncWorker extend_vsync_worker;

  int ebc_fd = -1;
  void *ebc_buffer_base = NULL;
  struct ebc_buf_info ebc_buf_info;

};

static void hwc_dump(struct hwc_composer_device_1 *dev, char *buff,
                     int buff_len) {
  return;
}

static hwc_drm_display_t hwc_info;
static int hwc_prepare(hwc_composer_device_1_t *dev, size_t num_displays,
                       hwc_display_contents_1_t **display_contents) {

   UN_USED(dev);

   init_log_level();
   for (int i = 0; i < (int)num_displays; ++i) {
      if (!display_contents[i])
        continue;
      int num_layers = display_contents[i]->numHwLayers;
      for (int j = 0; j < num_layers - 1; ++j) {
        hwc_layer_1_t *layer = &display_contents[i]->hwLayers[j];
        layer->compositionType = HWC_FRAMEBUFFER;
      }
  }
  return 0;
}

static void hwc_add_layer_to_retire_fence(
    hwc_layer_1_t *layer, hwc_display_contents_1_t *display_contents) {
  if (layer->releaseFenceFd < 0)
    return;

  if (display_contents->retireFenceFd >= 0) {
    int old_retire_fence = display_contents->retireFenceFd;
    display_contents->retireFenceFd =
        sync_merge("dc_retire", old_retire_fence, layer->releaseFenceFd);
    close(old_retire_fence);
  } else {
    display_contents->retireFenceFd = dup(layer->releaseFenceFd);
  }
}

#if 1 //RGA_POLICY
int hwc_rgba888_to_gray256(DrmRgaBuffer &rgaBuffer,hwc_layer_1_t *fb_target,hwc_drm_display_t *hd) {
    ATRACE_CALL();

    int ret = 0;
    int rga_transform = 0;
    int src_l,src_t,src_w,src_h;
    int dst_l,dst_t,dst_r,dst_b;

    int dst_w,dst_h,dst_stride;
    int src_buf_w,src_buf_h,src_buf_stride,src_buf_format;
    rga_info_t src, dst;
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    src.fd = -1;
    dst.fd = -1;

    //Get virtual address
    const gralloc_module_t *gralloc;
    ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc);
    if (ret) {
        ALOGE("Failed to open gralloc module");
        return ret;
    }

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(gralloc,fb_target->handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(gralloc,fb_target->handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(gralloc,fb_target->handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(gralloc,fb_target->handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(gralloc,fb_target->handle);
    src_buf_h = hwc_get_handle_height(gralloc,fb_target->handle);
    src_buf_stride = hwc_get_handle_stride(gralloc,fb_target->handle);
    src_buf_format = hwc_get_handle_format(gralloc,fb_target->handle);
#endif

    src_l = (int)fb_target->sourceCropf.left;
    src_t = (int)fb_target->sourceCropf.top;
    src_w = (int)(fb_target->sourceCropf.right - fb_target->sourceCropf.left);
    src_h = (int)(fb_target->sourceCropf.bottom - fb_target->sourceCropf.top);

    dst_l = (int)fb_target->displayFrame.left;
    dst_t = (int)fb_target->displayFrame.top;
    dst_w = (int)(fb_target->displayFrame.right - fb_target->displayFrame.left);
    dst_h = (int)(fb_target->displayFrame.bottom - fb_target->displayFrame.top);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, HAL_PIXEL_FORMAT_RGBA_8888);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, hd->framebuffer_width, hd->framebuffer_height, HAL_PIXEL_FORMAT_YCrCb_NV12);
    ALOGD("RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD("RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_target->handle, (void*)(rgaBuffer.buffer()->handle), src_buf_format, rga_transform);

    src.hnd = fb_target->handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_target->handle, (void*)(rgaBuffer.buffer()->handle));
    }
    DumpLayer("yuv", dst.hnd);


    return ret;
}

#define CLIP(x) (((x) > 255) ? 255 : (x))
void Luma8bit_to_4bit_row_16(int  *src,  int *dst, short int *res0,  short int*res1, int w)
{
    int i;
    int g0, g1, g2,g3,g4,g5,g6,g7,g_temp;
    int e;
    int v0, v1, v2, v3;
    int src_data;
    int src_temp_data;
    v0 = 0;
    for(i=0; i<w; i+=8)
    {

        src_data =  *src++;
        src_temp_data = src_data&0xff;
        g_temp = src_temp_data + res0[i] + v0;
        res0[i] = 0;
        g_temp = CLIP(g_temp);
        g0 = g_temp & 0xf0;
        e = g_temp - g0;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;

        if( i==0 )
        {
            res1[i] += v2;
            res1[i+1] += v3;
        }
        else
        {
            res1[i-1] += v1;
            res1[i]   += v2;
            res1[i+1] += v3;
        }



        src_temp_data = ((src_data&0x0000ff00)>>8);
        g_temp = src_temp_data + res0[i+1] + v0;
        res0[i+1] = 0;
        g_temp = CLIP(g_temp);
        g1 = g_temp & 0xf0;
        e = g_temp - g1;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        res1[i]     += v1;
        res1[i+1]   += v2;
        res1[i+2]   += v3;




        src_temp_data = ((src_data&0x00ff0000)>>16);
        g_temp = src_temp_data + res0[i+2] + v0;
        res0[i+2] = 0;
        g_temp = CLIP(g_temp);
        g2 = g_temp & 0xf0;
        e = g_temp - g2;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        res1[i+1]     += v1;
        res1[i+2]   += v2;
        res1[i+3]   += v3;


        src_temp_data = ((src_data&0xff000000)>>24);
        g_temp = src_temp_data + res0[i+3] + v0;
        res0[i+3] = 0;
        g_temp = CLIP(g_temp);
        g3 = g_temp & 0xf0;
        e = g_temp - g3;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        res1[i+2]     += v1;
        res1[i+3]   += v2;
        res1[i+4]   += v3;


        src_data =  *src++;
        src_temp_data = src_data&0xff;
        g_temp = src_temp_data + res0[i+4] + v0;
        res0[i+4] = 0;
        g_temp = CLIP(g_temp);
        g4 = g_temp & 0xf0;
        e = g_temp - g4;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;

        {
            res1[i+3] += v1;
            res1[i+4]   += v2;
            res1[i+5] += v3;
        }



        src_temp_data = ((src_data&0x0000ff00)>>8);
        g_temp = src_temp_data + res0[i+5] + v0;
        res0[i+5] = 0;
        g_temp = CLIP(g_temp);
        g5 = g_temp & 0xf0;
        e = g_temp - g5;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        res1[i+4]     += v1;
        res1[i+5]   += v2;
        res1[i+6]   += v3;




        src_temp_data = ((src_data&0x00ff0000)>>16);
        g_temp = src_temp_data + res0[i+6] + v0;
        res0[i+6] = 0;
        g_temp = CLIP(g_temp);
        g6 = g_temp & 0xf0;
        e = g_temp - g6;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        res1[i+5]     += v1;
        res1[i+6]   += v2;
        res1[i+7]   += v3;




        src_temp_data = ((src_data&0xff000000)>>24);
        g_temp = src_temp_data + res0[i+7] + v0;
        res0[i+7] = 0;
        g_temp = CLIP(g_temp);
        g7 = g_temp & 0xf0;
        e = g_temp - g7;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        if (i == w-8)
        {
            res1[i+6] += v1;
            res1[i+7] += v2;
        }
        else
        {
            res1[i+6]     += v1;
            res1[i+7]   += v2;
            res1[i+8]   += v3;
        }

        *dst++ =(g7<<24)|(g6<<20)|(g5<<16)|(g4<<12) |(g3<<8)|(g2<<4)|g1|(g0>>4);
    }

}


int gray256_to_gray16_dither(char *gray256_addr,int *gray16_buffer,int  panel_h, int panel_w,int vir_width){

  ATRACE_CALL();

  UN_USED(vir_width);
  int h;
  int w;
  short int *line_buffer[2];
  char *src_buffer;
  line_buffer[0] =(short int *) malloc(panel_w*2);
  line_buffer[1] =(short int *) malloc(panel_w*2);
  memset(line_buffer[0],0,panel_w*2);
  memset(line_buffer[1],0,panel_w*2);

  for(h = 0;h<panel_h;h++){
      Luma8bit_to_4bit_row_16((int*)gray256_addr,gray16_buffer,line_buffer[h&1],line_buffer[!(h&1)],panel_w);
      gray16_buffer = gray16_buffer+panel_w/8;
      gray256_addr = (char*)(gray256_addr+panel_w);
  }
  free(line_buffer[0]);
  free(line_buffer[1]);

  return 0;
}

int gray256_to_gray16(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w){
  ATRACE_CALL();
  char gamma_level[PROPERTY_VALUE_MAX];
  property_get("sys.gray.gammalevel",gamma_level,"30");
  if(atoi(gamma_level) != last_gamma_level){
    init_gamma_table(atoi(gamma_level));
  }

  char src_data;
  char  g0,g3;
  char *temp_dst = (char *)gray16_buffer;

  for(int i = 0; i < h;i++){
      for(int j = 0; j< w / 2;j++){
          src_data = *gray256_addr;
          g0 =  gama[(src_data)];
		      //g0 =  (src_data&0xf0)>>4;
          gray256_addr++;

          src_data = *gray256_addr;
          g3 =  gama[src_data] << 4;
		      //g3 =  src_data&0xf0;
          gray256_addr++;
          *temp_dst = g0|g3;
          temp_dst++;
      }
      //gray256_addr += (vir_w - w);
  }
  return 0;
}

int logo_gray256_to_gray16(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w){
  ATRACE_CALL();

  char src_data;
  char  g0,g3;
  char *temp_dst = (char *)gray16_buffer;

  for(int i = 0; i < h;i++){
      for(int j = 0; j< w / 2;j++){
          src_data = *gray256_addr;
          g0 =  (src_data&0xf0)>>4;
          gray256_addr++;

          src_data = *gray256_addr;
          g3 =  src_data&0xf0;
          gray256_addr++;
          *temp_dst = g0|g3;
          temp_dst++;
      }
      //gray256_addr += (vir_w - w);
  }
  return 0;
}

int gray256_to_gray2(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w){

  ATRACE_CALL();

  unsigned char src_data;
  unsigned char  g0,g3;
  unsigned char *temp_dst = (unsigned char *)gray16_buffer;

  for(int i = 0; i < h;i++){
      for(int j = 0; j< w / 2;j++){
          src_data = *gray256_addr;
          g0 = src_data > 0x80 ? 0xf0 : 0x00;
          gray256_addr++;

          src_data = *gray256_addr;
          g3 =  src_data > 0x80 ? 0xf : 0x0;
          gray256_addr++;
          *temp_dst = g0|g3;
          temp_dst++;
      }
      //gray256_addr += (vir_w - w);
  }
  return 0;

}

void Luma8bit_to_4bit_row_2(short int  *src,  char *dst, short int *res0,  short int*res1, int w,int threshold)
{
    int i;
    int g0, g1, g2,g3,g4,g5,g6,g7,g_temp;
    int e;
    int v0, v1, v2, v3;
    int src_data;
    int src_temp_data;
    v0 = 0;
    for(i=0; i<w; i+=2)
    {

        src_data =  *src++;
        src_temp_data = src_data&0xff;
        g_temp = src_temp_data + res0[i] + v0;
        res0[i] = 0;
        g_temp = CLIP(g_temp);
        if(g_temp >= threshold)
            g0 = 0xf0;
        else
            g0 = 0x00;
        e = g_temp - g0;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        if(g_temp >= threshold)
            g0 = 0x0f;
        else
            g0 = 0x00;
        if( i==0 )
        {
            res1[i] += v2;
            res1[i+1] += v3;
        }
        else
        {
            res1[i-1] += v1;
            res1[i]   += v2;
            res1[i+1] += v3;
        }



        src_temp_data = ((src_data&0x0000ff00)>>8);
        g_temp = src_temp_data + res0[i+1] + v0;
        res0[i+1] = 0;
        g_temp = CLIP(g_temp);
        if(g_temp >= threshold)
            g1 = 0xf0;
        else
            g1 = 0x00;
        e = g_temp - g1;
        v0 = (e * 7) >> 4;
        v1 = (e * 3) >> 4;
        v2 = (e * 5) >> 4;
        v3 = (e * 1) >> 4;
        if(g_temp >= threshold)
            g1 = 0x0f;
        else
            g1 = 0x00;
        res1[i]     += v1;
        res1[i+1]   += v2;
        res1[i+2]   += v3;

        *dst++ =(g1<<4)|(g0);
    }

}

void Luma8bit_to_4bit(unsigned int *graynew,unsigned int *gray8bit,int  vir_height, int vir_width,int panel_w)
{
    ATRACE_CALL();

    int i,j;
    unsigned int  g0, g1, g2, g3,g4,g5,g6,g7;
    unsigned int *gray_new_temp;
#if 0
    for(j=0; j<vir_height; j++) //c code
    {
        gray_new_temp = graynew;
        for(i=0; i<panel_w; i+=16)
        {
            g0 = (*gray8bit & 0x000000f0) >> 4;
            g1 = (*gray8bit & 0x0000f000) >> 8;
            g2 = (*gray8bit & 0x00f00000) >> 12;
            g3 = (*gray8bit & 0xf0000000) >> 16;
            gray8bit++;

            g4 = (*gray8bit & 0x000000f0) << 12;
            g5 = (*gray8bit & 0x0000f000) << 8;
            g6 = (*gray8bit & 0x00f00000) << 4;
            g7 = (*gray8bit & 0xf0000000) ;
            *graynew++ = g0 | g1 | g2 | g3 | g4 |g5 | g6 | g7;
            gray8bit++;

            g0 = (*gray8bit & 0x000000f0) >> 4;
            g1 = (*gray8bit & 0x0000f000) >> 8;
            g2 = (*gray8bit & 0x00f00000) >> 12;
            g3 = (*gray8bit & 0xf0000000) >> 16;
            gray8bit++;

            g4 = (*gray8bit & 0x000000f0) << 12;
            g5 = (*gray8bit & 0x0000f000) << 8;
            g6 = (*gray8bit & 0x00f00000) << 4;
            g7 = (*gray8bit & 0xf0000000) ;
            *graynew++ = g0 | g1 | g2 | g3 | g4 |g5 | g6 | g7;
            gray8bit++;


        }

        gray_new_temp += vir_width>>3;
        graynew = gray_new_temp;
    }
#endif
#if 1
    if((panel_w % 32) == 0){
        neon_gray256_to_gray16ARM_32(graynew,gray8bit,vir_height,vir_width,panel_w);
    }
    else if((panel_w % 16) == 0){
        neon_gray256_to_gray16ARM_16(graynew,gray8bit,vir_height,vir_width,panel_w);
    }

    for(j=0; j<vir_height; j++) //c code
    {
        gray_new_temp = graynew;
        for(i=0; i<panel_w; i+=8)
        {
            g0 = (*gray8bit & 0x000000f0) >> 4;
            g1 = (*gray8bit & 0x0000f000) >> 8;
            g2 = (*gray8bit & 0x00f00000) >> 12;
            g3 = (*gray8bit & 0xf0000000) >> 16;
            gray8bit++;

            g4 = (*gray8bit & 0x000000f0) << 12;
            g5 = (*gray8bit & 0x0000f000) << 8;
            g6 = (*gray8bit & 0x00f00000) << 4;
            g7 = (*gray8bit & 0xf0000000) ;
            *graynew++ = g0 | g1 | g2 | g3 | g4 |g5 | g6 | g7;
            gray8bit++;

        }

        gray_new_temp += vir_width>>3;
        graynew = gray_new_temp;
    }

#endif
}


int gray256_to_gray2_dither(char *gray256_addr,char *gray2_buffer,int  panel_h, int panel_w,int vir_width,Region region){

    ATRACE_CALL();

    //do dither
    short int *line_buffer[2];
    line_buffer[0] =(short int *) malloc(panel_w << 1);
    line_buffer[1] =(short int *) malloc(panel_w << 1);

    size_t count = 0;
    const Rect* rects = region.getArray(&count);
    for (size_t i = 0;i < (int)count;i++) {
        memset(line_buffer[0], 0, panel_w << 1);
        memset(line_buffer[1], 0, panel_w << 1);

        int w = rects[i].right - rects[i].left;
        int offset = rects[i].top * panel_w + rects[i].left;
        int offset_dst = rects[i].top * vir_width + rects[i].left;
        if (offset_dst % 2) {
            offset_dst += (2 - offset_dst % 2);
        }
        if (offset % 2) {
            offset += (2 - offset % 2);
        }
        if ((offset_dst + w) % 2) {
            w -= (offset_dst + w) % 2;
        }
        for (int h = rects[i].top;h <= rects[i].bottom && h < panel_h;h++) {
            //ALOGD("DEBUG_lb Luma8bit_to_4bit_row_2, w:%d, offset:%d, offset_dst:%d", w, offset, offset_dst);
            Luma8bit_to_4bit_row_2((short int*)(gray256_addr + offset), (char *)(gray2_buffer + (offset_dst >> 1)),
                    line_buffer[h&1], line_buffer[!(h&1)], w, 0x80);
            offset += panel_w;
            offset_dst += vir_width;
        }
    }

    free(line_buffer[0]);
    free(line_buffer[1]);
  return 0;
}

void Rgb888_to_color_eink(char *dst,int *src,int  fb_height, int fb_width,int vir_width)
{
    int src_data;
    int  r1, g1, b1;
    int  r2, g2, b2;
    int  r3, g3, b3;
    int  r4, g4, b4;
    int  r5, g5, b5;
    int  r6, g6, b6;
    int i,j;
    int *temp_src;
    char *temp_dst;
    char *temp_dst1;
    int dst_dep;

    int new_value = 0;
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.eink.rgb", value, "0");
    new_value = atoi(value);
    //ALOGD("lyx: fb_height = %d, fb_width = %d, vir_width = %d\n", fb_height, fb_width, vir_width);

    dst_dep = fb_width % 6;
    //ALOGD("lyx: dst_dep = %d\n", dst_dep);
    for (i = 0; i < fb_height; i++) {
        temp_src = src + (i * fb_width);
        temp_dst = dst + (i * 3 * vir_width / 2);
        for (j = 0; j < (fb_width / 6); j++) {
            src_data = *temp_src++;
            r1 =  (src_data&0xf0)>>4;
            g1 = (src_data&0xf000)>>12;
            b1 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r2 =  (src_data&0xf0)>>4;
            g2 = (src_data&0xf000)>>12;
            b2 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r3 =  (src_data&0xf0)>>4;
            g3 = (src_data&0xf000)>>12;
            b3 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r4 =  (src_data&0xf0)>>4;
            g4 = (src_data&0xf000)>>12;
            b4 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r5 =  (src_data&0xf0)>>4;
            g5 = (src_data&0xf000)>>12;
            b5 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r6 =  (src_data&0xf0)>>4;
            g6 = (src_data&0xf000)>>12;
            b6 = (src_data&0xf00000)>>20;

            if (new_value == 1) {//red
                r1 = r2 = r3 = r4 = r5 = r6 =  0xf;
                g1 = g2 = g3 = g4 = g5 = g6 = 0;
                b1 = b2 = b3 = b4 = b5 = b6 = 0;
            }
            else if (new_value == 2) {//green
                r1 = r2 = r3 = r4 = r5 = r6 =  0;
                g1 = g2 = g3 = g4 = g5 = g6 = 0xf;
                b1 = b2 = b3 = b4 = b5 = b6 = 0;
            }
            else if (new_value == 3) {//blue
                r1 = r2 = r3 = r4 = r5 = r6 =  0;
                g1 = g2 = g3 = g4 = g5 = g6 = 0;
                b1 = b2 = b3 = b4 = b5 = b6 = 0xf;
            }

            temp_dst1 = temp_dst + (j * 9);
            *temp_dst1++ = g1 | (g1<<4);
            *temp_dst1++ = g1 | (b2<<4);
            *temp_dst1++ = b2 | (b2<<4);

            *temp_dst1++ = r3 | (r3<<4);
            *temp_dst1++ = r3 | (g4<<4);
            *temp_dst1++ = g4 | (g4<<4);

            *temp_dst1++ = b5 | (b5<<4);
            *temp_dst1++ = b5 | (r6<<4);
            *temp_dst1++ = r6 | (r6<<4);

            temp_dst1 = temp_dst + (vir_width/2) + (j * 9);
            *temp_dst1++ = b1 | (b1<<4);
            *temp_dst1++ = b1 | (r2<<4);
            *temp_dst1++ = r2 | (r2<<4);

            *temp_dst1++ = g3 | (g3<<4);
            *temp_dst1++ = g3 | (b4<<4);
            *temp_dst1++ = b4 | (b4<<4);

            *temp_dst1++ = r5 | (r5<<4);
            *temp_dst1++ = r5 | (g6<<4);
            *temp_dst1++ = g6 | (g6<<4);

            temp_dst1 = temp_dst + vir_width + (j * 9);
            *temp_dst1++ = r1 | (r1<<4);
            *temp_dst1++ = r1 | (g2<<4);
            *temp_dst1++ = g2 | (g2<<4);

            *temp_dst1++ = b3 | (b3<<4);
            *temp_dst1++ = b3 | (r4<<4);
            *temp_dst1++ = r4 | (r4<<4);

            *temp_dst1++ = g5 | (g5<<4);
            *temp_dst1++ = g5 | (b6<<4);
            *temp_dst1++ = b6 | (b6<<4);
        }

        if (dst_dep == 4) {
            src_data = *temp_src++;
            r1 =  (src_data&0xf0)>>4;
            g1 = (src_data&0xf000)>>12;
            b1 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r2 =  (src_data&0xf0)>>4;
            g2 = (src_data&0xf000)>>12;
            b2 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r3 =  (src_data&0xf0)>>4;
            g3 = (src_data&0xf000)>>12;
            b3 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r4 =  (src_data&0xf0)>>4;
            g4 = (src_data&0xf000)>>12;
            b4 = (src_data&0xf00000)>>20;

            temp_dst1 = temp_dst + (j * 9);
            *temp_dst1++ = g1 | (g1<<4);
            *temp_dst1++ = g1 | (b2<<4);
            *temp_dst1++ = b2 | (b2<<4);

            *temp_dst1++ = r3 | (r3<<4);
            *temp_dst1++ = r3 | (g4<<4);
            *temp_dst1++ = g4 | (g4<<4);

            temp_dst1 = temp_dst + (vir_width/2) + (j * 9);
            *temp_dst1++ = b1 | (b1<<4);
            *temp_dst1++ = b1 | (r2<<4);
            *temp_dst1++ = r2 | (r2<<4);

            *temp_dst1++ = g3 | (g3<<4);
            *temp_dst1++ = g3 | (b4<<4);
            *temp_dst1++ = b4 | (b4<<4);

            temp_dst1 = temp_dst + vir_width + (j * 9);
            *temp_dst1++ = r1 | (r1<<4);
            *temp_dst1++ = r1 | (g2<<4);
            *temp_dst1++ = g2 | (g2<<4);

            *temp_dst1++ = b3 | (b3<<4);
            *temp_dst1++ = b3 | (r4<<4);
            *temp_dst1++ = r4 | (r4<<4);
        }
        else if (dst_dep == 2) {
            src_data = *temp_src++;
            r1 =  (src_data&0xf0)>>4;
            g1 = (src_data&0xf000)>>12;
            b1 = (src_data&0xf00000)>>20;
            src_data = *temp_src++;
            r2 =  (src_data&0xf0)>>4;
            g2 = (src_data&0xf000)>>12;
            b2 = (src_data&0xf00000)>>20;

            temp_dst1 = temp_dst + (j * 9);
            *temp_dst1++ = g1 | (g1<<4);
            *temp_dst1++ = g1 | (b2<<4);
            *temp_dst1++ = b2 | (b2<<4);

            temp_dst1 = temp_dst + (vir_width/2) + (j * 9);
            *temp_dst1++ = b1 | (b1<<4);
            *temp_dst1++ = b1 | (r2<<4);
            *temp_dst1++ = r2 | (r2<<4);

            temp_dst1 = temp_dst + vir_width + (j * 9);
            *temp_dst1++ = r1 | (r1<<4);
            *temp_dst1++ = r1 | (g2<<4);
            *temp_dst1++ = g2 | (g2<<4);
        }
    }
}

void Rgb888_to_color_eink2(char *dst,int *src,int  fb_height, int fb_width, int vir_width)
{
    int count;
    int src_data;
    int  r1,g1,b1,r2,b2,g2,r3,g3,b3;
    int i,j;
    int *temp_src;
    char *temp_dst,*temp1_dst,*temp2_dst;
#if 0
    for(i = 0; i < fb_height;i++){//RGB888->RGB444
		temp_dst = dst;

        for(j = 0; j<fb_width/2;j++){
            src_data = *src++;
            g0 =  (src_data&0xf0)>>4;
            g1 = (src_data&0xf000)>>8;
            g2 = (src_data&0xf00000)>>20;
            src_data = *src++;
            g3 =  src_data&0xf0;
            g4 = (src_data&0xf000)>>12;
            g5 = (src_data&0xf00000)>>16;
            *dst++ = g0|g1;
            *dst++ = g2|g3;
            *dst++ = g4|g5;
        }
	  dst = temp_dst + vir_width/2;
    }
#endif

  char value[PROPERTY_VALUE_MAX];
  property_get("debug.eink.rgb", value, "0");
  int new_value = 0;
  new_value = atoi(value);
  int width_tmp = fb_width /3;
  int width_lost = fb_width % 3;

  for(i = 0; i < fb_height;i++){//RGB888->RGB444
      temp_dst = dst;
      temp1_dst = dst;
      temp2_dst = dst + fb_width;
      for(j = 0; j < width_tmp;){
          if(new_value == 1){

              src_data = *src++;
              r1 = 0xf0;
              g1 = 0x0;
              b1 = 0x0;
              src_data = *src++;
              r2 = 0xf0;
              g2 = 0x0;
              b2 = 0x0;
              src_data = *src++;
              r3 = 0xf0;
              g3 = 0x0;
              b3 = 0x0;

          }else if(new_value == 2){
              src_data = *src++;
              r1 = 0x0;
              g1 = 0xf000;
              b1 = 0x0;
              src_data = *src++;
              r2 = 0x0;
              g2 = 0xf000;
              b2 = 0x0;
              src_data = *src++;
              r3 = 0x0;
              g3 = 0xf000;
              b3 = 0x0;
          }else if(new_value == 3){
              src_data = *src++;
              r1 = 0x0;
              g1 = 0x0;
              b1 = 0xf00000;
              src_data = *src++;
              r2 = 0x0;
              g2 = 0x0;
              b2 = 0xf00000;
              src_data = *src++;
              r3 = 0x0;
              g3 = 0x0;
              b3 = 0xf00000;
          }else{
              src_data = *src++;
              b1 = src_data&0xf00000;
              g1 = src_data&0xf000;
              r1 = src_data&0xf0;
              src_data = *src++;
              b2 = src_data&0xf00000;
              g2 = src_data&0xf000;
              r2 = src_data&0xf0;
              src_data = *src++;
              b3 = src_data&0xf00000;
              g3 = src_data&0xf000;
              r3 = src_data&0xf0;
          }
          if(i % 3 == 0){
              dst = temp1_dst;
              *dst++ = ((r1 >> 4) | (b1 >> 16));
              *dst++ = ((g2 >> 12)  | (r2 >> 0));
              *dst++ = ((b3 >> 20)  | (g3 >> 8));
              temp1_dst = dst;

              dst = temp2_dst;
              *dst++ = ((g1 >> 12) | (r1 >> 0));
              *dst++ = ((b2 >> 20) | (g2 >> 8));
              *dst++ = ((r3 >> 4)  | (b3 >> 16));
              temp2_dst = dst;

		j++;
		if ((width_lost == 1) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((r1 >> 4) | (b1 >> 16));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((g1 >> 12) | (r1 >> 0));
			temp2_dst = dst;
		}
		else if ((width_lost == 2) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;
			src_data = *src++;
			b2 = src_data&0xf00000;
			g2 = src_data&0xf000;
			r2 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((r1 >> 4) | (b1 >> 16));
			*dst++ = ((g2 >> 12)  | (r2 >> 0));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((g1 >> 12) | (r1 >> 0));
			*dst++ = ((b2 >> 20) | (g2 >> 8));
			temp2_dst = dst;
		}

          }else if(i % 3 == 1){
              dst = temp1_dst;
              *dst++ = ((b1 >> 20) | (g1 >> 8));
              *dst++ = ((r2 >> 4) | (b2 >> 16));
              *dst++ = ((g3 >> 12)  | (r3 >> 0));
              temp1_dst = dst;

              dst = temp2_dst;
              *dst++ = ((r1 >> 4) | (b1 >> 16));
              *dst++ = ((g2 >> 12)  | (r2 >> 0));
              *dst++ = ((b3 >> 20)  | (g3 >> 8));
              temp2_dst = dst;

		j++;
		if ((width_lost == 1) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((b1 >> 20) | (g1 >> 8));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((r1 >> 4) | (b1 >> 16));
			temp2_dst = dst;
		}
		else if ((width_lost == 2) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;
			src_data = *src++;
			b2 = src_data&0xf00000;
			g2 = src_data&0xf000;
			r2 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((b1 >> 20) | (g1 >> 8));
			*dst++ = ((r2 >> 4) | (b2 >> 16));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((r1 >> 4) | (b1 >> 16));
			*dst++ = ((g2 >> 12)  | (r2 >> 0));
			temp2_dst = dst;
		}
          }else if(i % 3 == 2){
              dst = temp1_dst;
              *dst++ = ((g1 >> 12) | (r1 >> 0));
              *dst++ = ((b2 >> 20) | (g2 >> 8));
              *dst++ = ((r3 >> 4)  | (b3 >> 16));
              temp1_dst = dst;

              dst = temp2_dst;
              *dst++ = ((b1 >> 20) | (g1 >> 8));
              *dst++ = ((r2 >> 4)  | (b2 >> 16));
              *dst++ = ((g3 >> 12)  | (r3 >> 0));
              temp2_dst = dst;

		j++;
		if ((width_lost == 1) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((g1 >> 12) | (r1 >> 0));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((b1 >> 20) | (g1 >> 8));
			temp2_dst = dst;
		}
		else if ((width_lost == 2) && (j >= width_tmp)) {
			src_data = *src++;
			b1 = src_data&0xf00000;
			g1 = src_data&0xf000;
			r1 = src_data&0xf0;
			src_data = *src++;
			b2 = src_data&0xf00000;
			g2 = src_data&0xf000;
			r2 = src_data&0xf0;

			dst = temp1_dst;
			*dst++ = ((g1 >> 12) | (r1 >> 0));
			*dst++ = ((b2 >> 20) | (g2 >> 8));
			temp1_dst = dst;

			dst = temp2_dst;
			*dst++ = ((b1 >> 20) | (g1 >> 8));
			*dst++ = ((r2 >> 4)  | (b2 >> 16));
			temp2_dst = dst;
		}
	}
      }
  dst = temp_dst + vir_width;
  }
}

void neon_rgb888_to_gray256ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w)
{
   if((vir_w % 32) == 0){
        neon_rgb888_to_gray256ARM_32(dest,src,h,w,vir_w);
    }
    else{
        neon_rgb888_to_gray256ARM_16(dest,src,h,w,vir_w);
    }
}

void rgb888_to_gray16_dither(int *dst,uint8_t *src,int  panel_h, int panel_w,int vir_width)
{
    uint8_t *gray_256;
    int h;
    int w;
    short int *line_buffer[2];
    char *src_buffer;
    gray_256 = (uint8_t*)malloc(panel_h*panel_w);
    line_buffer[0] =(short int *) malloc(panel_w*2);
    line_buffer[1] =(short int *) malloc(panel_w*2);
    memset(line_buffer[0],0,panel_w*2);
    memset(line_buffer[1],0,panel_w*2);
    neon_rgb888_to_gray256ARM((uint8_t*)gray_256,(uint8_t*)src,panel_h,panel_w,vir_width);

    src_buffer = (char*)gray_256;
    for(h = 0;h<panel_h;h++){
        Luma8bit_to_4bit_row_16((int*)src_buffer,dst,line_buffer[h&1],line_buffer[!(h&1)],panel_w);
        dst = dst+panel_w/8;
        src_buffer = (char*)src_buffer+panel_w;
    }
    free(line_buffer[0]);
    free(line_buffer[1]);
    free(gray_256);
}

void neon_rgb888_to_gray16ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w)
{
    if((vir_w % 32) == 0){
        neon_rgb888_to_gray16ARM_32(dest,src,h,w,vir_w);
    }
    else{
        neon_rgb888_to_gray16ARM_16(dest,src,h,w,vir_w);
    }

}

void Luma8bit_to_4bit_dither(int *dst,int *src,int  vir_height, int vir_width,int panel_w)
{
    int h;
    int w;
    char *gray_256;
    char *src_buffer;
    short int *line_buffer[2];
    gray_256 = (char*)malloc(vir_height*vir_width);
    line_buffer[0] =(short int *) malloc(panel_w*2);
    line_buffer[1] =(short int *) malloc(panel_w*2);
    memset(line_buffer[0],0,panel_w*2);
    memset(line_buffer[1],0,panel_w*2);

    neon_gray256_to_gray256((int*)gray_256,(int*)src,vir_height,vir_width);
    src_buffer = (char*)gray_256;
    for(h = 0;h<vir_height;h++){
        Luma8bit_to_4bit_row_16((int *)src_buffer,(int *)dst,line_buffer[h&1],line_buffer[!(h&1)],panel_w);
        dst = dst+vir_width/8;
        src_buffer = src_buffer+panel_w;//vir_width;
    }
    free(line_buffer[0]);
    free(line_buffer[1]);
    free(gray_256);
}

void rgb888_to_gray2_dither(uint8_t *dst, uint8_t *src, int panel_h, int panel_w,
        int vir_width, Region region)
{
    //convert to gray 256.
    uint8_t *gray_256;
    gray_256 = (uint8_t*)malloc(panel_h*panel_w);
    neon_rgb888_to_gray256ARM((uint8_t*)gray_256,(uint8_t*)src,panel_h,panel_w,panel_w);

    //do dither
    short int *line_buffer[2];
    line_buffer[0] =(short int *) malloc(panel_w << 1);
    line_buffer[1] =(short int *) malloc(panel_w << 1);

    size_t count = 0;
    const Rect* rects = region.getArray(&count);
    for (int i = 0;i < (int)count;i++) {
        memset(line_buffer[0], 0, panel_w << 1);
        memset(line_buffer[1], 0, panel_w << 1);

        int w = rects[i].right - rects[i].left;
        int offset = rects[i].top * panel_w + rects[i].left;
        int offset_dst = rects[i].top * vir_width + rects[i].left;
        if (offset_dst % 2) {
            offset_dst += (2 - offset_dst % 2);
        }
        if (offset % 2) {
            offset += (2 - offset % 2);
        }
        if ((offset_dst + w) % 2) {
            w -= (offset_dst + w) % 2;
        }
        for (int h = rects[i].top;h <= rects[i].bottom && h < panel_h;h++) {
            //LOGE("jeffy Luma8bit_to_4bit_row_2, w:%d, offset:%d, offset_dst:%d", w, offset, offset_dst);
            Luma8bit_to_4bit_row_2((short int*)(gray_256 + offset), (char*)(dst + (offset_dst >> 1)),
                    line_buffer[h&1], line_buffer[!(h&1)], w, 0x80);
            offset += panel_w;
            offset_dst += vir_width;
        }
    }

    free(line_buffer[0]);
    free(line_buffer[1]);
    free(gray_256);
}

static inline void apply_white_region(char *buffer, int height, int width, Region region)
{
	int left,right;
    if (region.isEmpty()) return;
    size_t count = 0;
    const Rect* rects = region.getArray(&count);
    for (int i = 0;i < (int)count;i++) {
	 left = rects[i].left;
	 right = rects[i].right;
        int w = right - left;
        int offset = rects[i].top * width + left;
        for (int h = rects[i].top;h <= rects[i].bottom && h < height;h++) {
            memset(buffer + (offset >> 1), 0xFF, w >> 1);
            offset += width;
        }
    }
}


int hwc_post_epd(int *buffer, Rect rect, int mode){
  ATRACE_CALL();

  struct ebc_buf_info buf_info;

  if(ioctl(ebc_fd, GET_EBC_BUFFER,&buf_info)!=0)
  {
     ALOGE("GET_EBC_BUFFER failed\n");
    return -1;
  }

  buf_info.win_x1 = rect.left;
  buf_info.win_x2 = rect.right;
  buf_info.win_y1 = rect.top;
  buf_info.win_y2 = rect.bottom;
  buf_info.epd_mode = mode;


  char value[PROPERTY_VALUE_MAX];
  property_get("debug.dump", value, "0");
  int new_value = 0;
  new_value = atoi(value);
  if(new_value > 0){
      char data_name[100] ;
      static int DumpSurfaceCount = 0;

      sprintf(data_name,"/data/dump/dmlayer%d_%d_%d.bin", DumpSurfaceCount,
               buf_info.vir_width, buf_info.vir_height);
      DumpSurfaceCount++;
      FILE *file = fopen(data_name, "wb+");
      if (!file)
      {
          ALOGW("Could not open %s\n",data_name);
      } else{
          ALOGW("open %s and write ok\n",data_name);
          fwrite(buffer, buf_info.vir_height * buf_info.vir_width >> 1 , 1, file);
          fclose(file);

      }
      if(DumpSurfaceCount > 20){
          property_set("debug.dump","0");
          DumpSurfaceCount = 0;
      }
  }

  ALOGD_IF(log_level(DBG_DEBUG),"%s, line = %d ,mode = %d, (x1,x2,y1,y2) = (%d,%d,%d,%d) ",__FUNCTION__,__LINE__,
      mode,buf_info.win_x1,buf_info.win_x2,buf_info.win_y1,buf_info.win_y2);
  unsigned long vaddr_real = intptr_t(ebc_buffer_base);
  memcpy((void *)(vaddr_real + buf_info.offset), buffer,
          buf_info.vir_height * buf_info.vir_width >> 1);

  if(ioctl(ebc_fd, SET_EBC_SEND_BUFFER,&buf_info)!=0)
  {
     ALOGE("SET_EBC_SEND_BUFFER failed\n");
     return -1;
  }
  return 0;
}


static int not_fullmode_count = 0;
static int not_fullmode_num = 500;
static int curr_not_fullmode_num = -1;
int hwc_set_epd(hwc_drm_display_t *hd, hwc_layer_1_t *fb_target, Region &A2Region,Region &updateRegion,Region &AutoRegion) {
  ATRACE_CALL();

  int ret = 0;
  Rect postRect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);

  ALOGD_IF(log_level(DBG_DEBUG), "%s:rgaBuffer_index=%d", __FUNCTION__, hd->rgaBuffer_index);

  int *gray16_buffer;

  char value[PROPERTY_VALUE_MAX];
  gray16_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height >> 1);
  //Get virtual address
  const gralloc_module_t *gralloc;
  ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                    (const hw_module_t **)&gralloc);
  if (ret) {
      ALOGE("Failed to open gralloc module");
      return ret;
  }

  char* gray256_addr = NULL;

  DumpLayer("rgba", fb_target->handle);

#if USE_RGA

  DrmRgaBuffer &gra256_buffer = hd->rgaBuffers[hd->rgaBuffer_index];
  if (!gra256_buffer.Allocate(hd->framebuffer_width, hd->framebuffer_height, HAL_PIXEL_FORMAT_YCrCb_NV12)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", hd->framebuffer_width, hd->framebuffer_height);
    return -ENOMEM;
  }
  ret = hwc_rgba888_to_gray256(gra256_buffer, fb_target, hd);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }


  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = gra256_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc,src_hnd,ATT_SIZE);

  gralloc->lock(gralloc, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                  0, 0, width, height, (void **)&gray256_addr);
#else

  char* framebuffer_base = NULL;
  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = fb_target->handle;

  width = hwc_get_handle_attibute(gralloc,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc,src_hnd,ATT_SIZE);

  gralloc->lock(gralloc, fb_target->handle, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                  0, 0, width, height, (void **)&framebuffer_base);
//  ALOGD("rk-debug %s,line = %d, w = %d , h = %d , lock ret = %d",__FUNCTION__,__LINE__,ebc_buf_info.width,ebc_buf_info.height,ret);
//  ALOGD("rk-debug %s,line = %d, framebuffer w = %d , h = %d , format =  %d",__FUNCTION__,__LINE__,width,height,format);
//  ALOGD("rk-debug %s,line = %d, framebuffer base = %p ",__FUNCTION__,__LINE__,framebuffer_base);

#endif
send_one_buffer:

  ALOGD_IF(log_level(DBG_DEBUG),"HWC %s,line = %d >>>>>>>>>>>>>> begin post frame = %d >>>>>>>>",__FUNCTION__,__LINE__,get_frame());
  //reset mode to default.
  int epdMode = gCurrentEpdMode;
  gCurrentEpdMode = gResetEpdMode;

	if(epdMode == EPD_NULL){
    gralloc->unlock(gralloc, src_hnd);
    gray256_addr = NULL;
    return 0;
	}

  if(epdMode == EPD_FULL || epdMode == EPD_BLOCK)
  {
      A2Region.clear();
  }
  else if(epdMode == EPD_A2)
  {
      epdMode = EPD_PART;
  }

  //only when has a2 region, we are in a2 mode.
  if(!A2Region.isEmpty() || !gLastA2Region.isEmpty())
  {
      epdMode = EPD_A2;
  }

  int *gray16_buffer_bak = gray16_buffer;

#if USE_RGA
  if(epdMode == EPD_AUTO)
  {

    char pro_value[PROPERTY_VALUE_MAX];
    property_get("debug.auto",pro_value,"0");
    switch(atoi(pro_value)){
      case 0:
        gray256_to_gray16(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
        break;
      case 1:
        gray256_to_gray16_dither(gray256_addr,gray16_buffer,ebc_buf_info.vir_height, ebc_buf_info.vir_width, ebc_buf_info.width);
        break;
      case 2:
        Luma8bit_to_4bit((unsigned int*)gray16_buffer,(unsigned int*)(gray256_addr),
                          ebc_buf_info.height, ebc_buf_info.width,ebc_buf_info.width);
        break;
      case 3:
        gray256_to_gray2_dither((char *)gray256_addr,
              (char *)gray16_buffer, ebc_buf_info.vir_height,
              ebc_buf_info.width,
              ebc_buf_info.vir_width, AutoRegion);
        break;
      case 4:
        gray256_to_gray2(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
        break;
      default:
        gray256_to_gray16(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
        break;
    }
  }
  else if (epdMode != EPD_A2)
  {
    if(epdMode == EPD_FULL_DITHER){
      gray256_to_gray16_dither(gray256_addr,gray16_buffer,ebc_buf_info.vir_height, ebc_buf_info.vir_width, ebc_buf_info.width);
    }else{
      gray256_to_gray16(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
#if USE_NENO_TO_Y16
      if((ebc_buf_info.width % 32) == 0){
        neon_gray256_to_gray16ARM_32((unsigned int *)gray16_buffer,(unsigned int *)gray256_addr,ebc_buf_info.height,ebc_buf_info.width,ebc_buf_info.width);
      }
      else if((ebc_buf_info.width % 16) == 0){
          neon_gray256_to_gray16ARM_16((unsigned int *)gray16_buffer,(unsigned int *)gray256_addr,ebc_buf_info.height,ebc_buf_info.width,ebc_buf_info.width);
      }
#endif
    }
  }

  switch(epdMode){
      case EPD_FULL:
      case EPD_FULL_WIN:
      case EPD_FULL_DITHER:
      case EPD_AUTO:
      case EPD_FULL_GL16:
      case EPD_FULL_GLR16:
      case EPD_FULL_GLD16:
          //LOGE("jeffy FULL");
          not_fullmode_count = 0;
          break;
      case EPD_A2:
          {
              Region screenRegion(Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height));
              if(log_level(DBG_DEBUG))
                  screenRegion.dump("fremebuffer1 screenRegion");
              if (screenRegion.subtract(A2Region).isEmpty() &&
                      screenRegion.subtract(gLastA2Region).isEmpty()) {
                  //all screen region was in a2 mode.
                  if(log_level(DBG_DEBUG))
                      screenRegion.dump("fremebuffer subtract screenRegion");
                  gray256_to_gray2_dither(gray256_addr,(char *)gray16_buffer,ebc_buf_info.vir_height, ebc_buf_info.vir_width, ebc_buf_info.width,screenRegion);
                  break;
              }
              //window a2.
              Luma8bit_to_4bit((unsigned int*)gray16_buffer,(unsigned int*)(gray256_addr),
                                ebc_buf_info.height, ebc_buf_info.width,ebc_buf_info.width);
              if (A2Region.isEmpty()) {
                 // ALOGE("jeffy quit A2");
                  //get out a2 mode.
                  //1.reset updated region to white.
                  updateRegion.orSelf(gLastA2Region);
                  updateRegion.orSelf(gSavedUpdateRegion);
                  gLastA2Region.clear();
                  gSavedUpdateRegion.clear();
                  apply_white_region((char*)gray16_buffer, ebc_buf_info.height,ebc_buf_info.vir_width, updateRegion);

                  //2.paint updated region.
                  epdMode = EPD_A2;//EPD_BLACK_WHITE;//
                  hwc_post_epd(gray16_buffer, postRect, epdMode);

                  //3.will repaint those regions in full mode.
                  gCurrentEpdMode = EPD_FULL_WIN;
                  Rect rect = updateRegion.getBounds();
                  postRect = rect;
                  goto    send_one_buffer;
              }

              Region newA2Region = A2Region - gSavedUpdateRegion - gLastA2Region;
              Region newUpdateRegion = updateRegion - gSavedUpdateRegion - gLastA2Region;
              if(log_level(DBG_DEBUG)){
                  newA2Region.dump("fremebuffer1 newA2Region");
                  newUpdateRegion.dump("fremebuffer1 newUpdateRegion");
                  A2Region.dump("fremebuffer1 currentA2Region");
              }

              //update saved region info.
              gSavedUpdateRegion.orSelf(gLastA2Region);
              gSavedUpdateRegion.orSelf(updateRegion);

              gLastA2Region = A2Region;
              gray256_to_gray2_dither((char *)gray256_addr,
                      (char *)gray16_buffer, ebc_buf_info.vir_height,
                      ebc_buf_info.width,
                      ebc_buf_info.vir_width, gSavedUpdateRegion);

              if (!newA2Region.isEmpty() || !newUpdateRegion.isEmpty()) {
                  //has new region.
                  if(log_level(DBG_DEBUG)){
                      newA2Region.dump("fremebuffer2 newA2Region");
                      newUpdateRegion.dump("fremebuffer2 newUpdateRegion");
                  }
                  //1.reset new region to white, and paint them.
                  apply_white_region((char*)gray16_buffer, ebc_buf_info.height,
                          ebc_buf_info.vir_width, newUpdateRegion | newA2Region);
                  epdMode = EPD_BLACK_WHITE;
                  hwc_post_epd(gray16_buffer, postRect, epdMode);
                  //2.will repaint those regions in a2 mode.
                  goto    send_one_buffer;
              }
              //not_fullmode_count++;
              break;
          }
      case EPD_BLOCK:
         // release_wake_lock("show_advt_lock");
      default:
          //LOGE("jeffy part:%d", epdMode);
          not_fullmode_count++;
          break;

  }

#else
  if (epdMode != EPD_A2)
  {
      if(ebc_buf_info.color_panel == 1)
      {
          int i;
          int *temp_rgb;
          int*temp_gray;
          temp_rgb = (int*)(framebuffer_base);
          temp_gray = (int*)gray16_buffer;
          //image_to_cfa_grayscale(ebc_buf_info.fb_width, ebc_buf_info.fb_height, (unsigned char*)(framebuffer_base), (unsigned char*)(new_buffer));
          //neon_rgb888_to_gray16ARM((uint8_t *)gray16_buffer, (uint8_t *)(new_buffer),ebc_buf_info.fb_height,ebc_buf_info.fb_width,ebc_buf_info.vir_width);
          Rgb888_to_color_eink((char*)gray16_buffer,(int*)(framebuffer_base),ebc_buf_info.fb_height,ebc_buf_info.fb_width,ebc_buf_info.vir_width);
      }
      else if(gPixel_format==8) {
          if (epdMode == EPD_FULL_DITHER)
          {
              Luma8bit_to_4bit_dither((int*)gray16_buffer,(int*)(framebuffer_base),ebc_buf_info.vir_height,ebc_buf_info.vir_width,ebc_buf_info.width);
          }
          else
          {
              Luma8bit_to_4bit((unsigned int*)gray16_buffer,(unsigned int*)(framebuffer_base),ebc_buf_info.height,ebc_buf_info.width,ebc_buf_info.width);
          }
      }
      else
      {
          if (epdMode == EPD_FULL_DITHER)
          {
              rgb888_to_gray16_dither((int*)gray16_buffer,(uint8_t*)(framebuffer_base), ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
          }
          else
          {
              neon_rgb888_to_gray16ARM((uint8_t*)gray16_buffer,(uint8_t*)(framebuffer_base), ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
      }
      }
  }
  switch(epdMode){
        case EPD_FULL:
        case EPD_FULL_WIN:
        case EPD_FULL_DITHER:
        case EPD_AUTO:
        case EPD_FULL_GL16:
        case EPD_FULL_GLR16:
        case EPD_FULL_GLD16:
            //LOGE("jeffy FULL");
            not_fullmode_count = 0;
            break;
        case EPD_A2:
            {
                Region screenRegion(Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height));
                if(log_level(DBG_DEBUG))
                    screenRegion.dump("fremebuffer1 screenRegion");
                if (screenRegion.subtract(A2Region).isEmpty() &&
                        screenRegion.subtract(gLastA2Region).isEmpty()) {
                    //all screen region was in a2 mode.
                    if(log_level(DBG_DEBUG))
                        screenRegion.dump("fremebuffer subtract screenRegion");
                    rgb888_to_gray2_dither((uint8_t*)gray16_buffer,
                                    (uint8_t*)framebuffer_base, ebc_buf_info.vir_height,
                                    ebc_buf_info.width, ebc_buf_info.vir_width, screenRegion);
                    break;
                }
                //window a2.
                uint8_t *gray_256;
			          gray_256 = (uint8_t*)malloc(ebc_buf_info.height * ebc_buf_info.width);
                neon_rgb888_to_gray256ARM((uint8_t*)gray_256,
                            (uint8_t*)framebuffer_base,
                            ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);
                Luma8bit_to_4bit((unsigned int*)gray16_buffer,(unsigned int*)(gray_256),
                                  ebc_buf_info.height, ebc_buf_info.width,ebc_buf_info.width);
                if (A2Region.isEmpty()) {
                   // ALOGE("jeffy quit A2");
                    //get out a2 mode.
                    //1.reset updated region to white.
                    updateRegion.orSelf(gLastA2Region);
                    updateRegion.orSelf(gSavedUpdateRegion);
                    gLastA2Region.clear();
                    gSavedUpdateRegion.clear();
                    apply_white_region((char*)gray16_buffer, ebc_buf_info.height,ebc_buf_info.width, updateRegion);

                    //2.paint updated region.
                    epdMode = EPD_A2;//EPD_BLACK_WHITE;//
                    hwc_post_epd(gray16_buffer, postRect, epdMode);

                    //3.will repaint those regions in full mode.
                    gCurrentEpdMode = EPD_FULL_WIN;
                    Rect rect = updateRegion.getBounds();
                    postRect = rect;
                    free(gray_256);
                    goto    send_one_buffer;
                }

                Region newA2Region = A2Region - gSavedUpdateRegion - gLastA2Region;
                Region newUpdateRegion = updateRegion - gSavedUpdateRegion - gLastA2Region;

                if(log_level(DBG_DEBUG)){
                    newA2Region.dump("fremebuffer1 newA2Region");
                    newUpdateRegion.dump("fremebuffer1 newUpdateRegion");
                    A2Region.dump("fremebuffer1 currentA2Region");
                }

                //update saved region info.
                gSavedUpdateRegion.orSelf(gLastA2Region);
                gSavedUpdateRegion.orSelf(updateRegion);
                gLastA2Region = A2Region;
                gray256_to_gray2_dither((char *)gray_256,
                        (char *)gray16_buffer, ebc_buf_info.vir_height,
                        ebc_buf_info.width, ebc_buf_info.vir_width,
                        gSavedUpdateRegion);
                if (!newA2Region.isEmpty() || !newUpdateRegion.isEmpty()) {
                    //has new region.
                    if(log_level(DBG_DEBUG)){
                        newA2Region.dump("fremebuffer2 newA2Region");
                        newUpdateRegion.dump("fremebuffer2 newUpdateRegion");
                    }
                    //1.reset new region to white, and paint them.
                    apply_white_region((char*)gray16_buffer, ebc_buf_info.height,
                            ebc_buf_info.vir_width, newUpdateRegion | newA2Region);
                    epdMode = EPD_BLACK_WHITE;
                    hwc_post_epd(gray16_buffer, postRect, epdMode);
                    //2.will repaint those regions in a2 mode.
                    free(gray_256);
                    goto    send_one_buffer;
                }
                //not_fullmode_count++;
                break;
            }
        case EPD_BLOCK:
           // release_wake_lock("show_advt_lock");
        default:
            //LOGE("jeffy part:%d", epdMode);
            not_fullmode_count++;
            break;

    }
#endif

  char pro_value[PROPERTY_VALUE_MAX];
  property_get("persist.vendor.fullmode_cnt",pro_value,"500");

  //not fullmode count will do by kernel, do not deal now here
  //if(not_fullmode_count > atoi(pro_value)){
  //    epdMode = EPD_FULL;
  //    not_fullmode_count = 0;
  //}

  not_fullmode_num = atoi(pro_value);
  if (not_fullmode_num != curr_not_fullmode_num) {
    if(ioctl(ebc_fd, SET_EBC_NOT_FULL_NUM, &not_fullmode_num) != 0) {
        ALOGE("SET_EBC_NOT_FULL_NUM failed\n");
    }
    curr_not_fullmode_num = not_fullmode_num;
  }
  hwc_post_epd(gray16_buffer_bak, postRect, epdMode);

  ALOGD_IF(log_level(DBG_DEBUG),"HWC %s,line = %d >>>>>>>>>>>>>> end post frame = %d >>>>>>>>",__FUNCTION__,__LINE__,get_frame());

  gralloc->unlock(gralloc, src_hnd);
  free(gray16_buffer_bak);
  gray16_buffer_bak = NULL;
  gray16_buffer = NULL;
  return 0;
}

void hwc_free_buffer(hwc_drm_display_t *hd) {
    for(int i = 0; i < MaxRgaBuffers; i++) {
        hd->rgaBuffers[i].Clear();
    }
}
#endif

bool decode_image_file(const char* filename, SkBitmap* bitmap,
                               SkColorType colorType = kN32_SkColorType,
                               bool requireUnpremul = false) {
    sk_sp<SkData> data(SkData::MakeFromFileName(filename));
    std::unique_ptr<SkCodec> codec(SkCodec::NewFromData(data));
    if (!codec) {
        return false;
    }

    SkImageInfo info = codec->getInfo().makeColorType(colorType);
    if (requireUnpremul && kPremul_SkAlphaType == info.alphaType()) {
        info = info.makeAlphaType(kUnpremul_SkAlphaType);
    }

    if (!bitmap->tryAllocPixels(info)) {
        return false;
    }

    return SkCodec::kSuccess == codec->getPixels(info, bitmap->getPixels(), bitmap->rowBytes());
}

void drawLogoPic(const char src_path[], void* buf, int width, int height)
{
    ALOGD(" in drawLogoPic begin");
    SkBitmap bitmap;
    int x = 0;
    int y = 0;

    if (!decode_image_file(src_path, &bitmap)) {
        ALOGE("drawLogoPic decode_image_file error path:%s", src_path);
        return;
    }

    SkBitmap dst;
    SkImageInfo info = SkImageInfo::MakeN32(width, height,
                           kOpaque_SkAlphaType);
    dst.installPixels(info, buf, width * 4);

    SkCanvas canvas(dst);
    canvas.drawColor(SK_ColorWHITE);

    if (width > bitmap.width())
		x = (width - bitmap.width()) / 2;

    if (height > bitmap.height())
		y = (height - bitmap.height()) / 2;

    canvas.drawBitmap(bitmap, x, y, NULL);
}

static void inputJpgLogo(const char src_path[],void *dst, int w, int h, int color)
{
        int bpp = 1.5;
        uint8_t*dest_data = (uint8_t *)dst;
        char *buf = NULL;
        bool ret = false;
        FILE *fp = NULL;
        size_t file_size = 0;
        uint32_t h_stride, v_stride, width, height;
        uint8_t *base = NULL;
        MpiJpegDecoder decoder;
        MpiJpegDecoder::OutputFrame_t frameOut;

        fp = fopen(src_path, "rb");
        if (NULL == fp) {
                ALOGE("failed to open file %s - %s", src_path, strerror(errno));
                return;
        }
        ALOGD("open file %s", src_path);

        fseek(fp, 0L, SEEK_END);
        file_size = ftell(fp);
        rewind(fp);

        buf = (char*)malloc(file_size);
        if (NULL == *buf) {
                ALOGE("failed to malloc buffer - file %s", src_path);
                fclose(fp);
                return;
        }

        fread(buf, 1, file_size, fp);
        fclose(fp);

        if (color)
            ret = decoder.prepareDecoder(MpiJpegDecoder::OUT_FORMAT_ARGB);
        else
            ret = decoder.prepareDecoder(MpiJpegDecoder::OUT_FORMAT_YUV420SP);
        if (!ret) {
                ALOGE("failed to prepare JPEG decoder");
                goto DECODE_OUT;
        }

        memset(&frameOut, 0, sizeof(frameOut));
        ret = decoder.decodePacket(buf, file_size, &frameOut);
        if (!ret) {
                ALOGE("failed to decode packet");
                goto DECODE_OUT;
        }

        ALOGE("decode packet size = %d, width=%d, h_stride=%d, height=%d\n", frameOut.OutputSize, frameOut.DisplayWidth, frameOut.FrameWidth,
                frameOut.DisplayHeight);
        /* TODO - get diaplay for the frameOut.
        * - frame address: frameOut.MemVirAddr
        * - frame size: frameOut.OutputSize */
        h_stride = frameOut.FrameWidth;
        v_stride = frameOut.FrameHeight;
        width = frameOut.DisplayWidth;
        height = frameOut.DisplayHeight;
        base = frameOut.MemVirAddr;
        if (color)
            bpp = 4;
        else
            bpp = 1.5;
        for (int i = 0; i< height; i++, base += h_stride*bpp) {
                memcpy(dest_data, base, width*bpp);
                dest_data += width*bpp;
        }

        /* Output frame buffers within limits, so release frame buffer if one
        frame has been display successful. */
        decoder.deinitOutputFrame(&frameOut);

DECODE_OUT:
        decoder.flushBuffer();
        if (buf)
                free(buf);
        return ;
}

int hwc_post_epd_logo(const char src_path[]) {
    int *gray16_buffer;
    void *image_addr;
    void *image_new_addr;

    if (ebc_buf_info.color_panel == 1) {
        image_new_addr = (char *)malloc(ebc_buf_info.width * ebc_buf_info.height * 4);
        image_addr = (char *)malloc(ebc_buf_info.width * ebc_buf_info.height * 4);
        drawLogoPic(src_path, (void *)image_new_addr, ebc_buf_info.width, ebc_buf_info.height);
	 image_to_cfa_grayscale(ebc_buf_info.width, ebc_buf_info.height, (unsigned char*)(image_new_addr), (unsigned char*)(image_addr));
	 free(image_new_addr);
	 image_new_addr = NULL;
    }
    else if (ebc_buf_info.color_panel == 2) {
        image_addr = (char *)malloc((ebc_buf_info.width/2) * (ebc_buf_info.height/2) * 4);
        drawLogoPic(src_path, (void *)image_addr, ebc_buf_info.width/2, ebc_buf_info.height/2);
    }
    else {
        image_addr = (char *)malloc(ebc_buf_info.width * ebc_buf_info.height * 4);
        drawLogoPic(src_path, (void *)image_addr, ebc_buf_info.width, ebc_buf_info.height);
    }

    gray16_buffer = (int *)malloc(ebc_buf_info.vir_width * ebc_buf_info.vir_height >> 1);
    int *gray16_buffer_bak = gray16_buffer;
    char isNeedWhiteScreenWithStandby[PROPERTY_VALUE_MAX] = "n";
    /* add white screen before power-off picture, reduce shadow, open by property [ro.need.white.with.standby] */
    property_get("ro.need.white.with.standby", isNeedWhiteScreenWithStandby, "n");
    if (strcmp(isNeedWhiteScreenWithStandby, "y") == 0) {
        memset(gray16_buffer_bak, 0xff, ebc_buf_info.vir_width * ebc_buf_info.vir_height >> 1);
        ALOGD_IF(log_level(DBG_DEBUG), "%s,line = %d", __FUNCTION__, __LINE__);
        //EPD post
        Rect rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
        hwc_post_epd(gray16_buffer_bak, rect, EPD_BLACK_WHITE);
    }

    if (ebc_buf_info.color_panel == 2)
        Rgb888_to_color_eink2((char *)gray16_buffer, (int *)image_addr, ebc_buf_info.height/2, ebc_buf_info.width/2, ebc_buf_info.vir_width);
    else
        neon_rgb888_to_gray16ARM((uint8_t*)gray16_buffer,(uint8_t*)(image_addr), ebc_buf_info.vir_height, ebc_buf_info.vir_width, ebc_buf_info.vir_width);

    //EPD post
    gCurrentEpdMode = EPD_BLOCK;
    Rect rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
    if (gPowerMode == EPD_POWEROFF)
      hwc_post_epd(gray16_buffer, rect, EPD_POWEROFF);
    else
      hwc_post_epd(gray16_buffer, rect, EPD_BLOCK);
    gCurrentEpdMode = EPD_BLOCK;

    free(image_addr);
    image_addr = NULL;
    free(gray16_buffer);
    gray16_buffer = NULL;
    gray16_buffer_bak = NULL;

    return 0;
}

static int unflattenRegion(const struct region_buffer_t &buffer, Region &region) {
    region.clear();
    if (buffer.size)
        if (!buffer.size || buffer.size > sizeof(buffer.buffer))
            return -1;
    if (region.unflatten((void const*)buffer.buffer,
                buffer.size) != NO_ERROR)
        return -1;
    return 0;
}

static int hwc_handle_eink_mode(int mode){

  if(gPowerMode == EPD_POWEROFF || gPowerMode == EPD_STANDBY || gPowerMode == EPD_NOPOWER)
  {
      ALOGD_IF(log_level(DBG_DEBUG),"%s,line=%d gPowerMode = %d,gCurrentEpdMode = %d",__FUNCTION__,__LINE__,gPowerMode,gCurrentEpdMode);
      gCurrentEpdMode = EPD_BLOCK;
      return 0;
  }

  if(gPowerMode == EPD_UNBLOCK){
      gCurrentEpdMode = EPD_UNBLOCK;
      gPowerMode = EPD_NULL;
      return 0;
  }else{
      switch (mode) {
        case HWC_POWER_MODE_EPD_NULL:
          gCurrentEpdMode = EPD_NULL;
          break;
        case HWC_POWER_MODE_EPD_AUTO:
          gCurrentEpdMode = EPD_AUTO;
          gResetEpdMode = EPD_AUTO;
          break;
        case HWC_POWER_MODE_EPD_FULL:
          gCurrentEpdMode = EPD_FULL;
          break;
        case HWC_POWER_MODE_EPD_A2:
          gCurrentEpdMode = EPD_A2;
          break;
        case HWC_POWER_MODE_EPD_PART:
          gCurrentEpdMode = EPD_PART;
          gResetEpdMode = EPD_PART;
          break;
        case HWC_POWER_MODE_EPD_FULL_DITHER:
          gCurrentEpdMode = EPD_FULL_DITHER;
          break;
        case HWC_POWER_MODE_EPD_RESET:
          gCurrentEpdMode = EPD_RESET;
          break;
        case HWC_POWER_MODE_EPD_BLACK_WHITE:
          gCurrentEpdMode = EPD_BLACK_WHITE;
          break;
        case HWC_POWER_MODE_EPD_BG:
          gCurrentEpdMode = EPD_BG;
          break;
        case HWC_POWER_MODE_EPD_BLOCK:
          gCurrentEpdMode = EPD_BLOCK;
          break;
        case HWC_POWER_MODE_EPD_FULL_WIN:
          gCurrentEpdMode = EPD_FULL_WIN;
          break;
        case HWC_POWER_MODE_EPD_OED_PART:
          gCurrentEpdMode = EPD_OED_PART;
          gResetEpdMode = EPD_OED_PART;
          break;
        case HWC_POWER_MODE_EPD_DIRECT_PART:
          gCurrentEpdMode = EPD_DIRECT_PART;
          break;
        case HWC_POWER_MODE_EPD_DIRECT_A2:
          gCurrentEpdMode = EPD_DIRECT_A2;
          break;
        case HWC_POWER_MODE_EPD_STANDBY:
          //gCurrentEpdMode = EPD_STANDBY;
          //hwc_post_epd_logo(STANDBY_IMAGE_PATH);
          break;
        case HWC_POWER_MODE_EPD_POWEROFF:
          //gCurrentEpdMode = EPD_POWEROFF;
          //hwc_post_epd_logo(POWEROFF_IMAGE_PATH);
          break;
        case HWC_POWER_MODE_EPD_NOPOWER:
          //gCurrentEpdMode = EPD_NOPOWER;
          //hwc_post_epd_logo(NOPOWER_IMAGE_PATH);
          break;
        case HWC_POWER_MODE_EPD_AUTO_BG:
          gCurrentEpdMode = EPD_AUTO_BG;
          break;
        case HWC_POWER_MODE_EPD_PART_GL16:
          gCurrentEpdMode = EPD_PART_GL16;
          break;
	 case HWC_POWER_MODE_EPD_PART_GLR16:
          gCurrentEpdMode = EPD_PART_GLR16;
          break;
        case HWC_POWER_MODE_EPD_PART_GLD16:
          gCurrentEpdMode = EPD_PART_GLD16;
          break;
	 case HWC_POWER_MODE_EPD_FULL_GL16:
          gCurrentEpdMode = EPD_FULL_GL16;
          break;
        case HWC_POWER_MODE_EPD_FULL_GLR16:
          gCurrentEpdMode = EPD_FULL_GLR16;
          break;
	 case HWC_POWER_MODE_EPD_FULL_GLD16:
          gCurrentEpdMode = EPD_FULL_GLD16;
          break;
      };
  }
  return 0;
}

static int hwc_set(hwc_composer_device_1_t *dev, size_t num_displays,
                   hwc_display_contents_1_t **sf_display_contents) {
  ATRACE_CALL();
  Mutex::Autolock lock(mEinkModeLock);

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  int ret = 0;
  inc_frame();
  char value[PROPERTY_VALUE_MAX];

  property_get("sys.eink.mode", value, "0");
  int requestEpdMode = atoi(value);
  //Handle eink mode.
  ret = hwc_handle_eink_mode(requestEpdMode);

  if(gCurrentEpdMode != EPD_BLOCK){
    for (size_t i = 0; i < num_displays; ++i) {
        hwc_display_contents_1_t *dc = sf_display_contents[i];

    if (!sf_display_contents[i])
      continue;

      size_t num_dc_layers = dc->numHwLayers;
      for (size_t j = 0; j < num_dc_layers; ++j) {
        hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
        if (sf_layer != NULL && sf_layer->handle != NULL && sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
            ctx->eink_compositor_worker.QueueComposite(dc,gCurrentEpdMode,gResetEpdMode);
        }
      }

    }
  }else{
    ALOGD_IF(log_level(DBG_DEBUG),"%s:line = %d, gCurrentEpdMode = %d,skip this frame = %d",__FUNCTION__,__LINE__,gCurrentEpdMode,get_frame());
    for (size_t i = 0; i < num_displays; ++i) {
            hwc_display_contents_1_t *dc = sf_display_contents[i];
      if (!sf_display_contents[i])
        continue;
      size_t num_dc_layers = dc->numHwLayers;
      for (size_t j = 0; j < num_dc_layers; ++j) {
        hwc_layer_1_t *sf_layer = &dc->hwLayers[j];
        dump_layer(ctx->gralloc, false, sf_layer, j);
        if (sf_layer != NULL && sf_layer->compositionType == HWC_FRAMEBUFFER_TARGET) {
          if(sf_layer->acquireFenceFd > 0){
            sync_wait(sf_layer->acquireFenceFd, -1);
            close(sf_layer->acquireFenceFd);
            sf_layer->acquireFenceFd = -1;
          }
        }
      }
    }
  }
  return 0;
}

static int hwc_event_control(struct hwc_composer_device_1 *dev, int display,
                             int event, int enabled) {
  if (event != HWC_EVENT_VSYNC || (enabled != 0 && enabled != 1))
    return -EINVAL;

  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  if (display == HWC_DISPLAY_PRIMARY)
    return ctx->primary_vsync_worker.VSyncControl(enabled);
  else if (display == HWC_DISPLAY_EXTERNAL)
    return ctx->extend_vsync_worker.VSyncControl(enabled);

  ALOGE("Can't support vsync control for display %d\n", display);
  return -EINVAL;
}

static int hwc_set_power_mode(struct hwc_composer_device_1 *dev, int display,
                              int mode) {
  Mutex::Autolock lock(mEinkModeLock);
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d , display = %d ,mode = %d",__FUNCTION__,__LINE__,display,mode);

  switch (mode) {
    case HWC_POWER_MODE_OFF:
      gPowerMode = EPD_POWEROFF;
      ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d , mode = %d , gPowerMode = %d,gCurrentEpdMode = %d",__FUNCTION__,__LINE__,mode,gPowerMode,gCurrentEpdMode);
      gCurrentEpdMode = EPD_BLOCK;

      char nopower_flag[255];
      property_get("sys.shutdown.nopower",nopower_flag, "0");
      if(atoi(nopower_flag) == 1){
        if (!access(NOPOWER_IMAGE_PATH_USER, R_OK)){
          hwc_post_epd_logo(NOPOWER_IMAGE_PATH_USER);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s exist,use it.",__FUNCTION__,__LINE__,NOPOWER_IMAGE_PATH_USER);
        }else{
          hwc_post_epd_logo(NOPOWER_IMAGE_PATH_DEFAULT);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s not found ,use %s.",__FUNCTION__,__LINE__,NOPOWER_IMAGE_PATH_USER,NOPOWER_IMAGE_PATH_DEFAULT);
        }
      } else {
        if (!access(POWEROFF_IMAGE_PATH_USER, R_OK)){
          hwc_post_epd_logo(POWEROFF_IMAGE_PATH_USER);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s exist,use it.",__FUNCTION__,__LINE__,POWEROFF_IMAGE_PATH_USER);
        }else{
          hwc_post_epd_logo(POWEROFF_IMAGE_PATH_DEFAULT);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s not found ,use %s.",__FUNCTION__,__LINE__,POWEROFF_IMAGE_PATH_USER,POWEROFF_IMAGE_PATH_DEFAULT);
        }
      }
      break;
    /* We can't support dozing right now, so go full on */
    case HWC_POWER_MODE_DOZE:
      gPowerMode = EPD_STANDBY;
      gCurrentEpdMode = EPD_BLOCK;
      ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d , mode = %d , gPowerMode = %d,gCurrentEpdMode = %d",__FUNCTION__,__LINE__,mode,gPowerMode,gCurrentEpdMode);

      char standby_nopower_flag[255];
      char standby_charge_flag[255];
      property_get("sys.standby.nopower",standby_nopower_flag, "0");
      property_get("sys.standby.charge",standby_charge_flag, "0");
      if (atoi(standby_nopower_flag) == 1){
        if (!access(STANDBY_NOPOWER_PATH_USER, R_OK)){
          hwc_post_epd_logo(STANDBY_NOPOWER_PATH_USER);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s exist,use it.",__FUNCTION__,__LINE__,STANDBY_NOPOWER_PATH_USER);
        }else{
          hwc_post_epd_logo(STANDBY_NOPOWER_PATH_DEFAULT);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s not found ,use %s.",__FUNCTION__,__LINE__,STANDBY_NOPOWER_PATH_USER,STANDBY_NOPOWER_PATH_DEFAULT);
        }
      } else if (atoi(standby_charge_flag) == 1){
        if (!access(STANDBY_CHARGE_PATH_USER, R_OK)){
          hwc_post_epd_logo(STANDBY_CHARGE_PATH_USER);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s exist,use it.",__FUNCTION__,__LINE__,STANDBY_CHARGE_PATH_USER);
        }else{
          hwc_post_epd_logo(STANDBY_CHARGE_PATH_DEFAULT);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s not found ,use %s.",__FUNCTION__,__LINE__,STANDBY_CHARGE_PATH_USER,STANDBY_CHARGE_PATH_DEFAULT);
        }
      } else {
        if (!access(STANDBY_IMAGE_PATH_USER, R_OK)){
          hwc_post_epd_logo(STANDBY_IMAGE_PATH_USER);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s exist,use it.",__FUNCTION__,__LINE__,STANDBY_IMAGE_PATH_USER);
        }else{
          hwc_post_epd_logo(STANDBY_IMAGE_PATH_DEFAULT);
          ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d ,%s not found ,use %s.",__FUNCTION__,__LINE__,STANDBY_IMAGE_PATH_USER,STANDBY_IMAGE_PATH_DEFAULT);
        }
      }
      break;
    case HWC_POWER_MODE_DOZE_SUSPEND:
    case HWC_POWER_MODE_NORMAL:
      gPowerMode = EPD_UNBLOCK;
      gCurrentEpdMode = EPD_FULL;
      not_fullmode_count = 50;
      break;
  }
  return 0;
}

static int hwc_query(struct hwc_composer_device_1 * /* dev */, int what,
                     int *value) {
  switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
      *value = 0; /* TODO: We should do this */
      break;
    case HWC_VSYNC_PERIOD:
      ALOGW("Query for deprecated vsync value, returning 60Hz");
      *value = 1000 * 1000 * 1000 / 60;
      break;
    case HWC_DISPLAY_TYPES_SUPPORTED:
      *value = HWC_DISPLAY_PRIMARY_BIT | HWC_DISPLAY_EXTERNAL_BIT |
               HWC_DISPLAY_VIRTUAL_BIT;
      break;
  }
  return 0;
}

static void hwc_register_procs(struct hwc_composer_device_1 *dev,
                               hwc_procs_t const *procs) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  UN_USED(dev);

  ctx->procs = procs;

  ctx->primary_vsync_worker.SetProcs(procs);
  ctx->extend_vsync_worker.SetProcs(procs);
}

static int hwc_get_display_configs(struct hwc_composer_device_1 *dev,
                                   int display, uint32_t *configs,
                                   size_t *num_configs) {
  UN_USED(dev);
  UN_USED(display);
  if (!num_configs)
    return 0;

  uint32_t width = 0, height = 0 , vrefresh = 0 ;
  if (ebc_buf_info.color_panel == 2) {
    width = ebc_buf_info.fb_width/2;// - ((ebc_buf_info.fb_width/2) % 8);
    height = ebc_buf_info.fb_height/2;// - ((ebc_buf_info.fb_height/2) % 2);
  }
  else {
    width = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
  }
  hwc_info.framebuffer_width = width;
  hwc_info.framebuffer_height = height;
  hwc_info.vrefresh = vrefresh ? vrefresh : 10;
  *num_configs = 1;
  for(int i = 0 ; i < static_cast<int>(*num_configs); i++  )
    configs[i] = i;

  return 0;
}

static float getDefaultDensity(uint32_t width, uint32_t height) {
    // Default density is based on TVs: 1080p displays get XHIGH density,
    // lower-resolution displays get TV density. Maybe eventually we'll need
    // to update it for 4K displays, though hopefully those just report
    // accurate DPI information to begin with. This is also used for virtual
    // displays and even primary displays with older hwcomposers, so be
    // careful about orientation.

    uint32_t h = width < height ? width : height;
    if (h >= 1080) return ACONFIGURATION_DENSITY_XHIGH;
    else           return ACONFIGURATION_DENSITY_TV;
}

static int hwc_get_display_attributes(struct hwc_composer_device_1 *dev,
                                      int display, uint32_t config,
                                      const uint32_t *attributes,
                                      int32_t *values) {
  UN_USED(config);
  UN_USED(display);
  UN_USED(dev);

  uint32_t mm_width = ebc_buf_info.width_mm;
  uint32_t mm_height = ebc_buf_info.height_mm;
  int w = hwc_info.framebuffer_width;
  int h = hwc_info.framebuffer_height;
  int vrefresh = hwc_info.vrefresh;

  for (int i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
    switch (attributes[i]) {
      case HWC_DISPLAY_VSYNC_PERIOD:
        values[i] = 1000 * 1000 * 1000 / vrefresh;
        break;
      case HWC_DISPLAY_WIDTH:
        values[i] = w;
        break;
      case HWC_DISPLAY_HEIGHT:
        values[i] = h;
        break;
      case HWC_DISPLAY_DPI_X:
        /* Dots per 1000 inches */
        values[i] = mm_width ? (w * UM_PER_INCH) / mm_width : getDefaultDensity(w,h)*1000;
        break;
      case HWC_DISPLAY_DPI_Y:
        /* Dots per 1000 inches */
        values[i] =
            mm_height ? (h * UM_PER_INCH) / mm_height : getDefaultDensity(w,h)*1000;
        break;
    }
  }
  return 0;
}

static int hwc_get_active_config(struct hwc_composer_device_1 *dev,
                                 int display) {
  UN_USED(dev);
  UN_USED(display);
  ALOGD_IF(log_level(DBG_DEBUG),"DEBUG_lb getActiveConfig mode = %d",gCurrentEpdMode);
  return gCurrentEpdMode;
}

static int hwc_set_active_config(struct hwc_composer_device_1 *dev, int display,
                                 int index) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)&dev->common;
  UN_USED(display);
  ALOGD_IF(log_level(DBG_DEBUG),"%s,line = %d mode = %d",__FUNCTION__,__LINE__,index);
  return 0;
}

static int hwc_device_close(struct hw_device_t *dev) {
  struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
  delete ctx;
  return 0;
}

static int hwc_initialize_display(struct hwc_context_t *ctx, int display) {
    hwc_drm_display_t *hd = &ctx->displays[display];
    hd->ctx = ctx;
    hd->gralloc = ctx->gralloc;
    hd->framebuffer_width = 0;
    hd->framebuffer_height = 0;

#if RK_RGA_PREPARE_ASYNC
    hd->rgaBuffer_index = 0;
    hd->mUseRga = false;
#endif
    return 0;
}

static int hwc_enumerate_displays(struct hwc_context_t *ctx) {
  int ret, num_connectors = 0;
  ret = ctx->eink_compositor_worker.Init(ctx);
  if (ret) {
    ALOGE("Failed to initialize virtual compositor worker");
    return ret;
  }
  ret = hwc_initialize_display(ctx, 0);
  if (ret) {
    ALOGE("Failed to initialize display %d", 0);
    return ret;
  }

  ret = ctx->primary_vsync_worker.Init(HWC_DISPLAY_PRIMARY);
  if (ret) {
    ALOGE("Failed to create event worker for primary display %d\n", ret);
    return ret;
  }

  return 0;
}

static int hwc_device_open(const struct hw_module_t *module, const char *name,
                           struct hw_device_t **dev) {
  if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
    ALOGE("Invalid module name- %s", name);
    return -EINVAL;
  }

  init_rk_debug();

  std::unique_ptr<hwc_context_t> ctx(new hwc_context_t());
  if (!ctx) {
    ALOGE("Failed to allocate hwc context");
    return -ENOMEM;
  }

  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&ctx->gralloc);
  if (ret) {
    ALOGE("Failed to open gralloc module %d", ret);
    return ret;
  }

  ret = hwc_enumerate_displays(ctx.get());
  if (ret) {
    ALOGE("Failed to enumerate displays: %s", strerror(ret));
    return ret;
  }

  ctx->device.common.tag = HARDWARE_DEVICE_TAG;
  ctx->device.common.version = HWC_DEVICE_API_VERSION_1_4;
  ctx->device.common.module = const_cast<hw_module_t *>(module);
  ctx->device.common.close = hwc_device_close;

  ctx->device.dump = hwc_dump;
  ctx->device.prepare = hwc_prepare;
  ctx->device.set = hwc_set;
  ctx->device.eventControl = hwc_event_control;
  ctx->device.setPowerMode = hwc_set_power_mode;
  ctx->device.query = hwc_query;
  ctx->device.registerProcs = hwc_register_procs;
  ctx->device.getDisplayConfigs = hwc_get_display_configs;
  ctx->device.getDisplayAttributes = hwc_get_display_attributes;
  ctx->device.getActiveConfig = hwc_get_active_config;
  ctx->device.setActiveConfig = hwc_set_active_config;
  ctx->device.setCursorPositionAsync = NULL; /* TODO: Add cursor */


  g_ctx = ctx.get();

  ebc_fd = open("/dev/ebc", O_RDWR,0);
  if (ebc_fd < 0){
      ALOGE("open /dev/ebc failed\n");
  }

  if(ioctl(ebc_fd, GET_EBC_BUFFER_INFO,&ebc_buf_info)!=0){
      ALOGE("GET_EBC_BUFFER failed\n");
  }
  ebc_buffer_base = mmap(0, ebc_buf_info.vir_width*ebc_buf_info.vir_height*3, PROT_READ|PROT_WRITE, MAP_SHARED, ebc_fd, 0);
  if (ebc_buffer_base == MAP_FAILED) {
      ALOGE("Error mapping the ebc buffer (%s)\n", strerror(errno));
  }

  hwc_init_version();

  *dev = &ctx->device.common;
  ctx.release();

  return 0;
}
}

static struct hw_module_methods_t hwc_module_methods = {
  .open = android::hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
  .common = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = HWC_HARDWARE_MODULE_ID,
    .name = "DRM hwcomposer module",
    .author = "The Android Open Source Project",
    .methods = &hwc_module_methods,
    .dso = NULL,
    .reserved = {0},
  }
};
