#include "lvgl_demo.h"
#include "spilcd.h"               // 你的底层 LCD 驱动头文件
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"    // esp_lcd 框架的操作函数
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

LV_IMG_DECLARE(picture1);

static const char *TAG = "LVGL_PORT";

/* 引用在 spilcd.c 中定义的全局变量 */
extern esp_lcd_panel_handle_t panel_handle;
extern _spilcd_dev spilcddev;

/**
 * @brief       将内部缓冲区的内容刷新到显示屏上的特定区域
 * @param       drv : 显示设备
 * @param       area : 要刷新的区域
 * @param       color_map : 颜色数组
 * @retval      无
 */
void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;

    /* 使用 esp_lcd 的底层 API 把颜色数据通过 DMA 发给屏幕 */
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_map);

    /* 重要!!! 通知图形库，已经刷新完毕了 */
    lv_disp_flush_ready(drv);
}

/**
 * @brief       初始化并注册显示设备
 * @param       无
 * @retval      无
 */
void lv_port_disp_init(void)
{
    /* 1. 初始化底层 SPI LCD */
    spilcd_init();

    /* 2. 创建绘图缓冲区 
     * 针对 SPI 屏幕，通常分配 40~50 行的缓冲区大小。
     * 因为用到了 SPI DMA，所以内存必须使用 MALLOC_CAP_DMA 申请！
     */
    uint32_t buffer_lines = 40; 
    uint32_t buffer_size = spilcddev.width * buffer_lines;
    
    lv_color_t *buf1 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = heap_caps_malloc(buffer_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    
    if (buf1 == NULL) {
        ESP_LOGE(TAG, "LVGL 缓冲区内存分配失败！");
        return;
    }

    /* 3. 初始化显示缓冲区结构体 */
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buffer_size);

    /* 4. 初始化并在 LVGL 中注册显示设备 */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = spilcddev.width;         // 从 spilcddev 获取宽度
    disp_drv.ver_res = spilcddev.height;        // 从 spilcddev 获取高度
    disp_drv.flush_cb = lvgl_disp_flush_cb;     // 设置刷新回调函数
    disp_drv.draw_buf = &disp_buf;              // 设置绘画缓冲区
    disp_drv.user_data = panel_handle;          // 传递屏幕控制句柄
    
    lv_disp_drv_register(&disp_drv);
}

/**
 * @brief       告诉 LVGL 运行时间的心跳函数
 * @param       arg : 传入参数(未用到)
 */
void increase_lvgl_tick(void *arg)
{
    /* 每次定时器中断，告诉 LVGL 过了 2 毫秒 */
    lv_tick_inc(2);
}

void draw_image(void);

/**
 * @brief       lvgl_demo 入口函数
 * @param       无
 * @retval      无
 */
void lvgl_demo(void)
{
    ESP_LOGI(TAG, "初始化 LVGL...");
    lv_init();              /* 初始化LVGL图形库 */
    
    ESP_LOGI(TAG, "初始化显示接口...");
    lv_port_disp_init();    /* lvgl显示接口初始化 */

    /* 为 LVGL 提供时基单元 (2ms 一次) */
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,    
        .name = "lvgl_tick"                 
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000)); /* 2ms = 2 * 1000 us */

    /* ============================================================== */
    /* 跑一个简单的测试 UI：在屏幕正中间显示一段文字 */
    // lv_obj_t * label = lv_label_create(lv_scr_act());
    // lv_label_set_text(label, "Hello ESP32-P4 & LVGL V8!");
    // lv_obj_center(label);
    /* ============================================================== */

    draw_image();

    ESP_LOGI(TAG, "进入 LVGL 主循环");
    while (1)
    {
        lv_timer_handler();             /* LVGL 任务处理器 */
        vTaskDelay(pdMS_TO_TICKS(10));  /* 让出 CPU 给其他任务 (延时 10 毫秒) */
    }
}

void draw_image(void)
{
    lv_obj_t * img = lv_img_create(lv_scr_act());
    lv_img_set_src(img, &picture1); 
    
    /* 设置缩放比例，将图片缩小到一半 (50%) */
    // lv_img_set_zoom(img, 100); 
    
    /* 开启抗锯齿（可选，缩小后图片边缘会更平滑，但会消耗一点点CPU） */
    // lv_img_set_antialias(img, true);

    lv_obj_center(img);
}