#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "nvs_flash.h"
#include "led.h"
#include "myiic.h"
#include "mipi_cam.h"
#include "mmwave.h"
#include "plant_display.h"
#include "plant_sensor.h"
#include "plant_touch.h"
#include "touch_sensor.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static esp_timer_handle_t s_touch_sensor_timer;
static const char *TAG = "app_main";

/* 电容板检测定时回调，固定周期调用 Touch 更新函数 */
static void touch_sensor_timer_callback(void *arg)
{
    (void)arg;
    touch_sensor_update();
}

static void plant_ai_console_init(void)
{
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    usb_serial_jtag_driver_config_t jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&jtag_config));
    usb_serial_jtag_vfs_use_driver();
#endif

    setvbuf(stdin, NULL, _IONBF, 0);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    plant_ai_console_init();
    led_init();
    myiic_init();
    mmwave_init();
    plant_display_init();
    if (plant_touch_init() == ESP_OK)
    {
        plant_touch_start();
    }
    if (plant_sensor_init() == ESP_OK)
    {
        plant_sensor_start();
    }

    /* 输出触摸模块初始化结果，便于直接从终端确认模块是否启动 */
    if (touch_sensor_init() == ESP_OK)
    {
        ESP_LOGI(TAG, "[Touch] touch_sensor_init success");
        const esp_timer_create_args_t touch_sensor_timer_args = {
            .callback = touch_sensor_timer_callback,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "touch_sensor",
            .skip_unhandled_events = true,
        };

        ESP_ERROR_CHECK(esp_timer_create(&touch_sensor_timer_args, &s_touch_sensor_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(s_touch_sensor_timer, TOUCH_UPDATE_PERIOD_MS * 1000ULL));
        ESP_LOGI(TAG, "[Touch] touch sensor timer started");
    }
    else
    {
        ESP_LOGW(TAG, "[Touch] touch_sensor_init failed");
    }



#if CONFIG_PLANT_AI_SCREEN_TEST_ON_BOOT
    int lcd_test_step = 0;
    while (1)
    {
        /* 轮询毫米波 OUT 检测状态 */
        mmwave_update();
        plant_display_show_color_test(lcd_test_step++);
        // LED0_TOGGLE();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    mipi_cam_init();

    while (1)
    {
        // if(gpio_get_level(GPIO_NUM_36)){
        //     gpio_set_level(LED0_GPIO_PIN, 1);
        //     ESP_LOGW("MMWAVE", "Human detected");
        // }else{
        //     gpio_set_level(LED0_GPIO_PIN, 0);
        //     ESP_LOGW("MMWAVE", "Human lost");
        // }
        // mmwave_update();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
