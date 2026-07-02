#include "plant_display.h"

#include "esp_log.h"
#include "sdkconfig.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
#include "lcd.h"
#include "plant_zh_font.h"

extern void *lcd_buffer[2];

#define RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static const uint16_t UI_BG = RGB565(239, 248, 239);
static const uint16_t UI_BG_TOUCH = RGB565(251, 244, 229);
static const uint16_t UI_PANEL = RGB565(255, 255, 249);
static const uint16_t UI_PANEL_SOFT = RGB565(226, 241, 230);
static const uint16_t UI_SOIL = RGB565(126, 83, 52);
static const uint16_t UI_SOIL_DARK = RGB565(84, 55, 38);
static const uint16_t UI_TEXT = RGB565(31, 48, 39);
static const uint16_t UI_MUTED = RGB565(91, 111, 98);
static const uint16_t UI_GREEN = RGB565(44, 157, 95);
static const uint16_t UI_GREEN_DARK = RGB565(19, 104, 63);
static const uint16_t UI_GREEN_LIGHT = RGB565(128, 206, 145);
static const uint16_t UI_MINT = RGB565(196, 235, 208);
static const uint16_t UI_YELLOW = RGB565(234, 177, 61);
static const uint16_t UI_RED = RGB565(214, 78, 74);
static const uint16_t UI_CORAL = RGB565(232, 116, 91);
static const uint16_t UI_BLUE = RGB565(74, 139, 196);
static const uint16_t UI_WHITE = RGB565(255, 255, 255);
#endif

static const char *TAG = "plant_display";

static bool char_equal_ci(char left, char right)
{
    return toupper((unsigned char)left) == toupper((unsigned char)right);
}

static bool starts_with_ci(const char *text, const char *prefix)
{
    while (*prefix != '\0')
    {
        if (*text == '\0' || !char_equal_ci(*text, *prefix))
        {
            return false;
        }
        text++;
        prefix++;
    }
    return true;
}

static const char *find_key_ci(const char *command, const char *key)
{
    const size_t key_len = strlen(key);
    for (const char *cursor = command; *cursor != '\0'; cursor++)
    {
        size_t index = 0;
        while (index < key_len && cursor[index] != '\0' && char_equal_ci(cursor[index], key[index]))
        {
            index++;
        }
        if (index == key_len)
        {
            return cursor + key_len;
        }
    }
    return NULL;
}

static void copy_token_value(char *dst, size_t dst_size, const char *value)
{
    if (dst_size == 0)
    {
        return;
    }

    size_t index = 0;
    while (value[index] != '\0' && !isspace((unsigned char)value[index]) && index < dst_size - 1)
    {
        dst[index] = value[index];
        index++;
    }
    dst[index] = '\0';
}

static int parse_int_token(const char *command, const char *key, int fallback)
{
    const char *found = find_key_ci(command, key);
    if (!found)
    {
        return fallback;
    }
    return atoi(found);
}

static float parse_float_token(const char *command, const char *key, float fallback)
{
    const char *found = find_key_ci(command, key);
    return found ? strtof(found, NULL) : fallback;
}

#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
static int clamp_percent(int value)
{
    if (value < 0)
    {
        return 0;
    }
    if (value > 100)
    {
        return 100;
    }
    return value;
}

static void ui_fill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0)
    {
        return;
    }
    lcd_fill(x, y, x + w, y + h, color);
}

static void ui_text(uint16_t x,
                    uint16_t y,
                    uint16_t width,
                    uint16_t height,
                    uint8_t size,
                    const char *text,
                    uint16_t color)
{
    const uint16_t x0 = x;
    const uint16_t max_x = x + width;
    const uint16_t max_y = y + height;
    const uint8_t zh_size = size >= 24 ? 24 : 16;
    const uint8_t zh_bytes_per_row = zh_size / 8;

    while (*text != '\0' && y + zh_size <= max_y)
    {
        uint32_t codepoint = 0;
        uint8_t advance = size / 2;
        const plant_zh_glyph_t *glyph = NULL;

        if ((uint8_t)text[0] < 0x80)
        {
            if (*text == '\n')
            {
                x = x0;
                y += zh_size + 4;
                text++;
                continue;
            }

            codepoint = (uint8_t)*text++;
            advance = size / 2;
            if (x + advance > max_x)
            {
                x = x0;
                y += zh_size + 4;
            }
            if (y + size <= max_y && codepoint >= ' ' && codepoint <= '~')
            {
                lcd_show_char(x, y, (char)codepoint, size, 1, color);
            }
            x += advance;
            continue;
        }

        if (((uint8_t)text[0] & 0xE0) == 0xC0)
        {
            codepoint = ((uint32_t)(text[0] & 0x1F) << 6) |
                        ((uint32_t)(text[1] & 0x3F));
            text += 2;
        }
        else if (((uint8_t)text[0] & 0xF0) == 0xE0)
        {
            codepoint = ((uint32_t)(text[0] & 0x0F) << 12) |
                        ((uint32_t)(text[1] & 0x3F) << 6) |
                        ((uint32_t)(text[2] & 0x3F));
            text += 3;
        }
        else
        {
            text++;
            continue;
        }

        for (size_t i = 0; i < PLANT_ZH_GLYPH_COUNT; i++)
        {
            if (PLANT_ZH_GLYPHS[i].codepoint == codepoint)
            {
                glyph = &PLANT_ZH_GLYPHS[i];
                break;
            }
        }

        if (x + zh_size > max_x)
        {
            x = x0;
            y += zh_size + 4;
        }
        if (!glyph || y + zh_size > max_y)
        {
            x += zh_size;
            continue;
        }

        const uint8_t *bits = zh_size == 24 ? glyph->large : glyph->small;
        for (uint8_t gy = 0; gy < zh_size; gy++)
        {
            for (uint8_t gx = 0; gx < zh_size; gx++)
            {
                uint8_t byte = bits[gy * zh_bytes_per_row + gx / 8];
                if (byte & (0x80 >> (gx % 8)))
                {
                    lcd_draw_point(x + gx, y + gy, color);
                }
            }
        }
        x += zh_size;
    }
}

static void ui_panel(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ui_fill(x + 8, y, w - 16, h, color);
    ui_fill(x, y + 8, w, h - 16, color);
    lcd_fill_circle(x + 8, y + 8, 8, color);
    lcd_fill_circle(x + w - 8, y + 8, 8, color);
    lcd_fill_circle(x + 8, y + h - 8, 8, color);
    lcd_fill_circle(x + w - 8, y + h - 8, 8, color);
}

static void ui_pill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    const uint16_t radius = h / 2;
    if (w <= h)
    {
        lcd_fill_circle(x + radius, y + radius, radius, color);
        return;
    }
    ui_fill(x + radius, y, w - h, h, color);
    lcd_fill_circle(x + radius, y + radius, radius, color);
    lcd_fill_circle(x + w - radius, y + radius, radius, color);
}

static void ui_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    ui_fill(x, y, w, 2, color);
}

static void ui_progress(uint16_t x,
                        uint16_t y,
                        uint16_t w,
                        uint16_t h,
                        int value,
                        uint16_t color)
{
    value = clamp_percent(value);
    ui_pill(x, y, w, h, UI_PANEL_SOFT);
    ui_pill(x, y, (uint32_t)w * value / 100, h, color);
}

static void ui_filled_leaf(uint16_t cx,
                           uint16_t cy,
                           uint16_t rx,
                           uint16_t ry,
                           int8_t bend,
                           uint16_t color)
{
    const int ry_sq = ry * ry;
    for (int dy = -(int)ry; dy <= (int)ry; dy++)
    {
        int y = (int)cy + dy;
        int width = (int)rx * (ry_sq - dy * dy) / ry_sq;
        int x = (int)cx + (bend * dy) / 10;
        if (width > 0)
        {
            ui_fill(x - width, y, width * 2, 2, color);
        }
    }
}

static void ui_draw_spark(uint16_t x, uint16_t y, uint16_t color)
{
    ui_fill(x - 1, y - 9, 3, 18, color);
    ui_fill(x - 9, y - 1, 18, 3, color);
    ui_fill(x - 5, y - 5, 11, 11, color);
    lcd_fill_circle(x, y, 3, UI_WHITE);
}

static void ui_draw_header(bool touched)
{
    ui_fill(0, 0, 800, 58, touched ? UI_GREEN_DARK : UI_GREEN);
    ui_fill(0, 58, 800, 3, UI_GREEN_LIGHT);
    lcd_fill_circle(32, 28, 12, UI_MINT);
    ui_filled_leaf(42, 22, 18, 8, -3, UI_WHITE);
    ui_filled_leaf(24, 22, 15, 7, 3, UI_WHITE);
    ui_text(64, 16, 230, 28, 24, "植物健康", UI_WHITE);
    ui_text(548, 20, 130, 20, 16, "中文界面二版", UI_WHITE);
    ui_text(700, 20, 60, 20, 16, touched ? "触摸" : "实时", UI_WHITE);
}

static uint16_t ui_status_color(const char *status)
{
    if (starts_with_ci(status, "HEALTHY"))
    {
        return UI_GREEN;
    }
    if (starts_with_ci(status, "LOCAL") || starts_with_ci(status, "WARN"))
    {
        return UI_YELLOW;
    }
    if (starts_with_ci(status, "SEVERE") || starts_with_ci(status, "ALERT"))
    {
        return UI_RED;
    }
    return UI_BLUE;
}

static const char *ui_status_title(const char *status)
{
    if (starts_with_ci(status, "HEALTHY"))
    {
        return "健康生长";
    }
    if (starts_with_ci(status, "LOCAL") || starts_with_ci(status, "WARN"))
    {
        return "叶片观察";
    }
    if (starts_with_ci(status, "SEVERE") || starts_with_ci(status, "ALERT"))
    {
        return "需要养护";
    }
    return "正在读取";
}

static const char *ui_advice_text(const char *status, const char *advice, bool touched)
{
    if (touched)
    {
        if (starts_with_ci(status, "SEVERE") || starts_with_ci(status, "ALERT"))
        {
            return "触摸已收到 请检查叶片";
        }
        return "触摸已收到 植物正在响应";
    }
    if (starts_with_ci(advice, "KEEP"))
    {
        return "护理节奏良好";
    }
    if (starts_with_ci(advice, "WATCH"))
    {
        return "请持续观察叶片";
    }
    if (starts_with_ci(advice, "PETTED"))
    {
        return "触摸已收到";
    }
    if (starts_with_ci(advice, "COMFORT"))
    {
        return "触摸已收到 环境需要检查";
    }
    if (starts_with_ci(advice, "WATER"))
    {
        return "土壤偏干 请适当浇水";
    }
    if (starts_with_ci(advice, "DRAIN"))
    {
        return "土壤偏湿 加强排水";
    }
    if (starts_with_ci(advice, "LIGHT"))
    {
        return "光照不足 移到柔和光线";
    }
    if (starts_with_ci(advice, "SHADE"))
    {
        return "光照过强 注意遮阴";
    }
    if (starts_with_ci(advice, "COOL"))
    {
        return "温度偏高";
    }
    if (starts_with_ci(advice, "WARM"))
    {
        return "温度偏低";
    }
    if (starts_with_ci(advice, "HUMIDIFY"))
    {
        return "空气偏干 提高湿度";
    }
    if (starts_with_ci(advice, "VENTILATE"))
    {
        return "空气偏湿 加强通风";
    }
    if (starts_with_ci(advice, "ALERT"))
    {
        return "风险较高 检查叶片";
    }
    return "稍后复查";
}

static const char *ui_mood_text(const char *mood, bool touched)
{
    if (touched)
    {
        return "开心";
    }
    if (starts_with_ci(mood, "HAPPY") || starts_with_ci(mood, "CALM"))
    {
        return "均衡";
    }
    if (starts_with_ci(mood, "CLINGY") || starts_with_ci(mood, "COMFORTED"))
    {
        return "舒适";
    }
    if (starts_with_ci(mood, "LEISURELY"))
    {
        return "明亮";
    }
    if (starts_with_ci(mood, "HYDRATED"))
    {
        return "水分充足";
    }
    if (starts_with_ci(mood, "WARM"))
    {
        return "温暖";
    }
    if (starts_with_ci(mood, "FADING") || starts_with_ci(mood, "SAD") || starts_with_ci(mood, "HELP"))
    {
        return "压力";
    }
    return "观察中";
}

static void ui_draw_plant(uint16_t x,
                          uint16_t y,
                          const char *status,
                          bool touched,
                          uint16_t accent)
{
    const uint16_t leaf = starts_with_ci(status, "SEVERE") ? RGB565(135, 151, 89) : UI_GREEN;
    const uint16_t leaf_light = starts_with_ci(status, "SEVERE") ? RGB565(184, 175, 105) : UI_GREEN_LIGHT;
    const uint16_t issue = starts_with_ci(status, "LOCAL") || starts_with_ci(status, "WARN");
    const int lift = touched ? 12 : 0;

    ui_panel(x, y, 352, 306, RGB565(223, 241, 226));
    ui_hline(x + 38, y + 236, 276, RGB565(181, 211, 185));

    ui_fill(x + 160, y + 120 - lift, 10, 120 + lift, UI_GREEN_DARK);
    ui_fill(x + 180, y + 148 - lift, 8, 92 + lift, UI_GREEN_DARK);
    ui_fill(x + 136, y + 164 - lift, 8, 76 + lift, UI_GREEN_DARK);

    ui_filled_leaf(x + 130, y + 128 - lift, 64, 26, -5, leaf);
    ui_filled_leaf(x + 208, y + 116 - lift, 70, 28, 5, leaf_light);
    ui_filled_leaf(x + 112, y + 178 - lift, 58, 24, 5, leaf_light);
    ui_filled_leaf(x + 228, y + 184 - lift, 62, 25, -5, leaf);
    ui_filled_leaf(x + 174, y + 80 - lift, 50, 23, 0, leaf_light);
    ui_filled_leaf(x + 176, y + 150 - lift, 42, 18, 0, UI_GREEN_DARK);

    if (issue)
    {
        lcd_fill_circle(x + 244, y + 176 - lift, 8, UI_YELLOW);
        lcd_fill_circle(x + 116, y + 169 - lift, 6, UI_YELLOW);
    }
    if (starts_with_ci(status, "SEVERE") || starts_with_ci(status, "ALERT"))
    {
        lcd_fill_circle(x + 210, y + 109 - lift, 8, UI_RED);
        lcd_fill_circle(x + 142, y + 126 - lift, 7, UI_RED);
    }

    ui_fill(x + 92, y + 236, 170, 44, UI_SOIL);
    ui_fill(x + 112, y + 216, 130, 24, RGB565(181, 112, 67));
    ui_fill(x + 106, y + 278, 142, 9, UI_SOIL_DARK);

    ui_pill(x + 126, y + 18, 98, 28, accent);
    ui_text(x + 150, y + 25, 66, 16, 16, touched ? "你好" : "植物", UI_WHITE);

    if (touched)
    {
        lcd_draw_circle(x + 176, y + 145, 96, UI_CORAL);
        lcd_draw_circle(x + 176, y + 145, 112, UI_CORAL);
        ui_draw_spark(x + 72, y + 82, UI_CORAL);
        ui_draw_spark(x + 282, y + 76, UI_YELLOW);
        ui_draw_spark(x + 302, y + 212, UI_CORAL);
    }
}

static void ui_draw_metric(uint16_t x,
                           uint16_t y,
                           const char *label,
                           const char *value,
                           int percent,
                           uint16_t color)
{
    ui_panel(x, y, 172, 58, UI_PANEL);
    ui_text(x + 18, y + 10, 86, 18, 16, label, UI_MUTED);
    ui_text(x + 18, y + 30, 86, 22, 16, value, UI_TEXT);
    ui_progress(x + 100, y + 26, 48, 10, percent, color);
}

static void ui_format_sensor(char *dst,
                             size_t dst_size,
                             float value,
                             const char *unit,
                             int decimals,
                             const char *empty)
{
    if ((value < 0.0f && unit[0] != 'C') || value < -40.0f)
    {
        snprintf(dst, dst_size, "%s", empty);
        return;
    }
    if (decimals == 0)
    {
        snprintf(dst, dst_size, "%.0f%s", value, unit);
    }
    else
    {
        snprintf(dst, dst_size, "%.1f%s", value, unit);
    }
}

static void ui_draw_status_panel(uint16_t x,
                                 uint16_t y,
                                 const char *status,
                                 int confidence_percent,
                                 int health_score,
                                 const char *advice,
                                 bool touched,
                                 uint16_t accent)
{
    char health_text[12];
    char conf_text[12];
    health_score = clamp_percent(health_score);
    confidence_percent = clamp_percent(confidence_percent);

    snprintf(health_text, sizeof(health_text), "%d%%", health_score);
    snprintf(conf_text, sizeof(conf_text), "%d%%", confidence_percent);

    ui_panel(x, y, 366, 202, UI_PANEL);
    ui_text(x + 24, y + 18, 116, 18, 16, "植株状态", UI_MUTED);
    ui_text(x + 24, y + 44, 270, 36, 32, touched ? "触摸响应" : ui_status_title(status), accent);
    ui_text(x + 24, y + 88, 288, 36, 16, ui_advice_text(status, advice, touched), UI_MUTED);

    ui_text(x + 24, y + 140, 70, 18, 16, "健康", UI_MUTED);
    ui_text(x + 96, y + 132, 58, 26, 24, health_text, UI_TEXT);
    ui_progress(x + 164, y + 140, 158, 16, health_score, accent);

    ui_text(x + 24, y + 168, 70, 18, 16, "叶片", UI_MUTED);
    ui_text(x + 96, y + 160, 58, 26, 24, conf_text, UI_TEXT);
    ui_progress(x + 164, y + 168, 158, 16, confidence_percent, UI_BLUE);
}

static void ui_draw_touch_panel(uint16_t x,
                                uint16_t y,
                                const char *mood,
                                bool touched,
                                uint16_t accent)
{
    ui_panel(x, y, 366, 86, touched ? RGB565(255, 239, 223) : UI_PANEL);
    ui_text(x + 24, y + 15, 80, 18, 16, "心情", UI_MUTED);
    ui_text(x + 24, y + 42, 145, 26, 24, ui_mood_text(mood, touched), UI_TEXT);
    ui_pill(x + 210, y + 28, 116, 30, touched ? UI_CORAL : UI_PANEL_SOFT);
    ui_text(x + 234, y + 36, 82, 18, 16, touched ? "被安抚" : "平稳", touched ? UI_WHITE : UI_MUTED);
    lcd_draw_circle(x + 184, y + 44, 18, accent);
    if (touched)
    {
        lcd_fill_circle(x + 184, y + 44, 10, accent);
    }
}
#endif

static void parse_string_token(const char *command,
                               const char *key,
                               char *dst,
                               size_t dst_size,
                               const char *fallback)
{
    const char *found = find_key_ci(command, key);
    if (!found)
    {
        snprintf(dst, dst_size, "%s", fallback);
        return;
    }
    copy_token_value(dst, dst_size, found);
}

void plant_display_init(void)
{
#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
    lcd_init();
#if !CONFIG_PLANT_AI_SCREEN_TEST_ON_BOOT
    plant_display_show_status("BOOTING", 0, "WAIT", 0, "Waiting");
#endif
    ESP_LOGI(TAG, "LCD display enabled");
#else
    ESP_LOGI(TAG, "LCD display disabled");
#endif
}

void plant_display_show_status(const char *status,
                               int confidence_percent,
                               const char *mood,
                               int health_score,
                               const char *advice)
{
    plant_display_show_status_full(status,
                                   confidence_percent,
                                   mood,
                                   health_score,
                                   advice,
                                   false,
                                   -1.0f,
                                   -100.0f,
                                   -1.0f,
                                   -1.0f);
}

void plant_display_show_status_full(const char *status,
                                    int confidence_percent,
                                    const char *mood,
                                    int health_score,
                                    const char *advice,
                                    bool touched,
                                    float soil_moisture,
                                    float temperature,
                                    float air_humidity,
                                    float light)
{
#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
    const uint16_t accent = ui_status_color(status);
    const uint16_t bg = touched ? UI_BG_TOUCH : UI_BG;
    char soil_text[24];
    char temp_text[24];
    char hum_text[24];
    char light_text[24];

    ui_format_sensor(soil_text, sizeof(soil_text), soil_moisture, "%", 0, "--");
    ui_format_sensor(temp_text, sizeof(temp_text), temperature, " 摄氏度", 1, "--");
    ui_format_sensor(hum_text, sizeof(hum_text), air_humidity, "%", 0, "--");
    ui_format_sensor(light_text, sizeof(light_text), light, " 勒克斯", 0, "--");

    lcd_frame_begin(bg);

    ui_draw_header(touched);
    ui_draw_plant(28, 82, status, touched, accent);
    ui_draw_status_panel(402, 82, status, confidence_percent, health_score, advice, touched, accent);
    ui_draw_touch_panel(402, 306, mood, touched, accent);

    ui_draw_metric(28, 408, "土壤", soil_text, soil_moisture >= 0.0f ? (int)soil_moisture : 0, UI_GREEN);
    ui_draw_metric(222, 408, "温度", temp_text, temperature > -40.0f ? (int)((temperature + 5.0f) * 2.0f) : 0, UI_CORAL);
    ui_draw_metric(416, 408, "湿度", hum_text, air_humidity >= 0.0f ? (int)air_humidity : 0, UI_BLUE);
    ui_draw_metric(610, 408, "光照", light_text, light >= 0.0f ? (int)(light / 15.0f) : 0, UI_YELLOW);

    lcd_frame_end();

    ESP_LOGI(TAG,
             "UI status=%s mood=%s advice=%s touch=%d layout=plant_monitor",
             status,
             mood,
             advice,
             touched ? 1 : 0);
#else
    ESP_LOGI(TAG,
             "RESULT status=%s confidence=%d mood=%s health=%d advice=%s touch=%d soil=%.1f temp=%.1f hum=%.1f light=%.1f",
             status,
             confidence_percent,
             mood,
             health_score,
             advice,
             touched ? 1 : 0,
             soil_moisture,
             temperature,
             air_humidity,
             light);
#endif
}

static void pattern_set_pixel(uint16_t *buffer, uint16_t width, uint16_t height, int x, int y, uint16_t color)
{
    if (x >= 0 && y >= 0 && x < width && y < height)
    {
        buffer[(uint32_t)y * width + x] = color;
    }
}

static void pattern_draw_line(uint16_t *buffer,
                              uint16_t width,
                              uint16_t height,
                              int x0,
                              int y0,
                              int x1,
                              int y1,
                              uint16_t color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        pattern_set_pixel(buffer, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

static void pattern_draw_circle(uint16_t *buffer,
                                uint16_t width,
                                uint16_t height,
                                int cx,
                                int cy,
                                int radius,
                                uint16_t color)
{
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        pattern_set_pixel(buffer, width, height, cx + x, cy + y, color);
        pattern_set_pixel(buffer, width, height, cx + y, cy + x, color);
        pattern_set_pixel(buffer, width, height, cx - y, cy + x, color);
        pattern_set_pixel(buffer, width, height, cx - x, cy + y, color);
        pattern_set_pixel(buffer, width, height, cx - x, cy - y, color);
        pattern_set_pixel(buffer, width, height, cx - y, cy - x, color);
        pattern_set_pixel(buffer, width, height, cx + y, cy - x, color);
        pattern_set_pixel(buffer, width, height, cx + x, cy - y, color);

        y++;
        if (err <= 0)
        {
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x--;
            err -= 2 * x + 1;
        }
    }
}

static void pattern_fill_buffer(uint16_t *buffer, uint16_t width, uint16_t height)
{
    const uint16_t bars[] = {
        WHITE,
        YELLOW,
        CYAN,
        GREEN,
        MAGENTA,
        RED,
        BLUE,
        BLACK,
    };
    const uint16_t bar_count = sizeof(bars) / sizeof(bars[0]);

    for (uint16_t y = 0; y < height; y++)
    {
        for (uint16_t x = 0; x < width; x++)
        {
            uint16_t bar = (uint32_t)x * bar_count / width;
            buffer[(uint32_t)y * width + x] = bars[bar];
        }
    }

    for (uint16_t x = 0; x < width; x++)
    {
        pattern_set_pixel(buffer, width, height, x, 0, WHITE);
        pattern_set_pixel(buffer, width, height, x, 1, WHITE);
        pattern_set_pixel(buffer, width, height, x, height - 2, WHITE);
        pattern_set_pixel(buffer, width, height, x, height - 1, WHITE);
    }
    for (uint16_t y = 0; y < height; y++)
    {
        pattern_set_pixel(buffer, width, height, 0, y, WHITE);
        pattern_set_pixel(buffer, width, height, 1, y, WHITE);
        pattern_set_pixel(buffer, width, height, width - 2, y, WHITE);
        pattern_set_pixel(buffer, width, height, width - 1, y, WHITE);
    }

    pattern_draw_line(buffer, width, height, 0, 0, width - 1, height - 1, WHITE);
    pattern_draw_line(buffer, width, height, width - 1, 0, 0, height - 1, WHITE);
    pattern_draw_circle(buffer, width, height, width / 2, height / 2, 60, WHITE);
    pattern_draw_circle(buffer, width, height, width / 2, height / 2, 61, BLACK);
    pattern_draw_circle(buffer, width, height, width / 2, height / 2, 62, WHITE);
}

void plant_display_show_pattern(void)
{
#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
    for (int i = 0; i < 2; i++)
    {
        pattern_fill_buffer((uint16_t *)lcd_buffer[i], lcddev.width, lcddev.height);
        esp_lcd_panel_draw_bitmap(lcddev.lcd_panel_handle,
                                  0,
                                  0,
                                  lcddev.width,
                                  lcddev.height,
                                  lcd_buffer[i]);
    }
    ESP_LOGI(TAG, "LCD full-screen pattern shown");
#else
    ESP_LOGI(TAG, "PATTERN command ignored because LCD display is disabled");
#endif
}

void plant_display_show_color_test(int step)
{
#if CONFIG_PLANT_AI_ENABLE_LCD_DISPLAY
    static const uint16_t colors[] = {
        RED,
        GREEN,
        BLUE,
        WHITE,
        BLACK,
        CYAN,
        MAGENTA,
        YELLOW,
    };
    static const char *names[] = {
        "红色",
        "绿色",
        "蓝色",
        "白色",
        "黑色",
        "青色",
        "紫色",
        "黄色",
    };
    const int count = sizeof(colors) / sizeof(colors[0]);
    const int index = step % count;
    char line[40];
    uint16_t text_color = (colors[index] == BLACK || colors[index] == BLUE) ? WHITE : BLACK;

    lcd_clear(colors[index]);
    snprintf(line, sizeof(line), "屏幕测试 %s", names[index]);
    ui_text(20, 20, 400, 32, 24, line, text_color);
    ESP_LOGI(TAG, "LCD color test step=%d color=%s", step, names[index]);
#else
    ESP_LOGI(TAG, "LCD color test step=%d ignored because LCD display is disabled", step);
#endif
}

bool plant_display_handle_result_command(const char *command)
{
    if (starts_with_ci(command, "PATTERN"))
    {
        plant_display_show_pattern();
        return true;
    }

    if (!starts_with_ci(command, "RESULT "))
    {
        return false;
    }

    char status[24];
    char mood[24];
    char advice[40];
    parse_string_token(command, "status=", status, sizeof(status), "UNKNOWN");
    parse_string_token(command, "mood=", mood, sizeof(mood), "OK");
    parse_string_token(command, "advice=", advice, sizeof(advice), "CHECK");

    int confidence = parse_int_token(command, "conf=", 0);
    int health = parse_int_token(command, "health=", 0);
    bool touched = parse_int_token(command, "touch=", 0) != 0;
    float soil_moisture = parse_float_token(command, "soil=", -1.0f);
    float temperature = parse_float_token(command, "temp=", -100.0f);
    float air_humidity = parse_float_token(command, "hum=", -1.0f);
    float light = parse_float_token(command, "light=", -1.0f);

    plant_display_show_status_full(status,
                                   confidence,
                                   mood,
                                   health,
                                   advice,
                                   touched,
                                   soil_moisture,
                                   temperature,
                                   air_humidity,
                                   light);
    return true;
}
