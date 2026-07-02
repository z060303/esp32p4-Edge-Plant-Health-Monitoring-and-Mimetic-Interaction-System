#ifndef PLANT_TOUCH_H
#define PLANT_TOUCH_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t plant_touch_init(void);
void plant_touch_start(void);
bool plant_touch_is_active(void);
bool plant_touch_was_touched(void);

#endif
