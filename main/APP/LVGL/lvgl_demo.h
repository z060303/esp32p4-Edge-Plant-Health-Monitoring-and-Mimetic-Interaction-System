#ifndef __LVGL_DEMO_H
#define __LVGL_DEMO_H

#include "lvgl.h"

/* 声明函数，避免 static 导致的未使用警告 */
void lvgl_demo(void);
void lv_port_disp_init(void);
void lvgl_disp_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);
void increase_lvgl_tick(void *arg);
void draw_image(void);
#endif /* __LVGL_DEMO_H */