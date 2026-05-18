#include "my_timer.h"

#include "driver/gptimer.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "MY_TIMER";

/* GPTimer句柄 */
static gptimer_handle_t s_gptimer = NULL;

/* 用户回调 */
static timer_cb_t s_user_cb = NULL;

/*-------------------------------------------------------
 * GPTimer中断回调
 *------------------------------------------------------*/
static bool timer_isr_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_ctx)
{
    if (s_user_cb != NULL)
    {
        s_user_cb();
    }

    return false;
}

/*-------------------------------------------------------
 * 注册用户回调
 *------------------------------------------------------*/
void my_timer_register_callback(timer_cb_t cb)
{
    s_user_cb = cb;
}

/*-------------------------------------------------------
 * 初始化定时器
 *------------------------------------------------------*/
void my_timer_init(uint64_t period_us)
{
    /* GPTimer配置 */
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1MHz
    };

    /* 创建定时器 */
    ESP_ERROR_CHECK(
        gptimer_new_timer(
            &timer_config,
            &s_gptimer));

    /* 注册回调 */
    gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_isr_callback,
    };

    ESP_ERROR_CHECK(
        gptimer_register_event_callbacks(
            s_gptimer,
            &cbs,
            NULL));

    /* 设置报警 */
    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = period_us,
        .flags.auto_reload_on_alarm = true,
    };

    ESP_ERROR_CHECK(
        gptimer_set_alarm_action(
            s_gptimer,
            &alarm_config));

    /* 使能定时器 */
    ESP_ERROR_CHECK(
        gptimer_enable(s_gptimer));

    ESP_LOGI(TAG, "Timer init success");
}

/*-------------------------------------------------------
 * 启动定时器
 *------------------------------------------------------*/
void my_timer_start(void)
{
    ESP_ERROR_CHECK(
        gptimer_start(s_gptimer));

    ESP_LOGI(TAG, "Timer start");
}

/*-------------------------------------------------------
 * 停止定时器
 *------------------------------------------------------*/
void my_timer_stop(void)
{
    ESP_ERROR_CHECK(
        gptimer_stop(s_gptimer));

    ESP_LOGI(TAG, "Timer stop");
}