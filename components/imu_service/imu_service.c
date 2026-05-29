#include "imu_service.h"

#include <math.h>
#include <stddef.h>

#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef M_PI
#undef M_PI
#endif
#include "qmi8658.h"

static const char *TAG = "imu_service";

static qmi8658_dev_t s_dev;
static bool s_initialized;
static bool s_available;
static bool s_calibrated;
static float s_bias_x;
static float s_bias_y;
static float s_smooth_x;
static float s_smooth_y;

static void map_accel_to_screen(const qmi8658_data_t *data, float *x, float *y)
{
    *x = -data->accelY / 1000.0f;
    *y = data->accelX / 1000.0f;
}

esp_err_t imu_service_init(void)
{
    if (s_available) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (bus == NULL) {
        ESP_LOGE(TAG, "BSP I2C handle is not available");
        s_initialized = true;
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = qmi8658_init(&s_dev, bus, QMI8658_ADDRESS_HIGH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "QMI8658 init failed: %s", esp_err_to_name(err));
        s_initialized = true;
        return err;
    }

    (void)qmi8658_set_accel_range(&s_dev, QMI8658_ACCEL_RANGE_8G);
    (void)qmi8658_set_accel_odr(&s_dev, QMI8658_ACCEL_ODR_500HZ);
    (void)qmi8658_set_accel_unit_mps2(&s_dev, false);

    s_available = true;
    s_initialized = true;
    ESP_LOGI(TAG, "QMI8658 ready");
    return ESP_OK;
}

esp_err_t imu_service_calibrate(void)
{
    esp_err_t err = imu_service_init();
    if (err != ESP_OK) {
        return err;
    }

    const int samples = 140;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    int valid_samples = 0;

    ESP_LOGI(TAG, "Starting IMU calibration");

    for (int i = 0; i < samples; ++i) {
        qmi8658_data_t data = {0};
        err = qmi8658_read_sensor_data(&s_dev, &data);
        if (err == ESP_OK) {
            float x = 0.0f;
            float y = 0.0f;
            map_accel_to_screen(&data, &x, &y);
            sum_x += x;
            sum_y += y;
            ++valid_samples;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    if (valid_samples < samples / 2) {
        ESP_LOGE(TAG, "IMU calibration failed: only %d/%d samples", valid_samples, samples);
        return ESP_ERR_INVALID_RESPONSE;
    }

    s_bias_x = sum_x / (float)valid_samples;
    s_bias_y = sum_y / (float)valid_samples;
    s_smooth_x = 0.0f;
    s_smooth_y = 0.0f;
    s_calibrated = true;

    ESP_LOGI(TAG, "IMU calibration done: bias=(%.4f, %.4f)", s_bias_x, s_bias_y);
    return ESP_OK;
}

esp_err_t imu_service_read(imu_service_accel_t *accel)
{
    if (accel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    accel->x = 0.0f;
    accel->y = 0.0f;

    if (!s_available) {
        return ESP_ERR_INVALID_STATE;
    }

    qmi8658_data_t data = {0};
    esp_err_t err = qmi8658_read_sensor_data(&s_dev, &data);
    if (err != ESP_OK) {
        return err;
    }

    float raw_x = 0.0f;
    float raw_y = 0.0f;
    map_accel_to_screen(&data, &raw_x, &raw_y);

    if (s_calibrated) {
        raw_x -= s_bias_x;
        raw_y -= s_bias_y;
    }

    const float alpha = 0.28f;
    const float deadzone = 0.025f;

    s_smooth_x += alpha * (raw_x - s_smooth_x);
    s_smooth_y += alpha * (raw_y - s_smooth_y);

    if (fabsf(s_smooth_x) < deadzone) {
        s_smooth_x = 0.0f;
    }
    if (fabsf(s_smooth_y) < deadzone) {
        s_smooth_y = 0.0f;
    }

    accel->x = s_smooth_x;
    accel->y = s_smooth_y;
    return ESP_OK;
}

bool imu_service_is_available(void)
{
    return s_available;
}

bool imu_service_is_calibrated(void)
{
    return s_calibrated;
}
