/*
 * Copyright 2019, 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "board.h"

#include "vglite_support.h"
#include "vglite_window.h"
/*-----------------------------------------------------------*/
#include "vg_lite.h"

#include "fsl_soc_src.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define DEFAULT_SIZE 256.0f;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void vglite_task(void *pvParameters);

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*
            *-----*
           /       \
          /         \
         *           *
         |          /
         |         X
         |          \
         *           *
          \         /
           \       /
            *-----*
 */
static int8_t path_data[] = {
    2, -5,  -10, // moveto   -5,-10
    4, 5,   -10, // lineto    5,-10
    4, 10,  -5,  // lineto   10, -5
    4, 0,   0,   // lineto    0,  0
    4, 10,  5,   // lineto   10,  5
    4, 5,   10,  // lineto    5, 10
    4, -5,  10,  // lineto   -5, 10
    4, -10, 5,   // lineto  -10,  5
    4, -10, -5,  // lineto  -10, -5
    0,           // end
};

static vg_lite_path_t path = {{-10, -10,         // left,top
                               10, 10},          // right,bottom
                              VG_LITE_HIGH,      // quality
                              VG_LITE_S8,        // -128 to 127 coordinate range
                              {0},               // uploaded
                              sizeof(path_data), // path length
                              path_data,         // path data
                              1};

static vg_lite_matrix_t matrix;

/*******************************************************************************
 * Code
 ******************************************************************************/
static void BOARD_ResetDisplayMix(void)
{
    /*
     * Reset the displaymix, otherwise during debugging, the
     * debugger may not reset the display, then the behavior
     * is not right.
     */
    SRC_AssertSliceSoftwareReset(SRC, kSRC_DisplaySlice);
    while (kSRC_SliceResetInProcess == SRC_GetSliceResetState(SRC, kSRC_DisplaySlice))
    {
    }
}


int main(void)
{
    /* Init board hardware. */
    BOARD_ConfigMPU();
    BOARD_BootClockRUN();
    BOARD_ResetDisplayMix();
    BOARD_InitLpuartPins();
    BOARD_InitMipiPanelPins();
    BOARD_InitDebugConsole();

    if (xTaskCreate(vglite_task, "vglite_task", configMINIMAL_STACK_SIZE + 200, NULL, configMAX_PRIORITIES - 1, NULL) !=
        pdPASS)
    {
        PRINTF("Task creation failed!.\r\n");
        while (1)
            ;
    }

    vTaskStartScheduler();
    for (;;)
        ;
}

static void cleanup(void)
{
    vg_lite_clear_path(&path);
    vg_lite_close();
}

static vg_lite_error_t init_vg_lite(void)
{
    // Initialize the draw.
    vg_lite_error_t error = vg_lite_init(DEFAULT_VG_LITE_TW_WIDTH, DEFAULT_VG_LITE_TW_HEIGHT);
    if (error)
    {
        PRINTF("vg_lite engine init failed: vg_lite_init() returned error %d\n", error);
        cleanup();
        return error;
    }
    // Set GPU command buffer size for this drawing task.
    error = vg_lite_set_command_buffer_size(VG_LITE_COMMAND_BUFFER_SIZE);
    if (error)
    {
        PRINTF("vg_lite_set_command_buffer_size() returned error %d\n", error);
        cleanup();
        return error;
    }

    return error;
}

static void redraw(vg_lite_window_t* window)
{
    vg_lite_error_t error = VG_LITE_SUCCESS;

    vg_lite_buffer_t *rt = VGLITE_GetRenderTarget(window);
    if (rt == NULL)
    {
        PRINTF("vg_lite_get_renderTarget error\r\n");
        while (1)
            ;
    }
    vg_lite_identity(&matrix);
    vg_lite_translate(window->width / 2.0f, window->height / 2.0f, &matrix);
    vg_lite_scale(10, 10, &matrix);

    vg_lite_clear(rt, NULL, 0xFFFF0000);
    error = vg_lite_draw(rt, &path, VG_LITE_FILL_EVEN_ODD, &matrix, VG_LITE_BLEND_NONE, 0xFF0000FF);
    if (error)
    {
        PRINTF("vg_lite_draw() returned error %d\n", error);
        cleanup();
        return;
    }
    VGLITE_SwapBuffers(window);

    return;
}

uint32_t getTime()
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void vglite_task(void *pvParameters)
{
    status_t status;
    vg_lite_error_t error;

    status = BOARD_PrepareVGLiteController();
    if (status != kStatus_Success)
    {
        PRINTF("Prepare VGlite controller error\r\n");
        while (1)
            ;
    }
    status = BOARD_PrepareDisplayController();
    if (status != kStatus_Success)
        {
            PRINTF("Prepare Display controller error\r\n");
            while (1)
                ;
        }

    error = init_vg_lite();
    if (error)
    {
        PRINTF("init_vg_lite failed: init_vg_lite() returned error %d\r\n", error);
        while (1)
            ;
    }

    // Initialize the window.
    vg_lite_rectangle_t area0 = {0, 0, 720, 1280 };
    vg_lite_window_t* window = VGLITE_CreateWindow(0, &area0, VG_LITE_BGRX8888);
    if (window == NULL)
    {
        PRINTF("VGLITE_CreateWindow failed: VGLITE_CreateWindow() returned nullptr\n");
        while (1)
            ;
    }

    uint32_t startTime, time, n = 0;
    startTime = getTime();

    while (1)
    {
        redraw(window);
        if (n++ >= 59)
        {
            time = getTime() - startTime;
            PRINTF("%d frames in %d seconds: %d fps\r\n", n, time / 1000, n * 1000 / time);
            n         = 0;
            startTime = getTime();
        }
    }
}
