#include "maze_game.h"

#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "imu_service.h"
#include "lvgl.h"

static const char *TAG = "maze_game";

#define MAZE_MAX_ROWS 60
#define MAZE_MAX_COLS 48
#define MAZE_MAX_CELLS (MAZE_MAX_ROWS * MAZE_MAX_COLS)
#define MAZE_ADVENTURE_VISIBLE_COLS 12
#define MAZE_ADVENTURE_VISIBLE_ROWS 15
#define MAZE_WALL_POOL_OBJECTS 384
#define MAZE_BOOT_GPIO GPIO_NUM_0
#define MAZE_BOOT_POLL_MS 25
#define MAZE_BOOT_DEBOUNCE_MS 40
#define MAZE_BOOT_SHORT_PRESS_MAX_MS 700

#define MAZE_WALL_TOP    (1U << 0)
#define MAZE_WALL_RIGHT  (1U << 1)
#define MAZE_WALL_BOTTOM (1U << 2)
#define MAZE_WALL_LEFT   (1U << 3)
#define MAZE_ALL_WALLS (MAZE_WALL_TOP | MAZE_WALL_RIGHT | MAZE_WALL_BOTTOM | MAZE_WALL_LEFT)

typedef enum {
    MAZE_DIFFICULTY_EASY = 0,
    MAZE_DIFFICULTY_NORMAL,
    MAZE_DIFFICULTY_HARD,
} maze_difficulty_t;

typedef enum {
    MAZE_MODE_NORMAL = 0,
    MAZE_MODE_ADVENTURE,
} maze_mode_t;

typedef enum {
    MAZE_CORNER_TOP_LEFT = 0,
    MAZE_CORNER_TOP_RIGHT,
    MAZE_CORNER_BOTTOM_LEFT,
    MAZE_CORNER_BOTTOM_RIGHT,
} maze_corner_t;

typedef struct {
    const char *name;
    int cols;
    int rows;
    float ball_radius_factor;
    int branch_select_percent;
    float min_solution_ratio;
} maze_difficulty_config_t;

typedef struct {
    uint8_t walls;
    bool valid;
    bool visited;
} maze_cell_t;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float radius;
} maze_ball_t;

typedef struct {
    float x0;
    float y0;
    float x1;
    float y1;
} maze_rect_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *status_label;
    lv_obj_t *ball_obj;
    lv_obj_t *hole_obj;
    lv_obj_t *overlay;
    lv_obj_t *pause_overlay;
    lv_obj_t *pause_card;
    lv_obj_t *pause_confirm_card;
    lv_obj_t *board_obj;
    lv_obj_t *world_layer;
    lv_obj_t *floor_obj;
    lv_obj_t *hint_checkbox;
    lv_obj_t *hint_box;
    lv_obj_t *hint_arrow;
    lv_timer_t *timer;
    lv_timer_t *boot_timer;
    int screen_w;
    int screen_h;
    int visible_cols;
    int visible_rows;
    int cols;
    int rows;
    int start_col;
    int start_row;
    int goal_col;
    int goal_row;
    float cell_w;
    float cell_h;
    float cell_size;
    float world_w;
    float world_h;
    float wall_thickness;
    float ball_radius;
    float hole_radius;
    float camera_x;
    float camera_y;
    maze_ball_t ball;
    maze_mode_t mode;
    maze_difficulty_t difficulty;
    uint32_t seed;
    uint32_t rng;
    uint32_t last_tick_ms;
    int render_start_row;
    int render_start_col;
    int render_end_row;
    int render_end_col;
    int active_wall_count;
    int wall_pool_index;
    int last_world_offset_x;
    int last_world_offset_y;
    bool playing;
    bool paused;
    bool hint_enabled;
    bool hole_hidden;
    bool boot_raw_pressed;
    bool boot_stable_pressed;
    bool boot_pressed_event_active;
    uint32_t boot_last_change_ms;
    uint32_t boot_press_start_ms;
    maze_cell_t cells[MAZE_MAX_ROWS][MAZE_MAX_COLS];
    int16_t work_rows[MAZE_MAX_CELLS];
    int16_t work_cols[MAZE_MAX_CELLS];
    int16_t work_queue[MAZE_MAX_CELLS];
    int16_t work_distance[MAZE_MAX_CELLS];
    lv_obj_t *wall_pool[MAZE_WALL_POOL_OBJECTS];
    maze_rect_t wall_world_rects[MAZE_WALL_POOL_OBJECTS];
    lv_point_precise_t hint_points[5];
} maze_app_t;

static const maze_difficulty_config_t NORMAL_DIFFICULTIES[] = {
    [MAZE_DIFFICULTY_EASY] = {
        .name = "Facil",
        .cols = 9,
        .rows = 11,
        .ball_radius_factor = 0.30f,
        .branch_select_percent = 25,
        .min_solution_ratio = 0.35f,
    },
    [MAZE_DIFFICULTY_NORMAL] = {
        .name = "Normal",
        .cols = 12,
        .rows = 15,
        .ball_radius_factor = 0.28f,
        .branch_select_percent = 40,
        .min_solution_ratio = 0.45f,
    },
    [MAZE_DIFFICULTY_HARD] = {
        .name = "Dificil",
        .cols = 15,
        .rows = 18,
        .ball_radius_factor = 0.25f,
        .branch_select_percent = 55,
        .min_solution_ratio = 0.55f,
    },
};

static const maze_difficulty_config_t ADVENTURE_DIFFICULTIES[] = {
    [MAZE_DIFFICULTY_EASY] = {
        .name = "Facil",
        .cols = 24,
        .rows = 30,
        .ball_radius_factor = 0.28f,
        .branch_select_percent = 15,
        .min_solution_ratio = 0.70f,
    },
    [MAZE_DIFFICULTY_NORMAL] = {
        .name = "Normal",
        .cols = 36,
        .rows = 45,
        .ball_radius_factor = 0.28f,
        .branch_select_percent = 25,
        .min_solution_ratio = 0.85f,
    },
    [MAZE_DIFFICULTY_HARD] = {
        .name = "Dificil",
        .cols = 48,
        .rows = 60,
        .ball_radius_factor = 0.28f,
        .branch_select_percent = 35,
        .min_solution_ratio = 1.00f,
    },
};

static maze_app_t s_app;

static void show_mode_menu(void);
static void show_difficulty_menu(void);
static void start_game(maze_difficulty_t difficulty);
static void show_victory(void);
static void show_pause_menu(void);
static esp_err_t boot_button_init(void);
static void resume_game_clicked(lv_event_t *event);
static void request_exit_clicked(lv_event_t *event);
static void cancel_exit_clicked(lv_event_t *event);
static void confirm_exit_clicked(lv_event_t *event);
static float cell_left(int col);
static float cell_right(int col);
static float cell_top(int row);
static float cell_bottom(int row);

static const maze_difficulty_config_t *current_difficulty_config(void)
{
    return s_app.mode == MAZE_MODE_ADVENTURE ? &ADVENTURE_DIFFICULTIES[s_app.difficulty]
                                             : &NORMAL_DIFFICULTIES[s_app.difficulty];
}

static const char *current_mode_name(void)
{
    return s_app.mode == MAZE_MODE_ADVENTURE ? "Aventura" : "Normal";
}

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int round_to_int(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static int wall_thickness_px(void)
{
    int thickness = clamp_int(round_to_int(s_app.wall_thickness), 2, 4);
    if ((thickness & 1) != 0) {
        ++thickness;
    }
    return thickness;
}

static float wall_half_px(void)
{
    return (float)wall_thickness_px() * 0.5f;
}

static uint32_t maze_rand_u32(void)
{
    uint32_t x = s_app.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_app.rng = x ? x : 0x4d595df4U;
    return s_app.rng;
}

static int maze_rand_range(int limit)
{
    if (limit <= 1) {
        return 0;
    }
    return (int)(maze_rand_u32() % (uint32_t)limit);
}

static maze_cell_t *cell_at(int row, int col)
{
    return &s_app.cells[row][col];
}

static bool cell_valid(int row, int col)
{
    if (row < 0 || row >= s_app.rows || col < 0 || col >= s_app.cols) {
        return false;
    }
    return cell_at(row, col)->valid;
}

static void stop_timer(void)
{
    if (s_app.timer != NULL) {
        lv_timer_delete(s_app.timer);
        s_app.timer = NULL;
    }
    s_app.playing = false;
    s_app.paused = false;
}

static void clear_screen(void)
{
    stop_timer();

    s_app.ball_obj = NULL;
    s_app.hole_obj = NULL;
    s_app.overlay = NULL;
    s_app.pause_overlay = NULL;
    s_app.pause_card = NULL;
    s_app.pause_confirm_card = NULL;
    s_app.board_obj = NULL;
    s_app.world_layer = NULL;
    s_app.floor_obj = NULL;
    s_app.status_label = NULL;
    s_app.hint_checkbox = NULL;
    s_app.hint_box = NULL;
    s_app.hint_arrow = NULL;
    s_app.active_wall_count = 0;
    s_app.wall_pool_index = 0;
    s_app.render_start_row = -1;
    s_app.render_start_col = -1;
    s_app.render_end_row = -1;
    s_app.render_end_col = -1;
    s_app.last_world_offset_x = INT32_MIN;
    s_app.last_world_offset_y = INT32_MIN;
    s_app.hole_hidden = false;
    for (int i = 0; i < MAZE_WALL_POOL_OBJECTS; ++i) {
        s_app.wall_pool[i] = NULL;
    }

    lv_obj_t *screen = lv_screen_active();
    s_app.root = screen;
    lv_obj_clean(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x070b12), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

static void style_label(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

static lv_obj_t *create_button(lv_obj_t *parent, const char *text, int width, int height,
                               lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_radius(button, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1f7a8c), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(button, LV_OPA_30, LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    style_label(label, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_center(label);
    return button;
}

static void boot_button_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    const uint32_t now = lv_tick_get();
    const bool pressed = gpio_get_level(MAZE_BOOT_GPIO) == 0;

    if (pressed != s_app.boot_raw_pressed) {
        s_app.boot_raw_pressed = pressed;
        s_app.boot_last_change_ms = now;
        return;
    }

    if (pressed == s_app.boot_stable_pressed ||
        now - s_app.boot_last_change_ms < MAZE_BOOT_DEBOUNCE_MS) {
        return;
    }

    s_app.boot_stable_pressed = pressed;
    if (pressed) {
        s_app.boot_press_start_ms = now;
        s_app.boot_pressed_event_active = true;
        return;
    }

    if (!s_app.boot_pressed_event_active) {
        return;
    }
    s_app.boot_pressed_event_active = false;

    const uint32_t press_ms = now - s_app.boot_press_start_ms;
    if (press_ms <= MAZE_BOOT_SHORT_PRESS_MAX_MS && s_app.playing && !s_app.paused) {
        show_pause_menu();
    }
}

static esp_err_t boot_button_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << MAZE_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    const bool pressed = gpio_get_level(MAZE_BOOT_GPIO) == 0;
    s_app.boot_raw_pressed = pressed;
    s_app.boot_stable_pressed = pressed;
    s_app.boot_pressed_event_active = false;
    s_app.boot_last_change_ms = lv_tick_get();
    s_app.boot_press_start_ms = s_app.boot_last_change_ms;

    if (s_app.boot_timer == NULL) {
        s_app.boot_timer = lv_timer_create(boot_button_timer_cb, MAZE_BOOT_POLL_MS, NULL);
    }
    return s_app.boot_timer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool point_inside_safe_screen(float x, float y, float margin)
{
    const float w = (float)s_app.screen_w;
    const float h = (float)s_app.screen_h;
    const float px_per_mm_x = w / 33.09f;
    const float px_per_mm_y = h / 40.51f;
    const float corner_radius = 9.2f * fminf(px_per_mm_x, px_per_mm_y);
    const float effective_radius = corner_radius - margin;

    if (effective_radius <= 0.0f) {
        return false;
    }
    if (x < margin || x > w - margin || y < margin || y > h - margin) {
        return false;
    }

    if (x < corner_radius && y < corner_radius) {
        const float dx = x - corner_radius;
        const float dy = y - corner_radius;
        return dx * dx + dy * dy <= effective_radius * effective_radius;
    }
    if (x > w - corner_radius && y < corner_radius) {
        const float dx = x - (w - corner_radius);
        const float dy = y - corner_radius;
        return dx * dx + dy * dy <= effective_radius * effective_radius;
    }
    if (x < corner_radius && y > h - corner_radius) {
        const float dx = x - corner_radius;
        const float dy = y - (h - corner_radius);
        return dx * dx + dy * dy <= effective_radius * effective_radius;
    }
    if (x > w - corner_radius && y > h - corner_radius) {
        const float dx = x - (w - corner_radius);
        const float dy = y - (h - corner_radius);
        return dx * dx + dy * dy <= effective_radius * effective_radius;
    }

    return true;
}

static bool cell_inside_safe_screen(int row, int col)
{
    const float x0 = (float)col * s_app.cell_w;
    const float y0 = (float)row * s_app.cell_h;
    const float cx = x0 + s_app.cell_w * 0.5f;
    const float cy = y0 + s_app.cell_h * 0.5f;
    const float margin = s_app.ball_radius + s_app.wall_thickness + 2.0f;
    const float qx = s_app.cell_w * 0.25f;
    const float qy = s_app.cell_h * 0.25f;

    return point_inside_safe_screen(cx, cy, margin) &&
           point_inside_safe_screen(cx - qx, cy - qy, margin) &&
           point_inside_safe_screen(cx + qx, cy - qy, margin) &&
           point_inside_safe_screen(cx - qx, cy + qy, margin) &&
           point_inside_safe_screen(cx + qx, cy + qy, margin);
}

static maze_corner_t opposite_corner(maze_corner_t corner)
{
    switch (corner) {
    case MAZE_CORNER_TOP_LEFT:
        return MAZE_CORNER_BOTTOM_RIGHT;
    case MAZE_CORNER_TOP_RIGHT:
        return MAZE_CORNER_BOTTOM_LEFT;
    case MAZE_CORNER_BOTTOM_LEFT:
        return MAZE_CORNER_TOP_RIGHT;
    case MAZE_CORNER_BOTTOM_RIGHT:
    default:
        return MAZE_CORNER_TOP_LEFT;
    }
}

static int corner_distance(maze_corner_t corner, int row, int col)
{
    switch (corner) {
    case MAZE_CORNER_TOP_LEFT:
        return row + col;
    case MAZE_CORNER_TOP_RIGHT:
        return row + (s_app.cols - 1 - col);
    case MAZE_CORNER_BOTTOM_LEFT:
        return (s_app.rows - 1 - row) + col;
    case MAZE_CORNER_BOTTOM_RIGHT:
    default:
        return (s_app.rows - 1 - row) + (s_app.cols - 1 - col);
    }
}

static bool find_corner_cell(maze_corner_t corner, int *out_row, int *out_col)
{
    for (int dist = 0; dist < s_app.rows + s_app.cols; ++dist) {
        int chosen_row = -1;
        int chosen_col = -1;
        int count = 0;

        for (int row = 0; row < s_app.rows; ++row) {
            for (int col = 0; col < s_app.cols; ++col) {
                if (!cell_valid(row, col) || corner_distance(corner, row, col) != dist) {
                    continue;
                }

                ++count;
                if (maze_rand_range(count) == 0) {
                    chosen_row = row;
                    chosen_col = col;
                }
            }
        }

        if (count > 0) {
            *out_row = chosen_row;
            *out_col = chosen_col;
            return true;
        }
    }

    return false;
}

static bool find_center_cell(int *out_row, int *out_col)
{
    const int center_row = s_app.rows / 2;
    const int center_col = s_app.cols / 2;

    for (int dist = 0; dist < s_app.rows + s_app.cols; ++dist) {
        int chosen_row = -1;
        int chosen_col = -1;
        int count = 0;

        for (int row = 0; row < s_app.rows; ++row) {
            for (int col = 0; col < s_app.cols; ++col) {
                if (!cell_valid(row, col) || abs(row - center_row) + abs(col - center_col) != dist) {
                    continue;
                }

                ++count;
                if (maze_rand_range(count) == 0) {
                    chosen_row = row;
                    chosen_col = col;
                }
            }
        }

        if (count > 0) {
            *out_row = chosen_row;
            *out_col = chosen_col;
            return true;
        }
    }

    return false;
}

static uint8_t wall_for_dir(int dir)
{
    static const uint8_t walls[] = {
        MAZE_WALL_TOP,
        MAZE_WALL_RIGHT,
        MAZE_WALL_BOTTOM,
        MAZE_WALL_LEFT,
    };
    return walls[dir];
}

static uint8_t opposite_wall_for_dir(int dir)
{
    static const uint8_t walls[] = {
        MAZE_WALL_BOTTOM,
        MAZE_WALL_LEFT,
        MAZE_WALL_TOP,
        MAZE_WALL_RIGHT,
    };
    return walls[dir];
}

static void step_for_dir(int dir, int *dr, int *dc)
{
    static const int row_delta[] = {-1, 0, 1, 0};
    static const int col_delta[] = {0, 1, 0, -1};
    *dr = row_delta[dir];
    *dc = col_delta[dir];
}

static void carve_maze(const maze_difficulty_config_t *cfg)
{
    int16_t *active_rows = s_app.work_rows;
    int16_t *active_cols = s_app.work_cols;
    int active_count = 0;

    cell_at(s_app.start_row, s_app.start_col)->visited = true;
    active_rows[active_count] = (int16_t)s_app.start_row;
    active_cols[active_count] = (int16_t)s_app.start_col;
    ++active_count;

    while (active_count > 0) {
        const bool pick_random = maze_rand_range(100) < cfg->branch_select_percent;
        const int active_index = pick_random ? maze_rand_range(active_count) : active_count - 1;
        const int row = active_rows[active_index];
        const int col = active_cols[active_index];
        int dirs[4];
        int dir_count = 0;

        for (int dir = 0; dir < 4; ++dir) {
            int dr = 0;
            int dc = 0;
            step_for_dir(dir, &dr, &dc);
            const int nr = row + dr;
            const int nc = col + dc;
            if (cell_valid(nr, nc) && !cell_at(nr, nc)->visited) {
                dirs[dir_count++] = dir;
            }
        }

        if (dir_count == 0) {
            active_rows[active_index] = active_rows[active_count - 1];
            active_cols[active_index] = active_cols[active_count - 1];
            --active_count;
            continue;
        }

        const int dir = dirs[maze_rand_range(dir_count)];
        int dr = 0;
        int dc = 0;
        step_for_dir(dir, &dr, &dc);
        const int nr = row + dr;
        const int nc = col + dc;

        cell_at(row, col)->walls &= (uint8_t)~wall_for_dir(dir);
        cell_at(nr, nc)->walls &= (uint8_t)~opposite_wall_for_dir(dir);
        cell_at(nr, nc)->visited = true;

        active_rows[active_count] = (int16_t)nr;
        active_cols[active_count] = (int16_t)nc;
        ++active_count;
    }
}

static bool can_move_between(int row, int col, int dir)
{
    if (!cell_valid(row, col)) {
        return false;
    }

    int dr = 0;
    int dc = 0;
    step_for_dir(dir, &dr, &dc);
    const int nr = row + dr;
    const int nc = col + dc;
    if (!cell_valid(nr, nc)) {
        return false;
    }

    return (cell_at(row, col)->walls & wall_for_dir(dir)) == 0;
}

static bool validate_maze(const maze_difficulty_config_t *cfg, int *out_solution_len, int *out_dead_ends)
{
    int16_t *queue = s_app.work_queue;
    int16_t *distance = s_app.work_distance;
    int valid_count = 0;
    int reached_count = 0;
    int dead_ends = 0;

    for (int i = 0; i < MAZE_MAX_CELLS; ++i) {
        distance[i] = -1;
    }

    for (int row = 0; row < s_app.rows; ++row) {
        for (int col = 0; col < s_app.cols; ++col) {
            if (!cell_valid(row, col)) {
                continue;
            }
            ++valid_count;

            int open_count = 0;
            for (int dir = 0; dir < 4; ++dir) {
                if (can_move_between(row, col, dir)) {
                    ++open_count;
                }
            }
            if (open_count == 1) {
                ++dead_ends;
            }
        }
    }

    const int start_index = s_app.start_row * s_app.cols + s_app.start_col;
    const int goal_index = s_app.goal_row * s_app.cols + s_app.goal_col;
    int head = 0;
    int tail = 0;
    queue[tail++] = (int16_t)start_index;
    distance[start_index] = 0;

    while (head < tail) {
        const int index = queue[head++];
        const int row = index / s_app.cols;
        const int col = index % s_app.cols;
        ++reached_count;

        for (int dir = 0; dir < 4; ++dir) {
            if (!can_move_between(row, col, dir)) {
                continue;
            }

            int dr = 0;
            int dc = 0;
            step_for_dir(dir, &dr, &dc);
            const int nr = row + dr;
            const int nc = col + dc;
            const int next_index = nr * s_app.cols + nc;
            if (distance[next_index] >= 0) {
                continue;
            }
            distance[next_index] = distance[index] + 1;
            queue[tail++] = (int16_t)next_index;
        }
    }

    const int solution_len = distance[goal_index];
    const int min_solution = s_app.mode == MAZE_MODE_ADVENTURE
                                 ? (int)((float)(s_app.rows + s_app.cols) * cfg->min_solution_ratio)
                                 : (int)((float)valid_count * cfg->min_solution_ratio);
    const int min_dead_ends = valid_count / 8;

    if (out_solution_len != NULL) {
        *out_solution_len = solution_len;
    }
    if (out_dead_ends != NULL) {
        *out_dead_ends = dead_ends;
    }

    return valid_count > 0 && reached_count == valid_count && solution_len >= min_solution &&
           dead_ends >= min_dead_ends;
}

static void init_cells(void)
{
    for (int row = 0; row < s_app.rows; ++row) {
        for (int col = 0; col < s_app.cols; ++col) {
            maze_cell_t *cell = cell_at(row, col);
            cell->walls = MAZE_ALL_WALLS;
            cell->valid = s_app.mode == MAZE_MODE_ADVENTURE || cell_inside_safe_screen(row, col);
            cell->visited = false;
        }
    }
}

static bool generate_maze(void)
{
    const maze_difficulty_config_t *cfg = current_difficulty_config();
    s_app.cols = cfg->cols;
    s_app.rows = cfg->rows;
    s_app.visible_cols = s_app.mode == MAZE_MODE_ADVENTURE ? MAZE_ADVENTURE_VISIBLE_COLS : s_app.cols;
    s_app.visible_rows = s_app.mode == MAZE_MODE_ADVENTURE ? MAZE_ADVENTURE_VISIBLE_ROWS : s_app.rows;
    s_app.cell_w = (float)s_app.screen_w / (float)s_app.visible_cols;
    s_app.cell_h = (float)s_app.screen_h / (float)s_app.visible_rows;
    s_app.cell_size = fminf(s_app.cell_w, s_app.cell_h);
    s_app.world_w = s_app.cell_w * (float)s_app.cols;
    s_app.world_h = s_app.cell_h * (float)s_app.rows;
    s_app.wall_thickness = (float)clamp_int(round_to_int(s_app.cell_size * 0.08f), 2, 4);
    if (((int)s_app.wall_thickness & 1) != 0) {
        s_app.wall_thickness += 1.0f;
    }

    const int ball_diameter = round_to_int(s_app.cell_size * cfg->ball_radius_factor * 2.0f);
    s_app.ball_radius = (float)ball_diameter * 0.5f;
    s_app.hole_radius = (float)round_to_int(s_app.ball_radius * 2.2f) * 0.5f;

    for (int attempt = 0; attempt < 48; ++attempt) {
        s_app.seed = esp_random();
        if (s_app.seed == 0) {
            s_app.seed = 0x8badf00dU;
        }
        s_app.rng = s_app.seed;

        init_cells();

        if (s_app.mode == MAZE_MODE_ADVENTURE) {
            const maze_corner_t goal_corner = (maze_corner_t)maze_rand_range(4);
            if (!find_center_cell(&s_app.start_row, &s_app.start_col) ||
                !find_corner_cell(goal_corner, &s_app.goal_row, &s_app.goal_col)) {
                continue;
            }
        } else {
            const maze_corner_t start_corner = (maze_corner_t)maze_rand_range(4);
            const maze_corner_t goal_corner = opposite_corner(start_corner);
            if (!find_corner_cell(start_corner, &s_app.start_row, &s_app.start_col) ||
                !find_corner_cell(goal_corner, &s_app.goal_row, &s_app.goal_col)) {
                continue;
            }
        }

        carve_maze(cfg);

        int solution_len = 0;
        int dead_ends = 0;
        const bool valid = validate_maze(cfg, &solution_len, &dead_ends);
        ESP_LOGI(TAG,
                 "maze attempt=%d mode=%s diff=%s size=%dx%d seed=%" PRIu32 " start=(%d,%d) goal=(%d,%d) path=%d dead=%d valid=%d",
                 attempt + 1, current_mode_name(), cfg->name, s_app.cols, s_app.rows, s_app.seed,
                 s_app.start_row, s_app.start_col, s_app.goal_row, s_app.goal_col, solution_len,
                 dead_ends, valid);
        if (valid) {
            return true;
        }
    }

    return false;
}

static lv_obj_t *create_rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color)
{
    if (w <= 0 || h <= 0) {
        return NULL;
    }

    const int x0 = clamp_int(x, 0, s_app.screen_w);
    const int y0 = clamp_int(y, 0, s_app.screen_h);
    const int x1 = clamp_int(x + w, 0, s_app.screen_w);
    const int y1 = clamp_int(y + h, 0, s_app.screen_h);

    if (x1 <= x0 || y1 <= y0) {
        return NULL;
    }

    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x0, y0);
    lv_obj_set_size(obj, x1 - x0, y1 - y0);
    lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static void draw_maze(lv_obj_t *parent)
{
    const int wall_t = wall_thickness_px();
    const int wall_before = wall_t / 2;
    const lv_color_t wall_color = lv_color_hex(0x5b3418);
    const lv_color_t invalid_color = lv_color_hex(0x070b12);

    for (int row = 0; row < s_app.rows; ++row) {
        for (int col = 0; col < s_app.cols; ++col) {
            if (cell_at(row, col)->valid) {
                continue;
            }

            const int x0 = round_to_int((float)col * s_app.cell_w);
            const int y0 = round_to_int((float)row * s_app.cell_h);
            const int x1 = round_to_int((float)(col + 1) * s_app.cell_w);
            const int y1 = round_to_int((float)(row + 1) * s_app.cell_h);
            create_rect(parent, x0, y0, x1 - x0, y1 - y0, invalid_color);
        }
    }

    for (int row = 0; row < s_app.rows; ++row) {
        for (int col = 0; col < s_app.cols; ++col) {
            const int x0 = round_to_int((float)col * s_app.cell_w);
            const int y0 = round_to_int((float)row * s_app.cell_h);
            const int x1 = round_to_int((float)(col + 1) * s_app.cell_w);
            const int y1 = round_to_int((float)(row + 1) * s_app.cell_h);
            const int cell_w = x1 - x0;
            const int cell_h = y1 - y0;
            const maze_cell_t *cell = cell_at(row, col);

            if (!cell->valid) {
                continue;
            }

            if ((cell->walls & MAZE_WALL_TOP) != 0) {
                create_rect(parent, x0 - wall_before, y0 - wall_before,
                            cell_w + wall_t, wall_t, wall_color);
            }
            if ((cell->walls & MAZE_WALL_LEFT) != 0) {
                create_rect(parent, x0 - wall_before, y0 - wall_before,
                            wall_t, cell_h + wall_t, wall_color);
            }
            if ((row == s_app.rows - 1 || !cell_valid(row + 1, col)) &&
                (cell->walls & MAZE_WALL_BOTTOM) != 0) {
                create_rect(parent, x0 - wall_before, y1 - wall_before,
                            cell_w + wall_t, wall_t, wall_color);
            }
            if ((col == s_app.cols - 1 || !cell_valid(row, col + 1)) &&
                (cell->walls & MAZE_WALL_RIGHT) != 0) {
                create_rect(parent, x1 - wall_before, y0 - wall_before,
                            wall_t, cell_h + wall_t, wall_color);
            }
        }
    }
}

static void configure_existing_rect(lv_obj_t *obj, int x, int y, int w, int h)
{
    if (obj == NULL) {
        return;
    }
    if (w <= 0 || h <= 0) {
        if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    if (lv_obj_get_x(obj) != x || lv_obj_get_y(obj) != y) {
        lv_obj_set_pos(obj, x, y);
    }
    if (lv_obj_get_width(obj) != w || lv_obj_get_height(obj) != h) {
        lv_obj_set_size(obj, w, h);
    }
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void position_adventure_rect(lv_obj_t *obj, const maze_rect_t *rect)
{
    const int x0 = round_to_int(rect->x0 - s_app.camera_x);
    const int y0 = round_to_int(rect->y0 - s_app.camera_y);
    const int x1 = round_to_int(rect->x1 - s_app.camera_x);
    const int y1 = round_to_int(rect->y1 - s_app.camera_y);
    configure_existing_rect(obj, x0, y0, x1 - x0, y1 - y0);
}

static void update_adventure_floor(void)
{
    if (s_app.floor_obj == NULL) {
        return;
    }

    const float x0 = fmaxf(0.0f, s_app.camera_x);
    const float y0 = fmaxf(0.0f, s_app.camera_y);
    const float x1 = fminf(s_app.world_w, s_app.camera_x + (float)s_app.screen_w);
    const float y1 = fminf(s_app.world_h, s_app.camera_y + (float)s_app.screen_h);

    if (x1 <= x0 || y1 <= y0) {
        lv_obj_add_flag(s_app.floor_obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    maze_rect_t floor_rect = {
        .x0 = x0,
        .y0 = y0,
        .x1 = x1,
        .y1 = y1,
    };
    position_adventure_rect(s_app.floor_obj, &floor_rect);
}

static void update_adventure_wall_positions(void)
{
    for (int i = 0; i < s_app.active_wall_count; ++i) {
        position_adventure_rect(s_app.wall_pool[i], &s_app.wall_world_rects[i]);
    }
}

static void create_adventure_wall_pool(lv_obj_t *parent)
{
    const lv_color_t wall_color = lv_color_hex(0x5b3418);

    s_app.wall_pool_index = 0;
    s_app.active_wall_count = 0;
    for (int i = 0; i < MAZE_WALL_POOL_OBJECTS; ++i) {
        lv_obj_t *wall = lv_obj_create(parent);
        lv_obj_set_style_bg_color(wall, wall_color, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(wall, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(wall, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(wall, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(wall, 0, LV_PART_MAIN);
        lv_obj_clear_flag(wall, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(wall, LV_OBJ_FLAG_HIDDEN);
        s_app.wall_pool[i] = wall;
    }
}

static void add_adventure_wall_rect(float x0, float y0, float x1, float y1)
{
    if (s_app.wall_pool_index >= MAZE_WALL_POOL_OBJECTS) {
        ESP_LOGW(TAG, "Adventure wall pool exhausted");
        return;
    }

    maze_rect_t rect = {
        .x0 = clamp_float(x0, 0.0f, s_app.world_w),
        .y0 = clamp_float(y0, 0.0f, s_app.world_h),
        .x1 = clamp_float(x1, 0.0f, s_app.world_w),
        .y1 = clamp_float(y1, 0.0f, s_app.world_h),
    };
    if (rect.x1 <= rect.x0 || rect.y1 <= rect.y0) {
        return;
    }

    const int index = s_app.wall_pool_index++;
    s_app.wall_world_rects[index] = rect;
    position_adventure_rect(s_app.wall_pool[index], &rect);
}

static void update_adventure_camera(void)
{
    s_app.camera_x = s_app.ball.x - (float)s_app.screen_w * 0.5f;
    s_app.camera_y = s_app.ball.y - (float)s_app.screen_h * 0.5f;
}

static bool adventure_has_top_wall(int row, int col)
{
    return cell_valid(row, col) && (cell_at(row, col)->walls & MAZE_WALL_TOP) != 0;
}

static bool adventure_has_left_wall(int row, int col)
{
    return cell_valid(row, col) && (cell_at(row, col)->walls & MAZE_WALL_LEFT) != 0;
}

static bool adventure_has_bottom_wall(int row, int col)
{
    return row == s_app.rows - 1 && cell_valid(row, col) &&
           (cell_at(row, col)->walls & MAZE_WALL_BOTTOM) != 0;
}

static bool adventure_has_right_wall(int row, int col)
{
    return col == s_app.cols - 1 && cell_valid(row, col) &&
           (cell_at(row, col)->walls & MAZE_WALL_RIGHT) != 0;
}

static void add_adventure_horizontal_segments(int row, int start_col, int end_col, bool bottom)
{
    const float wall_half = wall_half_px();
    const float y = bottom ? cell_bottom(row) : cell_top(row);
    int col = start_col;

    while (col < end_col) {
        const bool has_wall = bottom ? adventure_has_bottom_wall(row, col)
                                     : adventure_has_top_wall(row, col);
        if (!has_wall) {
            ++col;
            continue;
        }

        const int seg_start = col;
        do {
            ++col;
        } while (col < end_col &&
                 (bottom ? adventure_has_bottom_wall(row, col)
                         : adventure_has_top_wall(row, col)));

        add_adventure_wall_rect(cell_left(seg_start) - wall_half, y - wall_half,
                                cell_right(col - 1) + wall_half, y + wall_half);
    }
}

static void add_adventure_vertical_segments(int col, int start_row, int end_row, bool right)
{
    const float wall_half = wall_half_px();
    const float x = right ? cell_right(col) : cell_left(col);
    int row = start_row;

    while (row < end_row) {
        const bool has_wall = right ? adventure_has_right_wall(row, col)
                                    : adventure_has_left_wall(row, col);
        if (!has_wall) {
            ++row;
            continue;
        }

        const int seg_start = row;
        do {
            ++row;
        } while (row < end_row &&
                 (right ? adventure_has_right_wall(row, col)
                        : adventure_has_left_wall(row, col)));

        add_adventure_wall_rect(x - wall_half, cell_top(seg_start) - wall_half,
                                x + wall_half, cell_bottom(row - 1) + wall_half);
    }
}

static void draw_adventure_visible(bool force)
{
    const int target_start_col = clamp_int((int)floorf(s_app.camera_x / s_app.cell_w) - 1, 0, s_app.cols - 1);
    const int target_start_row = clamp_int((int)floorf(s_app.camera_y / s_app.cell_h) - 1, 0, s_app.rows - 1);
    const int target_end_col = clamp_int((int)floorf(((float)s_app.screen_w + s_app.camera_x) / s_app.cell_w) + 3,
                                        target_start_col + 1, s_app.cols);
    const int target_end_row = clamp_int((int)floorf(((float)s_app.screen_h + s_app.camera_y) / s_app.cell_h) + 3,
                                        target_start_row + 1, s_app.rows);

    if (!force && target_start_col == s_app.render_start_col &&
        target_start_row == s_app.render_start_row && target_end_col == s_app.render_end_col &&
        target_end_row == s_app.render_end_row) {
        return;
    }

    s_app.render_start_col = target_start_col;
    s_app.render_start_row = target_start_row;
    s_app.render_end_col = target_end_col;
    s_app.render_end_row = target_end_row;
    const int previous_count = s_app.active_wall_count;
    s_app.wall_pool_index = 0;

    for (int row = s_app.render_start_row; row < s_app.render_end_row; ++row) {
        add_adventure_horizontal_segments(row, s_app.render_start_col, s_app.render_end_col, false);
        add_adventure_horizontal_segments(row, s_app.render_start_col, s_app.render_end_col, true);
    }

    for (int col = s_app.render_start_col; col < s_app.render_end_col; ++col) {
        add_adventure_vertical_segments(col, s_app.render_start_row, s_app.render_end_row, false);
        add_adventure_vertical_segments(col, s_app.render_start_row, s_app.render_end_row, true);
    }

    for (int i = s_app.wall_pool_index; i < previous_count; ++i) {
        if (s_app.wall_pool[i] != NULL) {
            lv_obj_add_flag(s_app.wall_pool[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    s_app.active_wall_count = s_app.wall_pool_index;
}

static void update_hint_arrow(void)
{
    if (!s_app.hint_enabled || s_app.hint_box == NULL || s_app.hint_arrow == NULL) {
        return;
    }

    const float goal_x = ((float)s_app.goal_col + 0.5f) * s_app.cell_w;
    const float goal_y = ((float)s_app.goal_row + 0.5f) * s_app.cell_h;
    const float dx = goal_x - s_app.ball.x;
    const float dy = goal_y - s_app.ball.y;
    const float dist = sqrtf(dx * dx + dy * dy);

    if (dist <= 0.001f) {
        lv_obj_add_flag(s_app.hint_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const float ux = dx / dist;
    const float uy = dy / dist;
    const float px = -uy;
    const float py = ux;
    const float center_x = 28.0f;
    const float center_y = 28.0f;
    const float tip_len = 20.0f;
    const float tail_len = 12.0f;
    const float head_len = 10.0f;
    const float head_w = 7.0f;
    const float tip_x = center_x + ux * tip_len;
    const float tip_y = center_y + uy * tip_len;
    const float tail_x = center_x - ux * tail_len;
    const float tail_y = center_y - uy * tail_len;
    const float head_x = tip_x - ux * head_len;
    const float head_y = tip_y - uy * head_len;

    s_app.hint_points[0].x = round_to_int(tail_x);
    s_app.hint_points[0].y = round_to_int(tail_y);
    s_app.hint_points[1].x = round_to_int(tip_x);
    s_app.hint_points[1].y = round_to_int(tip_y);
    s_app.hint_points[2].x = round_to_int(head_x + px * head_w);
    s_app.hint_points[2].y = round_to_int(head_y + py * head_w);
    s_app.hint_points[3].x = round_to_int(tip_x);
    s_app.hint_points[3].y = round_to_int(tip_y);
    s_app.hint_points[4].x = round_to_int(head_x - px * head_w);
    s_app.hint_points[4].y = round_to_int(head_y - py * head_w);
    lv_line_set_points_mutable(s_app.hint_arrow, s_app.hint_points, 5);
    lv_obj_clear_flag(s_app.hint_box, LV_OBJ_FLAG_HIDDEN);
}

static void update_adventure_world_transform(void)
{
    update_adventure_floor();
    update_adventure_wall_positions();

    if (s_app.hole_obj != NULL) {
        const float goal_x = ((float)s_app.goal_col + 0.5f) * s_app.cell_w;
        const float goal_y = ((float)s_app.goal_row + 0.5f) * s_app.cell_h;
        const float hole_screen_x = goal_x - s_app.hole_radius - s_app.camera_x;
        const float hole_screen_y = goal_y - s_app.hole_radius - s_app.camera_y;
        const float hole_d = s_app.hole_radius * 2.0f;
        const int hole_d_px = round_to_int(hole_d);
        const int hole_x = round_to_int(hole_screen_x);
        const int hole_y = round_to_int(hole_screen_y);
        const bool hide_hole = hole_screen_x < -hole_d || hole_screen_x > (float)s_app.screen_w ||
                               hole_screen_y < -hole_d || hole_screen_y > (float)s_app.screen_h;
        if (lv_obj_get_width(s_app.hole_obj) != hole_d_px ||
            lv_obj_get_height(s_app.hole_obj) != hole_d_px) {
            lv_obj_set_size(s_app.hole_obj, hole_d_px, hole_d_px);
        }
        if (lv_obj_get_x(s_app.hole_obj) != hole_x || lv_obj_get_y(s_app.hole_obj) != hole_y) {
            lv_obj_set_pos(s_app.hole_obj, hole_x, hole_y);
        }
        if (hide_hole != s_app.hole_hidden) {
            if (hide_hole) {
                lv_obj_add_flag(s_app.hole_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_clear_flag(s_app.hole_obj, LV_OBJ_FLAG_HIDDEN);
            }
            s_app.hole_hidden = hide_hole;
        }
    }

    update_hint_arrow();
}

static float cell_left(int col)
{
    return (float)col * s_app.cell_w;
}

static float cell_right(int col)
{
    return (float)(col + 1) * s_app.cell_w;
}

static float cell_top(int row)
{
    return (float)row * s_app.cell_h;
}

static float cell_bottom(int row)
{
    return (float)(row + 1) * s_app.cell_h;
}

static bool clip_rect_to_world(maze_rect_t *rect)
{
    rect->x0 = clamp_float(rect->x0, 0.0f, s_app.world_w);
    rect->y0 = clamp_float(rect->y0, 0.0f, s_app.world_h);
    rect->x1 = clamp_float(rect->x1, 0.0f, s_app.world_w);
    rect->y1 = clamp_float(rect->y1, 0.0f, s_app.world_h);

    return rect->x1 > rect->x0 && rect->y1 > rect->y0;
}

static bool make_invalid_cell_rect(int row, int col, maze_rect_t *rect)
{
    if (row < 0 || row >= s_app.rows || col < 0 || col >= s_app.cols || cell_valid(row, col)) {
        return false;
    }

    rect->x0 = cell_left(col);
    rect->y0 = cell_top(row);
    rect->x1 = cell_right(col);
    rect->y1 = cell_bottom(row);
    return clip_rect_to_world(rect);
}

static bool make_wall_rect(int row, int col, uint8_t wall, maze_rect_t *rect)
{
    if (!cell_valid(row, col) || (cell_at(row, col)->walls & wall) == 0) {
        return false;
    }

    const float wall_half = wall_half_px();
    const float x0 = cell_left(col);
    const float y0 = cell_top(row);
    const float x1 = cell_right(col);
    const float y1 = cell_bottom(row);

    switch (wall) {
    case MAZE_WALL_TOP:
        rect->x0 = x0 - wall_half;
        rect->y0 = y0 - wall_half;
        rect->x1 = x1 + wall_half;
        rect->y1 = y0 + wall_half;
        break;
    case MAZE_WALL_LEFT:
        rect->x0 = x0 - wall_half;
        rect->y0 = y0 - wall_half;
        rect->x1 = x0 + wall_half;
        rect->y1 = y1 + wall_half;
        break;
    case MAZE_WALL_BOTTOM:
        if (row != s_app.rows - 1 && cell_valid(row + 1, col)) {
            return false;
        }
        rect->x0 = x0 - wall_half;
        rect->y0 = y1 - wall_half;
        rect->x1 = x1 + wall_half;
        rect->y1 = y1 + wall_half;
        break;
    case MAZE_WALL_RIGHT:
        if (col != s_app.cols - 1 && cell_valid(row, col + 1)) {
            return false;
        }
        rect->x0 = x1 - wall_half;
        rect->y0 = y0 - wall_half;
        rect->x1 = x1 + wall_half;
        rect->y1 = y1 + wall_half;
        break;
    default:
        return false;
    }

    return clip_rect_to_world(rect);
}

static bool depenetrate_rect(float *x, float *y, const maze_rect_t *rect)
{
    const float radius = s_app.ball.radius;
    const bool center_inside = *x >= rect->x0 && *x <= rect->x1 && *y >= rect->y0 && *y <= rect->y1;
    float normal_x = 0.0f;
    float normal_y = 0.0f;
    float push = 0.0f;

    if (center_inside) {
        const float left = *x - rect->x0;
        const float right = rect->x1 - *x;
        const float top = *y - rect->y0;
        const float bottom = rect->y1 - *y;
        float min_dist = left;
        normal_x = -1.0f;
        normal_y = 0.0f;
        push = radius + left;

        if (right < min_dist) {
            min_dist = right;
            normal_x = 1.0f;
            normal_y = 0.0f;
            push = radius + right;
        }
        if (top < min_dist) {
            min_dist = top;
            normal_x = 0.0f;
            normal_y = -1.0f;
            push = radius + top;
        }
        if (bottom < min_dist) {
            normal_x = 0.0f;
            normal_y = 1.0f;
            push = radius + bottom;
        }
    } else {
        const float closest_x = clamp_float(*x, rect->x0, rect->x1);
        const float closest_y = clamp_float(*y, rect->y0, rect->y1);
        const float dx = *x - closest_x;
        const float dy = *y - closest_y;
        const float dist_sq = dx * dx + dy * dy;
        const float radius_sq = radius * radius;

        if (dist_sq >= radius_sq || dist_sq <= 0.0001f) {
            return false;
        }

        const float dist = sqrtf(dist_sq);
        normal_x = dx / dist;
        normal_y = dy / dist;
        push = radius - dist;
    }

    *x += normal_x * push;
    *y += normal_y * push;

    const float entering_velocity = s_app.ball.vx * normal_x + s_app.ball.vy * normal_y;
    if (entering_velocity < 0.0f) {
        s_app.ball.vx -= entering_velocity * normal_x;
        s_app.ball.vy -= entering_velocity * normal_y;
    }

    return true;
}

static void depenetrate_nearby_walls(float *x, float *y)
{
    const float radius = s_app.ball.radius;
    const float wall_half = wall_half_px();
    const float reach = radius + wall_half + 2.0f;

    for (int iter = 0; iter < 3; ++iter) {
        bool moved = false;
        const int min_col = clamp_int((int)floorf((*x - reach) / s_app.cell_w) - 1, 0, s_app.cols - 1);
        const int max_col = clamp_int((int)floorf((*x + reach) / s_app.cell_w) + 1, 0, s_app.cols - 1);
        const int min_row = clamp_int((int)floorf((*y - reach) / s_app.cell_h) - 1, 0, s_app.rows - 1);
        const int max_row = clamp_int((int)floorf((*y + reach) / s_app.cell_h) + 1, 0, s_app.rows - 1);

        for (int row = min_row; row <= max_row; ++row) {
            for (int col = min_col; col <= max_col; ++col) {
                maze_rect_t rect = {0};

                if (make_invalid_cell_rect(row, col, &rect)) {
                    moved |= depenetrate_rect(x, y, &rect);
                    continue;
                }

                const uint8_t walls[] = {
                    MAZE_WALL_TOP,
                    MAZE_WALL_LEFT,
                    MAZE_WALL_BOTTOM,
                    MAZE_WALL_RIGHT,
                };

                for (size_t i = 0; i < sizeof(walls) / sizeof(walls[0]); ++i) {
                    if (make_wall_rect(row, col, walls[i], &rect)) {
                        moved |= depenetrate_rect(x, y, &rect);
                    }
                }
            }
        }

        if (!moved) {
            break;
        }
    }
}

static void resolve_collision(float target_x, float target_y, float *out_x, float *out_y)
{
    const float radius = s_app.ball.radius;
    const float wall_half = wall_half_px();
    float current_x = s_app.ball.x;
    float current_y = s_app.ball.y;
    float resolved_x = target_x;

    int row = clamp_int((int)floorf(current_y / s_app.cell_h), 0, s_app.rows - 1);
    int col = clamp_int((int)floorf(current_x / s_app.cell_w), 0, s_app.cols - 1);

    if (target_x > current_x) {
        for (int guard = 0; guard < 4 && resolved_x + radius > cell_right(col) - wall_half; ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row, col + 1) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_RIGHT) != 0);
            if (blocked) {
                resolved_x = cell_right(col) - radius - wall_half;
                s_app.ball.vx = -s_app.ball.vx * 0.08f;
                break;
            }
            ++col;
        }
    } else if (target_x < current_x) {
        for (int guard = 0; guard < 4 && resolved_x - radius < cell_left(col) + wall_half; ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row, col - 1) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_LEFT) != 0);
            if (blocked) {
                resolved_x = cell_left(col) + radius + wall_half;
                s_app.ball.vx = -s_app.ball.vx * 0.08f;
                break;
            }
            --col;
        }
    }

    resolved_x = clamp_float(resolved_x, radius + wall_half,
                             s_app.world_w - radius - wall_half);

    float resolved_y = target_y;
    col = clamp_int((int)floorf(resolved_x / s_app.cell_w), 0, s_app.cols - 1);
    row = clamp_int((int)floorf(current_y / s_app.cell_h), 0, s_app.rows - 1);

    if (target_y > current_y) {
        for (int guard = 0; guard < 4 && resolved_y + radius > cell_bottom(row) - wall_half; ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row + 1, col) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_BOTTOM) != 0);
            if (blocked) {
                resolved_y = cell_bottom(row) - radius - wall_half;
                s_app.ball.vy = -s_app.ball.vy * 0.08f;
                break;
            }
            ++row;
        }
    } else if (target_y < current_y) {
        for (int guard = 0; guard < 4 && resolved_y - radius < cell_top(row) + wall_half; ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row - 1, col) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_TOP) != 0);
            if (blocked) {
                resolved_y = cell_top(row) + radius + wall_half;
                s_app.ball.vy = -s_app.ball.vy * 0.08f;
                break;
            }
            --row;
        }
    }

    resolved_y = clamp_float(resolved_y, radius + wall_half,
                             s_app.world_h - radius - wall_half);

    if (s_app.mode == MAZE_MODE_NORMAL &&
        !point_inside_safe_screen(resolved_x, resolved_y, radius + wall_half + 1.0f)) {
        resolved_x = current_x;
        resolved_y = current_y;
        s_app.ball.vx *= 0.2f;
        s_app.ball.vy *= 0.2f;
    }

    depenetrate_nearby_walls(&resolved_x, &resolved_y);

    resolved_x = clamp_float(resolved_x, radius + wall_half,
                             s_app.world_w - radius - wall_half);
    resolved_y = clamp_float(resolved_y, radius + wall_half,
                             s_app.world_h - radius - wall_half);

    if (s_app.mode == MAZE_MODE_NORMAL &&
        !point_inside_safe_screen(resolved_x, resolved_y, radius + wall_half + 1.0f)) {
        resolved_x = current_x;
        resolved_y = current_y;
        s_app.ball.vx *= 0.2f;
        s_app.ball.vy *= 0.2f;
    }

    *out_x = resolved_x;
    *out_y = resolved_y;
}

static void update_physics(float ax, float ay, float dt)
{
    const float base_dt = 0.020f;
    const float max_dt = 0.100f;
    const float accel_factor = 780.0f;
    const float friction = 0.92f;
    const float max_velocity = s_app.cell_size * 8.0f;

    if (dt <= 0.0f) {
        return;
    }
    if (dt > max_dt) {
        dt = base_dt;
    }

    const float predicted_dx = fabsf(s_app.ball.vx * dt);
    const float predicted_dy = fabsf(s_app.ball.vy * dt);
    const float max_step = fmaxf(2.0f, s_app.ball.radius * 0.45f);
    int steps = (int)ceilf(fmaxf(predicted_dx, predicted_dy) / max_step);
    steps = clamp_int(steps, 1, 5);

    const float step_dt = dt / (float)steps;
    const float dt_scale = step_dt / base_dt;
    const float frame_friction = powf(friction, dt_scale);
    for (int i = 0; i < steps; ++i) {
        s_app.ball.vx += ax * accel_factor * step_dt;
        s_app.ball.vy += ay * accel_factor * step_dt;

        s_app.ball.vx *= frame_friction;
        s_app.ball.vy *= frame_friction;

        const float speed_sq = s_app.ball.vx * s_app.ball.vx + s_app.ball.vy * s_app.ball.vy;
        const float max_speed_sq = max_velocity * max_velocity;
        if (speed_sq > max_speed_sq) {
            const float scale = max_velocity / sqrtf(speed_sq);
            s_app.ball.vx *= scale;
            s_app.ball.vy *= scale;
        }

        const float target_x = s_app.ball.x + s_app.ball.vx * step_dt;
        const float target_y = s_app.ball.y + s_app.ball.vy * step_dt;
        float final_x = s_app.ball.x;
        float final_y = s_app.ball.y;
        resolve_collision(target_x, target_y, &final_x, &final_y);
        s_app.ball.x = final_x;
        s_app.ball.y = final_y;
    }
}

static bool check_win(void)
{
    const float goal_x = ((float)s_app.goal_col + 0.5f) * s_app.cell_w;
    const float goal_y = ((float)s_app.goal_row + 0.5f) * s_app.cell_h;
    const float dx = s_app.ball.x - goal_x;
    const float dy = s_app.ball.y - goal_y;
    const float win_radius = s_app.ball.radius * 0.75f;
    return dx * dx + dy * dy <= win_radius * win_radius;
}

static void game_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_app.playing || s_app.ball_obj == NULL) {
        return;
    }
    if (s_app.paused) {
        s_app.last_tick_ms = lv_tick_get();
        return;
    }

    const uint32_t now = lv_tick_get();
    uint32_t elapsed_ms = now - s_app.last_tick_ms;
    s_app.last_tick_ms = now;
    if (elapsed_ms == 0 || elapsed_ms > 100) {
        elapsed_ms = 20;
    }

    imu_service_accel_t accel = {0};
    if (imu_service_read(&accel) != ESP_OK) {
        accel.x = 0.0f;
        accel.y = 0.0f;
    }

    update_physics(accel.x, accel.y, (float)elapsed_ms * 0.001f);

    if (s_app.mode == MAZE_MODE_ADVENTURE) {
        update_adventure_camera();
        draw_adventure_visible(false);
        update_adventure_world_transform();
    } else {
        const int ball_x = round_to_int(s_app.ball.x - s_app.ball.radius);
        const int ball_y = round_to_int(s_app.ball.y - s_app.ball.radius);
        if (lv_obj_get_x(s_app.ball_obj) != ball_x || lv_obj_get_y(s_app.ball_obj) != ball_y) {
            lv_obj_set_pos(s_app.ball_obj, ball_x, ball_y);
        }
    }

    if (check_win()) {
        show_victory();
    }
}

static void close_pause_overlay(void)
{
    if (s_app.pause_overlay != NULL) {
        lv_obj_delete(s_app.pause_overlay);
        s_app.pause_overlay = NULL;
        s_app.pause_card = NULL;
        s_app.pause_confirm_card = NULL;
    }
}

static lv_obj_t *create_overlay_card(lv_obj_t *parent, int height)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, s_app.screen_w - 56, height);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x0f172a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(card, lv_color_hex(0x38bdf8), LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 28, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(card);
    return card;
}

static void show_pause_menu(void)
{
    if (!s_app.playing || s_app.paused) {
        return;
    }

    s_app.paused = true;
    s_app.ball.vx = 0.0f;
    s_app.ball.vy = 0.0f;
    s_app.last_tick_ms = lv_tick_get();

    s_app.pause_overlay = lv_obj_create(s_app.root);
    lv_obj_set_size(s_app.pause_overlay, s_app.screen_w, s_app.screen_h);
    lv_obj_set_pos(s_app.pause_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_app.pause_overlay, lv_color_hex(0x020617), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_app.pause_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_app.pause_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_app.pause_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_app.pause_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_app.pause_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_app.pause_card = create_overlay_card(s_app.pause_overlay, 224);

    lv_obj_t *title = lv_label_create(s_app.pause_card);
    lv_label_set_text(title, "Pausa");
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *resume = create_button(s_app.pause_card, "Volver al juego", 220, 54,
                                     resume_game_clicked, NULL);
    lv_obj_align(resume, LV_ALIGN_TOP_MID, 0, 86);

    lv_obj_t *exit = create_button(s_app.pause_card, "Salir", 160, 46, request_exit_clicked, NULL);
    lv_obj_set_style_bg_color(exit, lv_color_hex(0x9f1239), LV_PART_MAIN);
    lv_obj_align(exit, LV_ALIGN_TOP_MID, 0, 152);
}

static void show_exit_confirmation(void)
{
    if (s_app.pause_overlay == NULL || s_app.pause_confirm_card != NULL) {
        return;
    }
    if (s_app.pause_card != NULL) {
        lv_obj_add_flag(s_app.pause_card, LV_OBJ_FLAG_HIDDEN);
    }

    s_app.pause_confirm_card = create_overlay_card(s_app.pause_overlay, 238);

    lv_obj_t *title = lv_label_create(s_app.pause_confirm_card);
    lv_label_set_text(title, "Seguro que quieres salir?");
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
#endif
    lv_obj_set_width(title, s_app.screen_w - 92);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    lv_obj_t *cancel = create_button(s_app.pause_confirm_card, "Cancelar", 180, 50,
                                     cancel_exit_clicked, NULL);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_align(cancel, LV_ALIGN_TOP_MID, 0, 96);

    lv_obj_t *exit = create_button(s_app.pause_confirm_card, "Salir", 160, 46,
                                   confirm_exit_clicked, NULL);
    lv_obj_set_style_bg_color(exit, lv_color_hex(0x9f1239), LV_PART_MAIN);
    lv_obj_align(exit, LV_ALIGN_TOP_MID, 0, 162);
}

static void resume_game_clicked(lv_event_t *event)
{
    (void)event;
    close_pause_overlay();
    s_app.paused = false;
    s_app.ball.vx = 0.0f;
    s_app.ball.vy = 0.0f;
    s_app.last_tick_ms = lv_tick_get();
}

static void request_exit_clicked(lv_event_t *event)
{
    (void)event;
    show_exit_confirmation();
}

static void cancel_exit_clicked(lv_event_t *event)
{
    (void)event;
    if (s_app.pause_confirm_card != NULL) {
        lv_obj_delete(s_app.pause_confirm_card);
        s_app.pause_confirm_card = NULL;
    }
    if (s_app.pause_card != NULL) {
        lv_obj_clear_flag(s_app.pause_card, LV_OBJ_FLAG_HIDDEN);
    }
}

static void confirm_exit_clicked(lv_event_t *event)
{
    (void)event;
    s_app.paused = false;
    show_mode_menu();
}

static void normal_mode_clicked(lv_event_t *event)
{
    (void)event;
    s_app.mode = MAZE_MODE_NORMAL;
    s_app.hint_enabled = false;
    show_difficulty_menu();
}

static void adventure_mode_clicked(lv_event_t *event)
{
    (void)event;
    s_app.mode = MAZE_MODE_ADVENTURE;
    s_app.hint_enabled = false;
    show_difficulty_menu();
}

static void difficulty_clicked(lv_event_t *event)
{
    const maze_difficulty_t difficulty = (maze_difficulty_t)(intptr_t)lv_event_get_user_data(event);
    s_app.hint_enabled = s_app.mode == MAZE_MODE_ADVENTURE && s_app.hint_checkbox != NULL &&
                         lv_obj_has_state(s_app.hint_checkbox, LV_STATE_CHECKED);
    start_game(difficulty);
}

static void menu_clicked(lv_event_t *event)
{
    (void)event;
    show_mode_menu();
}

static void replay_clicked(lv_event_t *event)
{
    (void)event;
    start_game(s_app.difficulty);
}

static void calibrate_clicked(lv_event_t *event)
{
    (void)event;
    if (s_app.status_label != NULL) {
        lv_label_set_text(s_app.status_label, "Calibrando... manten quieto");
        lv_refr_now(NULL);
    }

    const esp_err_t err = imu_service_calibrate();
    if (s_app.status_label == NULL) {
        return;
    }

    if (err == ESP_OK) {
        lv_label_set_text(s_app.status_label, "IMU calibrada");
    } else {
        lv_label_set_text_fmt(s_app.status_label, "IMU no disponible: %s", esp_err_to_name(err));
    }
}

static void show_mode_menu(void)
{
    clear_screen();

    lv_obj_t *title = lv_label_create(s_app.root);
    lv_label_set_text(title, "Laberinto");
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 54);

    s_app.status_label = lv_label_create(s_app.root);
    if (imu_service_is_calibrated()) {
        lv_label_set_text(s_app.status_label, "Inclina el reloj para mover la bola");
    } else if (imu_service_is_available()) {
        lv_label_set_text(s_app.status_label, "IMU lista, pendiente de calibrar");
    } else {
        lv_label_set_text(s_app.status_label, "IMU no disponible");
    }
    style_label(s_app.status_label, lv_color_hex(0x9ccbd8));
#if LV_FONT_MONTSERRAT_16
    lv_obj_set_style_text_font(s_app.status_label, &lv_font_montserrat_16, LV_PART_MAIN);
#endif
    lv_obj_set_width(s_app.status_label, s_app.screen_w - 44);
    lv_obj_align(s_app.status_label, LV_ALIGN_TOP_MID, 0, 102);

    lv_obj_t *normal = create_button(s_app.root, "Modo Normal", 250, 58, normal_mode_clicked, NULL);
    lv_obj_align(normal, LV_ALIGN_CENTER, 0, -46);

    lv_obj_t *adventure = create_button(s_app.root, "Modo Aventura", 250, 58, adventure_mode_clicked, NULL);
    lv_obj_align(adventure, LV_ALIGN_CENTER, 0, 28);

    lv_obj_t *calibrate = create_button(s_app.root, "Calibrar", 150, 46, calibrate_clicked, NULL);
    lv_obj_set_style_bg_color(calibrate, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_align(calibrate, LV_ALIGN_BOTTOM_MID, 0, -42);
}

static void show_difficulty_menu(void)
{
    clear_screen();

    lv_obj_t *title = lv_label_create(s_app.root);
    lv_label_set_text_fmt(title, "%s", current_mode_name());
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 48);

    const maze_difficulty_config_t *configs = s_app.mode == MAZE_MODE_ADVENTURE ? ADVENTURE_DIFFICULTIES
                                                                               : NORMAL_DIFFICULTIES;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *button = create_button(s_app.root, configs[i].name, 240, 58,
                                         difficulty_clicked, (void *)(intptr_t)i);
        lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 116 + i * 72);
    }

    if (s_app.mode == MAZE_MODE_ADVENTURE) {
        s_app.hint_checkbox = lv_checkbox_create(s_app.root);
        lv_checkbox_set_text(s_app.hint_checkbox, "Pista hacia el agujero");
        lv_obj_set_style_text_color(s_app.hint_checkbox, lv_color_hex(0xf8fafc), LV_PART_MAIN);
        lv_obj_set_style_text_color(s_app.hint_checkbox, lv_color_hex(0xf8fafc), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s_app.hint_checkbox, lv_color_hex(0x0f172a), LV_PART_INDICATOR);
        lv_obj_set_style_border_color(s_app.hint_checkbox, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);
#if LV_FONT_MONTSERRAT_16
        lv_obj_set_style_text_font(s_app.hint_checkbox, &lv_font_montserrat_16, LV_PART_MAIN);
#endif
        lv_obj_align(s_app.hint_checkbox, LV_ALIGN_TOP_MID, 0, 342);
    }

    lv_obj_t *back = create_button(s_app.root, "Menu", 150, 44, menu_clicked, NULL);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -34);
}

static void start_game(maze_difficulty_t difficulty)
{
    s_app.difficulty = difficulty;
    clear_screen();

    if (!generate_maze()) {
        lv_obj_t *error = lv_label_create(s_app.root);
        lv_label_set_text(error, "No se pudo generar el laberinto");
        style_label(error, lv_color_hex(0xfca5a5));
        lv_obj_center(error);
        return;
    }

    s_app.ball.radius = s_app.ball_radius;
    s_app.ball.x = ((float)s_app.start_col + 0.5f) * s_app.cell_w;
    s_app.ball.y = ((float)s_app.start_row + 0.5f) * s_app.cell_h;
    s_app.ball.vx = 0.0f;
    s_app.ball.vy = 0.0f;

    s_app.board_obj = lv_obj_create(s_app.root);
    lv_obj_set_size(s_app.board_obj, s_app.screen_w, s_app.screen_h);
    lv_obj_set_pos(s_app.board_obj, 0, 0);
    lv_obj_set_style_bg_color(s_app.board_obj,
                              s_app.mode == MAZE_MODE_ADVENTURE ? lv_color_hex(0x070b12)
                                                                 : lv_color_hex(0xd9c29d),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_app.board_obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_app.board_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_app.board_obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_app.board_obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_app.board_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if (s_app.mode == MAZE_MODE_ADVENTURE) {
        update_adventure_camera();

        s_app.floor_obj = lv_obj_create(s_app.board_obj);
        lv_obj_set_style_bg_color(s_app.floor_obj, lv_color_hex(0xd9c29d), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_app.floor_obj, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_app.floor_obj, 0, LV_PART_MAIN);
        lv_obj_set_style_radius(s_app.floor_obj, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_app.floor_obj, 0, LV_PART_MAIN);
        lv_obj_clear_flag(s_app.floor_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        create_adventure_wall_pool(s_app.board_obj);

        s_app.hole_obj = lv_obj_create(s_app.board_obj);
        lv_obj_set_style_bg_color(s_app.hole_obj, lv_color_hex(0x020617), LV_PART_MAIN);
        lv_obj_set_style_border_color(s_app.hole_obj, lv_color_hex(0x475569), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_app.hole_obj, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(s_app.hole_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_clear_flag(s_app.hole_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        draw_adventure_visible(true);
        update_adventure_world_transform();
    } else {
        draw_maze(s_app.board_obj);

        const float goal_x = ((float)s_app.goal_col + 0.5f) * s_app.cell_w;
        const float goal_y = ((float)s_app.goal_row + 0.5f) * s_app.cell_h;
        s_app.hole_obj = lv_obj_create(s_app.root);
        const int hole_d = round_to_int(s_app.hole_radius * 2.0f);
        lv_obj_set_size(s_app.hole_obj, hole_d, hole_d);
        lv_obj_set_pos(s_app.hole_obj, round_to_int(goal_x - s_app.hole_radius),
                       round_to_int(goal_y - s_app.hole_radius));
        lv_obj_set_style_bg_color(s_app.hole_obj, lv_color_hex(0x020617), LV_PART_MAIN);
        lv_obj_set_style_border_color(s_app.hole_obj, lv_color_hex(0x475569), LV_PART_MAIN);
        lv_obj_set_style_border_width(s_app.hole_obj, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(s_app.hole_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_clear_flag(s_app.hole_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    s_app.ball_obj = lv_obj_create(s_app.root);
    const int ball_d = round_to_int(s_app.ball.radius * 2.0f);
    lv_obj_set_size(s_app.ball_obj, ball_d, ball_d);
    if (s_app.mode == MAZE_MODE_ADVENTURE) {
        lv_obj_set_pos(s_app.ball_obj,
                       round_to_int((float)s_app.screen_w * 0.5f - s_app.ball.radius),
                       round_to_int((float)s_app.screen_h * 0.5f - s_app.ball.radius));
    } else {
        lv_obj_set_pos(s_app.ball_obj,
                       round_to_int(s_app.ball.x - s_app.ball.radius),
                       round_to_int(s_app.ball.y - s_app.ball.radius));
    }
    lv_obj_set_style_bg_color(s_app.ball_obj, lv_color_hex(0xe11d48), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_app.ball_obj, lv_color_hex(0xfda4af), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_app.ball_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_app.ball_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_app.ball_obj, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_app.ball_obj, LV_OPA_30, LV_PART_MAIN);
    lv_obj_clear_flag(s_app.ball_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    if (s_app.mode == MAZE_MODE_ADVENTURE && s_app.hint_enabled) {
        s_app.hint_box = lv_obj_create(s_app.root);
        lv_obj_set_size(s_app.hint_box, 56, 56);
        lv_obj_align(s_app.hint_box, LV_ALIGN_TOP_RIGHT, -22, 22);
        lv_obj_set_style_bg_color(s_app.hint_box, lv_color_hex(0x0f172a), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(s_app.hint_box, LV_OPA_60, LV_PART_MAIN);
        lv_obj_set_style_border_width(s_app.hint_box, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(s_app.hint_box, lv_color_hex(0x38bdf8), LV_PART_MAIN);
        lv_obj_set_style_radius(s_app.hint_box, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_pad_all(s_app.hint_box, 0, LV_PART_MAIN);
        lv_obj_clear_flag(s_app.hint_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        s_app.hint_arrow = lv_line_create(s_app.hint_box);
        lv_obj_set_size(s_app.hint_arrow, 56, 56);
        lv_obj_set_pos(s_app.hint_arrow, 0, 0);
        lv_obj_set_style_line_color(s_app.hint_arrow, lv_color_hex(0xf8fafc), LV_PART_MAIN);
        lv_obj_set_style_line_width(s_app.hint_arrow, 5, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(s_app.hint_arrow, true, LV_PART_MAIN);
        lv_obj_clear_flag(s_app.hint_arrow, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        update_hint_arrow();
    }

    s_app.timer = lv_timer_create(game_timer_cb, 20, NULL);
    if (s_app.timer == NULL) {
        ESP_LOGE(TAG, "Failed to create game timer");
        s_app.playing = false;
        return;
    }
    s_app.last_tick_ms = lv_tick_get();
    s_app.playing = true;
}

static void show_victory(void)
{
    stop_timer();

    s_app.overlay = lv_obj_create(s_app.root);
    lv_obj_set_size(s_app.overlay, s_app.screen_w - 56, 238);
    lv_obj_set_style_bg_color(s_app.overlay, lv_color_hex(0x0f172a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_app.overlay, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_app.overlay, lv_color_hex(0x38bdf8), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_app.overlay, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_app.overlay, 28, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_app.overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_app.overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(s_app.overlay);

    lv_obj_t *title = lv_label_create(s_app.overlay);
    lv_label_set_text(title, "Victoria");
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *again = create_button(s_app.overlay, "Jugar de nuevo", 220, 54, replay_clicked, NULL);
    lv_obj_align(again, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t *menu = create_button(s_app.overlay, "Menu", 160, 46, menu_clicked, NULL);
    lv_obj_set_style_bg_color(menu, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_align(menu, LV_ALIGN_TOP_MID, 0, 158);
}

esp_err_t maze_game_start(void)
{
    memset(&s_app, 0, sizeof(s_app));

    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_app.screen_w = (int)lv_display_get_horizontal_resolution(display);
    s_app.screen_h = (int)lv_display_get_vertical_resolution(display);
    if (s_app.screen_w <= 0 || s_app.screen_h <= 0) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t boot_err = boot_button_init();
    if (boot_err != ESP_OK) {
        ESP_LOGW(TAG, "BOOT button unavailable: %s", esp_err_to_name(boot_err));
    }

    ESP_LOGI(TAG, "Starting maze game on %dx%d", s_app.screen_w, s_app.screen_h);
    show_mode_menu();
    return ESP_OK;
}
