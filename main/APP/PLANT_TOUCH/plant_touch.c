#include "plant_touch.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PLANT_TOUCH_GPIO GPIO_NUM_48
#define PLANT_TOUCH_ACTIVE_LEVEL 1
#define PLANT_TOUCH_HOLD_MS 4000
#define PLANT_TOUCH_TASK_STACK_SIZE 4096

static const char *TAG = "plant_touch";

static TaskHandle_t touch_task_handle;
static volatile bool touch_active;
static volatile bool touch_event;
static volatile TickType_t touch_until_tick;

static void plant_touch_task(void *arg)
{
    (void)arg;
    int stable_level = 0;
    int last_level = 0;
    int same_count = 0;
    bool last_active = false;
    TickType_t next_log_tick = 0;

    while (1) {
        int level = gpio_get_level(PLANT_TOUCH_GPIO);
        if (level == last_level) {
            if (same_count < 8) {
                same_count++;
            }
        } else {
            same_count = 0;
            last_level = level;
        }

        if (same_count >= 2) {
            stable_level = level;
        }

        bool now_active = stable_level == PLANT_TOUCH_ACTIVE_LEVEL;
        TickType_t now = xTaskGetTickCount();

        if (now_active) {
            touch_until_tick = now + pdMS_TO_TICKS(PLANT_TOUCH_HOLD_MS);
            touch_active = true;
            if (!last_active) {
                touch_event = true;
                ESP_LOGI(TAG, "touch detected on GPIO%d", PLANT_TOUCH_GPIO);
            }
        } else if (touch_active && (int32_t)(now - touch_until_tick) >= 0) {
            touch_active = false;
        }

        if ((int32_t)(now - next_log_tick) >= 0) {
            /* 输出任务剩余栈水位，便于排查栈空间是否仍然紧张 */
            UBaseType_t stack_high_water = uxTaskGetStackHighWaterMark(NULL);
            ESP_LOGI(TAG,
                     "touch GPIO%d level=%d active=%d stack_free=%lu",
                     PLANT_TOUCH_GPIO,
                     stable_level,
                     touch_active ? 1 : 0,
                     (unsigned long)stack_high_water);
            next_log_tick = now + pdMS_TO_TICKS(1000);
        }

        last_active = now_active;
        vTaskDelay(pdMS_TO_TICKS(40));
    }
}

esp_err_t plant_touch_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << PLANT_TOUCH_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TTP223 touch input ready: SIG=GPIO%d active=HIGH", PLANT_TOUCH_GPIO);
    } else {
        ESP_LOGW(TAG, "touch input init failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

void plant_touch_start(void)
{
    if (touch_task_handle) {
        return;
    }

    xTaskCreatePinnedToCore(plant_touch_task,
                            "plant_touch",
                            PLANT_TOUCH_TASK_STACK_SIZE,
                            NULL,
                            5,
                            &touch_task_handle,
                            0);
}

bool plant_touch_is_active(void)
{
    return touch_active;
}

bool plant_touch_was_touched(void)
{
    bool was_touched = touch_event;
    touch_event = false;
    return was_touched;
}
