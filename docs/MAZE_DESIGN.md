# Maze Game Design

Documento de diseno para la rama `app/maze`. Define el juego de laberinto con bola controlada por IMU, el modo normal inicial y las decisiones que deben permitir anadir modos mas grandes sin reescribir el nucleo.

## Objetivo

Construir un juego tipo laberinto fisico:

- Al iniciar una partida se genera un laberinto nuevo.
- La bola empieza cerca de una esquina logica.
- El agujero aparece en la esquina opuesta.
- El par de esquinas se elige aleatoriamente en cada partida, siempre opuestas.
- El laberinto tiene solucion unica y muchos caminos equivocados.
- La bola se mueve inclinando el reloj con el QMI8658.
- Al ganar se muestra una pantalla de `Victoria` con botones `Jugar de nuevo` y `Menu`.

El primer modo implementado sera `Normal`. El menu debe estar preparado para listar varios modos aunque inicialmente solo exista este.

## Hardware Y Stack

- Placa: Waveshare `ESP32-S3-Touch-AMOLED-2.06`.
- Display: AMOLED `410 x 502`, QSPI, BSP Waveshare.
- IMU: `QMI8658` por I2C, usando `bsp_i2c_get_handle()` despues de arrancar el BSP.
- Framework: `ESP-IDF 5.5.4 + LVGL 9 + waveshare/esp32_s3_touch_amoled_2_06`.
- No usar ESP-Brookesia en esta rama salvo cambio explicito de direccion.

Reglas criticas:

- Mantener `main/main.c` como bootstrap pequeno.
- Encapsular juego e IMU en componentes o modulos separados.
- Toda llamada `lv_*` fuera del contexto de LVGL debe ir protegida con `bsp_display_lock()` y `bsp_display_unlock()`.
- No crear un segundo bus I2C; reutilizar el handle del BSP.

## Decisiones Cerradas

| Area | Decision |
| --- | --- |
| Modo inicial | `Normal` |
| Dificultades | `Facil`, `Normal`, `Dificil` |
| Start/goal | Esquinas opuestas aleatorias |
| Calibracion IMU | Una vez al arrancar la app, reutilizada durante partidas |
| Recalibracion manual | Boton pequeno en pantalla inicial |
| Victoria | Overlay/pantalla con `Jugar de nuevo` y `Menu` |
| Laberinto normal | Cabe completo en pantalla |
| Laberinto futuro grande | Mundo mayor que pantalla con camara centrada en bola |
| Solucion | Laberinto perfecto, sin loops en modo normal |

## Flujo De Pantallas

```text
App start
  -> Inicializar display/BSP
  -> Inicializar IMU
  -> Calibrar IMU una vez
  -> Pantalla inicial
       - Modo Normal
       - Calibrar
  -> Modo Normal
       - Facil
       - Normal
       - Dificil
  -> Partida
       - generar laberinto
       - arrancar fisica
  -> Victoria
       - Jugar de nuevo
       - Menu
```

Notas UX:

- El boton `Calibrar` debe ser accesible pero secundario.
- Durante calibracion mostrar feedback breve: `Calibrando... manten quieto`.
- Si falla IMU, mostrar aviso claro y permitir volver al menu.
- En la pantalla de victoria, `Jugar de nuevo` reutiliza mismo modo y dificultad; `Menu` vuelve a seleccion.

## Geometria De Pantalla

La pantalla no es un rectangulo visible completo: tiene esquinas redondeadas. El laberinto no debe generar caminos, bola ni agujero en zonas que puedan quedar ocultas.

Fuente oficial de dimensiones Waveshare:

- Area visible aproximada: `33.09 mm x 40.51 mm`.
- Radio de esquina: `R9.2 mm`.
- Resolucion BSP: `410 x 502`.

Conversion recomendada:

```text
px_per_mm_x = 410 / 33.09 = 12.39 px/mm
px_per_mm_y = 502 / 40.51 = 12.39 px/mm
corner_radius_px = 9.2 * min(px_per_mm_x, px_per_mm_y) = 114 px aprox.
```

El ejemplo oficial `04_Immersive_block` usa `SCREEN_WIDTH_MM=33.09`, `SCREEN_HEIGHT_MM=41.51`, `CORNER_RADIUS_MM=9.2`. La altura `41.51` no cuadra con la relacion `410x502`; la imagen oficial muestra `40.51`, que si cuadra. Para este juego usar `40.51` salvo que una medicion fisica indique otra cosa.

Modelo de rectangulo redondeado:

```text
screen_w = 410
screen_h = 502
corner_r = 114
safe_margin = ball_radius + wall_thickness + visual_margin
```

Una posicion de centro de bola `(x, y)` es jugable si queda dentro del rectangulo redondeado reducido por `safe_margin`.

Funcion conceptual:

```text
inside_rounded_rect(x, y, w, h, r, margin):
  x = clamp test point against [r, w-r]
  y = clamp test point against [r, h-r]
  if point is outside a corner square:
    require distance to corner center <= r - margin
  otherwise valid
```

Para celdas del laberinto, una celda es valida solo si la bola cabe dentro de esa celda y sus puntos de paso principales no invaden el recorte. Al principio usar una prueba conservadora:

- Probar el centro de celda.
- Probar cuatro puntos interiores desplazados por `ball_radius + wall_thickness`.
- Invalidar la celda si cualquiera de esos puntos queda fuera del area segura.

Esto reduce riesgo de que un pasillo parezca valido pero la bola o el agujero queden parcialmente invisibles.

## Dificultad

La dificultad inicial se controla principalmente por numero de celdas. El tablero se adapta al tamano de pantalla y deriva de la matriz.

| Dificultad | Columnas | Filas | Intencion |
| --- | ---: | ---: | --- |
| Facil | 9 | 11 | Celdas amplias, bola grande, lectura clara |
| Normal | 12 | 15 | Equilibrio entre precision y exploracion |
| Dificil | 15 | 18 | Pasillos estrechos y mas precision |

Valores derivados:

```text
cell_w = viewport_w / cols
cell_h = viewport_h / rows
cell_size = min(cell_w, cell_h)
wall_thickness = clamp(round(cell_size * 0.08), 2, 5)
ball_radius = cell_size * ball_radius_factor
hole_radius = ball_radius * hole_radius_factor
```

Factores iniciales:

| Dificultad | `ball_radius_factor` | `branch_select_percent` | `min_solution_ratio` |
| --- | ---: | ---: | ---: |
| Facil | `0.30` | `25` | `0.35` |
| Normal | `0.28` | `40` | `0.45` |
| Dificil | `0.25` | `55` | `0.55` |

`branch_select_percent` controla el algoritmo Growing Tree. Valores altos eligen mas celdas aleatorias del frente activo y tienden a generar mas bifurcaciones; valores bajos se comportan mas como DFS y producen pasillos mas largos.

## Modelo De Datos

El nucleo no debe depender de LVGL. Debe trabajar en coordenadas de mundo y con una matriz de celdas.

```c
typedef enum {
    MAZE_DIR_TOP = 0,
    MAZE_DIR_RIGHT,
    MAZE_DIR_BOTTOM,
    MAZE_DIR_LEFT,
} maze_dir_t;

typedef struct {
    uint8_t walls;       // bits TOP/RIGHT/BOTTOM/LEFT
    bool valid;          // dentro del area visible segura
    bool visited;        // solo durante generacion
} maze_cell_t;

typedef struct {
    int cols;
    int rows;
    maze_cell_t *cells;
    int start_col;
    int start_row;
    int goal_col;
    int goal_row;
} maze_board_t;
```

Acceso recomendado:

```text
cell(board, row, col) = cells[row * cols + col]
```

No usar arrays gigantes en stack. Para el modo normal el tablero es pequeno, pero el modo grande futuro puede crecer. Reservar la matriz en heap/PSRAM o como buffer propiedad del componente.

## Generacion Del Laberinto

Usar un laberinto perfecto sobre celdas validas. Un laberinto perfecto es un arbol: hay exactamente un camino entre cualquier par de celdas conectadas. Esto cumple `unico camino valido hasta la salida` y genera ramas falsas sin necesidad de disenar caminos a mano.

Algoritmo recomendado: Growing Tree con seleccion parametrizable.

Pasos:

1. Calcular geometria de pantalla, celdas, bola, pared y mascara segura.
2. Inicializar todas las celdas con cuatro paredes.
3. Marcar `valid=false` en celdas que invaden esquinas redondeadas.
4. Elegir una esquina aleatoria como origen logico.
5. Elegir la esquina opuesta como destino logico.
6. Buscar la celda valida mas cercana al origen logico.
7. Buscar la celda valida mas cercana al destino logico.
8. Ejecutar Growing Tree desde start solo por celdas validas.
9. Verificar que goal es alcanzable y que el tablero cumple minimos.
10. Regenerar con otro seed si no cumple, hasta un limite de intentos.

Seleccion de esquinas:

| Start | Goal |
| --- | --- |
| Top-left | Bottom-right |
| Top-right | Bottom-left |
| Bottom-left | Top-right |
| Bottom-right | Top-left |

Busqueda de celda segura de esquina:

- Iterar por distancia Manhattan desde la esquina logica.
- Elegir la primera celda `valid` que permita colocar bola/agujero centrados.
- Evitar celdas invalidas por radio de pantalla.
- Si varias empatan, elegir una aleatoria para variedad.

Verificaciones despues de generar:

| Check | Motivo |
| --- | --- |
| `start` y `goal` validos | Evita partidas imposibles |
| `goal` alcanzable | La mascara no debe aislar destino |
| camino solucion >= minimo | Evita laberintos triviales |
| dead ends >= minimo | Garantiza caminos equivocados |
| componente valida unica | Evita islas por mascara |

Si se usa Growing Tree desde `start` y todas las celdas validas forman una componente conectada, el resultado ya sera perfecto. La validacion sigue siendo util para logs y seguridad.

No anadir paredes extra despues de generar si pueden romper la propiedad de solucion unica. No eliminar paredes extra en modo normal, porque crearia loops y varias soluciones.

## Fisica E IMU

El QMI8658 devuelve aceleracion en milli-g por defecto. Usar el mapeo validado para pantalla:

```c
screen_x = -data.accelY / 1000.0f;
screen_y =  data.accelX / 1000.0f;
```

Inicializacion recomendada:

- Arrancar display con `bsp_display_start()`.
- Obtener I2C con `bsp_i2c_get_handle()`.
- Inicializar QMI8658 en `QMI8658_ADDRESS_HIGH`.
- Usar ODR alto, por ejemplo `500Hz`, aunque la app consuma snapshot a `50Hz`.
- Empezar con rango `8G` por coherencia con bring-up; considerar `2G` si se valida que mejora precision sin saturar.

Calibracion:

- Hacer una calibracion al arrancar la app.
- Tomar 100 a 200 muestras con el reloj quieto.
- Aplicar el mismo mapeo de ejes antes de promediar.
- Guardar `bias_x` y `bias_y` en RAM.
- El boton `Calibrar` de menu repite el proceso.
- Persistir en NVS solo si mas adelante se comprueba que ayuda; no hacerlo en la primera version.

Filtro inicial:

```text
smooth += alpha * (raw - smooth)
alpha = 0.20 a 0.35
deadzone = 0.015g a 0.05g
```

Arquitectura de lectura:

- Opcion preferida: `imu_service` con tarea FreeRTOS que publica el ultimo vector filtrado.
- La tarea no debe llamar a LVGL.
- El timer de juego lee el ultimo vector y actualiza fisica/render en contexto LVGL.
- Si se simplifica leyendo IMU en `lv_timer`, mantener la lectura corta y moverla a servicio si causa jitter.

## Modelo De Fisica

La bola se modela como circulo en coordenadas de mundo.

Estado:

```c
typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
} maze_ball_t;
```

Reglas:

- `(x, y)` es el centro de la bola.
- La bola empieza centrada en `start`.
- El agujero esta centrado en `goal`.
- La aceleracion IMU modifica velocidad.
- La friccion se aplica por `dt`.
- La velocidad maxima evita tunneling y mantiene control.

Constantes iniciales:

```text
physics_hz = 50
base_dt = 0.020 s
accel_factor = 500 a 900 px/s^2 por g
friction = 0.90 a 0.94 por frame base
max_velocity = cell_size * 7 a 10 por segundo
bounce = 0.0 a 0.25
```

Para laberinto, conviene poco rebote. Un rebote alto hace que la bola se sienta inestable y puede atascarse en esquinas. La primera version deberia usar cancelacion o rebote muy bajo.

## Colisiones

Las colisiones no deben depender de objetos LVGL. LVGL solo representa visualmente el resultado.

Objetivos:

- La bola no atraviesa paredes aunque haya frames lentos.
- La bola se desliza por paredes cuando choca en diagonal.
- No se queda bloqueada en esquinas por correcciones contradictorias.
- Las celdas invalidas por esquina redondeada se tratan como solidas.

Algoritmo recomendado:

1. Calcular `dt` real desde tick anterior.
2. Dividir el movimiento en substeps si el desplazamiento previsto es grande.
3. En cada substep, resolver eje X y eje Y por separado.
4. Al cruzar una frontera de celda, comprobar pared y validez de celda vecina.
5. Si hay bloqueo, clamp al limite exacto menos/mas radio.
6. Reducir o invertir la velocidad del eje bloqueado.

Substeps:

```text
max_step = max(2 px, ball_radius * 0.45)
steps = ceil(max(abs(dx), abs(dy)) / max_step)
steps = clamp(steps, 1, 5)
```

Resolucion de eje X:

```text
target_x = x + dx
current row = floor(y / cell_h)
current col = floor(x / cell_w)

if moving right:
  while target_x + radius crosses right boundary:
    if right wall exists or neighbor invalid:
      target_x = boundary_x - radius
      vx = -vx * bounce
      break
    else col++

if moving left:
  while target_x - radius crosses left boundary:
    if left wall exists or neighbor invalid:
      target_x = boundary_x + radius
      vx = -vx * bounce
      break
    else col--
```

Resolver Y igual con `top/bottom`. Despues de X, recalcular columna antes de resolver Y para permitir deslizamiento natural.

Casos especiales:

- Si la bola queda en una celda invalida por bug o corrupcion, recolocarla en la ultima posicion valida y poner velocidad a cero.
- Si `dt > 100 ms`, clamplear a `20 ms` o pausar un frame para evitar saltos tras bloqueos.
- Evitar paredes mas finas que `2 px`.

## Victoria Y Agujero

El agujero se representa como circulo visual en el centro de `goal`.

Parametros iniciales:

```text
hole_radius = ball_radius * 1.10
win_radius = ball_radius * 0.65
```

Condicion de victoria:

```text
distance(ball_center, hole_center) <= win_radius
```

Opcional para ajustar sensacion:

- Exigir que la bola este dentro de la celda `goal`.
- Exigir velocidad baja para simular que cae en el agujero.
- Animar la bola reduciendo escala/opacidad antes de mostrar victoria.

Primera version: victoria inmediata al cumplir distancia. Es simple y facil de validar.

## Render LVGL

### Modo Normal

El tablero cabe completo en pantalla. Renderizar paredes una vez por partida y actualizar solo la bola por frame.

Objetos:

- `root`: pantalla o contenedor principal.
- `board_layer`: fondo y paredes.
- `hole`: circulo en goal.
- `ball`: circulo de la bola.
- `overlay`: victoria o mensajes.

Render de paredes:

- Crear rectangulos LVGL para paredes.
- Dibujar solo paredes `top` y `left` por celda, mas borde `bottom/right` del tablero.
- Rellenar celdas invalidas como bloques solidos o dejarlas fuera con fondo oscuro; para debug conviene rellenarlas.
- Crear objetos al iniciar partida, no por frame.
- No cambiar estilos por frame.

Orden visual:

```text
background
invalid/corner mask debug blocks
walls
hole
ball
overlay
```

Estilo inicial sugerido:

- Fondo: oscuro o madera sobria.
- Paredes: alto contraste, `2..5 px`.
- Bola: color vivo con sombra ligera.
- Agujero: negro con aro sutil.

### Modo Grande Futuro

El modo grande debe reutilizar `maze_board`, `maze_generator` y `maze_physics`. Solo cambia el viewport/camara.

Modelo:

- El mundo puede ser `50x50` o mayor.
- La bola tiene posicion en mundo.
- La camara sigue la bola y se clampa a limites del mundo.
- En pantalla la bola queda cerca del centro mientras el laberinto se desplaza.
- El agujero aparece en una esquina aleatoria del mundo, sin indicar cual.

Render eficiente:

- Mantener un pool de objetos LVGL para paredes visibles.
- Redibujar pool solo cuando la camara cruza a otra celda.
- Entre cruces, mover un contenedor padre con offset sub-celda.
- Ocultar agujero si esta fuera de viewport.

Este patron ya fue probado parcialmente en el proyecto de referencia y es mejor que recrear cientos de objetos por frame.

## Arquitectura Propuesta

Estructura inicial recomendada:

```text
components/
|-- imu_service/
|   |-- include/imu_service.h
|   |-- imu_service.c
|   |-- CMakeLists.txt
|   `-- idf_component.yml
|-- maze_game/
|   |-- include/maze_game.h
|   |-- maze_app.c
|   |-- maze_generator.c
|   |-- maze_physics.c
|   |-- maze_render_lvgl.c
|   |-- CMakeLists.txt
|   `-- idf_component.yml
main/
`-- main.c
```

Responsabilidades:

| Modulo | Responsabilidad |
| --- | --- |
| `main.c` | Arrancar BSP/display, brillo y app |
| `imu_service` | QMI8658, calibracion, filtro, snapshot de aceleracion |
| `maze_generator` | Crear tablero perfecto y validarlo |
| `maze_physics` | Bola, integracion, colisiones, victoria |
| `maze_render_lvgl` | Objetos LVGL y actualizaciones visuales |
| `maze_app` | Estado de pantallas, menu, dificultad, timers |

Si se quiere avanzar mas rapido, `imu_service` puede empezar dentro de `maze_game`, pero el limite debe estar claro para extraerlo despues.

## Estado De App

```c
typedef enum {
    MAZE_APP_STATE_BOOT,
    MAZE_APP_STATE_CALIBRATING,
    MAZE_APP_STATE_MODE_MENU,
    MAZE_APP_STATE_DIFFICULTY_MENU,
    MAZE_APP_STATE_PLAYING,
    MAZE_APP_STATE_VICTORY,
    MAZE_APP_STATE_ERROR,
} maze_app_state_t;
```

Transiciones principales:

| Desde | Evento | Hacia |
| --- | --- | --- |
| `BOOT` | IMU ok | `CALIBRATING` |
| `CALIBRATING` | ok | `MODE_MENU` |
| `MODE_MENU` | Normal | `DIFFICULTY_MENU` |
| `MODE_MENU` | Calibrar | `CALIBRATING` |
| `DIFFICULTY_MENU` | dificultad | `PLAYING` |
| `PLAYING` | win | `VICTORY` |
| `VICTORY` | Jugar de nuevo | `PLAYING` |
| `VICTORY` | Menu | `MODE_MENU` |

## Rendimiento

Presupuesto inicial:

| Area | Objetivo |
| --- | --- |
| Fisica | 50 Hz |
| Actualizacion bola | 50 Hz maximo |
| Redibujado paredes normal | Solo al generar partida |
| Redibujado paredes grande | Solo al cambiar celda de camara |
| Logs | Nunca por frame |

Reglas:

- No crear ni destruir objetos LVGL en el loop de fisica.
- No usar `lv_obj_clean()` por frame.
- Precalcular geometria de celdas.
- Guardar paredes como bitmask.
- Usar `esp_random()` para seed y loguear seed/config para reproducir fallos.
- Medir tiempos con `esp_timer_get_time()` durante desarrollo si hay jitter.

## Validacion

Validacion de generacion:

- Loguear dificultad, seed, filas, columnas, start, goal.
- Contar celdas validas.
- BFS desde start para confirmar goal alcanzable.
- Confirmar que todas las celdas validas pertenecen a la misma componente.
- Contar longitud solucion.
- Contar dead ends.
- Regenerar si no cumple minimos.

Validacion de colisiones:

- Inclinar contra cada pared exterior y comprobar que no atraviesa.
- Entrar en esquinas internas en diagonal y comprobar que desliza o se detiene sin bloquearse.
- Forzar `dt` alto temporalmente y comprobar que no atraviesa paredes.
- Probar celdas cerca de esquinas redondeadas.

Validacion de IMU:

- Tilt derecha mueve bola a derecha.
- Tilt izquierda mueve bola a izquierda.
- Tilt arriba mueve bola arriba.
- Tilt abajo mueve bola abajo.
- Recalibrar desde menu reduce drift cuando el reloj esta quieto.

Validacion visual:

- Start y goal siempre visibles.
- Bola completa siempre visible.
- Agujero completo siempre visible.
- Ningun pasillo jugable queda cortado por esquinas fisicas.
- La pantalla de victoria se puede tocar sin que la fisica siga moviendo bola.

Verificacion minima antes de dar por cerrada una iteracion:

```sh
source "/Users/alex/.espressif/v5.5.4/esp-idf/export.sh"
idf.py build
```

La validacion final de controles, esquinas y rendimiento requiere hardware real.

## Riesgos Y Mitigaciones

| Riesgo | Mitigacion |
| --- | --- |
| Bola atraviesa paredes | Substeps, resolver X/Y por separado, limitar velocidad |
| Bola se atasca en esquinas | Rebote bajo/cero, clamp exacto, conservar ultima posicion valida |
| Caminos en zonas no visibles | Mascara con `R9.2 mm` y margen por bola/pared |
| Laberinto trivial | Minimo de longitud y dead ends, regeneracion |
| Jitter por I2C/LVGL | `imu_service` separado, no I2C pesado en render |
| Demasiados objetos LVGL | Crear una vez, top/left walls, pool en modo grande |
| Drift IMU | Calibracion inicial y boton manual |

## Fases De Implementacion

1. Crear `imu_service` minimo con init, calibracion y lectura filtrada.
2. Crear `maze_generator` sin LVGL y validar logs de laberinto.
3. Crear pantalla de menus y seleccion de dificultad.
4. Renderizar laberinto normal estatico.
5. Anadir fisica y colisiones con bola.
6. Anadir victoria, replay y vuelta a menu.
7. Ajustar constantes en hardware.
8. Documentar resultados en `docs/BRINGUP.md` o notas de app.

## Criterios Para Futuras Extensiones

El modo grande no debe forkear la logica de laberinto. Debe reutilizar:

- `maze_board_t`.
- Generacion perfecta.
- Mascara de celdas validas, adaptada a viewport/camara.
- Fisica y colisiones en coordenadas de mundo.
- IMU/calibracion.

Solo deberia cambiar:

- Configuracion de tamano de mundo.
- Posicion inicial en centro o zona segura cercana.
- Colocacion oculta del agujero en una esquina del mundo.
- Render con camara y pool de paredes visibles.
- UX de busqueda/exploracion.
