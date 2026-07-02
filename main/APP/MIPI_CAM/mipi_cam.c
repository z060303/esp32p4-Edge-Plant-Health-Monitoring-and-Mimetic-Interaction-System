/**
 ****************************************************************************************************
 * @file        mipi_cam.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       mipicamera驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-P4 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "mipi_cam.h"
#include "mipi_lcd.h"
#include "plant_engine.h"
#include "plant_display.h"
#include "mbedtls/base64.h"
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static const char *mipi_cam_tag = "mipi_cam";

#define FPS_FUNC_ON   1         /* 帧率显示开关 */
#define ALIGN_UP_BY(num, align) (((num) + ((align) - 1)) & ~((align) - 1))  /* 对齐操作 */
#define PHOTO_EXPORT_FRAME 0
#define PHOTO_EXPORT_WIDTH 320
#define PHOTO_EXPORT_HEIGHT 240
#define PHOTO_PREVIEW_WIDTH 160
#define PHOTO_PREVIEW_HEIGHT 120
#define PHOTO_PREVIEW_INTERVAL_FRAMES 30
#define PHOTO_RAW_CHUNK_SIZE 768

static void lcd_cam_task(void *arg);
static void headless_cam_task(void *arg);
typedef enum {
    PHOTO_COMMAND_NONE = 0,
    PHOTO_COMMAND_SNAP,
    PHOTO_COMMAND_PREVIEW,
} photo_command_t;

static photo_command_t consume_photo_command(void);
static bool command_equals_ci(const char *left, const char *right);
static void export_rgb565_photo(const uint8_t *src,
                                uint32_t src_width,
                                uint32_t src_height,
                                uint32_t src_bytes,
                                int frame_count,
                                uint32_t export_width,
                                uint32_t export_height);

/**
 * @brief       mipi_cam初始化
 * @param       无
 * @retval      ESP_OK:开启成功; ESP_FAIL:开启失败
 */
esp_err_t mipi_cam_init(void)
{
    esp_err_t ret = ESP_OK;
    mipi_dev_bsp_enable_dsi_phy_power();    /* 配置MIPI设备电源 */

    ret = app_video_main(bus_handle);       /* 初始化摄像头硬件和软件接口 */
    if (ret != ESP_OK)
    {
        ESP_LOGE(mipi_cam_tag, "video main init failed with error 0x%x", ret);
        return ESP_FAIL;
    }

    int video_cam_fd0 = app_video_open(0);  /* 打开摄像头设备 */
    if (video_cam_fd0 < 0)
    {
        ESP_LOGE(mipi_cam_tag, "video cam open failed");
        return ESP_FAIL;
    }

    ret = app_video_init(video_cam_fd0, APP_VIDEO_FMT_RGB565);  /* 初始化设备并设置捕获视频格式 */
    if (ret != ESP_OK)
    {
        ESP_LOGE(mipi_cam_tag, "Video cam init failed with error 0x%x", ret);
        return ESP_FAIL;
    }

    ESP_LOGI(mipi_cam_tag, "OV5645 CSI init succeeded, starting headless frame capture");
    xTaskCreatePinnedToCore(headless_cam_task, "headless cam capture", 8192, (void *)(intptr_t)video_cam_fd0, 4, NULL, 0);

    return ESP_OK;
}

static void headless_cam_task(void *arg)
{
    int video_fd = (int)(intptr_t)arg;
    struct v4l2_buffer buf;
    struct v4l2_format format = {0};
    void *camera_outbuf[2] = {0};
    int frame_count = 0;
    bool auto_export_done = false;
    photo_command_t pending_command = PHOTO_COMMAND_NONE;

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    ESP_ERROR_CHECK(camera_set_bufs(video_fd, 2, NULL));
    ESP_ERROR_CHECK(camera_get_bufs(2, &camera_outbuf[0]));
    ESP_ERROR_CHECK(camera_stream_start(video_fd));

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_G_FMT, &format) != 0)
    {
        ESP_LOGE(mipi_cam_tag, "get fmt failed");
        return;
    }

    if (PHOTO_EXPORT_FRAME > 0)
    {
        ESP_LOGI(mipi_cam_tag,
                 "photo export ready; send SNAP or PREVIEW over serial, or wait for automatic frame %d export",
                 PHOTO_EXPORT_FRAME);
    }
    else
    {
        ESP_LOGI(mipi_cam_tag, "photo export ready; send SNAP or PREVIEW over serial");
    }

    while (1)
    {
        photo_command_t command = consume_photo_command();
        if (command != PHOTO_COMMAND_NONE)
        {
            pending_command = command;
            ESP_LOGI(mipi_cam_tag,
                     "%s command received; exporting next frame",
                     command == PHOTO_COMMAND_PREVIEW ? "PREVIEW" : "SNAP");
        }

        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(video_fd, VIDIOC_DQBUF, &buf) != 0)
        {
            ESP_LOGE(mipi_cam_tag, "failed to receive video frame");
            continue;
        }

        frame_count++;
        if (frame_count <= 5 || frame_count % 30 == 0)
        {
            ESP_LOGI(mipi_cam_tag, "frame=%d index=%" PRIu32 " bytes=%" PRIu32,
                     frame_count, buf.index, buf.bytesused);
        }

        bool should_auto_snap = PHOTO_EXPORT_FRAME > 0 && !auto_export_done && frame_count >= PHOTO_EXPORT_FRAME;
        bool should_auto_preview = false;

        if (pending_command != PHOTO_COMMAND_NONE || should_auto_snap || should_auto_preview)
        {
            uint32_t export_width = PHOTO_EXPORT_WIDTH;
            uint32_t export_height = PHOTO_EXPORT_HEIGHT;
            if (pending_command == PHOTO_COMMAND_PREVIEW || should_auto_preview)
            {
                export_width = PHOTO_PREVIEW_WIDTH;
                export_height = PHOTO_PREVIEW_HEIGHT;
            }

            export_rgb565_photo((const uint8_t *)camera_outbuf[buf.index],
                                format.fmt.pix.width,
                                format.fmt.pix.height,
                                buf.bytesused,
                                frame_count,
                                export_width,
                                export_height);
            pending_command = PHOTO_COMMAND_NONE;
            if (should_auto_snap)
            {
                auto_export_done = true;
            }
        }

        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0)
        {
            ESP_LOGE(mipi_cam_tag, "failed to free video frame");
        }
    }
}

static photo_command_t consume_photo_command(void)
{
    static char command[128];
    static size_t command_len = 0;
    uint8_t ch;
    photo_command_t received_command = PHOTO_COMMAND_NONE;

    while (read(STDIN_FILENO, &ch, 1) == 1)
    {
        if (ch == '\r' || ch == '\n')
        {
            command[command_len] = '\0';
            if (command_equals_ci(command, "SNAP"))
            {
                received_command = PHOTO_COMMAND_SNAP;
            }
            else if (command_equals_ci(command, "PREVIEW"))
            {
                received_command = PHOTO_COMMAND_PREVIEW;
            }
            else
            {
                if (!plant_display_handle_result_command(command) &&
                    !plant_engine_handle_command(command))
                {
                    ESP_LOGW(mipi_cam_tag, "unknown serial command: %s", command);
                }
            }
            command_len = 0;
            continue;
        }

        if (command_len < sizeof(command) - 1)
        {
            command[command_len++] = (char)ch;
        }
        else
        {
            command_len = 0;
        }
    }

    return received_command;
}

static bool command_equals_ci(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0')
    {
        if (toupper((unsigned char)*left) != toupper((unsigned char)*right))
        {
            return false;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static void export_rgb565_photo(const uint8_t *src,
                                uint32_t src_width,
                                uint32_t src_height,
                                uint32_t src_bytes,
                                int frame_count,
                                uint32_t export_width,
                                uint32_t export_height)
{
    uint8_t raw_chunk[PHOTO_RAW_CHUNK_SIZE];
    uint8_t encoded_chunk[((PHOTO_RAW_CHUNK_SIZE + 2) / 3) * 4 + 1];
    size_t raw_len = 0;
    size_t encoded_len = 0;
    const uint32_t exported_bytes = export_width * export_height * 2;

    printf("\nPHOTO_BEGIN format=RGB565 width=%d height=%d bytes=%" PRIu32
           " source_width=%" PRIu32 " source_height=%" PRIu32 " frame=%d\n",
           (int)export_width,
           (int)export_height,
           exported_bytes,
           src_width,
           src_height,
           frame_count);

    for (uint32_t y = 0; y < export_height; y++)
    {
        const uint32_t src_y = y * src_height / export_height;
        for (uint32_t x = 0; x < export_width; x++)
        {
            const uint32_t src_x = x * src_width / export_width;
            const uint32_t src_offset = (src_y * src_width + src_x) * 2;
            uint8_t lo = 0;
            uint8_t hi = 0;

            if (src_offset + 1 < src_bytes)
            {
                lo = src[src_offset];
                hi = src[src_offset + 1];
            }

            raw_chunk[raw_len++] = lo;
            raw_chunk[raw_len++] = hi;

            if (raw_len == sizeof(raw_chunk))
            {
                if (mbedtls_base64_encode(encoded_chunk, sizeof(encoded_chunk), &encoded_len, raw_chunk, raw_len) == 0)
                {
                    fwrite(encoded_chunk, 1, encoded_len, stdout);
                    putchar('\n');
                }
                raw_len = 0;
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
    }

    if (raw_len > 0 &&
        mbedtls_base64_encode(encoded_chunk, sizeof(encoded_chunk), &encoded_len, raw_chunk, raw_len) == 0)
    {
        fwrite(encoded_chunk, 1, encoded_len, stdout);
        putchar('\n');
    }

    printf("PHOTO_END\n");
    fflush(stdout);
    ESP_LOGI(mipi_cam_tag, "photo export complete: %" PRIu32 " bytes RGB565 -> serial base64", exported_bytes);
}

/**
 * @brief       摄像头显示任务
 * @param       arg: 传入参数(未用到)
 * @retval      无
 */
static void lcd_cam_task(void *arg)
{
    int video_fd = *((int *)arg);       /* 任务参数 */

    int res = 0;
    struct v4l2_buffer buf;             /* 视频缓冲信息 */

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format format = {0};    /* 数据流格式 */
    format.type = type;                 

    /* 获取LCD BUF(双buffer) */
    void *lcd_buffer[2];
    void *draw_buffer = NULL;
    size_t data_cache_line_size = 0;

    if (lcddev.id <= 0x7084)        /* RGB屏触摸屏 */
    {
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(lcddev.lcd_panel_handle, 2, &lcd_buffer[0], &lcd_buffer[1]));
    }
    else
    {
        ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(lcddev.lcd_panel_handle, 2, &lcd_buffer[0], &lcd_buffer[1]));
    }
    draw_buffer = lcd_buffer[1];
    ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

    /*  CAM BUF设置两个,使用MMAP */
    void *camera_outbuf[2];
    ESP_ERROR_CHECK(camera_set_bufs(video_fd, 2, NULL));    /* 设置CAMERA的帧缓存数量 */
    ESP_ERROR_CHECK(camera_get_bufs(2, &camera_outbuf[0])); /* 获取CAMERA的帧缓冲 */

    /*---------------PPA的设置-SRM------------------*/
    ppa_client_handle_t ppa_srm_handle = NULL;
    ppa_client_config_t ppa_srm_config = {                  /* 设置客户端的属性 */
        .oper_type             = PPA_OPERATION_SRM,         /* PPA操作类型(已注册的PPA客户只能请求一种) */
        .max_pending_trans_num = 1,                         /* 客户端可以持有的最大PPA事务挂起数量 */
    };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_srm_config, &ppa_srm_handle)); /* 注册PPA客户端 */

    ESP_ERROR_CHECK(camera_stream_start(video_fd));         /* 开启视频流 */

#if FPS_FUNC_ON == 1                                        /* FPS_FUNC_ON该宏 打开帧率显示功能 */
    int fps_count = 0;
    int64_t start_time = esp_timer_get_time();
#endif

    if (ioctl(video_fd, VIDIOC_G_FMT, &format) != 0)        /* 获取设置支持的视频格式 */
    {
        ESP_LOGE(mipi_cam_tag, "get fmt failed");
    }

    while (1)
    {
        draw_buffer = lcd_buffer[0] == draw_buffer ? lcd_buffer[1] : lcd_buffer[0];     /* 双缓冲情况下,切换buffer */

#if FPS_FUNC_ON == 1                                    /* FPS_FUNC_ON该宏 打开帧率显示功能 */
        fps_count++;
        if (fps_count == 50) {
            int64_t end_time = esp_timer_get_time();
            ESP_LOGI(mipi_cam_tag, "fps: %f", 1000000.0 / ((end_time - start_time) / 50.0));
            start_time = end_time;
            fps_count = 0;
        }
#endif
        memset(&buf, 0, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;       /* 与前面format.type要一致 */
        buf.memory = V4L2_MEMORY_MMAP;                  /* 内存使用mmap方式 */
        
        res = ioctl(video_fd, VIDIOC_DQBUF, &buf);      /* 将已经捕获好视频的内存拉出已捕获视频的队列 */
        if (res != 0)
        {
            ESP_LOGE(mipi_cam_tag, "failed to receive video frame");
        }

        if (mipidev.id == 0x8399)   /* 1080p */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],     /* 输入图片数据源 */
                .in.pic_w           = format.fmt.pix.width,         /* 输入图片的宽度 */
                .in.pic_h           = format.fmt.pix.height,        /* 输入图片的高度 */
                .in.block_w         = format.fmt.pix.width,         /* 输入块的宽度 */
                .in.block_h         = format.fmt.pix.height,        /* 输入块的高度 */
                .in.block_offset_x  = 0,                            /* 输入块的X方向偏移 */
                .in.block_offset_y  = 0,                            /* 输入块的Y方向偏移 */
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,    /* 输入图片的色彩模式 */

                .out.buffer         = draw_buffer,                  /* 输出块的数据源 */
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size), /* 数据源大小 */
                .out.pic_w          = lcddev.width,                 /* 输出块的宽度(由输入的宽度、缩放因子和旋转角度决定) */
                .out.pic_h          = lcddev.height,                /* 输出块的高度(由输入的宽度、缩放因子和旋转角度决定) */
                .out.block_offset_x = 60,                           /* 输出图片的X方向偏移 */
                .out.block_offset_y = 0,                            /* 输出图片的Y方向偏移 */
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,    /* 输出图片的色彩模式 */
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_90,    /* 输出图片的旋转角度 */
                .scale_x            = 1.5,                          /* 输出图片的X轴缩放因子 */
                .scale_y            = 1,                            /* 输出图片的Y轴缩放因子 */
                .mirror_x           = true,                         /* 输出图片的X轴镜像选择 */
                .mode               = PPA_TRANS_MODE_BLOCKING,      /* PPA操作执行方式 */
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   /* 对图像内部的目标块进行缩放、旋转和镜像操作 */
        }
        else if (mipidev.id == 0x8394)  /* 720p */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,          
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width  / 2,
                .in.block_h         = format.fmt.pix.height / 2,
                .in.block_offset_x  = 320,
                .in.block_offset_y  = 240,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 0,
                .out.block_offset_y = 0,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_90,
                .scale_x            = 2,
                .scale_y            = 1.5,
                .mirror_x           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);
        }
        else if (mipidev.id == 0x9881)  /* 800p */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,   
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width,
                .in.block_h         = format.fmt.pix.height / 2,
                .in.block_offset_x  = 0,
                .in.block_offset_y  = 240,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 40,
                .out.block_offset_y = 0,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_90,
                .scale_x            = 1,
                .scale_y            = 1.5,
                .mirror_x           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   
        }
        else if (lcddev.id == 0X4342)
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,   
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width  / 4,    /* 320 */
                .in.block_h         = format.fmt.pix.height / 4,    /* 240 */
                .in.block_offset_x  = 480,
                .in.block_offset_y  = 360,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 0,
                .out.block_offset_y = 16,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,
                .scale_x            = 1.5,
                .scale_y            = 1,
                .mirror_x           = true,
                .mirror_y           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   
        }
        else if (lcddev.id == 0X4384)       /* PCLK时钟要调整为20MHz  */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,   
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width / 2,
                .in.block_h         = format.fmt.pix.height / 2,
                .in.block_offset_x  = 320,
                .in.block_offset_y  = 240,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 0,
                .out.block_offset_y = 0,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,
                .scale_x            = 1.25,
                .scale_y            = 1,
                .mirror_x           = true,
                .mirror_y           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   
        }
        else if (lcddev.id == 0X7084)       /* PCLK时钟要调整为20MHz  */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,   
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width / 2,
                .in.block_h         = format.fmt.pix.height / 2,
                .in.block_offset_x  = 320,
                .in.block_offset_y  = 240,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 0,
                .out.block_offset_y = 0,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,
                .scale_x            = 1.25,
                .scale_y            = 1,
                .mirror_x           = true,
                .mirror_y           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   
        }
        else if (lcddev.id == 0X7016)       /* PCLK时钟要调整为18MHz  */
        {
            ppa_srm_oper_config_t oper_config = {
                .in.buffer          = camera_outbuf[buf.index],
                .in.pic_w           = format.fmt.pix.width,   
                .in.pic_h           = format.fmt.pix.height,
                .in.block_w         = format.fmt.pix.width / 2,
                .in.block_h         = format.fmt.pix.height / 2,
                .in.block_offset_x  = 320,
                .in.block_offset_y  = 240,
                .in.srm_cm          = PPA_SRM_COLOR_MODE_RGB565,

                .out.buffer         = draw_buffer,
                .out.buffer_size    = ALIGN_UP_BY(lcddev.height * lcddev.width * 16 / 8, data_cache_line_size),
                .out.pic_w          = lcddev.width,
                .out.pic_h          = lcddev.height,
                .out.block_offset_x = 32,
                .out.block_offset_y = 12,
                .out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565,
                .rotation_angle     = PPA_SRM_ROTATION_ANGLE_0,
                .scale_x            = 1.5,
                .scale_y            = 1.2,
                .mirror_x           = true,
                .mirror_y           = true,
                .mode               = PPA_TRANS_MODE_BLOCKING,
            };
            ppa_do_scale_rotate_mirror(ppa_srm_handle, &oper_config);   
        }

        if (ioctl(video_fd, VIDIOC_QBUF, &buf) != 0)    /* 将空闲的内存加入可捕获视频的队列 */
        {
            ESP_LOGE(mipi_cam_tag, "failed to free video frame");
        }

    }

}
