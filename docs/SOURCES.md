# Sources

Fuentes primarias y referencias usadas para este repo. Prioridad recomendada para agentes: codigo BSP/resuelto > manifests/lock > ejemplos oficiales ESP-IDF > wiki > ejemplos Arduino > proyectos de terceros.

## Oficiales Waveshare

| Recurso | URL | Uso |
| --- | --- | --- |
| Wiki del producto | https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06 | Hardware, FAQ, demos, recursos y datasheets. |
| Repo oficial | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06 | Ejemplos Arduino/ESP-IDF, firmware factory, material y esquematico. |
| Producto | https://www.waveshare.com/esp32-s3-touch-amoled-2.06.htm | Pagina comercial y datos generales. |
| Soporte displays ESP32 | https://github.com/waveshareteam/ESP32-display-support/tree/master | Issues/soporte recomendado por FAQ. |

## Componentes ESP-IDF

| Componente | URL | Version actual |
| --- | --- | --- |
| BSP board | https://components.espressif.com/components/waveshare/esp32_s3_touch_amoled_2_06 | `1.0.6` en `dependencies.lock`. |
| SH8601 panel | https://components.espressif.com/components/waveshare/esp_lcd_sh8601 | `1.0.2` resuelto por BSP. |
| QMI8658 IMU | https://components.espressif.com/components/waveshare/qmi8658 | Recomendado al integrar IMU. |
| LVGL | https://components.espressif.com/components/lvgl/lvgl | `9.3.0` en este repo. |
| esp_lvgl_port | https://components.espressif.com/components/espressif/esp_lvgl_port | `2.8.0~1` resuelto por BSP. |
| esp_codec_dev | https://components.espressif.com/components/espressif/esp_codec_dev | Audio speaker/mic via BSP. |

El BSP v1.0.6 apunta en Registry a este snapshot de `Waveshare-ESP32-components`:

```text
https://github.com/waveshareteam/Waveshare-ESP32-components/tree/781c68164378de68654a7bcd9a301dfded067a96/bsp/esp32_s3_touch_amoled_2_06
```

El componente QMI8658 apunta en Registry a:

```text
https://github.com/waveshareteam/Waveshare-ESP32-components/tree/07b4d537ddfb4b273a645f86d761c5bff5fbdf33/sensor/qmi8658
```

## Datasheets Y PDFs

| Documento | URL |
| --- | --- |
| Esquematico `ESP32-S3-Touch-AMOLED-2.06` | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06/ESP32-S3-Touch-AMOLED-2.06.pdf |
| Esquematico en repo oficial | https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-2.06/blob/main/Schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf |
| Dimensional drawing | https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.06/Esp32-s3-touch-amoled-2_06_dimensions.pdf |
| ESP32-S3 datasheet | https://files.waveshare.com/wiki/common/Esp32-s3_datasheet_en.pdf |
| ESP32-S3 TRM | https://files.waveshare.com/wiki/common/Esp32-s3_technical_reference_manual_en.pdf |
| QMI8658C datasheet | https://files.waveshare.com/wiki/common/QMI8658C.pdf |
| PCF85063A datasheet | https://files.waveshare.com/wiki/common/PCF85063A.pdf |
| AXP2101 datasheet | https://files.waveshare.com/wiki/common/X-power-AXP2101_SWcharge_V1.0.pdf |
| ES8311 datasheet | https://files.waveshare.com/wiki/common/ES8311.DS.pdf |
| ES8311 user guide | https://files.waveshare.com/wiki/common/ES8311.user.Guide.pdf |
| FT3168 datasheet | https://files.waveshare.com/wiki/common/FT3168.pdf |
| ES7210 datasheet | https://files.waveshare.com/wiki/common/ES7210-datasheet.pdf |

## Ejemplos Oficiales ESP-IDF

Ruta oficial: `examples/ESP-IDF-v5.4.2` en el repo Waveshare. Este repo usa ESP-IDF `5.5.4`, asi que los ejemplos son referencia, no baseline copiable sin revisar.

| Ejemplo | Uso |
| --- | --- |
| `01_AXP2101` | PMU AXP2101, XPowersLib, bateria, VBUS, PKEY, carga. |
| `02_lvgl_demo_v9` | LVGL v9 + BSP, particiones 8M factory + 7M SPIFFS. |
| `03_esp-brookesia` | Demo Brookesia phone framework. |
| `04_Immersive_block` | QMI8658 + LVGL + fisicas de inclinacion. |
| `05_Spec_Analyzer` | Captura de microfonos/audio y visualizacion de espectro. |
| `06_videoplayer` | AVI desde TF card con video MJPEG y audio PCM. |

## Ejemplos Arduino

Usarlos solo como referencia secundaria para comportamiento de hardware, no como fuente principal de pines ESP-IDF.

| Ejemplo | Dato util |
| --- | --- |
| `03_LVGL_PCF85063_simpleTime` | Uso funcional de RTC PCF85063. |
| `04_LVGL_QMI8658_ui` | Lecturas IMU y graficas LVGL. |
| `05_LVGL_AXP2101_ADC_Data` | Datos PMU, PKEY, bateria y VBUS. |
| `07_LVGL_SD_Test` | SD estilo Arduino con `CS GPIO17`; no equivale al BSP ESP-IDF. |
| `08_ES8311` | Reproduccion basica con ES8311. |

## Referencias Locales

| Ruta | Uso |
| --- | --- |
| `/Users/alex/Proyectos/Alex/ESP32/ESP32-S3-Touch-AMOLED-2.06/Schematic/ESP32-S3-Touch-AMOLED-2.06-Schematic-V1.0.pdf` | Esquematico V1.0 usado para pines no BSP, rails PMU, flash y direcciones I2C. |
| `/Users/alex/Proyectos/Alex/ESP32/ESP32-S3-Touch-AMOLED-2.06/examples/ESP-IDF-v5.4.2/01_AXP2101/` | Referencia local PMU AXP2101, XPowersLib, PKEY, carga y TS pin. |
| `/Users/alex/Proyectos/Alex/ESP32/ESP32-S3-Touch-AMOLED-2.06/examples/ESP-IDF-v5.4.2/04_Immersive_block/` | Referencia local IMU QMI8658, BOOT GPIO0 y fisicas LVGL. |
| `/Users/alex/Proyectos/Alex/ESP32/ESP32-S3-Touch-AMOLED-2.06/examples/ESP-IDF-v5.4.2/05_Spec_Analyzer/` | Referencia local audio capture ES7210 + FFT sobre BSP. |
| `/Users/alex/Proyectos/Alex/ESP32/ESP32-S3-Touch-AMOLED-2.06/examples/ESP-IDF-v5.4.2/06_videoplayer/` | Referencia local SD + AVI + audio playback. |
| `/Users/alex/Proyectos/Alex/ESP32/MyESP32S3Watch/LEARNINGS_AND_GUIDE.md` | Lecciones Brookesia, QMI8658, LVGL performance y registro estatico. |
| `/Users/alex/Proyectos/Alex/ESP32/MyESP32S3Watch/components/apps/app_gyro_games/` | Referencia local de juego con IMU, laberinto, render LVGL y modo grande. |
| `/Users/alex/Proyectos/Alex/ESP32/MyESP32S3Watch/POKETCH_DESIGN.md` | Diseno Poketch y decision de motor LVGL ligero. |
| `managed_components/waveshare__esp32_s3_touch_amoled_2_06/` | Codigo BSP resuelto localmente tras build. Generado, no editar. |
| `managed_components/waveshare__esp_lcd_sh8601/` | Driver panel SH8601 resuelto; confirma QSPI, comando `0x51` y restricciones de area. |
| `dependencies.lock` | Versiones exactas resueltas por ESP Component Manager. |

## Informacion Ya Sintetizada

La wiki, el repo oficial, el esquematico local, el BSP resuelto y el proyecto previo fueron usados para extraer datos a estos documentos:

| Documento | Contenido extraido |
| --- | --- |
| `docs/HARDWARE.md` | Piezas, pines, buses, sensores, rails PMU, bateria, botones, SD, audio, BSP. |
| `docs/GOTCHAS.md` | Recovery, flash, PWR, bateria, SD GPIO17, ES7210 scan, LVGL, BSP caveats. |
| `docs/SETUP.md` | Setup ESP-IDF, particiones, dependencias y ejemplos oficiales. |
| `docs/BRINGUP.md` | Checklist de validacion hardware. |
| `docs/MAZE_DESIGN.md` | Diseno del juego de laberinto, generacion, colisiones, IMU y rendimiento. |

## Notas De Fiabilidad De Fuentes

- La wiki mezcla contenido especifico de esta placa con texto generico de tutorial. No arrastrar menciones a un supuesto boton `Reset` sin validarlo: la placa documentada aqui se trata como `BOOT` + `PWR`.
- Algunos consejos de troubleshooting de Waveshare para demos sugieren borrar `dependencies.lock`. En este repo el lock es deliberado y solo debe cambiar si se aceptan nuevas versiones de componentes.
- Los ejemplos ESP-IDF oficiales son utiles para patrones aislados, pero si una app ya arranco el BSP hay que adaptar la inicializacion de buses, especialmente I2C.
