# ESP32Watch-Maze

Juego de laberinto para la Waveshare **ESP32-S3-Touch-AMOLED-2.06**: una bola que se mueve inclinando el reloj (IMU QMI8658) hasta el agujero.

- Laberintos generados en cada partida, con solucion unica, adaptados a las esquinas redondeadas de la pantalla.
- **Modo Normal** (el laberinto cabe en pantalla) con dificultad Facil / Normal / Dificil.
- **Modo Aventura**: laberinto mayor que la pantalla con camara que sigue a la bola y pista hacia el agujero.
- Calibracion de la IMU al arrancar y desde el menu.

## Controles

| Control | Accion |
| --- | --- |
| Inclinar el reloj | Mover la bola |
| Tactil | Menus, `Jugar de nuevo`, `Salir` |
| `PWR` (pulsacion corta) | Pausa; en la pausa, volver al juego; en "Salir?", cancelar |
| `BOOT` (pulsacion corta) | En la pausa, volver al juego; en "Salir?", confirmar salida |

Arrancado desde el launcher, el menu principal muestra ademas `Salir` (volver al launcher), y `PWR` en ese menu hace lo mismo.

Mantener el reloj quieto durante la calibracion inicial.

## Compilar y flashear

Requiere `ESP-IDF 5.5.4` (ver [SETUP](https://github.com/Sethyrus/ESP32Watch-core/blob/main/docs/SETUP.md)).

```sh
source "$HOME/.espressif/v5.5.4/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor   # p. ej. /dev/tty.usbmodem1101; sin -p lo autodetecta
```

Para tenerla junto a las demas apps y elegirla desde un menu de arranque, grabarla con [ESP32Watch-Launcher](https://github.com/Sethyrus/ESP32Watch-Launcher) (`./flash_all.sh`). `partitions.csv` es la tabla comun del launcher; en standalone la app ocupa `factory`.

El puerto puede variar. Si el firmware bloquea el USB, mantener `BOOT` al conectar para entrar en modo descarga.

## Estructura

| Ruta | Contenido |
| --- | --- |
| `main/main.c` | Arranque: BSP, display, calibracion IMU y juego. |
| `components/maze_game/` | Generacion, fisica, render LVGL y menus. |
| `docs/MAZE_DESIGN.md` | Diseno del juego y decisiones. |

La IMU y los botones `BOOT`/`PWR` vienen del componente `watch_board` de [ESP32Watch-core](https://github.com/Sethyrus/ESP32Watch-core), declarado en `main/idf_component.yml` y fijado en `dependencies.lock`. La documentacion de hardware de la placa esta alli.

## Licencia

MIT. Ver [LICENSE](LICENSE).
