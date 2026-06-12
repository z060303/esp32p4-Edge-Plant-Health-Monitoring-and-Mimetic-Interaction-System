#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "nvs_flash.h"
#include "led.h"
#include "myiic.h"
#include "mipi_cam.h"
#include "plant_display.h"
#include "plant_sensor.h"
#include "plant_touch.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

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
    plant_display_init();
    if (plant_touch_init() == ESP_OK)
    {
        plant_touch_start();
    }
    if (plant_sensor_init() == ESP_OK)
    {
        plant_sensor_start();
    }

#if CONFIG_PLANT_AI_SCREEN_TEST_ON_BOOT
    int lcd_test_step = 0;
    while (1)
    {
        plant_display_show_color_test(lcd_test_step++);
        LED0_TOGGLE();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    mipi_cam_init();

    while (1)
    {
        LED0_TOGGLE();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
