#include "plant_engine.h"

#include "esp_log.h"
#include "plant_display.h"
#include "plant_sensor.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "plant_engine";

static plant_engine_vision_t latest_vision = PLANT_ENGINE_VISION_HEALTHY;
static int latest_confidence_percent = 0;

#define SOIL_DRY_THRESHOLD 30.0f
#define SOIL_WET_THRESHOLD 80.0f
#define LIGHT_DARK_THRESHOLD 25.0f
#define LIGHT_BRIGHT_THRESHOLD 1200.0f
#define TEMP_HOT_THRESHOLD 32.0f
#define TEMP_COLD_THRESHOLD 14.0f
#define AIR_DRY_THRESHOLD 35.0f
#define AIR_HUMID_THRESHOLD 75.0f
#define LIGHT_RELAXED_THRESHOLD 850.0f
#define AIR_COMFY_HUMID_THRESHOLD 60.0f
#define TEMP_WARM_THRESHOLD 28.0f

#define ENV_THIRSTY (1u << 0)
#define ENV_OVERWATERED (1u << 1)
#define ENV_TOO_DARK (1u << 2)
#define ENV_TOO_BRIGHT (1u << 3)
#define ENV_TOO_HOT (1u << 4)
#define ENV_TOO_COLD (1u << 5)
#define ENV_AIR_TOO_DRY (1u << 6)
#define ENV_AIR_TOO_HUMID (1u << 7)

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

static bool equals_ci(const char *left, const char *right)
{
    while (*left != '\0' && *right != '\0')
    {
        if (!char_equal_ci(*left, *right))
        {
            return false;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
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

static float parse_float_token(const char *command, const char *key, float fallback)
{
    const char *value = find_key_ci(command, key);
    return value ? strtof(value, NULL) : fallback;
}

static int parse_int_token(const char *command, const char *key, int fallback)
{
    const char *value = find_key_ci(command, key);
    return value ? atoi(value) : fallback;
}

static bool parse_bool_token(const char *command, const char *key, bool fallback)
{
    const char *value = find_key_ci(command, key);
    if (!value)
    {
        return fallback;
    }
    return value[0] == '1' || char_equal_ci(value[0], 'y') || char_equal_ci(value[0], 't');
}

static void parse_string_token(const char *command,
                               const char *key,
                               char *dst,
                               size_t dst_size,
                               const char *fallback)
{
    const char *value = find_key_ci(command, key);
    if (!value)
    {
        snprintf(dst, dst_size, "%s", fallback);
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

static plant_engine_vision_t parse_vision_value(const char *value)
{
    if (equals_ci(value, "HEALTHY") || equals_ci(value, "HEALTHY_NO_DISEASE"))
    {
        return PLANT_ENGINE_VISION_HEALTHY;
    }
    if (equals_ci(value, "LOCAL_ISSUE") || equals_ci(value, "OVERALL_HEALTHY_ABNORMAL_LEAF"))
    {
        return PLANT_ENGINE_VISION_LOCAL_ISSUE;
    }
    if (equals_ci(value, "SEVERE") || equals_ci(value, "WIDESPREAD_SEVERE_ABNORMAL"))
    {
        return PLANT_ENGINE_VISION_SEVERE;
    }
    return PLANT_ENGINE_VISION_UNKNOWN;
}

static plant_engine_vision_t parse_vision_token(const char *command, plant_engine_vision_t fallback)
{
    char value[48];
    parse_string_token(command, "vision=", value, sizeof(value), "");
    if (value[0] == '\0')
    {
        parse_string_token(command, "status=", value, sizeof(value), "");
    }
    if (value[0] == '\0')
    {
        return fallback;
    }
    return parse_vision_value(value);
}

static unsigned infer_env_flags(const plant_engine_input_t *input)
{
    unsigned flags = 0;
    if (input->soil_moisture < SOIL_DRY_THRESHOLD)
    {
        flags |= ENV_THIRSTY;
    }
    if (input->soil_moisture > SOIL_WET_THRESHOLD)
    {
        flags |= ENV_OVERWATERED;
    }
    if (input->light < LIGHT_DARK_THRESHOLD)
    {
        flags |= ENV_TOO_DARK;
    }
    if (input->light > LIGHT_BRIGHT_THRESHOLD)
    {
        flags |= ENV_TOO_BRIGHT;
    }
    if (input->temperature > TEMP_HOT_THRESHOLD)
    {
        flags |= ENV_TOO_HOT;
    }
    if (input->temperature < TEMP_COLD_THRESHOLD)
    {
        flags |= ENV_TOO_COLD;
    }
    if (input->air_humidity < AIR_DRY_THRESHOLD)
    {
        flags |= ENV_AIR_TOO_DRY;
    }
    if (input->air_humidity > AIR_HUMID_THRESHOLD)
    {
        flags |= ENV_AIR_TOO_HUMID;
    }
    return flags;
}

static int calculate_comfort_score(unsigned flags)
{
    int score = 100;
    if (flags & ENV_THIRSTY)
    {
        score -= 25;
    }
    if (flags & ENV_OVERWATERED)
    {
        score -= 30;
    }
    if (flags & ENV_TOO_DARK)
    {
        score -= 25;
    }
    if (flags & ENV_TOO_BRIGHT)
    {
        score -= 25;
    }
    if (flags & ENV_TOO_HOT)
    {
        score -= 25;
    }
    if (flags & ENV_TOO_COLD)
    {
        score -= 25;
    }
    if (flags & ENV_AIR_TOO_DRY)
    {
        score -= 25;
    }
    if (flags & ENV_AIR_TOO_HUMID)
    {
        score -= 25;
    }
    if (score < 0)
    {
        return 0;
    }
    if (score > 100)
    {
        return 100;
    }
    return score;
}

static int calculate_health_score(int comfort_score, plant_engine_vision_t vision)
{
    int score = comfort_score;
    if (vision == PLANT_ENGINE_VISION_SEVERE)
    {
        score -= 40;
    }
    if (score < 0)
    {
        return 0;
    }
    if (score > 100)
    {
        return 100;
    }
    return score;
}

static const char *status_for_vision(plant_engine_vision_t vision)
{
    if (vision == PLANT_ENGINE_VISION_HEALTHY)
    {
        return "HEALTHY";
    }
    if (vision == PLANT_ENGINE_VISION_LOCAL_ISSUE)
    {
        return "LOCAL_ISSUE";
    }
    if (vision == PLANT_ENGINE_VISION_SEVERE)
    {
        return "SEVERE";
    }
    return "UNKNOWN";
}

static const char *infer_mood(const plant_engine_input_t *input,
                              unsigned flags,
                              int comfort_score,
                              int health_score)
{
    if (input->touched)
    {
        if (input->vision == PLANT_ENGINE_VISION_SEVERE && health_score < 70)
        {
            return "FADING";
        }
        if (comfort_score >= 80)
        {
            return "CLINGY";
        }
        return "COMFORTED";
    }

    if (input->vision == PLANT_ENGINE_VISION_SEVERE)
    {
        if (health_score >= 70)
        {
            return "WILTED";
        }
        if (health_score >= 40)
        {
            return "SAD";
        }
        if (health_score >= 25)
        {
            return "UNWELL";
        }
        return "HELP";
    }

    if (comfort_score >= 80)
    {
        if ((flags & ENV_TOO_BRIGHT) || input->light >= LIGHT_RELAXED_THRESHOLD)
        {
            return "LEISURELY";
        }
        if ((flags & ENV_AIR_TOO_HUMID) || input->air_humidity >= AIR_COMFY_HUMID_THRESHOLD)
        {
            return "HYDRATED";
        }
        if ((flags & ENV_TOO_HOT) || input->temperature >= TEMP_WARM_THRESHOLD)
        {
            return "WARM";
        }
        return "HAPPY";
    }

    if (comfort_score >= 60)
    {
        if (flags & ENV_TOO_BRIGHT)
        {
            return "DIZZY";
        }
        if (flags & ENV_TOO_DARK)
        {
            return "GLOOMY";
        }
        if (flags & ENV_TOO_HOT)
        {
            return "FLUSHED";
        }
        if (flags & ENV_TOO_COLD)
        {
            return "SHIVERING";
        }
        if ((flags & ENV_OVERWATERED) || (flags & ENV_AIR_TOO_HUMID))
        {
            return "STICKY";
        }
        if ((flags & ENV_THIRSTY) || (flags & ENV_AIR_TOO_DRY))
        {
            return "DRY";
        }
        return comfort_score >= 70 ? "UPSET" : "WILTED";
    }

    if (comfort_score >= 40)
    {
        return "SAD";
    }
    if (comfort_score >= 25)
    {
        return "UNWELL";
    }
    return "HELP";
}

static const char *infer_advice(const plant_engine_input_t *input,
                                unsigned flags,
                                int comfort_score)
{
    if (input->touched && input->vision != PLANT_ENGINE_VISION_SEVERE && comfort_score >= 60)
    {
        return "PETTED";
    }
    if (input->touched && comfort_score < 60)
    {
        return "COMFORT";
    }
    if (input->vision == PLANT_ENGINE_VISION_SEVERE)
    {
        return "ALERT";
    }
    if (flags & ENV_THIRSTY)
    {
        return "WATER";
    }
    if (flags & ENV_OVERWATERED)
    {
        return "DRAIN";
    }
    if (flags & ENV_TOO_DARK)
    {
        return "LIGHT";
    }
    if (flags & ENV_TOO_BRIGHT)
    {
        return "SHADE";
    }
    if (flags & ENV_TOO_HOT)
    {
        return "COOL";
    }
    if (flags & ENV_TOO_COLD)
    {
        return "WARM";
    }
    if (flags & ENV_AIR_TOO_DRY)
    {
        return "HUMIDIFY";
    }
    if (flags & ENV_AIR_TOO_HUMID)
    {
        return "VENTILATE";
    }
    if (input->vision == PLANT_ENGINE_VISION_LOCAL_ISSUE)
    {
        return "WATCH";
    }
    return "KEEP";
}

void plant_engine_analyze(const plant_engine_input_t *input, plant_engine_result_t *result)
{
    unsigned flags = infer_env_flags(input);
    int comfort_score = calculate_comfort_score(flags);
    int health_score = calculate_health_score(comfort_score, input->vision);

    snprintf(result->status, sizeof(result->status), "%s", status_for_vision(input->vision));
    result->confidence_percent = input->confidence_percent;
    snprintf(result->mood, sizeof(result->mood), "%s", infer_mood(input, flags, comfort_score, health_score));
    result->comfort_score = comfort_score;
    result->health_score = health_score;
    snprintf(result->advice, sizeof(result->advice), "%s", infer_advice(input, flags, comfort_score));
}

bool plant_engine_handle_command(const char *command)
{
    bool is_analyze = starts_with_ci(command, "ANALYZE");
    bool is_vision = starts_with_ci(command, "VISION");
    bool is_touch = starts_with_ci(command, "TOUCH");
    if (!is_analyze && !is_vision && !is_touch)
    {
        return false;
    }

    float soil_moisture = 45.0f;
    float temperature = 26.0f;
    float air_humidity = 55.0f;
    float light = 500.0f;
    plant_sensor_reading_t sensor_reading;
    if (plant_sensor_get_latest(&sensor_reading)) {
        if (sensor_reading.has_soil_moisture) {
            soil_moisture = sensor_reading.soil_moisture;
        }
        if (sensor_reading.has_air) {
            temperature = sensor_reading.temperature;
            air_humidity = sensor_reading.air_humidity;
        }
        if (sensor_reading.has_light) {
            light = sensor_reading.light;
        }
    }

    plant_engine_input_t input = {
        .soil_moisture = parse_float_token(command, "soil=", soil_moisture),
        .temperature = parse_float_token(command, "temp=", temperature),
        .air_humidity = parse_float_token(command, "hum=", air_humidity),
        .light = parse_float_token(command, "light=", light),
        .touched = is_touch || parse_bool_token(command, "touch=", false),
        .vision = parse_vision_token(command, latest_vision),
        .confidence_percent = parse_int_token(command, "conf=", latest_confidence_percent),
    };
    plant_engine_result_t result;
    plant_engine_analyze(&input, &result);

    latest_vision = input.vision;
    latest_confidence_percent = input.confidence_percent;

    ESP_LOGI(TAG,
             "ENGINE status=%s conf=%d mood=%s comfort=%d health=%d advice=%s touch=%d",
             result.status,
             result.confidence_percent,
             result.mood,
             result.comfort_score,
             result.health_score,
             result.advice,
             input.touched ? 1 : 0);

    plant_display_show_status_full(result.status,
                                   result.confidence_percent,
                                   result.mood,
                                   result.health_score,
                                   result.advice,
                                   input.touched,
                                   input.soil_moisture,
                                   input.temperature,
                                   input.air_humidity,
                                   input.light);
    return true;
}
