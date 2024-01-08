/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "vglite_support.h"
#include "FreeRTOS.h"
#include "task.h"
#include "vg_lite_platform.h"
#include "vglite_window.h"
#include "fsl_dc_fb_lcdifv2.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

static const int s_layerWidth[APP_HW_LAYERS_COUNT] = {
    L1_WIDTH,
    L2_WIDTH,
    L3_WIDTH,
    L4_WIDTH,
    L5_WIDTH,
    L6_WIDTH,
    L7_WIDTH,
};

AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer1[APP_BUFFER_COUNT][L1_HEIGHT][L1_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer2[APP_BUFFER_COUNT][L2_HEIGHT][L2_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer3[APP_BUFFER_COUNT][L3_HEIGHT][L3_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer4[APP_BUFFER_COUNT][L4_HEIGHT][L4_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer5[APP_BUFFER_COUNT][L5_HEIGHT][L5_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer6[APP_BUFFER_COUNT][L6_HEIGHT][L6_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);
AT_NONCACHEABLE_SECTION_ALIGN( static uint8_t s_frameBuffer7[APP_BUFFER_COUNT][L7_HEIGHT][L7_WIDTH][DEMO_BUFFER_BYTE_PER_PIXEL], FRAME_BUFFER_ALIGN);

uint8_t* s_frameBuffer[APP_HW_LAYERS_COUNT][APP_BUFFER_COUNT] = {
    {&s_frameBuffer1[0][0][0][0], &s_frameBuffer1[1][0][0][0]},
    {&s_frameBuffer2[0][0][0][0], &s_frameBuffer2[1][0][0][0]},
    {&s_frameBuffer3[0][0][0][0], &s_frameBuffer3[1][0][0][0]},
    {&s_frameBuffer4[0][0][0][0], &s_frameBuffer4[1][0][0][0]},
    {&s_frameBuffer5[0][0][0][0], &s_frameBuffer5[1][0][0][0]},
    {&s_frameBuffer6[0][0][0][0], &s_frameBuffer6[1][0][0][0]},
    {&s_frameBuffer7[0][0][0][0], &s_frameBuffer7[1][0][0][0]}
 };

vg_lite_display_t g_display[8];
vg_lite_window_t g_window[8];

/*******************************************************************************
 * Code
 ******************************************************************************/
static video_pixel_format_t vglite_to_video_format(vg_lite_buffer_format_t format)
{
    switch (format)
    {
    case VG_LITE_BGR565:
        return kVIDEO_PixelFormatRGB565;
    case VG_LITE_RGB565 :
        return kVIDEO_PixelFormatBGR565;
    case VG_LITE_BGRX8888:
    case VG_LITE_BGRA8888:
        return kVIDEO_PixelFormatXRGB8888;
    case VG_LITE_RGBX8888:
    case VG_LITE_RGBA8888:
        return kVIDEO_PixelFormatXBGR8888;
    default:
        break;
    }

    return kVIDEO_PixelFormatRGB565;
}

static uint16_t get_stride_bytes(uint16_t width, video_pixel_format_t format)
{
    switch(format)
    {
    case kVIDEO_PixelFormatXRGB8888:
    case kVIDEO_PixelFormatXBGR8888:
    case kVIDEO_PixelFormatRGBX8888:
    case kVIDEO_PixelFormatBGRX8888:
        return width * 4;
    case kVIDEO_PixelFormatRGB888:
    case kVIDEO_PixelFormatBGR888:
        return width * 3;
    case kVIDEO_PixelFormatLUT8:
        return width;
    }
    return width * 2;
}

vg_lite_window_t* VGLITE_CreateWindow(uint32_t displayId, vg_lite_rectangle_t* dimensions, vg_lite_buffer_format_t format)
{
    vg_lite_display_t* display = &g_display[displayId];
    vg_lite_window_t* window = &g_window[displayId];

    FBDEV_Open(&display->g_fbdev, &g_dc, displayId);
    status_t status;
    void *buffer;
    vg_lite_buffer_t *vg_buffer;
    fbdev_t *g_fbdev          = &(display->g_fbdev);
    fbdev_fb_info_t *g_fbInfo = &(display->g_fbInfo);

    window->bufferCount = APP_BUFFER_COUNT;
    window->display     = display;
    window->width       = dimensions->width;
    window->height      = dimensions->height;
    window->current     = -1;
    FBDEV_GetFrameBufferInfo(g_fbdev, g_fbInfo);

    g_fbInfo->bufInfo.pixelFormat = vglite_to_video_format(format);
    g_fbInfo->bufInfo.startX      = dimensions->x;
    g_fbInfo->bufInfo.startY      = dimensions->y;
    g_fbInfo->bufInfo.width       = window->width;
    g_fbInfo->bufInfo.height      = window->height;
    g_fbInfo->bufInfo.strideBytes = get_stride_bytes(s_layerWidth[displayId], g_fbInfo->bufInfo.pixelFormat);

    g_fbInfo->bufferCount = window->bufferCount;
    for (uint8_t i = 0; i < window->bufferCount; i++)
    {
        vg_buffer            = &(window->buffers[i]);
        g_fbInfo->buffers[i] = (void *)s_frameBuffer[displayId][i];
        vg_buffer->memory    = g_fbInfo->buffers[i];
        vg_buffer->address   = (uint32_t)g_fbInfo->buffers[i];
        vg_buffer->width     = g_fbInfo->bufInfo.width;
        vg_buffer->height    = g_fbInfo->bufInfo.height;
        vg_buffer->stride    = g_fbInfo->bufInfo.strideBytes;
        vg_buffer->format    = format;
    }

    status = FBDEV_SetFrameBufferInfo(g_fbdev, g_fbInfo);
    if (status != kStatus_Success)
    {
        while (1)
            ;
    }

    buffer = FBDEV_GetFrameBuffer(g_fbdev, 0);

    assert(buffer != NULL);

    memset(buffer, 0, g_fbInfo->bufInfo.height * g_fbInfo->bufInfo.strideBytes);

    FBDEV_SetFrameBuffer(g_fbdev, buffer, 0);

    // LCDIFV2_SetLayerBlendConfig writes to shadow register (i.e. it has no immediate effect)
    // FBDEV_Enable() will flush the blend config to the hardware
    dc_fb_lcdifv2_handle_t *dcHandle = g_dc.prvData;
    lcdifv2_blend_config_t blendConfig = { .alphaMode = kLCDIFV2_AlphaEmbedded };
    LCDIFV2_SetLayerBlendConfig(dcHandle->lcdifv2, displayId, &blendConfig);     // TODO: feels wrong to call it directly - should be probably part of FBDEV

    FBDEV_Enable(g_fbdev);

    return window;
}

void VGLITE_DestroyWindow(vg_lite_window_t* window)
{
}

vg_lite_buffer_t *VGLITE_GetRenderTarget(vg_lite_window_t *window)
{
    vg_lite_buffer_t *rt = NULL;
    void *memory         = FBDEV_GetFrameBuffer(&window->display->g_fbdev, 0);
    for (uint8_t i = 0; i < window->bufferCount; i++)
    {
        rt = &(window->buffers[i]);
        if (memory == rt->memory)
        {
            window->current = i;
            return rt;
        }
    }
    return NULL;
}

void VGLITE_SwapBuffers(vg_lite_window_t *window)
{
    vg_lite_buffer_t *rt;
    if (window->current >= 0 && window->current < window->bufferCount)
        rt = &(window->buffers[window->current]);
    else
        return;

    vg_lite_finish();

    FBDEV_SetFrameBuffer(&window->display->g_fbdev, rt->memory, 0);
}
