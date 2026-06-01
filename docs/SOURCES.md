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

## Referencias Doom

| Recurso | URL | Uso |
| --- | --- | --- |
| Espressif `esp32-doom` | https://github.com/espressif/esp32-doom | PoC PrBoom para ESP32 original; referencia de PSRAM, WAD en particion raw, framebuffer 320x240, DMA y stubs de audio. No copiar literalmente. |
| `doom1-cut.wad` de Espressif | https://dl.espressif.com/dl/doom1-cut.wad | WAD recortado usado por `esp32-doom`; referencia historica. No commitear ni usar sin revisar licencia/alcance. |
| DoomGeneric | https://github.com/ozkl/doomgeneric | Base importada en `components/doomgeneric/vendor` desde commit `dcb7a8dbc7a16ce3dda29382ac9aae9d77d21284`. API minima `DG_*`. Licencia GPL-2.0. |
| ESP32-S3 DoomGeneric port | https://github.com/Komedenden/esp32-s3-doom-port | Referencia reciente ESP-IDF para ESP32-S3 N16R8, framebuffer en PSRAM, SD, chunks DMA y 15 FPS reportados. Display/input no coinciden con esta placa. |
| Doom source release | https://github.com/id-Software/DOOM | Fuente historica GPL-2.0 del motor Doom. |
| Freedoom | https://freedoom.github.io/ | Assets libres compatibles con motores Doom; alternativa a WAD comercial/shareware para pruebas si tamano y compatibilidad encajan. |
| Freedoom repo | https://github.com/freedoom/freedoom | Fuentes/licencia de assets Freedoom; licencia BSD-like segun `COPYING.adoc`. |
| Porting Doom, Part 1 | https://actuallytaylor.com/blog/portingdoomp1 | Explicacion practica de DoomGeneric; confirma que `DG_DrawFrame()` e input son la mayor parte del trabajo de port. |
| Guide to source ports in classic Doom | https://old.reddit.com/r/Doom/comments/r3ziow/guide_to_source_ports_in_classic_doom/ | Comparativa informal de source ports; util para descartar GZDoom/Boom avanzado en microcontrolador MVP. |
| My easy to build Doom port | https://mattiasgustavsson.com/my-easy-to-build-doom-port/ | Referencia de filosofia minimalista: pocos cambios, build simple y fuente facil de auditar. |
| Doomworld basic source port thread | https://www.doomworld.com/forum/topic/92065-where-to-start-on-making-a-basic-simple-doom-source-port/ | Consejos de comunidad: no empezar de cero; compilar una base existente y modificar incrementalmente. |
| DoomWiki DoomGeneric | https://doomwiki.org/wiki/DoomGeneric | Contexto del port DoomGeneric y su objetivo de simplificar integraciones. |
| DoomWiki source port | https://doomwiki.org/wiki/Source_port | Taxonomia de ports y alcance de compatibilidad. |
| DoomWiki Doom source code | https://doomwiki.org/wiki/Doom_source_code | Estructura historica del codigo y subsistemas `I_*`, `W_*`, `R_*`, `P_*`, `Z_*`. |
| DoomWiki WAD | https://doomwiki.org/wiki/WAD | Formato de assets WAD y contexto de IWAD/PWAD. |
| DoomWiki static limits | https://doomwiki.org/wiki/Static_limits | Limites vanilla relevantes para evitar WADs/mods complejos en MVP. |
| DoomWiki Chocolate Doom | https://doomwiki.org/wiki/Chocolate_Doom | Referencia de port conservador/vanilla; util como filosofia, no como base inicial. |
| DoomWiki PrBoom+ | https://doomwiki.org/wiki/PrBoom%2B | Contexto de PrBoom/PrBoom+ frente a `esp32-doom`; mayor superficie que DoomGeneric. |
| DoomWiki Crispy Doom | https://doomwiki.org/wiki/Crispy_Doom | Port conservador extendido; referencia, no objetivo de MVP. |
| DoomWiki GZDoom | https://doomwiki.org/wiki/GZDoom | Port moderno orientado a PC/modding; fuera de alcance para ESP32-S3 MVP. |
| Retro-Go | https://github.com/ducalex/retro-go | Referencia de firmware de emulacion/juegos en ESP32; demasiado amplio para el primer port standalone. |

## Informacion Ya Sintetizada

La wiki, el repo oficial, el esquematico local, el BSP resuelto y el proyecto previo fueron usados para extraer datos a estos documentos:

| Documento | Contenido extraido |
| --- | --- |
| `docs/HARDWARE.md` | Piezas, pines, buses, sensores, rails PMU, bateria, botones, SD, audio, BSP. |
| `docs/GOTCHAS.md` | Recovery, flash, PWR, bateria, SD GPIO17, ES7210 scan, LVGL, BSP caveats. |
| `docs/SETUP.md` | Setup ESP-IDF, particiones, dependencias y ejemplos oficiales. |
| `docs/BRINGUP.md` | Checklist de validacion hardware. |
| `docs/MAZE_DESIGN.md` | Diseno del juego de laberinto, generacion, colisiones, IMU y rendimiento. |
| `docs/DOOM_PORT.md` | Investigacion inicial de Doom, repos evaluados, arquitectura, estado de implementacion, fases, riesgos y decisiones. |

## Notas De Fiabilidad De Fuentes

- La wiki mezcla contenido especifico de esta placa con texto generico de tutorial. No arrastrar menciones a un supuesto boton `Reset` sin validarlo: la placa documentada aqui se trata como `BOOT` + `PWR`.
- Algunos consejos de troubleshooting de Waveshare para demos sugieren borrar `dependencies.lock`. En este repo el lock es deliberado y solo debe cambiar si se aceptan nuevas versiones de componentes.
- Los ejemplos ESP-IDF oficiales son utiles para patrones aislados, pero si una app ya arranco el BSP hay que adaptar la inicializacion de buses, especialmente I2C.
