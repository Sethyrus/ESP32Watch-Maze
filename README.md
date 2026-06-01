# ESP32S3Watch

Firmware ESP-IDF para la placa Waveshare `ESP32-S3-Touch-AMOLED-2.06`.

La base actual usa `ESP-IDF 5.5.4`, `LVGL` y el BSP oficial de Waveshare. No usa ESP-Brookesia por defecto: el objetivo inicial es tener una base simple, estable y directa para validar pantalla, touch, brillo y perifericos antes de construir una capa de apps mas compleja.

## Hardware Objetivo

- Placa: Waveshare `ESP32-S3-Touch-AMOLED-2.06`.
- MCU: `ESP32-S3R8`, dual-core LX7 hasta 240 MHz.
- PSRAM: 8 MB octal.
- Flash: el esquematico monta `GD25Q256EYIGR` de 32 MB; el baseline usa config de 16 MB hasta validar la placa real.
- Pantalla: AMOLED 2.06", 410 x 502, QSPI.
- Touch: `FT3168` por I2C, driver BSP `esp_lcd_touch_ft5x06`.
- IMU: `QMI8658` por I2C.
- RTC: `PCF85063` por I2C.
- PMU/bateria: `AXP2101` por I2C.
- Audio: codec/speaker `ES8311`, doble microfono via ADC `ES7210`, I2S.
- Storage: microSD por SDMMC 1-bit.

Ver detalles en `docs/HARDWARE.md`.

## Quick Start

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/tty.usbmodem21301 flash monitor
```

Si el shell no encuentra `idf.py`, falta ejecutar el `source` anterior o el entorno de ESP-IDF no esta instalado completo.

## Estructura

- `main/main.c`: entrada `app_main()` y demo minima LVGL+BSP.
- `main/idf_component.yml`: dependencias del componente principal.
- `sdkconfig.defaults`: configuracion durable del proyecto.
- `partitions.csv`: tabla de particiones durable.
- `docs/HARDWARE.md`: sensores, pines, buses y APIs.
- `docs/SETUP.md`: entorno ESP-IDF y flujo de build/flash.
- `docs/GOTCHAS.md`: problemas conocidos y decisiones criticas.
- `docs/ARCHITECTURE.md`: arquitectura base y criterio Brookesia vs LVGL+BSP.
- `docs/DOOM_PORT.md`: investigacion, viabilidad y plan del port de Doom en la rama `app/doom`.
- `docs/MAZE_DESIGN.md`: diseno del juego de laberinto en la rama `app/maze`.
- `docs/BRINGUP.md`: checklist de validacion hardware antes de construir apps.
- `docs/SOURCES.md`: fuentes oficiales, datasheets, componentes y ejemplos usados.
- `AGENTS.md`: instrucciones resumidas para agentes.

## Estado Actual

La app arranca el BSP, inicializa LVGL, enciende la pantalla y muestra una pantalla de prueba. Es intencionadamente pequena para validar toolchain, dependencias, PSRAM, QSPI display y LVGL antes de anadir mas perifericos o una arquitectura de apps.
