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

#ifndef ANDROID_EINK_COMPOSITOR_WORKER_H_
#define ANDROID_EINK_COMPOSITOR_WORKER_H_

#include "drmhwcomposer.h"
#include "worker.h"
#include "hwc_rockchip.h"
#include "hwc_debug.h"

//open header
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//map header
#include <map>

//gui
#include <ui/Rect.h>
#include <ui/Region.h>
#include <ui/GraphicBufferMapper.h>

//Image jpg decoder
#include "MpiJpegDecoder.h"

//rga
#include <RockchipRga.h>

//Trace
#include <Trace.h>

#include <queue>

namespace android {

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

class EinkCompositorWorker : public Worker {
 public:
  EinkCompositorWorker();
  ~EinkCompositorWorker() override;

  int Init(struct hwc_context_t *ctx);
  void QueueComposite(hwc_display_contents_1_t *dc, Region &A2Region,Region &updateRegion,Region &AutoRegion,int gCurrentEpdMode,int gResetEpdMode);
   void SignalComposite();

 protected:
  void Routine() override;

 private:
  struct EinkComposition {
    UniqueFd outbuf_acquire_fence;
    std::vector<UniqueFd> layer_acquire_fences;
    int release_timeline;
    buffer_handle_t fb_handle = NULL;
    int einkMode;
    Region currentUpdateRegion;
    Region currentA2Region;
    Region currentAutoRegion;
  };

  int CreateNextTimelineFence();
  int FinishComposition(int timeline);
  int Rgba888ToGray256(DrmRgaBuffer &rgaBuffer,const buffer_handle_t          &fb_handle);
  int RgaClipGrayRect(DrmRgaBuffer &rgaBuffer,const buffer_handle_t &fb_handle);
  int PostEink(int *buffer, Rect rect, int mode);
  int SetEinkMode(const buffer_handle_t &fb_handle, Region &A2Region,Region &updateRegion,Region &AutoRegion);
  void Compose(std::unique_ptr<EinkComposition> composition);

  bool isSupportRkRga() {
    RockchipRga& rkRga(RockchipRga::get());
    return rkRga.RkRgaIsReady();
  }

  std::queue<std::unique_ptr<EinkComposition>> composite_queue_;
  int timeline_fd_;
  int timeline_;
  int timeline_current_;

  // mutable since we need to acquire in HaveQueuedComposites
  mutable pthread_mutex_t eink_lock_;

  pthread_cond_t eink_queue_cond_;
  //Eink support
  struct hwc_context_t *hwc_context;

  int ebc_fd = -1;
  void *ebc_buffer_base = NULL;
  struct ebc_buf_info ebc_buf_info;

  int gLastEpdMode = EPD_PART;
  int gCurrentEpdMode = EPD_PART;
  int gResetEpdMode = EPD_PART;
  Region gLastA2Region;
  Region gSavedUpdateRegion;

  int rgaBuffer_index = 0;
  DrmRgaBuffer rgaBuffers[MaxRgaBuffers];
  int *gray16_buffer = NULL;
  char* rga_output_addr = NULL;

};
}

#endif
