/* maze3d.c — Interactive 2.5D terminal maze game
 *
 * Turn-based exploration of a procedurally generated 512×512 maze.
 * Controls: arrow keys / ijkl (move/turn), Space (attack/use), Q (quit).
 * Uses the shared gameplay module for all game logic.                  */

#include "gameplay.h"
#include "enemies.h"
#include "gentex.h"
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define PANEL_H 8

/* ── Demo mode (auto-pilot screensaver) ─────────────────────────── */

static int auto_active = 0;
static int64_t auto_start_ms;
static int auto_type;  /* 1=move, 2=turn */
static int auto_duration;
static int force_move = 0;
static double old_x, old_y, old_dx, old_dy, old_px, old_py;
static double new_x, new_y, new_dx, new_dy, new_px, new_py;
static unsigned char *visited;
static int target_x = -1, target_y = -1;
static int step_count = 0;

static void auto_move(double dx, double dy) {
    double nx = player.x + dx, ny = player.y + dy;
    if (!can_move_to(nx, ny)) return;
    old_x = player.x; old_y = player.y; new_x = nx; new_y = ny;
    old_dx = new_dx = player.dir_x; old_dy = new_dy = player.dir_y;
    old_px = new_px = player.plane_x; old_py = new_py = player.plane_y;
    auto_type = 1; auto_active = 1;
    auto_start_ms = get_time_ms(); auto_duration = AUTO_MOVE_MS;
}

static void auto_turn(int left) {
    old_x = new_x = player.x; old_y = new_y = player.y;
    old_dx = player.dir_x; old_dy = player.dir_y;
    old_px = player.plane_x; old_py = player.plane_y;
    if (left) {
        new_dx = -old_dy; new_dy = old_dx;
        new_px = -old_py; new_py = old_px;
    } else {
        new_dx = old_dy; new_dy = -old_dx;
        new_px = old_py; new_py = -old_px;
    }
    auto_type = 2; auto_active = 1;
    auto_start_ms = get_time_ms(); auto_duration = AUTO_TURN_MS;
}

static void auto_update(void) {
    if (!auto_active) return;
    int64_t elapsed = get_time_ms() - auto_start_ms;
    if (elapsed >= auto_duration) { elapsed = auto_duration; auto_active = 0; }
    double t = (double)elapsed / auto_duration;
    player.x = old_x + (new_x - old_x) * t;
    player.y = old_y + (new_y - old_y) * t;
    if (auto_type == 2) {
        int lut_idx = (int)(t * MAX_ANIM_FRAMES + 0.5);
        if (lut_idx > MAX_ANIM_FRAMES) lut_idx = MAX_ANIM_FRAMES;
        double ca = cos_lut[lut_idx];
        double sa = sin_lut[lut_idx];
        if (new_dx == old_dy && new_dy == -old_dx) sa = -sa;
        player.dir_x = old_dx * ca - old_dy * sa;
        player.dir_y = old_dx * sa + old_dy * ca;
        player.plane_x = old_px * ca - old_py * sa;
        player.plane_y = old_px * sa + old_py * ca;
    }
}

static int passage_len(int dx, int dy) {
    int cx = (int)player.x + dx, cy = (int)player.y + dy;
    int len = 0;
    while (len < 20) {
        if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H && get_map(cx, cy) == 0) len++;
        else break;
        cx += dx; cy += dy;
    }
    return len;
}

static int has_side_space(int dx, int dy) {
    int px = (int)player.x, py = (int)player.y;
    int cx = px + dx, cy = py + dy;
    if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) return 0;
    if (get_map(cx, cy) != 0) return 0;
    int ndirs[][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for (int i = 0; i < 4; i++) {
        int nx = cx + ndirs[i][0], ny = cy + ndirs[i][1];
        if (nx == px && ny == py) continue;
        if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H && get_map(nx, ny) == 0)
            return 1;
    }
    return 0;
}

static void pick_target(void) {
    for (int i = 0; i < 100; i++) {
        int tx = rand() % MAP_W, ty = rand() % MAP_H;
        if (get_map(tx, ty) == 0) { target_x = tx; target_y = ty; return; }
    }
    target_x = target_y = -1;
}

static int target_bias(int lx, int ly, int rx, int ry) {
    if (target_x < 0) return -1;
    if ((rand() & 1) == 0) return -1;
    double dx = target_x + 0.5 - player.x, dy = target_y + 0.5 - player.y;
    return (dx * lx + dy * ly > dx * rx + dy * ry) ? 1 : 0;
}

static void auto_think(void) {
    if (auto_active) return;

    int px = (int)player.x, py = (int)player.y;
    visited[py * MAP_W + px] = 1;

    step_count++;
    if ((step_count % 32) == 0) pick_target();

    int lx = -(int)player.dir_y, ly = (int)player.dir_x;
    int rx = (int)player.dir_y, ry = -(int)player.dir_x;

    int fw = passage_len((int)player.dir_x, (int)player.dir_y);
    if (fw == 0) {
        force_move = 0;
        int left_ok = has_side_space(lx, ly);
        int right_ok = has_side_space(rx, ry);
        if (left_ok && right_ok) {
            int tb = target_bias(lx, ly, rx, ry);
            if (tb >= 0) { force_move = 1; auto_turn(tb); return; }
            force_move = 1; auto_turn(1); return;
        }
        if (left_ok) { force_move = 1; auto_turn(1); return; }
        if (right_ok) { force_move = 1; auto_turn(0); return; }
        if (passage_len(lx, ly) > 0) { auto_turn(1); return; }
        if (passage_len(rx, ry) > 0) { auto_turn(0); return; }
        auto_turn(1);
        return;
    }

    if (force_move) {
        force_move = 0;
        auto_move(player.dir_x, player.dir_y);
        return;
    }

    int ahead_visited = 0;
    int nx = px + (int)player.dir_x, ny = py + (int)player.dir_y;
    if (nx >= 0 && nx < MAP_W && ny >= 0 && ny < MAP_H)
        ahead_visited = visited[ny * MAP_W + nx];

    if (ahead_visited) {
        int left_free = has_side_space(lx, ly) && !visited[(py + ly) * MAP_W + (px + lx)];
        int right_free = has_side_space(rx, ry) && !visited[(py + ry) * MAP_W + (px + rx)];
        if (left_free || right_free) {
            if (left_free && right_free) {
                int tb = target_bias(lx, ly, rx, ry);
                if (tb >= 0) { force_move = 1; auto_turn(tb); return; }
                if (rand() & 1) { force_move = 1; auto_turn(1); return; }
                else { force_move = 1; auto_turn(0); return; }
            } else if (left_free) {
                force_move = 1; auto_turn(1); return;
            } else {
                force_move = 1; auto_turn(0); return;
            }
        }
    }

    int can_left = has_side_space(lx, ly);
    int can_right = has_side_space(rx, ry);

    if (can_left && can_right) {
        if ((rand() % 4) == 0) {
            int tb = target_bias(lx, ly, rx, ry);
            if (tb >= 0) { force_move = 1; auto_turn(tb); return; }
            if (rand() & 1) { force_move = 1; auto_turn(1); return; }
            else { force_move = 1; auto_turn(0); return; }
        }
    } else if (can_left && (rand() % 4) == 0) {
        force_move = 1; auto_turn(1); return;
    } else if (can_right && (rand() % 4) == 0) {
        force_move = 1; auto_turn(0); return;
    }

    auto_move(player.dir_x, player.dir_y);
}

static void run_demo(void) {
    signal(SIGINT, handle_signal);

    init_ncurses();
    atexit(cleanup);

    srand(time(0));
    map = malloc(MAP_W * MAP_H);
    if (!map) { endwin(); fprintf(stderr, "malloc failed\n"); return; }
    visited = calloc(MAP_W * MAP_H, 1);
    if (!visited) { endwin(); fprintf(stderr, "malloc failed\n"); return; }

    init_tex();
    init_luts();
    create_lab(MAZE_STEPS);
    init_player();

    while (running) {
        auto_think();
        auto_update();
        render_frame();
        refresh();
        napms(16);
        int ch;
        while ((ch = getch()) != ERR)
            if (ch == 'q' || ch == 'Q') running = 0;
    }
}

/* Debug flags (set via CLI, apply to ncurses path too) */
static int dbg_start_level = 1;
static int dbg_stats[6] = {0};
static int dbg_test_enemy = -1;

/* ── Enemy name display ---------------------------------------------- */

static void draw_enemy_name(void) {
    int fx = (int)player.x + (int)player.dir_x;
    int fy = (int)player.y + (int)player.dir_y;
    const char *name = get_enemy_name_at(fx, fy);
    if (name) {
        int cols, rows;
        getmaxyx(stdscr, rows, cols);
        (void)rows;
        int len = strlen(name);
        int x = (cols - len) / 2;
        if (x < 0) x = 0;
        attrset(COLOR_PAIR(panel_pair));
        mvaddstr(1, x, name);
    }
}

/* ── HUD panel (left) + Log panel (right) --------------------------- */

static void draw_panel(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int py = rows - PANEL_H;
    if (py < 0) py = 0;
    int pw = 40;
    if (pw > cols) pw = cols;

    /* Left side — stats panel */
    attrset(COLOR_PAIR(panel_pair));
    for (int y = py; y < rows; y++) {
        for (int x = 0; x < pw; x++)
            mvaddch(y, x, ' ');
    }

    char buf[256];
    int hb = p_health * 16 / p_max_health;
    if (hb > 16) hb = 16;
    int wb = p_water * 16 / p_max_water;
    if (wb > 16) wb = 16;

    attrset(COLOR_PAIR(panel_pair));
    mvaddstr(py, 0, " HP  ");
    attrset(COLOR_PAIR(health_pair));
    for (int i = 0; i < 16; i++)
        mvaddstr(py, 5 + i, i < hb ? "\xe2\x96\x88" : "\xe2\x96\x91");
    attrset(COLOR_PAIR(panel_pair));
    snprintf(buf, sizeof(buf), " %3d/%d", p_health, p_max_health);
    mvaddstr(py, 21, buf);

    attrset(COLOR_PAIR(panel_pair));
    mvaddstr(py + 1, 0, " WTR ");
    attrset(COLOR_PAIR(water_pair));
    for (int i = 0; i < 16; i++)
        mvaddstr(py + 1, 5 + i, i < wb ? "\xe2\x96\x88" : "\xe2\x96\x91");
    attrset(COLOR_PAIR(panel_pair));
    snprintf(buf, sizeof(buf), " %3d/%d", p_water, p_max_water);
    mvaddstr(py + 1, 21, buf);

    snprintf(buf, sizeof(buf), " ATK %d  DEF %d", p_attack, (p_defense + p_toughness) >> 1);
    mvaddstr(py + 2, 0, buf);
    snprintf(buf, sizeof(buf), " STA %d  END %d  LCK %d", p_stamina, p_endurance, p_luck);
    mvaddstr(py + 3, 0, buf);
    snprintf(buf, sizeof(buf), " LEVEL %d   STEPS %d   SCORE %d",
             p_level, p_steps, get_current_score());
    mvaddstr(py + 4, 0, buf);
    if (p_spirit_turns > 0) {
        snprintf(buf, sizeof(buf), " Souls: %d  [Spirit: %dt]%s", inventory[ITEM_SOULS], p_spirit_turns,
                 p_poisoned ? "  [POISON]" : "");
    } else {
        snprintf(buf, sizeof(buf), " Souls: %d%s", inventory[ITEM_SOULS],
                 p_poisoned ? "  [POISON]" : "");
    }
    mvaddstr(py + 5, 0, buf);

    snprintf(buf, sizeof(buf), " [\xe2\x86\x90\xe2\x86\x91\xe2\x86\x92\xe2\x86\x93] Move   [SPC] Attack/Use");
    mvaddstr(py + 6, 0, buf);
    snprintf(buf, sizeof(buf), " [Q] Quit [TAB] Inventory [F1] Help");
    mvaddstr(py + 7, 0, buf);

    /* Right side — log panel */
    int rw = 60;
    int rx = cols - rw;
    if (rx < pw) rx = pw;
    if (rx + rw > cols) { rx = cols - rw; if (rx < 0) rx = 0; }

    attrset(COLOR_PAIR(panel_pair));
    for (int y = py; y < rows; y++)
        for (int x = rx; x < cols; x++)
            mvaddch(y, x, ' ');

    /* Display newest entries at bottom */
    int lh = log_head;
    for (int i = 0; i < LOG_LINES && i < PANEL_H; i++) {
        int idx = (lh - 1 - i + LOG_LINES * 2) % LOG_LINES;
        if (log_buf[idx][0]) {
            int y = py + (PANEL_H - 1) - i;
            int maxc = (cols - rx - 1);
            if (maxc > LOG_LEN - 1) maxc = LOG_LEN - 1;
            char tmp[LOG_LEN];
            strncpy(tmp, log_buf[idx], maxc);
            tmp[maxc] = '\0';
            mvaddstr(y, rx + 1, tmp);
        }
    }
}

/* ── Stat upgrade menu (ncurses overlay) ---------------------------- */

static void open_upgrade_nc(void) {
    int selected = 0;
    static const int ORDER[] = { STAT_ATTACK, STAT_DEFENSE, STAT_ENDURANCE,
                                 STAT_STAMINA, STAT_LUCK };
    static const int NUM_ORDER = 5;
    static const int BOX_W = 36;
    static const char BACK_TEXT[] = "[           Back           ]";
    static const int ITEM_COUNT = NUM_ORDER + 1; /* stats + Back */

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int box_h = NUM_ORDER + 5;
        int bx = (cols - BOX_W) / 2;
        int by = (rows - box_h) / 2;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        /* Solid background */
        attrset(COLOR_PAIR(panel_pair));
        for (int y = by + 1; y < by + box_h - 1; y++)
            for (int x = bx + 1; x < bx + BOX_W - 1; x++)
                mvaddch(y, x, ' ');

        /* Borders */
        attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddch(by, bx, ACS_ULCORNER);
        mvaddch(by, bx + BOX_W - 1, ACS_URCORNER);
        mvaddch(by + box_h - 1, bx, ACS_LLCORNER);
        mvaddch(by + box_h - 1, bx + BOX_W - 1, ACS_LRCORNER);
        mvwhline(stdscr, by, bx + 1, ACS_HLINE, BOX_W - 2);
        mvwhline(stdscr, by + box_h - 1, bx + 1, ACS_HLINE, BOX_W - 2);
        for (int y = by + 1; y < by + box_h - 1; y++) {
            mvaddch(y, bx, ACS_VLINE);
            mvaddch(y, bx + BOX_W - 1, ACS_VLINE);
        }

        /* Title */
        char title[48];
        snprintf(title, sizeof(title), "Upgrade (souls: %d)",
                 inventory[ITEM_SOULS]);
        mvaddstr(by + 1, bx + (BOX_W - strlen(title)) / 2, title);

        /* Stats */
        for (int i = 0; i < NUM_ORDER; i++) {
            int si = ORDER[i];
            int val = si == STAT_DEFENSE ? (p_defense + p_toughness) >> 1 :
                      si == STAT_ATTACK    ? p_attack :
                      si == STAT_ENDURANCE ? p_endurance :
                      si == STAT_STAMINA   ? p_stamina : p_luck;
            int cost = si == STAT_DEFENSE
                       ? SOULS_UPGRADE_COST + p_upgrade_cnt[STAT_DEFENSE]
                       : SOULS_UPGRADE_COST + p_upgrade_cnt[si];
            char line[48];
            snprintf(line, sizeof(line), "  %c%-12s %3d [%d]",
                     selected == i ? '>' : ' ',
                     stat_names[si], val, cost);
            int lx = bx + (BOX_W - strlen(line)) / 2;
            int y = by + 3 + i;
            if (selected == i && inventory[ITEM_SOULS] >= cost)
                attrset(A_REVERSE);
            else
                attrset(COLOR_PAIR(panel_pair));
            mvaddstr(y, lx, line);
        }

        /* Back button */
        int back_y = by + 3 + NUM_ORDER;
        int blx = bx + (BOX_W - strlen(BACK_TEXT)) / 2;
        if (selected == NUM_ORDER)
            attrset(A_REVERSE);
        else
            attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddstr(back_y, blx, BACK_TEXT);

        /* Hint */
        attrset(COLOR_PAIR(panel_pair));
        int sel_si = selected < NUM_ORDER ? ORDER[selected] : -1;
        int sel_cost = sel_si == STAT_DEFENSE
                       ? SOULS_UPGRADE_COST + p_upgrade_cnt[STAT_DEFENSE]
                       : (sel_si >= 0 ? SOULS_UPGRADE_COST + p_upgrade_cnt[sel_si] : 0);
        mvaddstr(by + box_h - 2, bx + 2,
                 selected < NUM_ORDER && inventory[ITEM_SOULS] >= sel_cost
                 ? "[Space] upgrade   [TAB] close"
                 : "Not enough souls   [TAB] close");

        refresh();

        int ch = getch();
        if (ch == '\t' || ch == 27) break;

        switch (ch) {
            case KEY_UP:    case 'i':
                selected = (selected - 1 + ITEM_COUNT) % ITEM_COUNT;
                break;
            case KEY_DOWN:  case 'k':
                selected = (selected + 1) % ITEM_COUNT;
                break;
            case ' ':
                if (selected == NUM_ORDER) return;
                upgrade_stat(ORDER[selected]);
                draw_panel();
                break;
        }
    }
}

/* ── Inventory menu (ncurses overlay) ---------------------------------- */

static void open_inventory_nc(void) {
    int selected = 0;
    static const int BOX_W = 36;

    while (1) {
        int vis_items[NUM_ITEM_TYPES], vis_count = 0;
        for (int i = 0; i < NUM_ITEM_TYPES; i++)
            if (inventory[i] > 0 && i != ITEM_SOULS) vis_items[vis_count++] = i;
        if (inventory[ITEM_SOULS] > 0) vis_items[vis_count++] = ITEM_SOULS;
        int total = vis_count + 1;
        if (selected >= total) selected = total - 1;

        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int base = vis_count > 0 ? vis_count : 1;
        int box_h = 6 + base;
        int bx = (cols - BOX_W) / 2;
        int by = (rows - box_h) / 2;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        attrset(COLOR_PAIR(panel_pair));
        for (int y = by + 1; y < by + box_h - 1; y++)
            for (int x = bx + 1; x < bx + BOX_W - 1; x++)
                mvaddch(y, x, ' ');

        attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddch(by, bx, ACS_ULCORNER);
        mvaddch(by, bx + BOX_W - 1, ACS_URCORNER);
        mvaddch(by + box_h - 1, bx, ACS_LLCORNER);
        mvaddch(by + box_h - 1, bx + BOX_W - 1, ACS_LRCORNER);
        mvwhline(stdscr, by, bx + 1, ACS_HLINE, BOX_W - 2);
        mvwhline(stdscr, by + box_h - 1, bx + 1, ACS_HLINE, BOX_W - 2);
        for (int y = by + 1; y < by + box_h - 1; y++) {
            mvaddch(y, bx, ACS_VLINE);
            mvaddch(y, bx + BOX_W - 1, ACS_VLINE);
        }

        const char *title = "Inventory";
        mvaddstr(by + 1, bx + (BOX_W - strlen(title)) / 2, title);

        int line_y = by + 3;
        for (int i = 0; i < vis_count; i++) {
            int idx = vis_items[i];
            int maxst = (idx == ITEM_SOULS) ? MAX_SOULS_STACK : ITEM_MAX_STACK(p_stamina, p_endurance);
            char inner[48];
            snprintf(inner, sizeof(inner), "%s %s x%d/%d",
                     item_defs[idx].action_text, item_defs[idx].name, inventory[idx], maxst);
            char line[64];
            snprintf(line, sizeof(line), "[%-28s]", inner);
            int lx = bx + (BOX_W - strlen(line)) / 2;
            if (selected == i)
                attrset(A_REVERSE);
            else
                attrset(COLOR_PAIR(panel_pair) | A_BOLD);
            mvaddstr(line_y, lx, line);
            line_y++;
        }

        if (vis_count == 0) {
            attrset(COLOR_PAIR(panel_pair));
            const char *empty_msg = "   (no items)   ";
            mvaddstr(line_y, bx + (BOX_W - strlen(empty_msg)) / 2, empty_msg);
            line_y++;
        }

        line_y++;

        const char *back_text = "[           Back           ]";
        int blx = bx + (BOX_W - strlen(back_text)) / 2;
        if (selected == vis_count)
            attrset(A_REVERSE);
        else
            attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddstr(line_y, blx, back_text);

        draw_panel();
        refresh();

        int ch = getch();
        if (ch == '\t' || ch == 27) break;

        switch (ch) {
            case KEY_UP:    case 'i':
                selected = (selected - 1 + total) % total;
                break;
            case KEY_DOWN:  case 'k':
                selected = (selected + 1) % total;
                break;
            case ' ':
                if (selected == vis_count) return;
                if (vis_items[selected] == ITEM_SOULS) {
                    open_upgrade_nc();
                    draw_panel();
                } else if (inventory[vis_items[selected]] > 0) {
                    use_item(vis_items[selected]);
                    draw_panel();
                }
                break;
        }
    }
}

/* ── Game-level loop (called once per level) -------------------------- */

static void show_hs_overlay_nc(int interactive);
static void drop_popup_nc(void);
static void help_overlay_nc(void);

static int play_level(void) {
    int steps = 75 + (p_level - 1) * 15;
    if (steps > 200000) steps = 200000;
    generate_maze(steps);

    player.x = MIDDLE_W + 0.5;
    player.y = MIDDLE_H + 0.5;

    int order[] = {0,1,2,3};
    for (int i = 3; i > 0; i--) {
        int j = rand() % (i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    int found = 0;
    for (int i = 0; i < 4; i++) {
        int d = order[i];
        int tx = (int)player.x + dir_vec[d][0];
        int ty = (int)player.y + dir_vec[d][1];
        if (tx >= 0 && tx < MAP_W && ty >= 0 && ty < MAP_H) {
            int v = get_map(tx, ty);
            if (v == 0 || v >= MAP_WATER) {
                set_player_dir(d);
                found = 1;
                break;
            }
        }
    }
    if (!found) set_player_dir(0);

    /* Place test enemy in front for combat testing */
    if (dbg_test_enemy >= 0 && dbg_test_enemy < NUM_ENEMY_TYPES) {
        int fx = (int)player.x + (int)player.dir_x;
        int fy = (int)player.y + (int)player.dir_y;
        if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H && get_map(fx, fy) == 0) {
            int map_cell;
            if (dbg_test_enemy <= ENEMY_SKELETON_L12)
                map_cell = MAP_ENEMY_SKELE;
            else if (dbg_test_enemy <= ENEMY_ORC_L12)
                map_cell = MAP_ENEMY_ORC;
            else if (dbg_test_enemy <= ENEMY_CULTIST_L12)
                map_cell = MAP_ENEMY_CULTIST;
            else
                map_cell = MAP_ENEMY_BOSS;
            set_map(fx, fy, map_cell);
            add_sprite(fx, fy, SPRITE_ENEMY, 0.4, dbg_test_enemy);
            log_msg("A %s bars thy path!", enemy_info[dbg_test_enemy].name);
        }
    }

    anim_active = 0; queued = 0;

    log_msg("Thou dost enter level %d of the labyrinth", p_level);

    while (running) {
        render_frame();
        draw_enemy_name();
        draw_panel();

        int flash = 0;
        if (anim_type == 2 && anim_active) {
            int64_t flash_elapsed = get_time_ms() - anim_start_ms;
            if (flash_elapsed >= ATTACK_MS / 2 && flash_elapsed < ATTACK_MS / 2 + 16)
                flash = flash_pair;
        } else if (flash_type) {
            if (flash_type == 1) flash = blue_pair;
            else if (flash_type == 2) flash = green_pair;
            else if (flash_type == 3) flash = grey_pair;
            else flash = flash_pair;
            flash_type = 0;
        }
        if (flash) {
            int rows, cols;
            getmaxyx(stdscr, rows, cols);
            attrset(COLOR_PAIR(flash));
            for (int y = 0; y < rows; y++)
                mvwhline(stdscr, y, 0, ' ', cols);
        }

        refresh();
        napms(1);

        if (anim_active) {
            update_anim();

            if (!anim_active) {
                if (anim_type == 1) {
                    p_dir = anim_new_dir;
                    set_player_dir(p_dir);
                }
                if (anim_type == 0) {
                    int cx = (int)(a_nx);
                    int cy = (int)(a_ny);
                    int v = get_map(cx, cy);
                    if (MAP_IS_ENEMY(v)) {
                        attack_enemies();
                        draw_panel(); drop_popup_nc();
                        if (game_won) return 2;
                    } else if (v >= 1 && v % 5 == 0) { return 1; }
                    consume_item(cx, cy);
                    if (is_attack) {
                        attack_enemies();
                        draw_panel(); drop_popup_nc();
                        if (game_won) return 2;
                        if (p_health <= 0) { p_health = 0; return 0; }
                    } else if (end_turn()) {
                        p_health = 0; return 0;
                    }
                }
                if (anim_type == 2) {
                    attack_enemies();
                    draw_panel(); drop_popup_nc();
                    if (game_won) return 2;
                    if (p_health <= 0) { p_health = 0; return 0; }
                }
                if (queued) {
                    int q = queued; queued = 0;
                    if (q == 1) {
                        int r = try_move((int)player.dir_x, (int)player.dir_y, 0);
                        if (r == 2) { log_msg("Thou hast found the exit! Onward to level %d...", p_level + 1); return 1; }
                    } else if (q == 2) {
                        int r = try_move(-(int)player.dir_x, -(int)player.dir_y, 0);
                        if (r == 2) { log_msg("Thou hast found the exit! Onward to level %d...", p_level + 1); return 1; }
                    } else if (q == 3) try_turn(1);
                    else if (q == 4) try_turn(0);
                }
            } else {
                int ch = getch();
                if (ch == 'q' || ch == 'Q') { running = 0; return 0; }
                if (ch == '\t') { open_inventory_nc(); continue; }
                if (ch == KEY_F(2)) { show_hs_overlay_nc(1); continue; }
                if (ch == KEY_F(1)) { help_overlay_nc(); continue; }
                int act = 0;
                switch (ch) {
                    case KEY_UP:    case 'i': act = 1; break;
                    case KEY_DOWN:  case 'k': act = 2; break;
                    case KEY_LEFT:  case 'j': act = 3; break;
                    case KEY_RIGHT: case 'l': act = 4; break;
                }
                if (act) {
                    if (queued != 0 && is_opposite(queued, act))
                        queued = 0;
                    else
                        queued = act;
                }
                if (ch == ' ') {
                    int cx = (int)player.x, cy = (int)player.y;
                    int fx = cx + (int)player.dir_x, fy = cy + (int)player.dir_y;
                    if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H) {
                        int v = get_map(fx, fy);
                        if (MAP_IS_ENEMY(v)) {
                            attack_enemies();
                            draw_panel(); drop_popup_nc();
                            if (game_won) return 2;
                            if (p_health <= 0) { p_health = 0; return 0; }
                    } else if (v >= 1 && v % 5 == 0) { log_msg("Thou hast found the exit! Onward to level %d...", p_level + 1); return 1; }
                    }
                }
            }
            continue;
        }

        int cx = (int)player.x;
        int cy = (int)player.y;
        if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H)
            consume_item(cx, cy);

        int ch = getch();
        if (ch == 'q' || ch == 'Q') { running = 0; return 0; }
        if (ch == '\t') { open_inventory_nc(); continue; }
        if (ch == KEY_F(2)) { show_hs_overlay_nc(1); continue; }
        if (ch == KEY_F(1)) { help_overlay_nc(); continue; }

        if (ch == ' ') {
            int fx = cx + (int)player.dir_x, fy = cy + (int)player.dir_y;
            if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H) {
                int v = get_map(fx, fy);
                if (MAP_IS_ENEMY(v)) {
                    try_attack();
                    continue;
                }
                if (v >= 1 && v % 5 == 0) { log_msg("Thou hast found the exit! Onward to level %d...", p_level + 1); return 1; }
            }
            consume_item(cx, cy);
            continue;
        }
        if (ch == ERR) continue;

        switch (ch) {
           case KEY_UP:    case 'i': {
                    try_move((int)player.dir_x, (int)player.dir_y, 0);
                    break;
                }
                case KEY_DOWN:  case 'k': {
                    try_move(-(int)player.dir_x, -(int)player.dir_y, 0);
                    break;
                }
            case KEY_LEFT:  case 'j': try_turn(1); break;
            case KEY_RIGHT: case 'l': try_turn(0); break;
        }
    }
    return p_health > 0 ? 1 : 0;
}

/* ── Name input overlay (ncurses) ------------------------------------ */

static void name_input_nc(char *out, int maxlen) {
    int pos = 0;
    memset(out, 0, maxlen);

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int bw = 36, bh = 7;
        int bx = (cols - bw) / 2;
        int by = (rows - bh) / 2;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        attrset(COLOR_PAIR(panel_pair));
        for (int y = by + 1; y < by + bh - 1; y++)
            for (int x = bx + 1; x < bx + bw - 1; x++)
                mvaddch(y, x, ' ');

        attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddch(by, bx, ACS_ULCORNER);
        mvaddch(by, bx + bw - 1, ACS_URCORNER);
        mvaddch(by + bh - 1, bx, ACS_LLCORNER);
        mvaddch(by + bh - 1, bx + bw - 1, ACS_LRCORNER);
        mvwhline(stdscr, by, bx + 1, ACS_HLINE, bw - 2);
        mvwhline(stdscr, by + bh - 1, bx + 1, ACS_HLINE, bw - 2);
        for (int y = by + 1; y < by + bh - 1; y++) {
            mvaddch(y, bx, ACS_VLINE);
            mvaddch(y, bx + bw - 1, ACS_VLINE);
        }

        const char *msg = "NEW HIGH SCORE!";
        mvaddstr(by + 1, bx + (bw - strlen(msg)) / 2, msg);
        mvaddstr(by + 3, bx + 2, "Enter name:");
        mvaddstr(by + 4, bx + 2, out);
        mvaddch(by + 4, bx + 2 + pos, '_');
        refresh();

        int ch = getch();
        if (ch == '\n' || ch == '\r') {
            if (pos > 0) return;
            continue;
        }
        if (ch == 27) { out[0] = '\0'; return; }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (pos > 0) out[--pos] = '\0';
            continue;
        }
        if (pos < maxlen - 1 && ch >= 32 && ch <= 126) {
            out[pos++] = ch;
            out[pos] = '\0';
        }
    }
}

/* ── High score overlay (during gameplay) ───────────────────────── */

static void show_hs_overlay_nc(int interactive) {
    HighScoreEntry entries[MAX_HIGHSCORES];
    int n = load_high_scores(entries);
    if (n == 0) return;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int bw = 42;
    int bh = 4 + n + 3;
    int bx = (cols - bw) / 2;
    int by = (rows - bh) / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    attrset(COLOR_PAIR(panel_pair));
    for (int y = by + 1; y < by + bh - 1; y++)
        for (int x = bx + 1; x < bx + bw - 1; x++)
            mvaddch(y, x, ' ');

    attrset(COLOR_PAIR(panel_pair) | A_BOLD);
    mvaddch(by, bx, ACS_ULCORNER);
    mvaddch(by, bx + bw - 1, ACS_URCORNER);
    mvaddch(by + bh - 1, bx, ACS_LLCORNER);
    mvaddch(by + bh - 1, bx + bw - 1, ACS_LRCORNER);
    mvwhline(stdscr, by, bx + 1, ACS_HLINE, bw - 2);
    mvwhline(stdscr, by + bh - 1, bx + 1, ACS_HLINE, bw - 2);
    for (int y = by + 1; y < by + bh - 1; y++) {
        mvaddch(y, bx, ACS_VLINE);
        mvaddch(y, bx + bw - 1, ACS_VLINE);
    }

    const char *title = "HIGH SCORES";
    mvaddstr(by + 1, bx + (bw - strlen(title)) / 2, title);

    for (int i = 0; i < n; i++) {
        mvprintw(by + 3 + i, bx + 2, "  %2d. %-10s %6d (Lvl %d)", i + 1,
                 entries[i].name, entries[i].score, entries[i].level);
    }

    if (interactive) {
        const char *prompt = "[TAB / F2] close";
        mvaddstr(by + bh - 2, bx + (bw - strlen(prompt)) / 2, prompt);
    } else {
        const char *prompt = "Press any key to continue";
        mvaddstr(by + bh - 2, bx + (bw - strlen(prompt)) / 2, prompt);
    }
    refresh();

    if (interactive) {
        while (1) {
            int ch = getch();
            if (ch == '\t' || ch == KEY_F(2)) return;
        }
    } else {
        getch();
    }
}

/* ── Drop popup ───────────────────────────────────────────────────── */

static void drop_popup_nc(void) {
    if (drop_popup_item < 0) return;
    int item = drop_popup_item;
    const char *iname = drop_popup_name;
    drop_popup_item = -1;
    drop_popup_name = NULL;

    /* Map item to sprite type */
    int sp_type;
    switch (item) {
        case ITEM_BOTTLE_OF_WATER: sp_type = SPRITE_POTION_BLUE; break;
        case ITEM_HEALING_POTION:  sp_type = SPRITE_POTION_RED;  break;
        case ITEM_SPIRIT_LUCK:     sp_type = SPRITE_POTION_PINK; break;
        case ITEM_ANTIDOTE:        sp_type = SPRITE_POTION_GREEN; break;
        default:                   sp_type = SPRITE_POTION_RED;  break;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

#define SP_SCALE 1
    int sp_w = SPRITE_W / SP_SCALE; /* 32 */
    int sp_h = SPRITE_H / SP_SCALE; /* 32 */
    int bw = sp_w + 8;
    int bh = 2 + 1 + 1 + sp_h + 1 + 1 + 1 + 1;
    int bx = (cols - bw) / 2;
    int by = (rows - bh) / 2;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    attrset(COLOR_PAIR(panel_pair));
    for (int y = by + 1; y < by + bh - 1; y++)
        for (int x = bx + 1; x < bx + bw - 1; x++)
            mvaddch(y, x, ' ');

    attrset(COLOR_PAIR(panel_pair) | A_BOLD);
    mvaddch(by, bx, ACS_ULCORNER);
    mvaddch(by, bx + bw - 1, ACS_URCORNER);
    mvaddch(by + bh - 1, bx, ACS_LLCORNER);
    mvaddch(by + bh - 1, bx + bw - 1, ACS_LRCORNER);
    mvwhline(stdscr, by, bx + 1, ACS_HLINE, bw - 2);
    mvwhline(stdscr, by + bh - 1, bx + 1, ACS_HLINE, bw - 2);
    for (int y = by + 1; y < by + bh - 1; y++) {
        mvaddch(y, bx, ACS_VLINE);
        mvaddch(y, bx + bw - 1, ACS_VLINE);
    }

    const char *title = "Enemy dropped:";
    mvaddstr(by + 1, bx + (bw - strlen(title)) / 2, title);

    /* Render the full sprite at 2:1 scale (16×16 chars) */
    int sprite_x = bx + (bw - sp_w) / 2;
    int sprite_y = by + 3;
    for (int sy = 0; sy < sp_h; sy++) {
        for (int sx = 0; sx < sp_w; sx++) {
            /* Find first non-transparent pixel in 2×2 block */
            int xt = -1;
            for (int dy = 0; dy < SP_SCALE && xt < 0; dy++)
                for (int dx = 0; dx < SP_SCALE && xt < 0; dx++) {
                    int px = sprite_tex[sp_type][sy * SP_SCALE + dy][sx * SP_SCALE + dx];
                    if (px >= 0) xt = px;
                }
            if (xt >= 0) {
                int pair = xterm_pair[xt];
                if (pair < 0) pair = 1;
                attrset(COLOR_PAIR(pair));
                mvaddstr(sprite_y + sy, sprite_x + sx, "\xe2\x96\x88");
            }
        }
    }

    /* Item name */
    attrset(COLOR_PAIR(panel_pair) | A_BOLD);
    mvaddstr(sprite_y + sp_h + 1, bx + (bw - strlen(iname)) / 2, iname);

    attrset(COLOR_PAIR(panel_pair));
    const char *prompt = "[TAB]";
    mvaddstr(by + bh - 2, bx + (bw - strlen(prompt)) / 2, prompt);

    refresh();
    while (getch() != '\t') { }
}

/* ── Help overlay (ncurses) ──────────────────────────────────────── */

static void help_overlay_nc(void) {
    static const char *lines[] = {
        "╔═══════════════════ HELP ════════════════════╗",
        "║                                             ║",
        "║ You are trapped in a maze inside the mind   ║",
        "║ of the evil necromancer. Defeat the foul    ║",
        "║ necromancer and break free.                 ║",
        "║                                             ║",
        "║ Navigate the maze, collect water and        ║",
        "║ potions to survive, and battle the          ║",
        "║ necromancer's evil minions.                 ║",
        "║                                             ║",
        "║ Exit a level using the skull-shaped switch  ║",
        "║                                             ║",
        "║ ── Stats ──                                 ║",
        "║ ATK — how hard thou strikest                ║",
        "║ DEF — chance to dodge and reduce damage     ║",
        "║ STA — increases max HP & potion capacity    ║",
        "║ END — increases max water & potion capacity ║",
        "║ LCK — improves hit & drop chance            ║",
        "║                                             ║",
        "║ ── Controls ──                              ║",
        "║ Arrow keys / I J K L  — move / turn         ║",
        "║ Space       — attack / use / exit           ║",
        "║ TAB         — inventory / upgrade menu      ║",
        "║ F2          — high scores                   ║",
        "║ F1          — this help screen              ║",
        "║ Q           — quit                          ║",
        "║                                             ║",
        "║              [TAB / F1] close               ║",
        "╚═════════════════════════════════════════════╝",
    };
    int n = sizeof(lines) / sizeof(lines[0]);
    /* Count visible character width (not byte length, since UTF-8) */
    int bw = 0;
    for (const char *p = lines[0]; *p; p++)
        if ((*p & 0xC0) != 0x80) bw++;

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int bx = (cols - bw) / 2;
        int by = (rows - n) / 2;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        attrset(COLOR_PAIR(panel_pair));
        for (int y = by; y < by + n; y++)
            for (int x = bx; x < bx + bw; x++)
                mvaddch(y, x, ' ');

        attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        for (int i = 0; i < n; i++)
            mvaddstr(by + i, bx, lines[i]);

        refresh();

        int ch = getch();
        if (ch == '\t' || ch == KEY_F(1)) return;
    }
}

/* ── Death overlay (ncurses) ────────────────────────────────────── */
/* Returns 1 if player chose restart, 0 to quit.                     */

static int death_overlay_nc(void) {
    int selected = 0;

    HighScoreEntry entries[MAX_HIGHSCORES];
    int hs_count = load_high_scores(entries);

    while (1) {
        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        int bw = 44;
        int bh = 4 + (hs_count > 0 ? 1 + hs_count + 1 : 0) + 1 + 1 + 1 + 1;
        if (bh < 10) bh = 10;
        int bx = (cols - bw) / 2;
        int by = (rows - bh) / 2;
        if (bx < 0) bx = 0;
        if (by < 0) by = 0;

        attrset(COLOR_PAIR(panel_pair));
        for (int y = by + 1; y < by + bh - 1; y++)
            for (int x = bx + 1; x < bx + bw - 1; x++)
                mvaddch(y, x, ' ');

        attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddch(by, bx, ACS_ULCORNER);
        mvaddch(by, bx + bw - 1, ACS_URCORNER);
        mvaddch(by + bh - 1, bx, ACS_LLCORNER);
        mvaddch(by + bh - 1, bx + bw - 1, ACS_LRCORNER);
        mvwhline(stdscr, by, bx + 1, ACS_HLINE, bw - 2);
        mvwhline(stdscr, by + bh - 1, bx + 1, ACS_HLINE, bw - 2);
        for (int y = by + 1; y < by + bh - 1; y++) {
            mvaddch(y, bx, ACS_VLINE);
            mvaddch(y, bx + bw - 1, ACS_VLINE);
        }

        int line = by + 1;
        char buf[64];
        snprintf(buf, sizeof(buf), "THOU ART FALLEN  (Level %d  Score: %d)", p_level, get_current_score());
        mvaddstr(line, bx + (bw - strlen(buf)) / 2, buf);
        line += 2;

        mvaddstr(line, bx + (bw - 27) / 2, "Wouldst thou try again?");
        line += 2;

        if (hs_count > 0) {
            attrset(COLOR_PAIR(panel_pair) | A_BOLD);
            mvaddstr(line, bx + (bw - 11) / 2, "HIGH SCORES");
            line++;
            for (int i = 0; i < hs_count; i++) {
                attrset(COLOR_PAIR(panel_pair));
                mvprintw(line, bx + 2, "  %2d. %-10s %6d (Lvl %d)", i + 1,
                         entries[i].name, entries[i].score, entries[i].level);
                line++;
            }
            line++;
        }

        /* Yes / No buttons */
        const char *yes_text = "[ Yes ]";
        const char *no_text  = "[ No  ]";
        int gap = 6;
        int total_w = strlen(yes_text) + gap + strlen(no_text);
        int start_x = bx + (bw - total_w) / 2;
        int btn_y = bh - 3 + by;

        if (selected == 0) attrset(A_REVERSE);
        else attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddstr(btn_y, start_x, yes_text);
        start_x += strlen(yes_text) + gap;

        if (selected == 1) attrset(A_REVERSE);
        else attrset(COLOR_PAIR(panel_pair) | A_BOLD);
        mvaddstr(btn_y, start_x, no_text);

        attrset(COLOR_PAIR(panel_pair));
        const char *hint = "[\xe2\x86\x90\xe2\x86\x92] switch   [Enter] confirm";
        mvaddstr(by + bh - 2, bx + (bw - strlen(hint)) / 2, hint);

        refresh();

        int ch = getch();
        if (ch == '\t' || ch == 27) return 0;
        switch (ch) {
            case KEY_LEFT: case KEY_RIGHT: case 'j': case 'l':
                selected = !selected;
                break;
            case '\n':
            case '\r':
            case KEY_ENTER:
                return selected == 0 ? 1 : 0;
        }
    }
}

/* ── Main ------------------------------------------------------------- */

static void usage(void) {
    printf("Usage: maze3d [--demo] [--text] [--level N] [--atk N] [--def N]\n");
    printf("             [--tgh N] [--sta N] [--end N] [--lck N]\n");
    printf("             [--test-enemy N]\n");
    printf("  --demo       Auto-pilot screensaver (no RPG, no levels)\n");
    printf("  --text       Run in text-based debug mode (no ncurses)\n");
    printf("  --level N    Start at game level N\n");
    printf("  --atk  N     Override base attack stat\n");
    printf("  --def  N     Override base defense stat\n");
    printf("  --tgh  N     Override base toughness stat\n");
    printf("  --sta  N     Override base stamina stat\n");
    printf("  --end  N     Override base endurance stat\n");
    printf("  --lck  N     Override base luck stat\n");
    printf("  --test-enemy N  Place enemy type N in front for combat testing\n");
}

int main(int argc, char **argv) {
    int text_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--demo") == 0) {
            run_demo();
            return 0;
        } else if (strcmp(argv[i], "--text") == 0) {
            text_mode = 1;
        } else if (strcmp(argv[i], "--level") == 0 && i + 1 < argc) {
            dbg_start_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--atk") == 0 && i + 1 < argc) {
            dbg_stats[0] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--def") == 0 && i + 1 < argc) {
            dbg_stats[1] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--tgh") == 0 && i + 1 < argc) {
            dbg_stats[2] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--sta") == 0 && i + 1 < argc) {
            dbg_stats[3] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--end") == 0 && i + 1 < argc) {
            dbg_stats[4] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--lck") == 0 && i + 1 < argc) {
            dbg_stats[5] = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--test-enemy") == 0 && i + 1 < argc) {
            dbg_test_enemy = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (text_mode) {
        run_text_mode(dbg_start_level, dbg_stats, dbg_test_enemy);
        return 0;
    }

    signal(SIGINT, handle_signal);
    init_ncurses();
    atexit(cleanup);

    srand(time(0));
    map = malloc(MAP_W * MAP_H);
    if (!map) { endwin(); fprintf(stderr, "malloc failed\n"); return 1; }
    init_tex();
    init_luts();

    int dbg_first = 1;
    while (running) {
        init_game();
        if (dbg_first) {
            dbg_first = 0;
            if (dbg_start_level > 1) p_level = dbg_start_level;
            int *const stat_ptrs[] = {&p_attack, &p_defense, &p_toughness,
                                      &p_stamina, &p_endurance, &p_luck};
            for (int i = 0; i < 6; i++)
                if (dbg_stats[i] > 0) *stat_ptrs[i] = dbg_stats[i];
            recalc_player_max();
        }

        while (running) {
            int r = play_level();
            if (r == 0) {
                /* Death — check high score first */
                int score = get_current_score();
                if (is_high_score(score)) {
                    char name[HS_NAME_LEN] = {0};
                    name_input_nc(name, HS_NAME_LEN);
                    if (name[0])
                        add_high_score(name, score, p_level, p_steps);
                }
                /* Then show restart overlay */
                if (!death_overlay_nc()) {
                    show_hs_overlay_nc(0);
                    goto done;
                }
                /* Restart — break to outer loop which calls init_game again */
                break;
            }
            if (r == 2) {
                endwin();
                printf("\n");
                printf("  ╔══════════════════════════════════════════╗\n");
                printf("  ║                                        ║\n");
                printf("  ║   HARK! VICTORY!                       ║\n");
                printf("  ║   Thou hast defeated the Necromancer!  ║\n");
                printf("  ║                                        ║\n");
                printf("  ╚══════════════════════════════════════════╝\n");
                printf("\n");
                printf("  Victory achieved at level %d!\n", p_level);
                printf("  Total steps trod: %d\n", p_steps);
                printf("  Score: %d\n", get_current_score());
                return 0;
            }
            p_level++;
            p_score += 50;
        }
    }

done:
    endwin();
    return 0;
}
