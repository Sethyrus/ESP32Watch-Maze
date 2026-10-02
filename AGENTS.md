# AGENTS.md

## Project Shape
- ESP-IDF C firmware `ESP32WatchMaze`: IMU-controlled maze game. Entry point is `app_main()` in `main/main.c`; game in `components/maze_game/`.
- Target hardware is Waveshare `ESP32-S3-Touch-AMOLED-2.06` (ESP32-S3R8, AMOLED 410x502 QSPI, FT3168 touch, QMI8658 IMU, AXP2101 PMU).
- Stack: `ESP-IDF 5.5.4 + LVGL 9 + waveshare/esp32_s3_touch_amoled_2_06` BSP + `watch_board` from ESP32Watch-core. Do not migrate to ESP-IDF 6.x or ESP-Brookesia unless explicitly requested.
- Display (`watch_display.h`, never `bsp_display_start()`), IMU (`imu_service.h`) and BOOT/PWR buttons (`watch_buttons.h`) come from `watch_board` (https://github.com/Sethyrus/ESP32Watch-core), pinned by tag in `main/idf_component.yml` (v0.5.1). Fix hardware-level bugs there, not with local copies.
- Game design and decisions: `docs/MAZE_DESIGN.md`. Hardware docs: ESP32Watch-core `docs/`.
- Durable config lives in `sdkconfig.defaults`, `partitions.csv`, component manifests and `dependencies.lock`. `sdkconfig`, `build/` and `managed_components/` are generated.

## Commands
- Source ESP-IDF: `source "$HOME/.espressif/tools/activate_idf_v5.5.4.sh"` (EIM install; otherwise core `docs/SETUP.md`).
- First setup: `idf.py set-target esp32s3`. Verification: `idf.py build`.
- Flash and monitor: `idf.py -p <PORT> flash monitor` (macOS port looks like `/dev/tty.usbmodem*` and changes with the USB socket; `idf.py` auto-detects it if `-p` is omitted).
- No test, lint or format targets are configured; do not invent them.

## Critical Hardware Notes
- LVGL is not thread-safe: wrap `lv_*` calls outside LVGL callbacks/tasks with `bsp_display_lock()` / `bsp_display_unlock()`.
- Reuse `bsp_i2c_get_handle()` for devices on the shared I2C bus; never create a second master bus on the same port.
- QMI8658 accel is milli-g; screen axes are `screen_x = -accelY / 1000`, `screen_y = accelX / 1000` (already done by `imu_service`).
- BOOT is GPIO0, active low. PWR is not a GPIO (AXP2101 `PWRON`); holding it ~6 s powers off the board.
- Button convention: BOOT = accept/primary action, PWR short press = back/menu. See "Convencion De Botones" in core `docs/ARCHITECTURE.md`.
- Never write AXP2101 power or protection registers; read core `docs/PMU_SAFETY.md` before any PMU access. Core fixes reach this app by bumping the `watch_board` tag (core `AGENTS.md`, "Alineacion De Repos").
- Launcher mode (core `watch_launcher.h`): `app_main` calls `watch_launcher_boot_once()` first. On the mode menu (app root, `at_mode_menu`), `Salir` and PWR call `watch_launcher_exit()`, only when `watch_launcher_is_available()`. `partitions.csv` is a copy of the shared table in ESP32Watch-Launcher; do not change it here alone.
