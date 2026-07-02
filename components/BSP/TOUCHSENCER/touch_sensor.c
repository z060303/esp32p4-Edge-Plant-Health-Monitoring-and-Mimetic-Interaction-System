#include "touch_sensor.h"

#include "driver/gpio.h"
#include "driver/touch_sens.h"
#include "esp_log.h"
#include "soc/touch_sensor_channel.h"

typedef struct {
    gpio_num_t gpio_num;
    int channel_id;
} touch_sensor_candidate_t;

static const char *TAG = "touch_sensor";

/* Touch 采样配置，采用 ESP-IDF 官方推荐的基础配置 */
static touch_sensor_sample_config_t s_touch_sample_cfg[TOUCH_DRIVER_SAMPLE_CFG_NUM] = {
    TOUCH_SENSOR_V3_DEFAULT_SAMPLE_CONFIG(1, 1, 1),
};

/* 按优先级排列可用的 Touch GPIO，优先尝试 GPIO4 */
static const touch_sensor_candidate_t s_touch_candidates[] = {
    {(gpio_num_t)TOUCH_PREFERRED_GPIO, TOUCH_PAD_GPIO4_CHANNEL},
    {GPIO_NUM_2, TOUCH_PAD_GPIO2_CHANNEL},
    {GPIO_NUM_3, TOUCH_PAD_GPIO3_CHANNEL},
    {GPIO_NUM_5, TOUCH_PAD_GPIO5_CHANNEL},
    {GPIO_NUM_6, TOUCH_PAD_GPIO6_CHANNEL},
    {GPIO_NUM_7, TOUCH_PAD_GPIO7_CHANNEL},
    {GPIO_NUM_8, TOUCH_PAD_GPIO8_CHANNEL},
    {GPIO_NUM_9, TOUCH_PAD_GPIO9_CHANNEL},
    {GPIO_NUM_10, TOUCH_PAD_GPIO10_CHANNEL},
    {GPIO_NUM_11, TOUCH_PAD_GPIO11_CHANNEL},
    {GPIO_NUM_12, TOUCH_PAD_GPIO12_CHANNEL},
    {GPIO_NUM_13, TOUCH_PAD_GPIO13_CHANNEL},
    {GPIO_NUM_14, TOUCH_PAD_GPIO14_CHANNEL},
    {GPIO_NUM_15, TOUCH_PAD_GPIO15_CHANNEL},
};

static touch_sensor_handle_t s_touch_sensor_handle;
static touch_channel_handle_t s_touch_channel_handle;
static gpio_num_t s_touch_gpio = GPIO_NUM_NC;
static int s_touch_channel_id = -1;
static bool s_touch_initialized;
static bool s_touch_baseline_ready;
static bool s_touch_detected;
static uint32_t s_touch_filtered_value;
static float s_touch_baseline_value;
static uint32_t s_touch_filter_buffer[TOUCH_FILTER_SIZE];
static uint32_t s_touch_filter_count;
static uint32_t s_touch_filter_index;
static uint64_t s_touch_filter_sum;
static uint64_t s_touch_baseline_sum;
static uint32_t s_touch_baseline_count;
static uint32_t s_touch_touch_confirm_count;
static uint32_t s_touch_release_confirm_count;
static uint32_t s_touch_read_fail_count;
static float s_touch_last_ratio;
static int32_t s_touch_last_delta;

/* 固定使用 GPIO2 对应的 Touch 通道，避免自动选择带来不确定性 */
static void touch_sensor_select_fixed_gpio2(gpio_num_t *gpio_num, int *channel_id)
{
    *gpio_num = GPIO_NUM_2;
    *channel_id = TOUCH_PAD_GPIO2_CHANNEL;
}

/* 判断候选 Touch GPIO 是否已经被当前工程其它功能占用 */
static bool touch_sensor_is_gpio_reserved(gpio_num_t gpio_num)
{
    switch (gpio_num) {
    case GPIO_NUM_3:
    case GPIO_NUM_4:
    case GPIO_NUM_5:
    case GPIO_NUM_6:
    case GPIO_NUM_7:
    case GPIO_NUM_8:
    case GPIO_NUM_9:
    case GPIO_NUM_10:
    case GPIO_NUM_11:
    case GPIO_NUM_12:
    case GPIO_NUM_13:
    case GPIO_NUM_14:
    case GPIO_NUM_15:
        return true;
    default:
        return false;
    }
}

/* 自动选择未被当前工程占用的 Touch GPIO 和通道 */
static bool touch_sensor_select_channel(gpio_num_t *gpio_num, int *channel_id)
{
    uint32_t i = 0;

    for (i = 0; i < (sizeof(s_touch_candidates) / sizeof(s_touch_candidates[0])); i++) {
        if (!touch_sensor_is_gpio_reserved(s_touch_candidates[i].gpio_num)) {
            *gpio_num = s_touch_candidates[i].gpio_num;
            *channel_id = s_touch_candidates[i].channel_id;
            return true;
        }
    }

    return false;
}

/* 将新的 Touch 采样值写入滑动平均滤波器 */
static uint32_t touch_sensor_push_filter_value(uint32_t sample_value)
{
    if (s_touch_filter_count < TOUCH_FILTER_SIZE) {
        s_touch_filter_sum += sample_value;
        s_touch_filter_buffer[s_touch_filter_index] = sample_value;
        s_touch_filter_count++;
    } else {
        s_touch_filter_sum -= s_touch_filter_buffer[s_touch_filter_index];
        s_touch_filter_sum += sample_value;
        s_touch_filter_buffer[s_touch_filter_index] = sample_value;
    }

    s_touch_filter_index++;
    if (s_touch_filter_index >= TOUCH_FILTER_SIZE) {
        s_touch_filter_index = 0;
    }

    if (s_touch_filter_count == 0) {
        return 0;
    }

    return (uint32_t)(s_touch_filter_sum / s_touch_filter_count);
}

/* 获取当前 Touch 值相对基线的绝对变化比例，兼容不同硬件的变化方向 */
static float touch_sensor_get_ratio(uint32_t current_value)
{
    float delta = 0.0f;

    if (s_touch_baseline_value <= 1.0f) {
        return 0.0f;
    }

    delta = (float)current_value - s_touch_baseline_value;
    if (delta < 0.0f) {
        delta = -delta;
    }

    return delta / s_touch_baseline_value;
}

/* 初始化盆底电容板检测模块 */
esp_err_t touch_sensor_init(void)
{
    touch_sensor_config_t touch_sensor_cfg = {};
    touch_channel_config_t touch_channel_cfg = {
        .active_thresh = {
            TOUCH_DRIVER_ACTIVE_THRESH,
        },
    };
    touch_sensor_filter_config_t touch_filter_cfg = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    esp_err_t ret = ESP_OK;

    if (s_touch_initialized) {
        return ESP_OK;
    }

    /* 按用户要求固定指定 GPIO2 作为电容触摸输入 */
    touch_sensor_select_fixed_gpio2(&s_touch_gpio, &s_touch_channel_id);

    touch_sensor_cfg = (touch_sensor_config_t)TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(
        TOUCH_DRIVER_SAMPLE_CFG_NUM,
        s_touch_sample_cfg
    );

    ret = touch_sensor_new_controller(&touch_sensor_cfg, &s_touch_sensor_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[Touch] New controller failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = touch_sensor_new_channel(s_touch_sensor_handle, s_touch_channel_id, &touch_channel_cfg, &s_touch_channel_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[Touch] New channel failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = touch_sensor_enable(s_touch_sensor_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[Touch] Enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 关闭驱动自带平滑滤波，保证本模块的 8 点滑动平均是唯一主滤波 */
    touch_filter_cfg.data.smooth_filter = TOUCH_SMOOTH_NO_FILTER;
    touch_filter_cfg.data.active_hysteresis = 0;
    touch_filter_cfg.data.debounce_cnt = 0;

    ret = touch_sensor_config_filter(s_touch_sensor_handle, &touch_filter_cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[Touch] Filter config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = touch_sensor_start_continuous_scanning(s_touch_sensor_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "[Touch] Start scanning failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_touch_initialized = true;
    s_touch_baseline_ready = false;
    s_touch_detected = false;
    s_touch_filtered_value = 0;
    s_touch_baseline_value = 0.0f;
    s_touch_filter_count = 0;
    s_touch_filter_index = 0;
    s_touch_filter_sum = 0;
    s_touch_baseline_sum = 0;
    s_touch_baseline_count = 0;
    s_touch_touch_confirm_count = 0;
    s_touch_release_confirm_count = 0;
    s_touch_read_fail_count = 0;
    s_touch_last_ratio = 0.0f;
    s_touch_last_delta = 0;

    ESP_LOGI(TAG, "[Touch] Touch GPIO selected: GPIO%d CH%d", s_touch_gpio, s_touch_channel_id);
    ESP_LOGI(TAG, "[Touch] Touch sensor started, waiting baseline calibration");
    return ESP_OK;
}

/* 周期更新盆底电容板检测状态 */
void touch_sensor_update(void)
{
    uint32_t touch_value[TOUCH_DRIVER_SAMPLE_CFG_NUM] = {0};
    uint32_t filtered_value = 0;
    float ratio = 0.0f;
    bool touch_candidate = false;
    esp_err_t ret = ESP_OK;

    if (!s_touch_initialized) {
        return;
    }

    ret = touch_channel_read_data(s_touch_channel_handle, TOUCH_CHAN_DATA_TYPE_SMOOTH, touch_value);
    if (ret != ESP_OK) {
        /* 连续读取失败时周期性打印告警，便于串口排查驱动是否拿到有效采样 */
        s_touch_read_fail_count++;
        if ((s_touch_read_fail_count % TOUCH_READ_FAIL_LOG_COUNT) == 0U) {
            ESP_LOGW(TAG, "[Touch] Read data failed: %s", esp_err_to_name(ret));
        }
        return;
    }
    s_touch_read_fail_count = 0;

    filtered_value = touch_sensor_push_filter_value(touch_value[0]);
    s_touch_filtered_value = filtered_value;
    s_touch_last_delta = (int32_t)filtered_value - (int32_t)(s_touch_baseline_value);

    if (!s_touch_baseline_ready) {
        s_touch_baseline_sum += filtered_value;
        s_touch_baseline_count++;
        if (s_touch_baseline_count >= TOUCH_SAMPLE_COUNT) {
            s_touch_baseline_value = (float)s_touch_baseline_sum / (float)TOUCH_SAMPLE_COUNT;
            s_touch_baseline_ready = true;
            ESP_LOGI(TAG, "[Touch] Baseline = %lu", (unsigned long)touch_sensor_getBaseline());
        }
        return;
    }

    ratio = touch_sensor_get_ratio(filtered_value);
    s_touch_last_ratio = ratio;
    touch_candidate = ratio > TOUCH_TRIGGER_RATIO;

    if (!s_touch_detected && !touch_candidate) {
        /* 仅在未触摸状态下缓慢更新动态基线，适应环境变化 */
        s_touch_baseline_value =
            (s_touch_baseline_value * (1.0f - TOUCH_BASELINE_UPDATE_ALPHA)) +
            ((float)filtered_value * TOUCH_BASELINE_UPDATE_ALPHA);
    }

    if (touch_candidate) {
        s_touch_touch_confirm_count++;
        if (s_touch_touch_confirm_count > TOUCH_CONFIRM_COUNT) {
            s_touch_touch_confirm_count = TOUCH_CONFIRM_COUNT;
        }
        s_touch_release_confirm_count = 0;

        if (!s_touch_detected && s_touch_touch_confirm_count >= TOUCH_CONFIRM_COUNT) {
            s_touch_detected = true;
            ESP_LOGI(TAG, "[Touch] Plant touched");
        }
    } else {
        s_touch_release_confirm_count++;
        if (s_touch_release_confirm_count > TOUCH_CONFIRM_COUNT) {
            s_touch_release_confirm_count = TOUCH_CONFIRM_COUNT;
        }
        s_touch_touch_confirm_count = 0;

        if (s_touch_detected && s_touch_release_confirm_count >= TOUCH_CONFIRM_COUNT) {
            s_touch_detected = false;
            ESP_LOGI(TAG, "[Touch] Plant released");
        }
    }

#if TOUCH_DEBUG
    ESP_LOGI(TAG,
             "[Touch] value=%lu baseline=%lu delta=%ld ratio=%.3f state=%d",
             (unsigned long)s_touch_filtered_value,
             (unsigned long)touch_sensor_getBaseline(),
             (long)s_touch_last_delta,
             (double)ratio,
             s_touch_detected ? 1 : 0);
#endif
}

/* 获取当前是否检测到触摸 */
bool touch_sensor_isTouched(void)
{
    return s_touch_detected;
}

/* 获取当前滤波后的 Touch 实时值 */
uint32_t touch_sensor_getValue(void)
{
    return s_touch_filtered_value;
}

/* 获取当前动态基线值 */
uint32_t touch_sensor_getBaseline(void)
{
    if (s_touch_baseline_value <= 0.0f) {
        return 0;
    }

    return (uint32_t)(s_touch_baseline_value + 0.5f);
}
