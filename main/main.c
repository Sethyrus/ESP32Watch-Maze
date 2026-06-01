#include "esp_err.h"
#include "esp_log.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "imu_service.h"
#include "lvgl.h"
#include "maze_game.h"

static const char *TAG = "ESP32S3Watch";

static void create_calibration_ui(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x070b12), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Laberinto");
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -28);

    lv_obj_t *status = lv_label_create(screen);
    lv_label_set_text(status, "Calibrando IMU...\nmanten el reloj quieto");
    lv_obj_set_style_text_color(status, lv_color_hex(0x9ccbd8), LV_PART_MAIN);
    lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(status, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_align(status, LV_ALIGN_CENTER, 0, 34);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting maze game");

    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to start display");
        return;
    }

    esp_err_t err = bsp_display_brightness_set(80);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set display brightness: %s", esp_err_to_name(err));
    }

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    create_calibration_ui();

    bsp_display_unlock();

    err = imu_service_init();
    if (err == ESP_OK) {
        err = imu_service_calibrate();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "IMU calibration skipped: %s", esp_err_to_name(err));
    }

    if (!bsp_display_lock(0)) {
        ESP_LOGE(TAG, "Failed to lock LVGL");
        return;
    }

    err = maze_game_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start maze game: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Maze game ready");
    }

    bsp_display_unlock();
}
