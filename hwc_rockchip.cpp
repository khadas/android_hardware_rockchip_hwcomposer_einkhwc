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

#define LOG_TAG "hwc_rk"

// #define ENABLE_DEBUG_LOG
#include <log/custom_log.h>

#include <inttypes.h>
#ifdef TARGET_BOARD_PLATFORM_RK3368
#include <hardware/img_gralloc_public.h>
#endif
#include "hwc_rockchip.h"
#include "hwc_util.h"

namespace android {

int hwc_init_version()
{
    char acVersion[50];
    char acCommit[50];
    memset(acVersion,0,sizeof(acVersion));

    strcpy(acVersion,GHWC_VERSION);

#ifdef TARGET_BOARD_PLATFORM_RK3288
    strcat(acVersion,"-rk3288");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3368
    strcat(acVersion,"-rk3368");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3366
    strcat(acVersion,"-rk3366");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3399
    strcat(acVersion,"-rk3399");
#endif
#ifdef TARGET_BOARD_PLATFORM_RK3326
    strcat(acVersion,"-rk3326");
#endif


#ifdef TARGET_BOARD_PLATFORM_RK3126C
    strcat(acVersion,"-rk3126c");
#endif

#ifdef TARGET_BOARD_PLATFORM_RK3328
    strcat(acVersion,"-rk3328");
#endif

#ifdef RK_MID
    strcat(acVersion,"-MID");
#endif
#ifdef RK_BOX
    strcat(acVersion,"-BOX");
#endif
#ifdef RK_PHONE
    strcat(acVersion,"-PHONE");
#endif
#ifdef RK_VIR
    strcat(acVersion,"-VR");
#endif

    /* RK_GRAPHICS_VER=commit-id:067e5d0: only keep string after '=' */
    sscanf(RK_GRAPHICS_VER, "%*[^=]=%s", acCommit);

    property_set("sys.ghwc.version", acVersion);
    property_set("sys.ghwc.commit", acCommit);
    ALOGD(RK_GRAPHICS_VER);
    return 0;
}


#ifdef USE_HWC2
int hwc_get_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return rk_ashmem.displayStereo;
}

int hwc_set_handle_displayStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t displayStereo)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    if(displayStereo != rk_ashmem.displayStereo)
    {
        op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;
        rk_ashmem.displayStereo = displayStereo;

        if(gralloc && gralloc->perform)
            ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
        else
            ret = -EINVAL;

        if(ret != 0)
        {
            ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        }
    }

exit:
    return ret;
}

int hwc_get_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return rk_ashmem.alreadyStereo;
}

int hwc_set_handle_alreadyStereo(const gralloc_module_t *gralloc, buffer_handle_t hnd, int32_t alreadyStereo)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    if(alreadyStereo != rk_ashmem.alreadyStereo )
    {
        op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;
        rk_ashmem.alreadyStereo = alreadyStereo;

        if(gralloc && gralloc->perform)
            ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
        else
            ret = -EINVAL;

        if(ret != 0)
        {
            ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        }
    }

exit:
    return ret;
}

int hwc_get_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, char* layername, unsigned long len)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;
    unsigned long str_size;

    if(!layername)
        return -EINVAL;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    str_size = strlen(rk_ashmem.LayerName)+1;
    str_size = str_size > len ? len:str_size;
    memcpy(layername,rk_ashmem.LayerName,str_size);

exit:
    return ret;
}

int hwc_set_handle_layername(const gralloc_module_t *gralloc, buffer_handle_t hnd, const char* layername)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_t rk_ashmem;
    unsigned long str_size;

    if(!layername)
        return -EINVAL;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }

    op = GRALLOC_MODULE_PERFORM_SET_RK_ASHMEM;

    str_size = strlen(layername)+1;
    str_size = str_size > sizeof(rk_ashmem.LayerName) ? sizeof(rk_ashmem.LayerName):str_size;
    memcpy(rk_ashmem.LayerName,layername,str_size);

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

exit:
    return ret;
}
#endif

int hwc_get_handle_EinkInfo(const gralloc_module_t *gralloc,buffer_handle_t hnd, const struct rk_ashmem_eink_t *rk_ashmem_eink)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_RK_ASHMEM;
    struct rk_ashmem_eink_t rk_ashmem;

    if(!rk_ashmem_eink)
        return -EINVAL;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &rk_ashmem);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
        goto exit;
    }
    memcpy((void*)rk_ashmem_eink,(void*)(&(rk_ashmem)),sizeof(struct rk_ashmem_eink_t));


exit:
    return ret;
}


int hwc_get_handle_width(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->width;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_WIDTH;
    int width = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &width);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return width;
#endif
}

int hwc_get_handle_height(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->height;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_HEIGHT;
    int height = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &height);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return height;
#endif
}

int hwc_get_handle_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->pixel_stride;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_STRIDE;
    int stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return stride;
#endif
}

int hwc_get_handle_byte_stride(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->stride;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_BYTE_STRIDE;
    int byte_stride = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &byte_stride);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return byte_stride;
#endif
}

int hwc_get_handle_format(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->format;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_FORMAT;
    int format = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &format);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return format;
#endif
}

int hwc_get_handle_usage(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->usage;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_USAGE;
    int usage = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &usage);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return usage;
#endif
}

int hwc_get_handle_size(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->size;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_SIZE;
    int size = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &size);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return size;
#endif
}

/*
@func hwc_get_handle_attributes:get attributes from handle.Before call this api,As far as now,
    we need register the buffer first.May be the register is good for processer I think

@param hnd:
@param attrs: if size of attrs is small than 5,it will return EINVAL else
    width  = attrs[0]
    height = attrs[1]
    stride = attrs[2]
    format = attrs[3]
    size   = attrs[4]
*/
int hwc_get_handle_attributes(const gralloc_module_t *gralloc, buffer_handle_t hnd, std::vector<int> *attrs)
{
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_ATTRIBUTES;

    if (!hnd)
        return -EINVAL;

    if(gralloc && gralloc->perform)
    {
        ret = gralloc->perform(gralloc, op, hnd, attrs);
    }
    else
    {
        ret = -EINVAL;
    }


    if(ret) {
       ALOGE("hwc_get_handle_attributes fail %d for:%s hnd=%p",ret,strerror(ret),hnd);
    }

    return ret;
}

int hwc_get_handle_attibute(const gralloc_module_t *gralloc, buffer_handle_t hnd, attribute_flag_t flag)
{
    std::vector<int> attrs;
    int ret=0;

    if(!hnd)
    {
        ALOGE("%s handle is null",__FUNCTION__);
        return -1;
    }

    ret = hwc_get_handle_attributes(gralloc, hnd, &attrs);
    if(ret < 0)
    {
        ALOGE("getHandleAttributes fail %d for:%s",ret,strerror(ret));
        return ret;
    }
    else
    {
        return attrs.at(flag);
    }
}

/*
@func getHandlePrimeFd:get prime_fd  from handle.Before call this api,As far as now, we
    need register the buffer first.May be the register is good for processer I think

@param hnd:
@return fd: prime_fd. and driver can call the dma_buf_get to get the buffer

*/
int hwc_get_handle_primefd(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->prime_fd;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PRIME_FD;
    int fd = -1;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &fd);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return fd;
#endif
}

#if RK_DRM_GRALLOC
uint32_t hwc_get_handle_phy_addr(const gralloc_module_t *gralloc, buffer_handle_t hnd)
{
#if RK_PER_MODE
    struct gralloc_drm_handle_t* drm_hnd = (struct gralloc_drm_handle_t *)hnd;

    UN_USED(gralloc);
    return drm_hnd->phy_addr;
#else
    int ret = 0;
    int op = GRALLOC_MODULE_PERFORM_GET_HADNLE_PHY_ADDR;
    uint32_t phy_addr = 0;

    if(gralloc && gralloc->perform)
        ret = gralloc->perform(gralloc, op, hnd, &phy_addr);
    else
        ret = -EINVAL;

    if(ret != 0)
    {
        ALOGE("%s:cann't get value from gralloc", __FUNCTION__);
    }

    return phy_addr;
#endif
}
#endif

}

