#ifndef __MY_TIMER_H__
#define __MY_TIMER_H__

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*timer_cb_t)(void);

/* 初始化定时器 */
void my_timer_init(uint64_t period_us);

/* 启动定时器 */
void my_timer_start(void);

/* 停止定时器 */
void my_timer_stop(void);

/* 注册回调 */
void my_timer_register_callback(timer_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif