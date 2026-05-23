/* engine.c — 2.5D raycasting engine: renderer, map generation, sprite system
 *
 * Implements DDA (Digital Differential Analyzer) raycasting to project
 * a first-person view from the Player position onto the terminal grid.
 * Also handles procedural maze generation, sprite placement, texture
 * initialisation, ncurses setup, and collision detection.
 *
 * Dependencies: engine.h (types, constants), gentex.h + skull.h + potion.h
 *               + enemies.c/enemies.h (embedded enemy sprite data).       */

#include "engine.h"
#include "gentex.h"
#include "skull.h"
#include "potion.h"
#include "enemies.h"
#include "enemy_sprite_data.h"

#ifdef _WIN32
#include <windows.h>
#endif

/* ── Trig lookup tables ─────────────────────────────────────────── */
double cos_lut[MAX_ANIM_FRAMES + 1];
double sin_lut[MAX_ANIM_FRAMES + 1];

int64_t get_time_ms(void) {
#ifdef _WIN32
    return (int64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

void init_luts(void) {
    for (int i = 0; i <= MAX_ANIM_FRAMES; i++) {
        double t = (double)i / MAX_ANIM_FRAMES;
        double a = t * M_PI / 2.0;
        cos_lut[i] = cos(a);
        sin_lut[i] = sin(a);
    }
}

/* ── Globals ──────────────────────────────────────────────────── */
volatile int running = 1;           /* cleared by SIGINT or Q key          */
unsigned char *map;                 /* MAP_W × MAP_H cell grid             */
Player player;
int xterm_pair[256];                /* xterm-256 → ncurses colour_pair     */
int ceil_pair, floor_pair;          /* ceiling/floor colour pair indices   */
int panel_pair;                     /* HUD panel colour pair               */
int flash_pair;                     /* red full-screen flash pair          */
int green_pair, blue_pair, grey_pair;  /* solid-colour overlay pairs       */
int health_pair, water_pair;        /* green/blue bar colour pairs         */

Sprite sprites[MAX_SPRITES];
int num_sprites;
int sprite_tex[NUM_SPRITE_TYPES][SPRITE_H][SPRITE_W];

static int tex_xterm[NUM_TEX][TEX_H][TEX_W];  /* wall textures mapped to xterm-256 */

/* ── Colour conversion ──────────────────────────────────────────── */

/* rgb_to_xterm: Convert an sRGB 8-bit triplet to the nearest xterm-256
 * colour index.  Pure greys (r==g==b) map to the 24-entry grey ramp
 * (232-255).  Colours map to the 6×6×6 colour cube (16-231).          */
int rgb_to_xterm(unsigned char r, unsigned char g, unsigned char b) {
    if (r == g && g == b) {
        /* Greys: thresholds between the 24 grey-level entries (232-255) */
        if (r < 8) return 232;
        int xt = (r * 23 + 128) / 255 + 232;
        if (xt < 232) xt = 232;
        if (xt > 255) xt = 255;
        return xt;
    }
    /* Map to the 6×6×6 colour cube (16-231) */
    int r6 = (r * 5 + 127) / 255;
    int g6 = (g * 5 + 127) / 255;
    int b6 = (b * 5 + 127) / 255;
    return 16 + r6 * 36 + g6 * 6 + b6;
}

/* ── Maze validation ────────────────────────────────────────────── */

/* check_borders: Ensure a cell isn't opening a 2×2 empty area.
 * During random-walk carving, 2×2 open areas would look unnatural,
 * so this predicate rejects steps that would create one.
 * Returns 1 if the cell (x,y) is safe to carve, 0 otherwise.       */
static int check_borders(int x, int y) {
    if (x < 1 || x > MAP_W - 2 || y < 1 || y > MAP_H - 2) return 0;
    if (get_map(x + 1, y) == 0 && get_map(x, y + 1) == 0 && get_map(x + 1, y + 1) == 0) return 0;
    if (get_map(x + 1, y) == 0 && get_map(x, y - 1) == 0 && get_map(x + 1, y - 1) == 0) return 0;
    if (get_map(x - 1, y) == 0 && get_map(x, y - 1) == 0 && get_map(x - 1, y - 1) == 0) return 0;
    if (get_map(x - 1, y) == 0 && get_map(x, y + 1) == 0 && get_map(x - 1, y + 1) == 0) return 0;
    return 1;
}

/* ── Texture initialisation ─────────────────────────────────────── */

/* init_tex: Generate all 40 wall textures (4 colours × 2 styles × 5
 * variations), all sprite textures (water, 4 worms, 4 potions), and
 * the skeleton texture from embedded RGB data.  Every distinct xterm-256
 * colour found is registered as an ncurses colour pair on first use.   */
void init_tex(void) {
    unsigned char *rgb = malloc(TEX_W * TEX_H * 3);
    int pair_next = 1;

    for (int i = 0; i < 256; i++) xterm_pair[i] = -1;

    /* ── Generate 40 wall textures ─────────────────────────────── */
    int tex_idx = 0;
    for (int ct = 0; ct < NUM_COLOUR_TYPES; ct++) {
        for (int ts = 0; ts < NUM_TEXTURE_STYLES; ts++) {
            for (int var = 0; var < VARIANTS_PER_TYPE; var++, tex_idx++) {
                /* Generate the base stone or brick pattern */
                if (ts == 0)
                    gen_texture(rgb, ct);
                else
                    gen_brick_texture(rgb, ct);

                /* 5th variation: composite a skull over the texture */
                if (var == 4) {
                    int dim = TEX_W * 65 / 100;
                    int off = (TEX_W - dim) / 2;
                    for (int ty = off; ty < off + dim; ty++)
                        for (int tx = off; tx < off + dim; tx++) {
                            int sy = (ty - off) * SKULL_H / dim;
                            int sx = (tx - off) * SKULL_W / dim;
                            if (skull_opaqueness[sy * SKULL_W + sx]) {
                                int g = skull_colour[sy * SKULL_W + sx];
                                int idx = (ty * TEX_W + tx) * 3;
                                rgb[idx]     = g;
                                rgb[idx + 1] = g;
                                rgb[idx + 2] = g;
                            }
                        }
                }

                /* Quantise texture pixels to xterm-256, register colour pairs */
                for (int y = 0; y < TEX_H; y++) {
                    for (int x = 0; x < TEX_W; x++) {
                        int idx = (y * TEX_W + x) * 3;
                        int xt = rgb_to_xterm(rgb[idx], rgb[idx + 1], rgb[idx + 2]);
                        tex_xterm[tex_idx][y][x] = xt;
                        if (xterm_pair[xt] < 0) {
                            xterm_pair[xt] = pair_next++;
                            init_pair(xterm_pair[xt], xt, COLOR_BLACK);
                        }
                    }
                }
            }
        }
    }
    free(rgb);

    /* ── Generate sprite textures ──────────────────────────────── */
    unsigned char *spr_rgb = malloc(SPRITE_W * SPRITE_H * 3);

    /* Water puddle sprite */
    gen_water_sprite(spr_rgb);
    for (int y = 0; y < SPRITE_H; y++)
        for (int x = 0; x < SPRITE_W; x++) {
            int idx = (y * SPRITE_W + x) * 3;
            unsigned char r = spr_rgb[idx], g = spr_rgb[idx+1], b = spr_rgb[idx+2];
            if (r == 0 && g == 0 && b == 0) {
                sprite_tex[SPRITE_WATER][y][x] = -1;  /* transparent */
            } else {
                int xt = rgb_to_xterm(r, g, b);
                sprite_tex[SPRITE_WATER][y][x] = xt;
                if (xterm_pair[xt] < 0) {
                    xterm_pair[xt] = pair_next++;
                    init_pair(xterm_pair[xt], xt, COLOR_BLACK);
                }
            }
        }

    /* Worm sprites (4 colour variants) */
    for (int pt = 0; pt < 4; pt++) {
        int sp_type = SPRITE_WORM_BLUE + pt;
        gen_worm_texture(spr_rgb, pt);
        for (int y = 0; y < SPRITE_H; y++)
            for (int x = 0; x < SPRITE_W; x++) {
                int idx = (y * SPRITE_W + x) * 3;
                unsigned char r = spr_rgb[idx], g = spr_rgb[idx+1], b = spr_rgb[idx+2];
                if (r == 0 && g == 0 && b == 0) {
                    sprite_tex[sp_type][y][x] = -1;
                } else {
                    int xt = rgb_to_xterm(r, g, b);
                    sprite_tex[sp_type][y][x] = xt;
                    if (xterm_pair[xt] < 0) {
                        xterm_pair[xt] = pair_next++;
                        init_pair(xterm_pair[xt], xt, COLOR_BLACK);
                    }
                }
            }
    }

    /* Potion sprites (4 colour variants) */
    for (int pt = 0; pt < 4; pt++) {
        int sp_type = SPRITE_POTION_BLUE + pt;
        gen_potion_texture(spr_rgb, pt);
        for (int y = 0; y < SPRITE_H; y++)
            for (int x = 0; x < SPRITE_W; x++) {
                int idx = (y * SPRITE_W + x) * 3;
                unsigned char r = spr_rgb[idx], g = spr_rgb[idx+1], b = spr_rgb[idx+2];
                if (r == 0 && g == 0 && b == 0) {
                    sprite_tex[sp_type][y][x] = -1;
                } else {
                    int xt = rgb_to_xterm(r, g, b);
                    sprite_tex[sp_type][y][x] = xt;
                    if (xterm_pair[xt] < 0) {
                        xterm_pair[xt] = pair_next++;
                        init_pair(xterm_pair[xt], xt, COLOR_BLACK);
                    }
                }
            }
    }

    /* Enemy shade profiles: all families use grey ramp. */
    static const int enemy_profile_xt[4][9] = {
        { -1, 232, 234, 236, 238, 240, 242, 244, 246 },
        { -1, 232, 234, 236, 238, 240, 242, 244, 246 },
        { -1, 232, 234, 236, 238, 240, 242, 244, 246 },
        { -1, 232, 234, 236, 238, 240, 242, 244, 246 },
    };
    for (int p = 0; p < 4; p++) {
        for (int g = 0; g <= 8; g++) {
            enemy_profile_colors[p][g] = enemy_profile_xt[p][g];
            int xt = enemy_profile_xt[p][g];
            if (xt >= 0 && xterm_pair[xt] < 0) {
                xterm_pair[xt] = pair_next++;
                init_pair(xterm_pair[xt], xt, COLOR_BLACK);
            }
        }
    }

    free(spr_rgb);

     /* ── Environment colour pairs ─────────────────────────────── */
     ceil_pair = pair_next++;
     floor_pair = pair_next++;
     panel_pair = pair_next++;
     init_pair(ceil_pair, COLOR_BLACK, 18);
     init_pair(floor_pair, COLOR_BLACK, 232);
     init_pair(panel_pair, COLOR_WHITE, COLOR_BLACK);
     flash_pair = pair_next++;
     init_pair(flash_pair, COLOR_RED, COLOR_RED);
     green_pair = pair_next++;
     blue_pair = pair_next++;
     grey_pair = pair_next++;
     init_pair(green_pair, COLOR_GREEN, COLOR_GREEN);
     init_pair(blue_pair, COLOR_BLUE, COLOR_BLUE);
     init_pair(grey_pair, 242, 242);
     health_pair = pair_next++;
     water_pair = pair_next++;
     init_pair(health_pair, COLOR_RED, COLOR_BLACK);
     init_pair(water_pair, COLOR_BLUE, COLOR_BLACK);
}

/* ── Player initialisation ──────────────────────────────────────── */

/* init_player: Place the player at the centre of the grid and pick a
 * random facing direction that points at an empty (carved) cell.     */
void init_player(void) {
    player.x = MIDDLE_W + 0.5;
    player.y = MIDDLE_H + 0.5;

    /* Shuffle the 4 cardinal directions and try each one */
    int order[] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }

    double dirs[][2] = {{1,0}, {0,1}, {-1,0}, {0,-1}};
    for (int i = 0; i < 4; i++) {
        int d = order[i];
        int tx = (int)player.x + (int)dirs[d][0];
        int ty = (int)player.y + (int)dirs[d][1];
        if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H && get_map(tx, ty) == 0) {
            player.dir_x = dirs[d][0];
            player.dir_y = dirs[d][1];
            player.plane_x = dirs[d][1] * 0.767;    /* FOV factor ≈ tan(38°) */
            player.plane_y = -dirs[d][0] * 0.767;
            return;
        }
    }

    /* Fallback — should not be reached */
    player.dir_x = 1.0;
    player.dir_y = 0.0;
    player.plane_x = 0.0;
    player.plane_y = -0.767;
}

/* ── Maze generation ────────────────────────────────────────────── */

/* create_lab: Generate a procedural maze.
 *   1. Divide the map into 8×8 zones, each with a random colour/style.
 *   2. Carve a random-walk path through the centre for `num` steps.
 *   3. Scatter sprites (15% chance per empty cell) across 6 types.
 *
 * This is the "legacy" generator used by maze3d-demo.  The interactive
 * game uses its own generator in maze3d.c (generate_maze).           */
void create_lab(int num) {
    /* ── Step 1: Fill map with wall tiles ──────────────────────── */
    int n = 0, x = MIDDLE_W, y = MIDDLE_H;

    for (int sy = 0; sy < MAP_H; sy += 8)
        for (int sx = 0; sx < MAP_W; sx += 8) {
            int ct = rand() % NUM_COLOUR_TYPES;
            int ts = rand() % NUM_TEXTURE_STYLES;
            int zone = ct * NUM_TEXTURE_STYLES + ts;
            for (int dy = 0; dy < 8; dy++)
                for (int dx = 0; dx < 8; dx++) {
                    int var = (rand() % 10 == 0) ? 4 : rand() % 4;
                    set_map(sx + dx, sy + dy, zone * VARIANTS_PER_TYPE + 1 + var);
                }
        }

    /* ── Step 2: Carve a starting area in the centre ───────────── */
    for (int sy = MIDDLE_H - 4; sy < MIDDLE_H + 4; sy++)
        for (int sx = MIDDLE_W - 4; sx < MIDDLE_W + 4; sx++) {
            int var = (rand() % 10 == 0) ? 4 : rand() % 4;
            set_map(sx, sy, 3 * VARIANTS_PER_TYPE + 1 + var);
        }

    /* ── Step 3: Random-walk carving ───────────────────────────── */
    for (; n < num; n++) {
        if (get_map(x, y) > 0) set_map(x, y, 0);
        int nx = x, ny = y;
        switch (rand() & 3) {
            case 0: nx = x + 1; break;
            case 1: nx = x - 1; break;
            case 2: ny = y + 1; break;
            case 3: ny = y - 1; break;
        }
        if (check_borders(nx, ny)) { x = nx; y = ny; }
    }

    /* ── Step 4: Place sprites ─────────────────────────────────── */
    /* Enemy type table for random selection */
    static const int enemy_pool[] = {
        ENEMY_SKELETON_L1, ENEMY_SKELETON_L2, ENEMY_SKELETON_L3,
        ENEMY_SKELETON_L4, ENEMY_SKELETON_L5,
        ENEMY_ORC_L1, ENEMY_ORC_L2, ENEMY_ORC_L3,
        ENEMY_ORC_L4, ENEMY_ORC_L5,
        ENEMY_CULTIST_L1, ENEMY_CULTIST_L2, ENEMY_CULTIST_L3,
        ENEMY_CULTIST_L4, ENEMY_CULTIST_L5,
        ENEMY_BOSS
    };
#define NUM_ENEMY_POOL 16
    static const int sprite_types[] = {
        SPRITE_WATER,
        SPRITE_POTION_BLUE, SPRITE_POTION_RED,
        SPRITE_POTION_PINK, SPRITE_POTION_GREEN,
        SPRITE_ENEMY
    };
    num_sprites = 0;
    for (int sy = 0; sy < MAP_H && num_sprites < MAX_SPRITES; sy++)
        for (int sx = 0; sx < MAP_W && num_sprites < MAX_SPRITES; sx++)
            if (get_map(sx, sy) == 0 && (rand() % 100) < 15) {
                sprites[num_sprites].x = sx + 0.5;
                sprites[num_sprites].y = sy + 0.5;
                sprites[num_sprites].z = 0.0;
                sprites[num_sprites].level = 0;
                int st = sprite_types[rand() % 6];
                sprites[num_sprites].type = st;
                if (st == SPRITE_ENEMY) {
                    sprites[num_sprites].z = 0.4;
                    sprites[num_sprites].level = enemy_pool[rand() % NUM_ENEMY_POOL];
                }
                num_sprites++;
            }
#undef NUM_ENEMY_POOL
}

/* ── ncurses initialisation ─────────────────────────────────────── */

/* init_ncurses: Initialise the terminal for full-screen colour
 * rendering.  Requires a 256-colour xterm-compatible terminal.
 * Sets non-blocking input, hidden cursor, and keypad mode.          */
void init_ncurses(void) {
    setlocale(LC_ALL, "");
    initscr();
    start_color();
    use_default_colors();
    curs_set(0);
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);

    if (COLORS < 256) {
        endwin();
        fprintf(stderr, "256-color terminal required\n");
        exit(1);
    }
}

/* ── Cleanup ─────────────────────────────────────────────────────── */

/* cleanup: Standard ncurses shutdown (called via atexit).           */
void cleanup(void) {
    endwin();
}

/* handle_signal: SIGINT handler — sets running = 0 for a clean exit. */
void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

/* ── Collision detection ──────────────────────────────────────────── */

/* can_move_to: Check whether a (sub-cell) position is walkable.
 * Uses a margin of 0.15 cells around the point to avoid wall
 * grazing.  Returns 1 if the area is free of wall tiles, else 0.    */
int can_move_to(double nx, double ny) {
    double m = 0.15;
    int x0 = (int)(nx - m), x1 = (int)(nx + m);
    int y0 = (int)(ny - m), y1 = (int)(ny + m);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (x >= 0 && x < MAP_W && y >= 0 && y < MAP_H) {
                int v = get_map(x, y);
                if (v >= 1 && v <= NUM_TEX) return 0;
            }
    return 1;
}

/* ── Frame rendering ──────────────────────────────────────────────── */

/* render_frame: Render a single frame — fixed-point DDA with trig LUTs
 *
 * 1. Cast 1 ray per RAY_STEP columns using int64_t fixed-point DDA.
 * 2. Stretch each wall slice across RAY_STEP screen columns.
 * 3. Precompute tex_step for each column to avoid per-pixel division.
 * 4. Render ceiling/floor as single mvwhline batches.
 * 5. Sort sprites with qsort; precompute sprite tex_step per sprite.
 */
void render_frame(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    if (rows < 1 || cols < 1) return;

    static const char *SHADES[] = {
        "\xe2\x96\x88", "\xe2\x96\x93",
        "\xe2\x96\x92", "\xe2\x96\x91", " "
    };

    int nrays = (cols + RAY_STEP - 1) / RAY_STEP;
    if (nrays < 1) nrays = 1;

    /* Precompute player fixed-point state */
    fp_t fp_px = double_to_fp(player.x);
    fp_t fp_py = double_to_fp(player.y);
    fp_t fp_pdx = double_to_fp(player.dir_x);
    fp_t fp_pdy = double_to_fp(player.dir_y);
    fp_t fp_ppx = double_to_fp(player.plane_x);
    fp_t fp_ppy = double_to_fp(player.plane_y);

    /* Per-column perpendicular distance for sprite occlusion */
    fp_t perp_buf_fp[cols];

    /* ── WALL RENDERING ────────────────────────────────────────── */

    /* Ray LUTs (one entry per unique ray) */
    fp_t ray_dir_x_fp[nrays];
    fp_t ray_dir_y_fp[nrays];
    fp_t delta_x_fp[nrays];
    fp_t delta_y_fp[nrays];
    int   step_x[nrays];
    int   step_y[nrays];

    for (int r = 0; r < nrays; r++) {
        int x_col = r * RAY_STEP;
        if (x_col >= cols) { nrays = r; break; }
        fp_t cam_x_fp = (int_to_fp(2 * x_col)) / cols - FP_ONE;
        ray_dir_x_fp[r] = fp_pdx + ((fp_ppx * cam_x_fp) >> FP_SHIFT);
        ray_dir_y_fp[r] = fp_pdy + ((fp_ppy * cam_x_fp) >> FP_SHIFT);

        delta_x_fp[r] = (ray_dir_x_fp[r] == 0)
                        ? INT64_MAX
                        : llabs((FP_ONE * FP_SCALE) / ray_dir_x_fp[r]);
        delta_y_fp[r] = (ray_dir_y_fp[r] == 0)
                        ? INT64_MAX
                        : llabs((FP_ONE * FP_SCALE) / ray_dir_y_fp[r]);
        step_x[r] = (ray_dir_x_fp[r] >= 0) ? 1 : -1;
        step_y[r] = (ray_dir_y_fp[r] >= 0) ? 1 : -1;
    }

    /* ── Cast rays ─────────────────────────────────────────────── */
    for (int r = 0; r < nrays; r++) {
        int map_x = fp_to_int(fp_px);
        int map_y = fp_to_int(fp_py);

        /* Initial side distances */
        fp_t side_x_fp, side_y_fp;
        fp_t cell_frac_x_fp = fp_px - int_to_fp(map_x);
        fp_t cell_frac_y_fp = fp_py - int_to_fp(map_y);

        if (delta_x_fp[r] >= INT64_MAX / 2)
            side_x_fp = INT64_MAX;
        else if (step_x[r] < 0)
            side_x_fp = (cell_frac_x_fp * delta_x_fp[r]) >> FP_SHIFT;
        else
            side_x_fp = ((FP_ONE - cell_frac_x_fp) * delta_x_fp[r]) >> FP_SHIFT;

        if (delta_y_fp[r] >= INT64_MAX / 2)
            side_y_fp = INT64_MAX;
        else if (step_y[r] < 0)
            side_y_fp = (cell_frac_y_fp * delta_y_fp[r]) >> FP_SHIFT;
        else
            side_y_fp = ((FP_ONE - cell_frac_y_fp) * delta_y_fp[r]) >> FP_SHIFT;

        /* DDA */
        int hit = 0, side = 0, wall_type = 0, dda_step = 20;
        while (hit == 0 && dda_step-- > 0) {
            if (side_x_fp < side_y_fp) {
                side_x_fp += delta_x_fp[r];
                map_x += step_x[r];
                side = 0;
            } else {
                side_y_fp += delta_y_fp[r];
                map_y += step_y[r];
                side = 1;
            }
            if (map_y < 0 || map_y >= MAP_H ||
                map_x < 0 || map_x >= MAP_W) break;
            wall_type = get_map(map_x, map_y);
            if (wall_type >= 1 && wall_type <= NUM_TEX) hit = 1;
        }

        /* Perpendicular distance */
        fp_t perp_dist_fp;
        if (hit == 0) {
            perp_dist_fp = INT64_MAX;
        } else if (side == 0) {
            fp_t num = int_to_fp(map_x) - fp_px + (step_x[r] == -1 ? FP_ONE : 0);
            perp_dist_fp = (ray_dir_x_fp[r] == 0) ? INT64_MAX : (num * FP_SCALE) / ray_dir_x_fp[r];
        } else {
            fp_t num = int_to_fp(map_y) - fp_py + (step_y[r] == -1 ? FP_ONE : 0);
            perp_dist_fp = (ray_dir_y_fp[r] == 0) ? INT64_MAX : (num * FP_SCALE) / ray_dir_y_fp[r];
        }

        if (perp_dist_fp < double_to_fp(0.01)) perp_dist_fp = double_to_fp(0.01);

        /* Stretch across RAY_STEP columns */
        int col_start = r * RAY_STEP;
        int col_end = col_start + RAY_STEP - 1;
        if (col_end >= cols) col_end = cols - 1;

        /* Miss or too-far: fill columns with ceiling/floor */
        if (hit == 0 || perp_dist_fp > double_to_fp(12.0)) {
            if (hit == 0) perp_dist_fp = INT64_MAX;
            for (int x = col_start; x <= col_end; x++) {
                perp_buf_fp[x] = perp_dist_fp;
                int m = rows / 2;
                attrset(COLOR_PAIR(ceil_pair));
                mvvline(0, x, ' ', m);
                attrset(COLOR_PAIR(floor_pair));
                mvvline(m, x, ' ', rows - m);
            }
            continue;
        }

        /* Record per-column distance */
        for (int x = col_start; x <= col_end; x++)
            perp_buf_fp[x] = perp_dist_fp;

        /* Wall slice height */
        int wall_h = (int)((int64_t)rows * FP_SCALE / perp_dist_fp);
        if (wall_h < 1) wall_h = 1;
        int draw_start = -wall_h / 2 + rows / 2;
        int draw_end   =  wall_h / 2 + rows / 2;
        if (draw_start < 0) draw_start = 0;
        if (draw_end >= rows) draw_end = rows - 1;
        if (draw_end < draw_start) continue;

        /* Texture x-coordinate */
        fp_t wall_x_fp;
        if (side == 0)
            wall_x_fp = fp_py + ((perp_dist_fp * ray_dir_y_fp[r]) >> FP_SHIFT);
        else
            wall_x_fp = fp_px + ((perp_dist_fp * ray_dir_x_fp[r]) >> FP_SHIFT);
        int tex_x = (int)(((wall_x_fp & (FP_ONE - 1)) * TEX_W) >> FP_SHIFT);
        if (side == 0 && fp_to_int(ray_dir_x_fp[r]) > 0) tex_x = TEX_W - tex_x - 1;
        if (side == 1 && fp_to_int(ray_dir_y_fp[r]) < 0) tex_x = TEX_W - tex_x - 1;
        if (tex_x < 0) tex_x = 0;
        if (tex_x >= TEX_W) tex_x = TEX_W - 1;

        /* Shade level */
        int shade;
        if (perp_dist_fp < double_to_fp(1.5)) shade = 0;
        else if (perp_dist_fp < double_to_fp(2.5)) shade = 1;
        else if (perp_dist_fp < double_to_fp(4.0)) shade = 2;
        else if (perp_dist_fp < double_to_fp(7.0)) shade = 3;
        else shade = 4;

        int tex_id = wall_type - 1;
        if (tex_id < 0) tex_id = 0;
        if (tex_id >= NUM_TEX) tex_id = 0;

        /* Precompute tex_y step for this column (no per-pixel division) */
        int tex_step_fp = (TEX_H * FP_SCALE) / wall_h;
        int tex_y_fp = (draw_start - rows / 2 + wall_h / 2) * tex_step_fp;

        /* Draw ceiling & floor as single batched lines */
        for (int x = col_start; x <= col_end; x++) {
            attrset(COLOR_PAIR(ceil_pair));
            if (draw_start > 0)
                mvvline(0, x, ' ', draw_start);

            /* Draw wall slice */
            int y_tex = tex_y_fp;
            for (int y = draw_start; y <= draw_end; y++) {
                int tex_y = y_tex >> FP_SHIFT;
                if (tex_y < 0) tex_y = 0;
                if (tex_y >= TEX_H) tex_y = TEX_H - 1;

                int xt = tex_xterm[tex_id][tex_y][tex_x];
                int pair = xterm_pair[xt];
                if (pair < 0) pair = ceil_pair;

                int fade_extra = 0;
                if (tex_y < 4) {
                    fade_extra = (3 - tex_y) * 4 / 3;
                    pair = ceil_pair;
                } else if (tex_y >= TEX_H - 4) {
                    fade_extra = (tex_y - (TEX_H - 4)) * 4 / 3;
                    pair = floor_pair;
                }
                int s = shade + fade_extra;
                if (s > 4) s = 4;

                attrset(COLOR_PAIR(pair));
                mvaddstr(y, x, SHADES[s]);
                y_tex += tex_step_fp;
            }

            /* Floor */
            if (draw_end + 1 < rows) {
                attrset(COLOR_PAIR(floor_pair));
                mvvline(draw_end + 1, x, ' ', rows - draw_end - 1);
            }
        }
    }

    /* ── SPRITE RENDERING ──────────────────────────────────────── */
    if (num_sprites <= 0) return;

    double inv_det = 1.0 / (player.plane_x * player.dir_y
                          - player.dir_x * player.plane_y);

    /* Pre-filter & collect visible sprites */
    int vis_idx[MAX_SPRITES];
    double vis_dist[MAX_SPRITES];
    int nvis = 0;
    for (int i = 0; i < num_sprites; i++) {
        double dx = sprites[i].x - player.x;
        double dy = sprites[i].y - player.y;
        double ty = inv_det * (-player.plane_y * dx + player.plane_x * dy);
        if (ty <= 0 || ty > 12.0) continue;
        vis_idx[nvis] = i;
        vis_dist[nvis] = dx * dx + dy * dy;
        nvis++;
    }
    if (nvis <= 0) return;

    /* qsort by distance (farthest first) — shell sort for mixed sizes */
    static int sort_buf[MAX_SPRITES];
    if (nvis > 1) {
        for (int i = 0; i < nvis; i++) sort_buf[i] = i;
        if (nvis < 20) {
            for (int i = 1; i < nvis; i++)
                for (int j = i; j > 0 && vis_dist[sort_buf[j]] > vis_dist[sort_buf[j-1]]; j--) {
                    int t = sort_buf[j]; sort_buf[j] = sort_buf[j-1]; sort_buf[j-1] = t;
                }
        } else {
            for (int gap = nvis/2; gap > 0; gap /= 2)
                for (int i = gap; i < nvis; i++)
                    for (int j = i - gap; j >= 0 && vis_dist[sort_buf[j+gap]] > vis_dist[sort_buf[j]]; j -= gap) {
                        int t = sort_buf[j]; sort_buf[j] = sort_buf[j+gap]; sort_buf[j+gap] = t;
                    }
        }
    }

    /* Render each sprite */
    for (int si = 0; si < nvis; si++) {
        int sp_idx = (nvis > 1) ? vis_idx[sort_buf[si]] : vis_idx[si];
        Sprite *sp = &sprites[sp_idx];
        double rel_x = sp->x - player.x;
        double rel_y = sp->y - player.y;

        double tx = inv_det * (player.dir_y * rel_x - player.dir_x * rel_y);
        double ty = inv_det * (-player.plane_y * rel_x + player.plane_x * rel_y);

        int s_shade;
        if (ty < 1.5) s_shade = 0;
        else if (ty < 2.5) s_shade = 1;
        else if (ty < 4.0) s_shade = 2;
        else if (ty < 7.0) s_shade = 3;
        else s_shade = 4;

        int screen_x = (int)((cols / 2) * (1.0 + tx / ty));
        int sprite_h = abs((int)(rows / ty)) / 2;
        if (sprite_h < 1) sprite_h = 1;
        int sprite_w = sprite_h;
        if (sp->type >= SPRITE_WORM_FIRST) {
            sprite_h = sprite_h * 3 / 2;
            sprite_w = sprite_w * 3 / 2;
        }
        int is_enemy = (sp->type == SPRITE_ENEMY);
        if (is_enemy) {
            sprite_h = sprite_h * 5 / 4;
            sprite_w = sprite_w * 5 / 4;
            if (sp->level == ENEMY_BOSS) {
                sprite_h = sprite_h * 6 / 5;
                sprite_w = sprite_w * 6 / 5;
            }
        }

        int v_shift = (int)((0.5 - sp->z) * sprite_h);
        int center_y = rows / 2 + v_shift;
        int orig_start_y = center_y - sprite_h / 2;
        int orig_end_y = orig_start_y + sprite_h - 1;
        if (orig_start_y >= rows || orig_end_y < 0) continue;
        int draw_start_y = orig_start_y < 0 ? 0 : orig_start_y;
        int draw_end_y = orig_end_y >= rows ? rows - 1 : orig_end_y;

        int orig_start_x = -sprite_w / 2 + screen_x;
        int orig_end_x = orig_start_x + sprite_w - 1;
        if (orig_start_x >= cols || orig_end_x < 0) continue;
        int draw_start_x = orig_start_x < 0 ? 0 : orig_start_x;
        int draw_end_x = orig_end_x >= cols ? cols - 1 : orig_end_x;

        int tex_w = is_enemy ? SKELE_W : SPRITE_W;
        int tex_h = is_enemy ? SKELE_H : SPRITE_H;

        /* Precompute texture step per pixel (no per-pixel division) */
        int tex_step_x_fp = (tex_w * FP_SCALE) / sprite_w;
        int tex_step_y_fp = (tex_h * FP_SCALE) / sprite_h;

        for (int col = draw_start_x; col <= draw_end_x; col++) {
            int tex_x = ((col - orig_start_x) * tex_step_x_fp) >> FP_SHIFT;
            if (tex_x < 0) tex_x = 0;
            if (tex_x >= tex_w) tex_x = tex_w - 1;
            /* Occlusion check — skip if behind a wall */
            if (ty >= (double)perp_buf_fp[col] / FP_SCALE) continue;

            /* Precompute tex_y base for this column */
            int tex_y_fp = (draw_start_y - orig_start_y) * tex_step_y_fp;
            const char *shade_str = SHADES[s_shade];

            for (int row = draw_start_y; row <= draw_end_y; row++) {
                int tex_y = tex_y_fp >> FP_SHIFT;
                if (tex_y < 0) tex_y = 0;
                if (tex_y >= tex_h) tex_y = tex_h - 1;

                int xt;
                if (is_enemy) {
                    int si = enemy_sprite_idx[sp->level];
                    int ri = enemy_color_ramp_idx[sp->level];
                    int grey = enemy_sprite_ptrs[si][tex_y * SKELE_W + tex_x];
                    if (grey == 0) { tex_y_fp += tex_step_y_fp; continue; }
                    int dl = enemy_info[sp->level].level;
                    if (dl > 0 && dl % 2 == 0) grey = grey > 1 ? grey - 1 : 1;
                    xt = enemy_profile_colors[ri][grey];
                } else {
                    xt = sprite_tex[sp->type][tex_y][tex_x];
                }
                if (xt < 0) { tex_y_fp += tex_step_y_fp; continue; }

                int pair = xterm_pair[xt];
                if (pair < 0) pair = ceil_pair;
                attrset(COLOR_PAIR(pair));
                mvaddstr(row, col, shade_str);
                tex_y_fp += tex_step_y_fp;
            }
        }
    }
}
