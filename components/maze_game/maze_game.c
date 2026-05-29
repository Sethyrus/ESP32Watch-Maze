#include "maze_game.h"

#include <math.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_random.h"
#include "imu_service.h"
#include "lvgl.h"

static const char *TAG = "maze_game";

#define MAZE_MAX_ROWS 20
#define MAZE_MAX_COLS 20
#define MAZE_MAX_CELLS (MAZE_MAX_ROWS * MAZE_MAX_COLS)

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
    lv_obj_t *root;
    lv_obj_t *status_label;
    lv_obj_t *ball_obj;
    lv_obj_t *overlay;
    lv_timer_t *timer;
    int screen_w;
    int screen_h;
    int cols;
    int rows;
    int start_col;
    int start_row;
    int goal_col;
    int goal_row;
    float cell_w;
    float cell_h;
    float cell_size;
    float wall_thickness;
    float ball_radius;
    float hole_radius;
    maze_ball_t ball;
    maze_difficulty_t difficulty;
    uint32_t seed;
    uint32_t rng;
    uint32_t last_tick_ms;
    bool playing;
    maze_cell_t cells[MAZE_MAX_ROWS][MAZE_MAX_COLS];
} maze_app_t;

static const maze_difficulty_config_t DIFFICULTIES[] = {
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

static maze_app_t s_app;

static void show_mode_menu(void);
static void show_difficulty_menu(void);
static void start_game(maze_difficulty_t difficulty);
static void show_victory(void);

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
}

static void clear_screen(void)
{
    stop_timer();

    s_app.ball_obj = NULL;
    s_app.overlay = NULL;
    s_app.status_label = NULL;

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
    int active_rows[MAZE_MAX_CELLS];
    int active_cols[MAZE_MAX_CELLS];
    int active_count = 0;

    cell_at(s_app.start_row, s_app.start_col)->visited = true;
    active_rows[active_count] = s_app.start_row;
    active_cols[active_count] = s_app.start_col;
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

        active_rows[active_count] = nr;
        active_cols[active_count] = nc;
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
    int queue[MAZE_MAX_CELLS];
    int distance[MAZE_MAX_CELLS];
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
    queue[tail++] = start_index;
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
            queue[tail++] = next_index;
        }
    }

    const int solution_len = distance[goal_index];
    const int min_solution = (int)((float)valid_count * cfg->min_solution_ratio);
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
            cell->valid = cell_inside_safe_screen(row, col);
            cell->visited = false;
        }
    }
}

static bool generate_maze(void)
{
    const maze_difficulty_config_t *cfg = &DIFFICULTIES[s_app.difficulty];
    s_app.cols = cfg->cols;
    s_app.rows = cfg->rows;
    s_app.cell_w = (float)s_app.screen_w / (float)s_app.cols;
    s_app.cell_h = (float)s_app.screen_h / (float)s_app.rows;
    s_app.cell_size = fminf(s_app.cell_w, s_app.cell_h);
    s_app.wall_thickness = clamp_float(s_app.cell_size * 0.08f, 2.0f, 5.0f);
    s_app.ball_radius = s_app.cell_size * cfg->ball_radius_factor;
    s_app.hole_radius = s_app.ball_radius * 1.10f;

    for (int attempt = 0; attempt < 24; ++attempt) {
        s_app.seed = esp_random();
        if (s_app.seed == 0) {
            s_app.seed = 0x8badf00dU;
        }
        s_app.rng = s_app.seed;

        init_cells();

        const maze_corner_t start_corner = (maze_corner_t)maze_rand_range(4);
        const maze_corner_t goal_corner = opposite_corner(start_corner);
        if (!find_corner_cell(start_corner, &s_app.start_row, &s_app.start_col) ||
            !find_corner_cell(goal_corner, &s_app.goal_row, &s_app.goal_col)) {
            continue;
        }

        carve_maze(cfg);

        int solution_len = 0;
        int dead_ends = 0;
        const bool valid = validate_maze(cfg, &solution_len, &dead_ends);
        ESP_LOGI(TAG,
                 "maze attempt=%d diff=%s seed=%" PRIu32 " start=(%d,%d) goal=(%d,%d) path=%d dead=%d valid=%d",
                 attempt + 1, cfg->name, s_app.seed, s_app.start_row, s_app.start_col,
                 s_app.goal_row, s_app.goal_col, solution_len, dead_ends, valid);
        if (valid || attempt == 23) {
            return solution_len >= 0;
        }
    }

    return false;
}

static lv_obj_t *create_rect(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
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
    const int wall_t = clamp_int(round_to_int(s_app.wall_thickness), 2, 5);
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
                create_rect(parent, x0, y0, cell_w + wall_t, wall_t, wall_color);
            }
            if ((cell->walls & MAZE_WALL_LEFT) != 0) {
                create_rect(parent, x0, y0, wall_t, cell_h + wall_t, wall_color);
            }
            if ((row == s_app.rows - 1 || !cell_valid(row + 1, col)) &&
                (cell->walls & MAZE_WALL_BOTTOM) != 0) {
                create_rect(parent, x0, y1 - wall_t, cell_w + wall_t, wall_t, wall_color);
            }
            if ((col == s_app.cols - 1 || !cell_valid(row, col + 1)) &&
                (cell->walls & MAZE_WALL_RIGHT) != 0) {
                create_rect(parent, x1 - wall_t, y0, wall_t, cell_h + wall_t, wall_color);
            }
        }
    }
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

static void resolve_collision(float target_x, float target_y, float *out_x, float *out_y)
{
    const float radius = s_app.ball.radius;
    float current_x = s_app.ball.x;
    float current_y = s_app.ball.y;
    float resolved_x = target_x;

    int row = clamp_int((int)floorf(current_y / s_app.cell_h), 0, s_app.rows - 1);
    int col = clamp_int((int)floorf(current_x / s_app.cell_w), 0, s_app.cols - 1);

    if (target_x > current_x) {
        for (int guard = 0; guard < 4 && resolved_x + radius > cell_right(col); ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row, col + 1) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_RIGHT) != 0);
            if (blocked) {
                resolved_x = cell_right(col) - radius;
                s_app.ball.vx = -s_app.ball.vx * 0.08f;
                break;
            }
            ++col;
        }
    } else if (target_x < current_x) {
        for (int guard = 0; guard < 4 && resolved_x - radius < cell_left(col); ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row, col - 1) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_LEFT) != 0);
            if (blocked) {
                resolved_x = cell_left(col) + radius;
                s_app.ball.vx = -s_app.ball.vx * 0.08f;
                break;
            }
            --col;
        }
    }

    resolved_x = clamp_float(resolved_x, radius, (float)s_app.screen_w - radius);

    float resolved_y = target_y;
    col = clamp_int((int)floorf(resolved_x / s_app.cell_w), 0, s_app.cols - 1);
    row = clamp_int((int)floorf(current_y / s_app.cell_h), 0, s_app.rows - 1);

    if (target_y > current_y) {
        for (int guard = 0; guard < 4 && resolved_y + radius > cell_bottom(row); ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row + 1, col) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_BOTTOM) != 0);
            if (blocked) {
                resolved_y = cell_bottom(row) - radius;
                s_app.ball.vy = -s_app.ball.vy * 0.08f;
                break;
            }
            ++row;
        }
    } else if (target_y < current_y) {
        for (int guard = 0; guard < 4 && resolved_y - radius < cell_top(row); ++guard) {
            const bool blocked = !cell_valid(row, col) || !cell_valid(row - 1, col) ||
                                 ((cell_at(row, col)->walls & MAZE_WALL_TOP) != 0);
            if (blocked) {
                resolved_y = cell_top(row) + radius;
                s_app.ball.vy = -s_app.ball.vy * 0.08f;
                break;
            }
            --row;
        }
    }

    resolved_y = clamp_float(resolved_y, radius, (float)s_app.screen_h - radius);

    if (!point_inside_safe_screen(resolved_x, resolved_y, radius + 1.0f)) {
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
    for (int i = 0; i < steps; ++i) {
        const float dt_scale = step_dt / base_dt;
        s_app.ball.vx += ax * accel_factor * step_dt;
        s_app.ball.vy += ay * accel_factor * step_dt;

        const float frame_friction = powf(friction, dt_scale);
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

    lv_obj_set_pos(s_app.ball_obj,
                   round_to_int(s_app.ball.x - s_app.ball.radius),
                   round_to_int(s_app.ball.y - s_app.ball.radius));

    if (check_win()) {
        show_victory();
    }
}

static void normal_mode_clicked(lv_event_t *event)
{
    (void)event;
    show_difficulty_menu();
}

static void difficulty_clicked(lv_event_t *event)
{
    const maze_difficulty_t difficulty = (maze_difficulty_t)(intptr_t)lv_event_get_user_data(event);
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

    lv_obj_t *normal = create_button(s_app.root, "Modo Normal", 250, 64, normal_mode_clicked, NULL);
    lv_obj_align(normal, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *calibrate = create_button(s_app.root, "Calibrar", 150, 46, calibrate_clicked, NULL);
    lv_obj_set_style_bg_color(calibrate, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_align(calibrate, LV_ALIGN_BOTTOM_MID, 0, -42);
}

static void show_difficulty_menu(void)
{
    clear_screen();

    lv_obj_t *title = lv_label_create(s_app.root);
    lv_label_set_text(title, "Dificultad");
    style_label(title, lv_color_hex(0xf8fafc));
#if LV_FONT_MONTSERRAT_28
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, LV_PART_MAIN);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 48);

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *button = create_button(s_app.root, DIFFICULTIES[i].name, 240, 58,
                                         difficulty_clicked, (void *)(intptr_t)i);
        lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 132 + i * 76);
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

    lv_obj_t *board = lv_obj_create(s_app.root);
    lv_obj_set_size(board, s_app.screen_w, s_app.screen_h);
    lv_obj_set_pos(board, 0, 0);
    lv_obj_set_style_bg_color(board, lv_color_hex(0xd9c29d), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(board, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(board, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(board, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(board, 0, LV_PART_MAIN);
    lv_obj_clear_flag(board, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    draw_maze(board);

    const float goal_x = ((float)s_app.goal_col + 0.5f) * s_app.cell_w;
    const float goal_y = ((float)s_app.goal_row + 0.5f) * s_app.cell_h;
    lv_obj_t *hole = lv_obj_create(s_app.root);
    const int hole_d = round_to_int(s_app.hole_radius * 2.0f);
    lv_obj_set_size(hole, hole_d, hole_d);
    lv_obj_set_pos(hole, round_to_int(goal_x - s_app.hole_radius), round_to_int(goal_y - s_app.hole_radius));
    lv_obj_set_style_bg_color(hole, lv_color_hex(0x020617), LV_PART_MAIN);
    lv_obj_set_style_border_color(hole, lv_color_hex(0x475569), LV_PART_MAIN);
    lv_obj_set_style_border_width(hole, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_clear_flag(hole, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_app.ball.radius = s_app.ball_radius;
    s_app.ball.x = ((float)s_app.start_col + 0.5f) * s_app.cell_w;
    s_app.ball.y = ((float)s_app.start_row + 0.5f) * s_app.cell_h;
    s_app.ball.vx = 0.0f;
    s_app.ball.vy = 0.0f;

    s_app.ball_obj = lv_obj_create(s_app.root);
    const int ball_d = round_to_int(s_app.ball.radius * 2.0f);
    lv_obj_set_size(s_app.ball_obj, ball_d, ball_d);
    lv_obj_set_pos(s_app.ball_obj,
                   round_to_int(s_app.ball.x - s_app.ball.radius),
                   round_to_int(s_app.ball.y - s_app.ball.radius));
    lv_obj_set_style_bg_color(s_app.ball_obj, lv_color_hex(0xe11d48), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_app.ball_obj, lv_color_hex(0xfda4af), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_app.ball_obj, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_app.ball_obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(s_app.ball_obj, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(s_app.ball_obj, LV_OPA_30, LV_PART_MAIN);
    lv_obj_clear_flag(s_app.ball_obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    s_app.last_tick_ms = lv_tick_get();
    s_app.playing = true;
    s_app.timer = lv_timer_create(game_timer_cb, 20, NULL);
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

    ESP_LOGI(TAG, "Starting maze game on %dx%d", s_app.screen_w, s_app.screen_h);
    show_mode_menu();
    return ESP_OK;
}
