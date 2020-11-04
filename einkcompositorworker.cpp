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

#define LOG_TAG "hwc-eink-compositor-worker"

#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include "einkcompositorworker.h"
#include "worker.h"

#include <errno.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <hardware/hardware.h>
#include <hardware/hwcomposer.h>
#include <sched.h>
#include <sw_sync.h>
#include <sync/sync.h>
#include "libcfa/libcfa.h"

namespace android {

static const int kMaxQueueDepth = 1;
static const int kAcquireWaitTimeoutMs = 3000;

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


EinkCompositorWorker::EinkCompositorWorker()
    : Worker("Eink-compositor", HAL_PRIORITY_URGENT_DISPLAY),
      timeline_fd_(-1),
      timeline_(0),
      timeline_current_(0),
      hwc_context_(NULL),
      gralloc_(NULL) {
}

EinkCompositorWorker::~EinkCompositorWorker() {
  if (timeline_fd_ >= 0) {
    FinishComposition(timeline_);
    close(timeline_fd_);
    timeline_fd_ = -1;
  }
  if(ebc_fd > 0){
    close(ebc_fd);
    ebc_fd = -1;
  }
  if(rga_output_addr != NULL){
    //Get virtual address
    const gralloc_module_t *gralloc;
    int ret = 0;
    ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                      (const hw_module_t **)&gralloc);
    if (ret) {
        ALOGE("Failed to open gralloc module");
        return;
    }
    DrmRgaBuffer &gra_buffer = rgaBuffers[0];
    buffer_handle_t src_hnd = gra_buffer.buffer()->handle;
    gralloc->unlock(gralloc, src_hnd);
  }
  if(gray16_buffer != NULL)
    free(gray16_buffer);
  if (gray256_new_buffer != NULL)
    free(gray256_new_buffer);
  if (rgba_new_buffer != NULL)
    free(rgba_new_buffer);
}

int EinkCompositorWorker::Init(struct hwc_context_t *ctx) {
  hwc_context_ = ctx;
  int ret = sw_sync_timeline_create();
  if (ret < 0) {
    ALOGE("Failed to create sw sync timeline %d", ret);
    return ret;
  }
  timeline_fd_ = ret;

  ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                    (const hw_module_t **)&gralloc_);
  if (ret) {
      ALOGE("Failed to open gralloc module");
      return ret;
  }

  pthread_cond_init(&eink_queue_cond_, NULL);


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

  gray16_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height >> 1);
  gray256_new_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height);
  rgba_new_buffer = (int *)malloc(ebc_buf_info.width * ebc_buf_info.height * 4);

  return InitWorker();
}

void EinkCompositorWorker::QueueComposite(hwc_display_contents_1_t *dc, Region &A2Region,Region &updateRegion,Region &AutoRegion,int CurrentEpdMode,int ResetEpdMode) {
  ATRACE_CALL();

  std::unique_ptr<EinkComposition> composition(new EinkComposition);
  gCurrentEpdMode = CurrentEpdMode;
  gResetEpdMode = ResetEpdMode;
  composition->einkMode = -1;
  composition->currentUpdateRegion.clear();
  composition->currentA2Region.clear();
  composition->currentAutoRegion.clear();
  composition->fb_handle = NULL;

  composition->outbuf_acquire_fence.Set(dc->outbufAcquireFenceFd);
  dc->outbufAcquireFenceFd = -1;
  if (dc->retireFenceFd >= 0)
    close(dc->retireFenceFd);
  dc->retireFenceFd = CreateNextTimelineFence();

  for (size_t i = 0; i < dc->numHwLayers; ++i) {
    hwc_layer_1_t *layer = &dc->hwLayers[i];
  if (layer != NULL && layer->handle != NULL && layer->compositionType == HWC_FRAMEBUFFER_TARGET){
    composition->layer_acquire_fences.emplace_back(layer->acquireFenceFd);
    layer->acquireFenceFd = -1;
    if (layer->releaseFenceFd >= 0)
      close(layer->releaseFenceFd);
    layer->releaseFenceFd = CreateNextTimelineFence();
    composition->fb_handle = layer->handle;
    composition->einkMode = CurrentEpdMode;
    composition->currentUpdateRegion.orSelf(updateRegion);
    composition->currentA2Region.orSelf(A2Region);
    composition->currentAutoRegion.orSelf(AutoRegion);

    composition->release_timeline = timeline_;

    Lock();
    int ret = pthread_mutex_lock(&eink_lock_);
    if (ret) {
      ALOGE("Failed to acquire compositor lock %d", ret);
    }

    while (composite_queue_.size() >= kMaxQueueDepth) {
      Unlock();
      pthread_cond_wait(&eink_queue_cond_,&eink_lock_);
      Lock();
    }

    composite_queue_.push(std::move(composition));

    ret = pthread_mutex_unlock(&eink_lock_);
    if (ret)
      ALOGE("Failed to release compositor lock %d", ret);

    SignalLocked();
    Unlock();
    }
  }

}

void EinkCompositorWorker::Routine() {
  ATRACE_CALL();

  ALOGD_IF(log_level(DBG_INFO),"----------------------------EinkCompositorWorker Routine start----------------------------");

  int ret = Lock();
  if (ret) {
    ALOGE("Failed to lock worker, %d", ret);
    return;
  }

  int wait_ret = 0;
  if (composite_queue_.empty()) {
    wait_ret = WaitForSignalOrExitLocked();
  }

  ret = pthread_mutex_lock(&eink_lock_);
  if (ret) {
    ALOGE("Failed to acquire compositor lock %d", ret);
  }

  std::unique_ptr<EinkComposition> composition;
  if (!composite_queue_.empty()) {
    composition = std::move(composite_queue_.front());
    composite_queue_.pop();
    pthread_cond_signal(&eink_queue_cond_);
  }

  ret = pthread_mutex_unlock(&eink_lock_);
  if (ret) {
    ALOGE("Failed to release compositor lock %d", ret);
  }


  ret = Unlock();
  if (ret) {
    ALOGE("Failed to unlock worker, %d", ret);
    return;
  }

  if (wait_ret == -EINTR) {
    return;
  } else if (wait_ret) {
    ALOGE("Failed to wait for signal, %d", wait_ret);
    return;
  }

  Compose(std::move(composition));

  ALOGD_IF(log_level(DBG_INFO),"----------------------------EinkCompositorWorker Routine end----------------------------");
}

int EinkCompositorWorker::CreateNextTimelineFence() {
  ++timeline_;
  char acBuf[50];
  sprintf(acBuf,"eink-frame-%d",get_frame());
  return sw_sync_fence_create(timeline_fd_, acBuf, timeline_);
}

int EinkCompositorWorker::FinishComposition(int point) {
  int timeline_increase = point - timeline_current_;
  if (timeline_increase <= 0)
    return 0;
  int ret = sw_sync_timeline_inc(timeline_fd_, timeline_increase);
  if (ret)
    ALOGE("Failed to increment sync timeline %d", ret);
  else
    timeline_current_ = point;
  return ret;
}

#define CLIP(x) (((x) > 255) ? 255 : (x))

extern void Luma8bit_to_4bit_row_16(int  *src,  int *dst, short int *res0,  short int*res1, int w);



extern int gray256_to_gray16_dither(char *gray256_addr,int *gray16_buffer,int  panel_h, int panel_w,int vir_width);

extern int gray256_to_gray16(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w);

extern int gray256_to_gray2(char *gray256_addr,int *gray16_buffer,int h,int w,int vir_w);

extern void Luma8bit_to_4bit_row_2(short int  *src,  char *dst, short int *res0,  short int*res1, int w,int threshold);


extern int gray256_to_gray2_dither(char *gray256_addr,char *gray2_buffer,int  panel_h, int panel_w,int vir_width,Region region);

extern void Rgb888_to_color_eink(char *dst,int *src,int  fb_height, int fb_width,int vir_width);
extern void Rgb888_to_color_eink2(char *dst,int *src,int  fb_height, int fb_width,int vir_width);

extern void neon_rgb888_to_gray256ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);

extern void rgb888_to_gray16_dither(int *dst,uint8_t *src,int  panel_h, int panel_w,int vir_width);

extern void neon_rgb888_to_gray16ARM(uint8_t * dest,uint8_t *  src,int h,int w,int vir_w);

extern void Luma8bit_to_4bit_dither(int *dst,int *src,int  vir_height, int vir_width,int panel_w);

extern void rgb888_to_gray2_dither(uint8_t *dst, uint8_t *src, int panel_h, int panel_w,
        int vir_width, Region region);

extern void Luma8bit_to_4bit(unsigned int *graynew,unsigned int *gray8bit,int  vir_height, int vir_width,int panel_w);

static inline void apply_white_region(char *buffer, int height, int width, Region region,struct ebc_buf_info *ebc_buf_info_t )
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


int EinkCompositorWorker::Rgba8888ClipRgba(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle) {
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

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(gralloc_,fb_handle);
    src_buf_h = hwc_get_handle_height(gralloc_,fb_handle);
    src_buf_stride = hwc_get_handle_stride(gralloc_,fb_handle);
    src_buf_format = hwc_get_handle_format(gralloc_,fb_handle);
#endif

    src_l = 0;
    src_t = 0;
    dst_l = 0;
    dst_t = 0;
    if (ebc_buf_info.color_panel == 2) {
      src_w = ebc_buf_info.fb_width/2;// - ((ebc_buf_info.fb_width/2) % 8);
      src_h = ebc_buf_info.fb_height/2;// - ((ebc_buf_info.fb_height/2) % 2);
      dst_w = ebc_buf_info.fb_width/2;// - ((ebc_buf_info.fb_width/2) % 8);
      dst_h = ebc_buf_info.fb_height/2;// - ((ebc_buf_info.fb_height/2) % 2);
    }
    else {
      src_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
      src_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
      dst_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
      dst_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
    }

    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_w, dst_h, HAL_PIXEL_FORMAT_RGBA_8888);

    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }

    return ret;
}

int EinkCompositorWorker::Rgba888ToGray256(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle) {
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

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(gralloc_,fb_handle);
    src_buf_h = hwc_get_handle_height(gralloc_,fb_handle);
    src_buf_stride = hwc_get_handle_stride(gralloc_,fb_handle);
    src_buf_format = hwc_get_handle_format(gralloc_,fb_handle);
#endif

    src_l = 0;
    src_t = 0;
    src_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    src_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);


    dst_l = 0;
    dst_t = 0;
    dst_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    dst_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w, dst_h, dst_w, dst_h, HAL_PIXEL_FORMAT_YCrCb_NV12);

    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }
    DumpLayer("yuv", dst.hnd);


    return ret;
}


int EinkCompositorWorker::RgaClipGrayRect(DrmRgaBuffer &rgaBuffer,const buffer_handle_t       &fb_handle) {
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

#if (!RK_PER_MODE && RK_DRM_GRALLOC)
    src_buf_w = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_WIDTH);
    src_buf_h = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_HEIGHT);
    src_buf_stride = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_STRIDE);
    src_buf_format = hwc_get_handle_attibute(gralloc_,fb_handle,ATT_FORMAT);
#else
    src_buf_w = hwc_get_handle_width(gralloc_,fb_handle);
    src_buf_h = hwc_get_handle_height(gralloc_,fb_handle);
    src_buf_stride = hwc_get_handle_stride(gralloc_,fb_handle);
    src_buf_format = hwc_get_handle_format(gralloc_,fb_handle);
#endif

    src_l = 0;
    src_t = 0;
    src_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    src_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);


    dst_l = 0;
    dst_t = 0;
    dst_w = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    dst_h = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);


    if(dst_w < 0 || dst_h <0 )
      ALOGE("RGA invalid dst_w=%d,dst_h=%d",dst_w,dst_h);

    dst_stride = rgaBuffer.buffer()->getStride();

    src.sync_mode = RGA_BLIT_SYNC;
    rga_set_rect(&src.rect,
                src_l, src_t, src_w / 8, src_h,
                src_buf_stride, src_buf_h, src_buf_format);
    rga_set_rect(&dst.rect, dst_l, dst_t,  dst_w / 8, dst_h, dst_w / 8, dst_h, HAL_PIXEL_FORMAT_RGBA_8888);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale  : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
        src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
        dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
    ALOGD_IF(log_level(DBG_INFO),"RK_RGA_PREPARE_SYNC rgaRotateScale : src hnd=%p,dst hnd=%p, format=0x%x, transform=0x%x\n",
        (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle), HAL_PIXEL_FORMAT_RGBA_8888, rga_transform);

    src.hnd = fb_handle;
    dst.hnd = rgaBuffer.buffer()->handle;
    src.rotation = rga_transform;

    RockchipRga& rkRga(RockchipRga::get());
    ret = rkRga.RkRgaBlit(&src, &dst, NULL);
    if(ret) {
        ALOGE("rgaRotateScale error : src[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x],dst[x=%d,y=%d,w=%d,h=%d,ws=%d,hs=%d,format=0x%x]",
            src.rect.xoffset, src.rect.yoffset, src.rect.width, src.rect.height, src.rect.wstride, src.rect.hstride, src.rect.format,
            dst.rect.xoffset, dst.rect.yoffset, dst.rect.width, dst.rect.height, dst.rect.wstride, dst.rect.hstride, dst.rect.format);
        ALOGE("rgaRotateScale error : %s,src hnd=%p,dst hnd=%p",
            strerror(errno), (void*)fb_handle, (void*)(rgaBuffer.buffer()->handle));
    }
    DumpLayer("yuv", dst.hnd);

    return ret;
}

int EinkCompositorWorker::DumpEinkSurface(int *buffer){

  char value[PROPERTY_VALUE_MAX];
  property_get("debug.dump", value, "0");
  int new_value = 0;
  new_value = atoi(value);
  if(new_value > 0){
      char data_name[100] ;
      static int DumpSurfaceCount = 0;

      sprintf(data_name,"/data/dump/dmlayer%d_%d_%d.bin", DumpSurfaceCount,
               ebc_buf_info.vir_width, ebc_buf_info.vir_height);
      DumpSurfaceCount++;
      FILE *file = fopen(data_name, "wb+");
      if (!file)
      {
          ALOGW("Could not open %s\n",data_name);
      } else{
          ALOGW("open %s and write ok\n",data_name);
          fwrite(buffer, ebc_buf_info.vir_height * ebc_buf_info.vir_width >> 1 , 1, file);
          fclose(file);

      }
      if(DumpSurfaceCount > 20){
          property_set("debug.dump","0");
          DumpSurfaceCount = 0;
      }
  }
  return 0;
}
int EinkCompositorWorker::PostEink(int *buffer, Rect rect, int mode){
  ATRACE_CALL();

  DumpEinkSurface(buffer);

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

int EinkCompositorWorker::ConvertToColorEink2(const buffer_handle_t &fb_handle){

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;

  char* framebuffer_base = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  if (ebc_buf_info.color_panel == 1) {
    framebuffer_wdith = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    framebuffer_height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
    output_format = HAL_PIXEL_FORMAT_RGBA_8888;
  } else if (ebc_buf_info.color_panel == 2) {
    framebuffer_wdith = ebc_buf_info.fb_width / 2;
    framebuffer_height = ebc_buf_info.fb_height / 2;
    output_format = HAL_PIXEL_FORMAT_RGBA_8888;
  }

  DumpLayer("rgba", fb_handle);

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_SIZE);

  ret = Rgba8888ClipRgba(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  gralloc_->lock(gralloc_, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&framebuffer_base);

  Rgb888_to_color_eink2((char*)gray16_buffer,(int*)(framebuffer_base),height,width,ebc_buf_info.vir_width);

  if(rga_output_addr != NULL){
    gralloc_->unlock(gralloc_, src_hnd);
    rga_output_addr = NULL;
  }

  return 0;
}
int EinkCompositorWorker::ConvertToColorEink1(const buffer_handle_t &fb_handle){

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;

  char* framebuffer_base = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  if (ebc_buf_info.color_panel == 1) {
    framebuffer_wdith = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
    framebuffer_height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
    output_format = HAL_PIXEL_FORMAT_RGBA_8888;
  } else if (ebc_buf_info.color_panel == 2) {
    framebuffer_wdith = ebc_buf_info.fb_width / 2;
    framebuffer_height = ebc_buf_info.fb_height / 2;
    output_format = HAL_PIXEL_FORMAT_RGBA_8888;
  }

  DumpLayer("rgba", fb_handle);

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_SIZE);

  ret = Rgba8888ClipRgba(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  gralloc_->lock(gralloc_, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&framebuffer_base);

  image_to_cfa_grayscale(ebc_buf_info.fb_width, ebc_buf_info.fb_height, (unsigned char*)(framebuffer_base), (unsigned char*)(rgba_new_buffer));
  neon_rgb888_to_gray16ARM((uint8_t *)gray16_buffer, (uint8_t *)(rgba_new_buffer),ebc_buf_info.fb_height,ebc_buf_info.fb_width,ebc_buf_info.vir_width);

  if(rga_output_addr != NULL){
    gralloc_->unlock(gralloc_, src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ConvertToY8(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
  framebuffer_height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  gralloc_->lock(gralloc_, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);

  if(rga_output_addr != NULL){
    gralloc_->unlock(gralloc_, src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ConvertToY4Dither(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
  framebuffer_height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  gralloc_->lock(gralloc_, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);

  gray256_addr = rga_output_addr;
  gray256_to_gray16_dither(gray256_addr,gray16_buffer,ebc_buf_info.height, ebc_buf_info.width, ebc_buf_info.width);

  if(rga_output_addr != NULL){
    gralloc_->unlock(gralloc_, src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}

int EinkCompositorWorker::ConvertToY1Dither(const buffer_handle_t &fb_handle) {

  DumpLayer("rgba", fb_handle);

  ALOGD_IF(log_level(DBG_DEBUG), "%s", __FUNCTION__);

  char *gray256_addr = NULL;
  int framebuffer_wdith, framebuffer_height, output_format, ret;
  framebuffer_wdith = ebc_buf_info.fb_width - (ebc_buf_info.fb_width % 8);
  framebuffer_height = ebc_buf_info.fb_height - (ebc_buf_info.fb_height % 2);
  output_format = HAL_PIXEL_FORMAT_YCrCb_NV12;

  DrmRgaBuffer &rga_buffer = rgaBuffers[0];
  if (!rga_buffer.Allocate(framebuffer_wdith, framebuffer_height, output_format)) {
    ALOGE("Failed to allocate rga buffer with size %dx%d", framebuffer_wdith, framebuffer_height);
    return -ENOMEM;
  }

  int width,height,stride,byte_stride,format,size;
  buffer_handle_t src_hnd = rga_buffer.buffer()->handle;

  width = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_WIDTH);
  height = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_HEIGHT);
  stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_STRIDE);
  byte_stride = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_BYTE_STRIDE);
  format = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_FORMAT);
  size = hwc_get_handle_attibute(gralloc_,src_hnd,ATT_SIZE);

  ret = Rgba888ToGray256(rga_buffer, fb_handle);
  if (ret) {
    ALOGE("Failed to prepare rga buffer for RGA rotate %d", ret);
    return ret;
  }

  gralloc_->lock(gralloc_, src_hnd, GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK, //gr_handle->usage,
                0, 0, width, height, (void **)&rga_output_addr);

  gray256_addr = rga_output_addr;

  Region screen_region(Rect(0, 0, ebc_buf_info.width - 1, ebc_buf_info.height -1));
  gray256_to_gray2_dither(gray256_addr,(char *)gray16_buffer,ebc_buf_info.vir_height, ebc_buf_info.vir_width, ebc_buf_info.width,screen_region);


  if(rga_output_addr != NULL){
    gralloc_->unlock(gralloc_, src_hnd);
    rga_output_addr = NULL;
  }
  return 0;
}
int EinkCompositorWorker::ColorCommit(int epd_mode) {
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  PostEink(gray16_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = epd_mode;
  return 0;
}

int EinkCompositorWorker::Y4Commit(int epd_mode) {
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  PostEink(gray16_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = epd_mode;
  return 0;
}
int EinkCompositorWorker::A2Commit() {
  int epd_mode = EPD_NULL;
  Rect screen_rect = Rect(0, 0, ebc_buf_info.width, ebc_buf_info.height);
  int *gray16_buffer_bak = gray16_buffer;
  if(gLastEpdMode != EPD_A2)
      epd_mode = EPD_BLACK_WHITE;
  else
      epd_mode = EPD_A2;

  PostEink(gray16_buffer_bak, screen_rect, epd_mode);
  gLastEpdMode = EPD_A2;
  return 0;
}

int EinkCompositorWorker::update_fullmode_num(){
  char value[PROPERTY_VALUE_MAX];
  property_get("persist.vendor.fullmode_cnt",value,"500");

  not_fullmode_num = atoi(value);
  if (not_fullmode_num != curr_not_fullmode_num) {
    if(ioctl(ebc_fd, SET_EBC_NOT_FULL_NUM, &not_fullmode_num) != 0) {
        ALOGE("SET_EBC_NOT_FULL_NUM failed\n");
        return -1;
    }
    curr_not_fullmode_num = not_fullmode_num;
  }
  return 0;
}

int EinkCompositorWorker::SetEinkMode(const buffer_handle_t       &fb_handle) {
  ATRACE_CALL();

  if(!fb_handle){
    ALOGE("%s,line=%d fb_handle is null",__FUNCTION__,__LINE__);
    return -1;
  }

  switch(gCurrentEpdMode){
    case EPD_FULL:
    case EPD_FULL_WIN:
    case EPD_FULL_DITHER:
    case EPD_AUTO:
    case EPD_FULL_GL16:
    case EPD_FULL_GLR16:
    case EPD_FULL_GLD16:
      ConvertToY4Dither(fb_handle);
      Y4Commit(gCurrentEpdMode);
      not_fullmode_count = 0;
      break;
    case EPD_PART:
      ConvertToY4Dither(fb_handle);
      Y4Commit(gCurrentEpdMode);
      not_fullmode_count++;
      break;
    case EPD_A2:
      ConvertToY1Dither(fb_handle);
      A2Commit();
      not_fullmode_count++;
      break;
    case EPD_BLOCK:
       // release_wake_lock("show_advt_lock");
      not_fullmode_count++;
      break;
    case EPD_UNBLOCK:
      ConvertToY4Dither(fb_handle);
      Y4Commit(gCurrentEpdMode);
      not_fullmode_count = 0;
      break;
//    case EPD_PART_EINK:
//      ConvertToY8(fb_handle);
//      EinkCommit();
//    case EPD_COLOR_EINK1:
//      ConvertToColorEink1(fb_handle);
//      ColorCommit(gCurrentEpdMode);
//      not_fullmode_count++;
//      break;
//    case EPD_COLOR_EINK1:
//      ConvertToColorEink2(fb_handle);
//      ColorCommit(gCurrentEpdMode);
//      not_fullmode_count++;
//      break;
    default:
      //LOGE("jeffy part:%d", epdMode);
      not_fullmode_count++;
      break;
  }
  update_fullmode_num();

  return 0;
}


void EinkCompositorWorker::Compose(
    std::unique_ptr<EinkComposition> composition) {
    ATRACE_CALL();
  if (!composition.get())
    return;

  int ret;
  int outbuf_acquire_fence = composition->outbuf_acquire_fence.get();
  if (outbuf_acquire_fence >= 0) {
    ret = sync_wait(outbuf_acquire_fence, kAcquireWaitTimeoutMs);
    if (ret) {
      ALOGE("Failed to wait for outbuf acquire %d/%d", outbuf_acquire_fence,
            ret);
      return;
    }
    composition->outbuf_acquire_fence.Close();
  }
  for (size_t i = 0; i < composition->layer_acquire_fences.size(); ++i) {
    int layer_acquire_fence = composition->layer_acquire_fences[i].get();
    if (layer_acquire_fence >= 0) {
      ret = sync_wait(layer_acquire_fence, kAcquireWaitTimeoutMs);
      if (ret) {
        ALOGE("Failed to wait for layer acquire %d/%d", layer_acquire_fence,
              ret);
        return;
      }
      composition->layer_acquire_fences[i].Close();
    }
  }
  if(isSupportRkRga()){
    ret = SetEinkMode(composition->fb_handle);
    if (ret){
      for(int i = 0; i < MaxRgaBuffers; i++) {
        rgaBuffers[i].Clear();
      }
      return;
    }
  }
  FinishComposition(composition->release_timeline);
}
}
