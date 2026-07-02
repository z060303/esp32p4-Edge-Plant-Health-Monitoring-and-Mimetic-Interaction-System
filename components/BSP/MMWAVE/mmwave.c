#include "mmwave.h"

#include "driver/gpio.h"
#include "esp_log.h"

/* 选择当前工程未占用的 GPIO36 作为 LD2412 OUT 输入 */
#define MMWAVE_OUT_GPIO GPIO_NUM_36
/* LD2412 OUT 默认高电平表示有人 */
#define MMWAVE_OUT_ACTIVE_LEVEL 1

static const char *TAG = "mmwave";
/* 记录毫米波模块是否已经完成初始化 */
static bool s_mmwave_initialized;
/* 缓存当前人体检测状态，true 表示有人 */
static bool s_human_detected;

/* 直接读取毫米波 OUT 引脚状态 */
bool mmwave_gpio_detect(void)
{
    if (!s_mmwave_initialized) {
        return false;
    }

    return gpio_get_level(MMWAVE_OUT_GPIO) == MMWAVE_OUT_ACTIVE_LEVEL;
}

/* 初始化毫米波 OUT 输入引脚 */
esp_err_t mmwave_init(void)
{
    if (s_mmwave_initialized) {
        return ESP_OK;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << MMWAVE_OUT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[MMWave] OUT input init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_mmwave_initialized = true;
    s_human_detected = mmwave_gpio_detect();

    ESP_LOGI(TAG, "[MMWave] OUT input ready: GPIO%d active=HIGH", MMWAVE_OUT_GPIO);
    if (s_human_detected) {
        ESP_LOGI(TAG, "[MMWave] Human detected");
    }

    return ESP_OK;
}

/* 轮询毫米波 OUT 电平并更新人体状态 */
void mmwave_update(void)
{
    if (!s_mmwave_initialized) {
        ESP_LOGW(TAG, "[MMWave] error");
        return;
    }

    bool detected = mmwave_gpio_detect();
    // if (detected == s_human_detected) {
    //     return;
    // }

    s_human_detected = detected;
    if (s_human_detected) {
        gpio_set_level(LED0_GPIO_PIN, 0);
        ESP_LOGW(TAG, "[MMWave] Human detected");
    } else {
        gpio_set_level(LED0_GPIO_PIN, 1);
        ESP_LOGW(TAG, "[MMWave] Human lost");
    }
}

/* 获取当前人体检测状态 */
bool mmwave_isHumanDetected(void)
{
    return s_human_detected;
}
