#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef struct {
    float x;
    float y;
} imu_service_accel_t;

esp_err_t imu_service_init(void);
esp_err_t imu_service_calibrate(void);
esp_err_t imu_service_read(imu_service_accel_t *accel);
bool imu_service_is_available(void);
bool imu_service_is_calibrated(void);
