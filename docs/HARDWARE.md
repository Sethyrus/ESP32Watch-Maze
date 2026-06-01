# Hardware Reference

Referencia tecnica de la placa Waveshare `ESP32-S3-Touch-AMOLED-2.06` para este proyecto.

Fuentes usadas: wiki oficial Waveshare, repo oficial `waveshareteam/ESP32-S3-Touch-AMOLED-2.06`, componentes del ESP Component Registry, BSP `waveshare/esp32_s3_touch_amoled_2_06` v1.0.6, componente `waveshare/qmi8658` y proyecto previo local `MyESP32S3Watch`. Ver enlaces exactos en `docs/SOURCES.md`.

## Resumen De Placa

| Bloque | Modelo / dato | Notas |
| --- | --- | --- |
| MCU | `ESP32-S3R8` | Dual-core LX7, hasta 240 MHz. |
| Wireless | Wi-Fi 2.4 GHz + Bluetooth LE 5 | Antena SMD integrada segun wiki. |
| PSRAM | 8 MB octal | Necesaria para LVGL fluido y buffers de display. |
| Flash | 32 MB segun wiki y esquematico | Chip `GD25Q256EYIGR` = 256 Mbit; los ejemplos ESP-IDF oficiales usan config de 16 MB. |
| Display | AMOLED 2.06", 410 x 502 | QSPI, 16-bit RGB565 en BSP. |
| Touch | `FT3168` | I2C; BSP usa driver compatible `FT5x06`. |
| IMU | `QMI8658` | Acelerometro + giroscopio 6 ejes, I2C. |
| RTC | `PCF85063` | I2C, alimentado por bateria via PMU. |
| PMU | `AXP2101` | Gestion de bateria/carga/voltajes, I2C. |
| Audio out | `ES8311` | Codec para speaker, I2C control + I2S data. |
| Audio in | `ES7210` | ADC para doble microfono, I2C control + I2S data. |
| Storage | microSD | SDMMC 1-bit segun BSP. |
| Bateria | LiPo 3.7 V por conector MX1.25 | Carga/gestion via AXP2101. |
| Expansion | I2C, UART y USB pads | Ver `Interfaces Externas`; validar continuidad antes de disenar accesorios. |

## BSP Oficial

Componente recomendado: `waveshare/esp32_s3_touch_amoled_2_06`.

Version resuelta actual: `1.0.6` en `dependencies.lock`.

Capacidades declaradas por el BSP:

| Capacidad | Valor |
| --- | --- |
| Display | `BSP_CAPS_DISPLAY 1` |
| Touch | `BSP_CAPS_TOUCH 1` |
| Buttons | `BSP_CAPS_BUTTONS 0` |
| Audio | `BSP_CAPS_AUDIO 1` |
| Speaker | `BSP_CAPS_AUDIO_SPEAKER 1` |
| Mic | `BSP_CAPS_AUDIO_MIC 1` |
| SD card | `BSP_CAPS_SDCARD 1` |
| IMU | `BSP_CAPS_IMU 0` |

El BSP cubre display, touch, brillo, I2C, audio y SD con macros de capacidad. Tambien tiene APIs/Kconfig para SPIFFS, aunque no declara `BSP_CAPS_SPIFFS`. No cubre IMU, RTC, PMU ni botones como APIs de alto nivel.

## Defaults BSP Relevantes

Estos valores vienen del `Kconfig` del BSP y conviene tratarlos como contrato practico del baseline:

| Area | Valor | Nota |
| --- | --- | --- |
| I2C port | `CONFIG_BSP_I2C_NUM=1` | Bus compartido por touch, PMU, RTC, IMU y codecs. |
| I2C speed | `CONFIG_BSP_I2C_CLK_SPEED_HZ=400000` | Fast mode por defecto. |
| SPIFFS mount | `/spiffs` | Macro `BSP_SPIFFS_MOUNT_POINT`. |
| SPIFFS partition | `storage` | Debe existir en `partitions.csv`. |
| SPIFFS max files | `2` | Cambiar en `sdkconfig.defaults` si hace falta. |
| SD mount | `/sdcard` | Macro `BSP_SD_MOUNT_POINT`. |
| LVGL buffer height | `40` | `CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT`; reducido desde el default BSP `100` para bajar presion DMA. |
| RGB bounce height | `20` | Kconfig heredado; el panel real va por QSPI/SH8601. |
| I2S port | `CONFIG_BSP_I2S_NUM=1` | Audio speaker/mic. |

## Pines Principales

| Funcion | Pin ESP32-S3 | Fuente |
| --- | --- | --- |
| I2C SDA | GPIO15 | BSP |
| I2C SCL | GPIO14 | BSP |
| LCD CS | GPIO12 | BSP |
| LCD PCLK/SCLK | GPIO11 | BSP |
| LCD DATA0 | GPIO4 | BSP |
| LCD DATA1 | GPIO5 | BSP |
| LCD DATA2 | GPIO6 | BSP |
| LCD DATA3 | GPIO7 | BSP |
| LCD RST | GPIO8 | BSP |
| LCD TE | GPIO13 | Esquematico; no usado directamente por BSP v1.0.6. |
| Touch RST | GPIO9 | BSP |
| Touch INT | GPIO38 | BSP |
| QMI8658 INT1 | GPIO21 | Esquematico; no expuesto por BSP. |
| RTC INT | GPIO39 | Esquematico; no expuesto por BSP. |
| SD D0 | GPIO3 | BSP |
| SD CMD | GPIO1 | BSP |
| SD CLK | GPIO2 | BSP |
| SD CS | GPIO17 | Solo ejemplos Arduino/SPI-style; no usado por BSP SDMMC 1-bit. |
| I2S MCLK | GPIO16 | BSP |
| I2S SCLK/BCLK | GPIO41 | BSP |
| I2S LCLK/WS | GPIO45 | BSP |
| I2S DOUT | GPIO40 | BSP |
| I2S DSIN | GPIO42 | BSP |
| Speaker amp enable | GPIO46 | BSP |
| Motor | GPIO18 | Esquematico; driver/transistor de motor, no expuesto por BSP. |
| SYS_OUT | GPIO10 | Esquematico; ruta de sistema/PMU, no tratar como GPIO libre sin validar. |
| BOOT button | GPIO0 | ESP32-S3 convention / ejemplo oficial |
| PWR button | AXP2101 `PWRON`, wiki `EXIO6` | No es GPIO ESP32 directo documentado por BSP. |

## Pines De Esquematico Sin Wrapper BSP

Estos pines o nets aparecen en el esquematico oficial, pero no tienen API de alto nivel en el BSP `waveshare/esp32_s3_touch_amoled_2_06` v1.0.6. Usarlos requiere validar en hardware real y revisar si la funcion comparte bus, rail o comportamiento de alimentacion.

| Senal / pad | Pin o net | Uso probable | Cuidado |
| --- | --- | --- | --- |
| Motor | `GPIO18` | Motor/vibracion por transistor | Validar corriente, driver y polaridad antes de activar. |
| QMI8658 INT1 | `GPIO21` | Interrupcion IMU | El driver recomendado puede funcionar por polling; no asumir IRQ configurada. |
| RTC INT | `GPIO39` | Alarma/interrupcion PCF85063 | Requiere driver RTC propio y configuracion de GPIO input. |
| LCD TE | `GPIO13` | Tearing-effect del panel | El BSP no lo usa directamente; no activar anti-tearing suponiendo TE conectado al driver. |
| SYS_OUT | `GPIO10` / `SYS_OUT` | Net de sistema/PMU | No tratar como GPIO libre ni como PWR; validar antes de usar. |
| USB pads | `D+/IO20`, `D-/IO19`, `VBUS`, `GND` | USB nativo externo/debug | Compartido con USB del ESP32-S3. |
| I2C pads | `IO15` SDA, `IO14` SCL, `3V3`, `GND` | Expansion I2C | Mismo bus que touch, PMU, RTC, IMU y codecs. |
| UART pads | `RXD/U0RXD`, `TXD/U0TXD`, `3V3`, `GND` | Serial externo/debug | Revisar configuracion de consola antes de reutilizar. |

## I2C Devices

Todos comparten `SDA=GPIO15` y `SCL=GPIO14` en el bus del BSP.

Regla practica: si el display/touch ya arrancaron con `bsp_display_start()`, reutilizar `bsp_i2c_get_handle()` para IMU, RTC, PMU o codecs. No copiar ejemplos aislados que llaman `i2c_new_master_bus()` sobre el mismo puerto salvo que la app no haya inicializado el BSP.

| Dispositivo | Modelo | Direccion | Estado |
| --- | --- | --- | --- |
| Touch | `FT3168` / driver `FT5x06` | `0x38` | BSP lo inicializa con LVGL. |
| IMU | `QMI8658` | `0x6A` o `0x6B` | Usar `QMI8658_ADDRESS_HIGH` (`0x6B`) como en el proyecto previo. |
| RTC | `PCF85063` | tipica `0x51` | No expuesto por BSP. |
| PMU | `AXP2101` | tipica `0x34` | No expuesto por BSP; ejemplo oficial porta XPowersLib. |
| Speaker codec | `ES8311` | `0x30` en `esp_codec_dev` | BSP lo usa en `bsp_audio_codec_speaker_init()`. |
| Mic ADC | `ES7210` | `0x40` 7-bit en esquematico | `esp_codec_dev` usa macro `ES7210_CODEC_DEFAULT_ADDR=0x80`; para I2C scan esperar `0x40`. |

Antes de desarrollar drivers propios para RTC/PMU/audio, conviene hacer un I2C scan en hardware real y anotar los resultados aqui. Interpretar el scan como direcciones 7-bit.

## Display AMOLED

Datos validados:

- Resolucion BSP: `BSP_LCD_H_RES=410`, `BSP_LCD_V_RES=502`.
- Area visible segun dimensiones Waveshare: `33.09 mm x 40.51 mm`.
- Esquinas visibles segun dimensiones Waveshare: `R9.2 mm`, equivalente a unos `114 px` en `410 x 502`.
- Brillo maximo anunciado: 600 nit.
- Bus: QSPI por `SPI2_HOST`.
- Formato BSP: RGB565, `LV_COLOR_FORMAT_RGB565` en LVGL 9.
- Backlight real: no hay pin PWM; el brillo se controla con comando QSPI `0x51` y parametro `0x00..0xFF`.
- Offset de panel en BSP: `esp_lcd_panel_set_gap(panel_handle, 0x16, 0)`. Si se reemplaza el BSP, mantener este ajuste o validar visualmente el origen X.
- LCD TE: el esquematico conecta `LCD_TE` a `GPIO13`, pero el BSP v1.0.6 no lo usa directamente en la ruta LVGL actual.
- API BSP: `bsp_display_start()`, `bsp_display_start_with_config()`, `bsp_display_backlight_on()`, `bsp_display_backlight_off()`, `bsp_display_brightness_set(percent)`.
- `bsp_display_start()` termina llamando `bsp_display_brightness_init()`, que pone brillo al 100%; aplicar el brillo de la app despues de arrancar display.
- La wiki indica que AMOLED/touch soportan funcionamiento a 40-60 grados C; alta temperatura + humedad pueden provocar polarizacion normal.

Gotcha de controlador:

- La wiki y ejemplos Arduino hablan de `CO5300`.
- El BSP oficial usa `waveshare/esp_lcd_sh8601` y comandos SH8601.
- Para ESP-IDF, usar el BSP oficial como fuente de verdad practica.

Gotcha de areas LVGL:

- El driver necesita redondear areas a coordenadas pares/impares para refresco correcto.
- El BSP ya instala `rounder_event_cb`; no duplicarlo en la app salvo que se use display sin BSP.

## Touch FT3168

El touch se inicializa automaticamente al llamar `bsp_display_start()` porque el BSP registra el input device de LVGL.

Datos de wiki:

- Controlador `FT3168` de autocapacitancia.
- Panel de cristal templado superficial + film.
- I2C configurable entre 10 kHz y 400 kHz.

APIs utiles:

```c
lv_display_t *display = bsp_display_start();
lv_indev_t *touch = bsp_display_get_input_dev();
```

No hace falta leer el touch manualmente para widgets LVGL normales. LVGL recibira eventos de puntero por el input device registrado por el BSP.

## IMU QMI8658

Componente recomendado: `waveshare/qmi8658`.

Uso base con BSP:

```c
i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
qmi8658_dev_t dev;
ESP_ERROR_CHECK(qmi8658_init(&dev, bus, QMI8658_ADDRESS_HIGH));
ESP_ERROR_CHECK(qmi8658_set_accel_range(&dev, QMI8658_ACCEL_RANGE_8G));
ESP_ERROR_CHECK(qmi8658_set_accel_odr(&dev, QMI8658_ACCEL_ODR_500HZ));
```

Unidades:

- Por defecto, `qmi8658_read_sensor_data()` devuelve acelerometro en milli-g.
- `1000.0` equivale a 1 g.
- Si se llama `qmi8658_set_accel_unit_mps2(&dev, true)`, devuelve m/s2.

Mapeo de ejes probado para coordenadas de pantalla:

```c
qmi8658_data_t data;
qmi8658_read_sensor_data(&dev, &data);

float screen_x = -data.accelY / 1000.0f;
float screen_y =  data.accelX / 1000.0f;
```

Calibracion recomendada:

- Tomar 100 a 200 muestras al arrancar con el dispositivo quieto.
- Aplicar el mismo mapeo de ejes durante el promedio.
- Restar bias en lecturas futuras.
- Usar deadzone pequena despues de normalizar, por ejemplo `0.015f` a `0.05f` segun la app.

## RTC PCF85063

La wiki identifica `PCF85063` y enlaza datasheet `PCF85063A`.

El RTC esta conectado a la bateria via `AXP2101`, pensado para mantener hora cuando el resto de la placa no esta alimentado normalmente.

Estado actual:

- No esta expuesto por el BSP oficial.
- Los ejemplos Arduino usan `SensorPCF85063`/SensorLib.
- El esquematico conecta `RTC_INT` a `GPIO39`.
- Para ESP-IDF se puede crear un driver minimo por I2C si solo necesitamos fecha/hora.

Usos previstos:

- Reloj persistente sin Wi-Fi.
- Calendario, alarmas y sleep/wake si se anade gestion de PMU/RTC.

## PMU AXP2101

La wiki identifica `AXP2101` para energia, carga y bateria.

Datos que puede reportar segun wiki/ejemplos:

- Temperatura de chip.
- Estado de carga/descarga/standby.
- VBUS presente y estado de cargador.
- Voltaje de bateria.
- Voltaje VBUS.
- Voltaje de sistema.
- Porcentaje estimado de bateria.
- Power key / eventos `PKEY` mediante XPowersLib.

Gotcha:

- El porcentaje de bateria del AXP2101 se basa en voltaje y no es lineal.
- En cargas, descargas y cambios de carga puede fluctuar mucho.
- Para decisiones de autonomia, preferir voltaje y tendencia antes que porcentaje instantaneo.

Estado actual:

- No esta expuesto por el BSP oficial.
- El ejemplo ESP-IDF oficial `01_AXP2101` porta `XPowersLib`.
- El ejemplo ESP-IDF oficial usa `PMU_I2C_SDA=15`, `PMU_I2C_SCL=14`, direccion `0x34` y `PMU_INTERRUPT_PIN=-1`.
- El ejemplo oficial llama `PMU.disableTSPinMeasure()` porque la placa no tiene deteccion de temperatura de bateria en TS; dejar TS activo puede causar carga anomala.
- El esquematico conecta el boton `PWR` al `PWRON` del AXP2101; la wiki describe lectura logica por `EXIO6`.
- Si se integra, hacerlo como componente separado o driver minimo propio.

Configuracion de carga vista en el ejemplo oficial:

- Precharge: `XPOWERS_AXP2101_PRECHARGE_50MA`.
- Corriente constante: `XPOWERS_AXP2101_CHG_CUR_400MA`.
- Terminacion: `XPOWERS_AXP2101_CHG_ITERM_25MA`.
- Tension objetivo: `XPOWERS_AXP2101_CHG_VOL_4V2`.

Mapa de rails visto en el esquematico:

| Rail AXP2101 | Net / uso en placa |
| --- | --- |
| `DCDC1` | `VCC3V3` |
| `DCDC2` | `0.9 V` |
| `DCDC3` | `1.2 V` |
| `DCDC4` | `1.8 V` |
| `DCDC5` | `NC` |
| `RTCLDO` | `VCC-RTC` |
| `ALDO1` | `VL1_3.3V` |
| `ALDO2` | `VL2_3.3V` |
| `ALDO3` | `VCC3V` |
| `ALDO4` | `VL3_1.8V` |
| `BLDO1` | Sin uso claro en el texto extraido; revisar esquematico antes de tocar. |
| `BLDO2` | `VL_2.8V` |
| `CPUSLDO` | `VCL_1.2V` |

No copiar el ejemplo `01_AXP2101` como politica de energia final sin revisar estos rails. El ejemplo desactiva varios canales para demostrar la PMU y despues reactiva un subconjunto; una app real puede necesitar mantener activos display, touch, codecs, RTC o sensores.

## Bateria

La placa trae conector/header LiPo `3.7 V MX1.25` para carga/descarga via `AXP2101`.

Datos de FAQ/wiki:

| Dato | Valor |
| --- | --- |
| Bateria recomendada | `4*27*28`, `400 mAh` |
| Autonomia full brightness normal | aproximadamente 1 h |
| Autonomia con pantalla apagada | aproximadamente 3-4 h |
| Autonomia low-power maxima anunciada | aproximadamente 6 h |

Las cifras son orientativas de Waveshare; validar con mediciones reales de la app final.

## Audio

La placa integra speaker/amplificador y dos microfonos SMD. En ESP-IDF el BSP expone el speaker mediante `ES8311` y la captura de microfono mediante `ES7210`.

APIs BSP:

```c
ESP_ERROR_CHECK(bsp_audio_init(NULL));
esp_codec_dev_handle_t speaker = bsp_audio_codec_speaker_init();
esp_codec_dev_handle_t mic = bsp_audio_codec_microphone_init();
```

Pines I2S relevantes:

| Senal | Pin |
| --- | --- |
| MCLK | GPIO16 |
| BCLK/SCLK | GPIO41 |
| WS/LCLK | GPIO45 |
| DOUT | GPIO40 |
| DIN/DSIN | GPIO42 |
| Amp enable / PA_CTRL | GPIO46 |

El ejemplo oficial `05_Spec_Analyzer` usa el BSP para inicializar speaker y microfono, configura captura a 16 kHz, 16-bit, 2 canales y procesa FFT. El ejemplo `06_videoplayer` reutiliza audio para reproducir AVI desde SD. Este repo no activa todavia Wi-Fi/BLE/audio AI.

## microSD

APIs BSP:

```c
ESP_ERROR_CHECK(bsp_sdcard_mount());
FILE *f = fopen(BSP_SD_MOUNT_POINT "/file.txt", "r");
```

Pines:

| Senal | Pin |
| --- | --- |
| D0 | GPIO3 |
| CMD | GPIO1 |
| CLK | GPIO2 |

El BSP configura SDMMC 1-bit. No hay pin de card-detect declarado.

Gotcha Arduino vs ESP-IDF:

- La wiki/ejemplo Arduino etiqueta TF como SPI con `CS GPIO17`, `DI/MOSI GPIO1`, `DO/MISO GPIO3`, `SCK GPIO2`.
- En ESP-IDF usar el BSP SDMMC 1-bit salvo que se porte codigo Arduino deliberadamente.
- `GPIO17` no aparece en el BSP para SD; verificar en hardware/esquematico antes de usarlo.

## Botones Y Recuperacion

- El BSP declara `BSP_CAPS_BUTTONS 0`.
- `BOOT` esta en `GPIO0`, nivel bajo cuando se pulsa.
- `BOOT` se puede usar en runtime para click, doble click, multi-click y long press si la app implementa debounce.
- Mantener `BOOT` mientras se alimenta la placa fuerza modo descarga si el firmware se queda colgado.
- `PWR` apaga si se mantiene pulsado unos 6 s en estado encendido.
- `PWR` en apagado enciende la placa con una pulsacion.
- En runtime, la wiki dice que `PWR` se lee por `EXIO6`, nivel alto al pulsar; el esquematico muestra el boton en la ruta `PWRON` del AXP2101.
- No asumir `GPIO10` como PWR: el esquematico lo etiqueta como `SYS_OUT/GPIO10`, ruta de sistema/PMU que requiere validacion propia.
- Las pulsaciones largas de `PWR` para la app deben durar menos de 6 s para no apagar la placa.

## Interfaces Externas

La wiki indica que la placa saca pads/puertos externos. El esquematico y el silk/diagrama extraido muestran:

| Interfaz | Pads / pines | Uso previsto | Cuidado |
| --- | --- | --- | --- |
| I2C | `IO15` SDA, `IO14` SCL, `3V3`, `GND` | Expansion de sensores/perifericos | Es el bus compartido del BSP. |
| UART | `RXD/U0RXD`, `TXD/U0TXD`, `3V3`, `GND` | Conexion externa/debug | Revisar consola y uso de UART0 antes de reutilizar. |
| USB pad | `D+/IO20`, `D-/IO19`, `VBUS`, `GND` | Conexion/debug USB nativo | Compartido con USB del ESP32-S3. |

Aunque estos pads ya salen del esquematico, validar continuidad y funcion en la placa real antes de disenar accesorios o asumir tolerancia electrica.

## USB, Flash Y Recovery

- El puerto Type-C de flashing/debug sale directamente del USB nativo del ESP32-S3.
- La placa tiene circuito de descarga automatica, asi que normalmente `idf.py flash` no requiere pulsar `BOOT`.
- Si un firmware rompe USB, deja la CPU colgada o el auto-download no entra, mantener `BOOT` al alimentar/resetear fuerza modo descarga.
