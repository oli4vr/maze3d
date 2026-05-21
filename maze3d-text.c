/* maze3d-text.c — Text-based debug mode for maze3d
 *
 * Runs the same gameplay engine as maze3d but replaces the 3D renderer
 * with text descriptions.  Useful for debugging and automated testing.
 * Invoked via ./maze3d --text.
 *
 * CLI: --level N --atk N --def N --tgh N --sta N --end N --lck N
 * Commands: f=forward b=backward l=left r=right a=attack/use q=quit  */

#include "gameplay.h"
#include "enemies.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

/* ── Look helpers ─────────────────────────────────────────────────── */

static const char *dir_name(int d) {
    static const char *names[] = {"east", "south", "west", "north"};
    return names[d & 3];
}

static const char *item_desc(int v, int mx, int my) {
    if (v == MAP_WATER) return "a puddle of water";
    if (v == MAP_POTION) {
        int st = get_sprite_type_at(mx, my);
        if (st == SPRITE_POTION_BLUE) return "a blue potion";
        if (st == SPRITE_POTION_RED)  return "a red potion";
        if (st == SPRITE_POTION_PINK) return "a pink potion";
        if (st == SPRITE_POTION_GREEN) return "a green potion";
        return "a potion";
    }
    return NULL;
}

/* scan_line: Look along (dx,dy) from player position and describe
 * the first interesting thing found within `max_dist` cells.          */
static void scan_line(int dx, int dy, int max_dist, const char *where) {
    int px = (int)player.x, py = (int)player.y;
    for (int d = 1; d <= max_dist; d++) {
        int cx = px + dx * d, cy = py + dy * d;
        if (cx < 0 || cx >= MAP_W || cy < 0 || cy >= MAP_H) {
            printf("  %s: edge of the world\n", where);
            return;
        }
        int v = get_map(cx, cy);
        if (v >= 1 && v <= NUM_TEX) {
            if (v % 5 == 0) {
                printf("  %s: a skull wall blocks your path (%d steps ahead)\n", where, d);
            } else {
                printf("  %s: a wall blocks your path (%d steps ahead)\n", where, d);
            }
            return;
        }
        const char *en = get_enemy_name_at(cx, cy);
        if (en) {
            printf("  %s: a %s (%d steps ahead)\n", where, en, d);
            return;
        }
        const char *it = item_desc(v, cx, cy);
        if (it) {
            printf("  %s: %s (%d steps ahead)\n", where, it, d);
            return;
        }
    }
    printf("  %s: clear corridor (%d+ steps)\n", where, max_dist);
}

/* ── Describe surroundings ------------------------------------------- */

static void describe_view(void) {
    int dx = (int)player.dir_x, dy = (int)player.dir_y;
    int lx = -dy, ly = dx;
    int rx = dy, ry = -dx;
    int bx = -dx, by = -dy;

    printf("\n--- %s (Lvl %d, HP %d, WTR %d, Steps %d, Score %d%s%s) ---\n",
           dir_name(p_dir), p_level, p_health, p_water, p_steps, get_current_score(),
           p_poisoned ? " [POISON]" : "",
           p_spirit_turns > 0 ? " [SPIRIT]" : "");

    /* Check adjacent cell in front for immediate interaction */
    int fx = (int)player.x + dx, fy = (int)player.y + dy;
    if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H) {
        int v = get_map(fx, fy);
        if (MAP_IS_ENEMY(v)) {
            const char *en = get_enemy_name_at(fx, fy);
            if (en) printf("  An enemy is directly in front of you: %s\n", en);
        } else if (v == MAP_WATER) {
            printf("  A puddle of water is at your feet, one step ahead\n");
        } else if (v == MAP_POTION) {
            int st = get_sprite_type_at(fx, fy);
            const char *pname = "potion";
            if (st == SPRITE_POTION_BLUE) pname = "blue potion";
            else if (st == SPRITE_POTION_RED) pname = "red potion";
            else if (st == SPRITE_POTION_PINK) pname = "pink potion";
            else if (st == SPRITE_POTION_GREEN) pname = "green potion";
            printf("  A %s is one step ahead\n", pname);
        } else if (v >= 1 && v % 5 == 0) {
            printf("  A skull wall is directly ahead — the exit!\n");
        } else if (v >= 1 && v <= NUM_TEX) {
            printf("  A wall is directly ahead\n");
        }
    }

    scan_line(dx, dy, 10, "Forward");
    scan_line(lx, ly, 5, "Left");
    scan_line(rx, ry, 5, "Right");
    scan_line(bx, by, 3, "Behind");
}

/* ── Execute action immediately (skip animation) --------------------- */

static int do_move(int dx, int dy) {
    int r = try_move(dx, dy, 0);
    if (r == 0) {
        printf("  Blocked by a wall.\n");
        return 0;
    }
    if (r == -1) {
        printf("  An enemy blocks your path! Press 'a' to attack.\n");
        return 0;
    }
    /* Snap instantly to target */
    player.x = a_nx;
    player.y = a_ny;
    anim_rem = 0;

    int cx = (int)player.x, cy = (int)player.y;
    int v = get_map(cx, cy);
    if (MAP_IS_ENEMY(v)) {
        printf("  You step onto an enemy cell — attacking!\n");
        attack_enemies();
        if (game_won) { printf("  You defeated the Necromancer! Victory!\n"); return 3; }
        if (p_health == 0) { printf("  You have been slain!\n"); running = 0; return 1; }
        return 1;
    }
    if (v >= 1 && v % 5 == 0) {
        log_msg("Thou hast found the exit! Onward to level %d...", p_level + 1);
        printf("  Thou hast found the exit! Onward to level %d.\n", p_level + 1);
        return 2;
    }
    int old_v = get_map(cx, cy);
    int old_sp = (old_v == MAP_POTION) ? get_sprite_type_at(cx, cy) : -1;
    consume_item(cx, cy);
    if (flash_type) {
        const char *iname = NULL;
        if (old_v == MAP_WATER)
            iname = "refreshing water";
        else if (old_v == MAP_POTION) {
            if (old_sp == SPRITE_POTION_BLUE) iname = "Bottle of Water";
            else if (old_sp == SPRITE_POTION_RED) iname = "Healing Potion";
            else if (old_sp == SPRITE_POTION_PINK) iname = "Spirit 'o Luck";
            else if (old_sp == SPRITE_POTION_GREEN) iname = "Antidote";
        }
        if (iname) printf("  Acquired a %s!\n", iname);
        flash_type = 0;
    }
    if (end_turn()) {
        printf("  You have died%s!\n", p_poisoned ? " of poison" : " of thirst");
        p_poisoned = 0; /* reset for potential restart */
        running = 0;
        return 1;
    }
    printf("  You move forward.\n");
    return 1;
}

static void do_turn(int left) {
    try_turn(left);
    /* Snap instantly */
    p_dir = anim_new_dir;
    set_player_dir(p_dir);
    anim_rem = 0;
    printf("  You turn %s.\n", left ? "left" : "right");
}

static int do_attack(void) {
    int fx = (int)player.x + (int)player.dir_x;
    int fy = (int)player.y + (int)player.dir_y;
    int dead_before = p_health;
    int enemy_killed = 0;

    if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H) {
        int v = get_map(fx, fy);
        if (MAP_IS_ENEMY(v)) {
            const char *en = get_enemy_name_at(fx, fy);
            printf("  You attack the %s!\n", en ? en : "enemy");
            attack_enemies();
            if (game_won) { printf("  You defeated the Necromancer! Victory!\n"); return 3; }
            /* Check if the enemy was killed */
            if (!MAP_IS_ENEMY(get_map(fx, fy)))
                enemy_killed = 1;
        } else if (v >= 1 && v % 5 == 0) {
            printf("  Thou hast found the exit! Onward to level %d.\n", p_level + 1);
            return 2;
        } else if (v == MAP_WATER || v == MAP_POTION) {
            int was_water = (v == MAP_WATER);
            int old_sp = (v == MAP_POTION) ? get_sprite_type_at(fx, fy) : -1;
            consume_item(fx, fy);
            if (flash_type) {
                if (was_water)
                    printf("  You drink water — refreshing!\n");
                else {
                    const char *iname = "potion";
                    if (old_sp == SPRITE_POTION_BLUE) iname = "Bottle of Water";
                    else if (old_sp == SPRITE_POTION_RED) iname = "Healing Potion";
                    else if (old_sp == SPRITE_POTION_PINK) iname = "Spirit 'o Luck";
                    else if (old_sp == SPRITE_POTION_GREEN) iname = "Antidote";
                    printf("  Acquired a %s!\n", iname);
                }
                flash_type = 0;
            }
            return 1;
        } else {
            printf("  Nothing to attack there.\n");
            return 1;
        }
    } else {
        printf("  Nothing to attack there.\n");
        return 1;
    }

    int damage = dead_before - p_health;
    if (damage > 0) printf("  You take %d damage from the counter-attack!\n", damage);
    if (enemy_killed) printf("  You defeated the enemy!\n");
    if (p_health <= 0) {
        printf("  You have been slain!\n");
        running = 0;
    }
    if (p_poisoned) printf("  You are poisoned!\n");
    return 1;
}

/* ── Stat upgrade menu (text mode) ------------------------------------ */

static void open_upgrade_txt(void) {
    int selected = 0;
    static const int ORDER[] = { STAT_ATTACK, STAT_DEFENSE, STAT_ENDURANCE,
                                 STAT_STAMINA, STAT_LUCK };
    static const int NUM_ORDER = 5;

    while (1) {
        printf("\n--- Upgrade (souls: %d) ---\n",
               inventory[ITEM_SOULS]);
        for (int i = 0; i < NUM_ORDER; i++) {
            int si = ORDER[i];
            int val = si == STAT_DEFENSE ? (p_defense + p_toughness) >> 1 :
                      si == STAT_ATTACK    ? p_attack :
                      si == STAT_ENDURANCE ? p_endurance :
                      si == STAT_STAMINA   ? p_stamina : p_luck;
            int cost = si == STAT_DEFENSE
                       ? SOULS_UPGRADE_COST + p_upgrade_cnt[STAT_DEFENSE]
                       : SOULS_UPGRADE_COST + p_upgrade_cnt[si];
            printf("  %c%-12s %3d [%d]%s\n",
                   i == selected ? '>' : ' ',
                   stat_names[si], val, cost,
                   (i == selected && inventory[ITEM_SOULS] >= cost)
                       ? "  [Space to upgrade]" : "");
        }
        printf("  [k/8] up  [j/2] down  [Space] upgrade  [i/q] close\n> ");
        fflush(stdout);

        char line[64];
        if (!fgets(line, sizeof(line), stdin)) { running = 0; break; }
        char cmd = line[0];

        if (cmd == 'i' || cmd == 'q' || cmd == 'Q') break;
        switch (cmd) {
            case 'k': case '8':
                selected = (selected - 1 + NUM_ORDER) % NUM_ORDER;
                break;
            case 'j': case '2':
                selected = (selected + 1) % NUM_ORDER;
                break;
            case ' ':
                upgrade_stat(ORDER[selected]);
                printf("  Upgraded %s to %d!\n",
                       stat_names[ORDER[selected]],
                       ORDER[selected] == STAT_DEFENSE ? (p_defense + p_toughness) >> 1 :
                       ORDER[selected] == STAT_ATTACK ? p_attack :
                       ORDER[selected] == STAT_ENDURANCE ? p_endurance :
                       ORDER[selected] == STAT_STAMINA ? p_stamina : p_luck);
                break;
        }
    }
}

/* ── Inventory menu (text mode) --------------------------------------- */

static void open_inventory_txt(void) {
    int selected = 0;

    while (1) {
        /* Rebuild visible list each iteration (counts change on use) */
        int vis_items[NUM_ITEM_TYPES], vis_count = 0;
        for (int i = 0; i < NUM_ITEM_TYPES; i++)
            if (inventory[i] > 0 && i != ITEM_SOULS) vis_items[vis_count++] = i;
        if (inventory[ITEM_SOULS] > 0) vis_items[vis_count++] = ITEM_SOULS;
        int total = vis_count + 1; /* items + Back */
        if (selected >= total) selected = total - 1;
        printf("\n--- Inventory ---\n");
        for (int i = 0; i < vis_count; i++) {
            int idx = vis_items[i];
            int maxst = (idx == ITEM_SOULS) ? MAX_SOULS_STACK : ITEM_MAX_STACK(p_stamina, p_endurance);
            printf("%c %s %-20s x%02d/%d\n",
                   i == selected ? '>' : ' ',
                   item_defs[idx].action_text,
                   item_defs[idx].name,
                   inventory[idx], maxst);
        }
        if (vis_count == 0)
            printf("  (no items)\n");
        printf("%c %s\n",
               selected == vis_count ? '>' : ' ',
               "Back");
        printf("  [k/8] up  [j/2] down  [Space] select  [i/q] close\n> ");
        fflush(stdout);

        char line[64];
        if (!fgets(line, sizeof(line), stdin)) { running = 0; break; }
        char cmd = line[0];

        if (cmd == 'i' || cmd == 'q' || cmd == 'Q') break;
        switch (cmd) {
            case 'k': case '8':
                selected--;
                if (selected < 0) selected = total - 1;
                break;
            case 'j': case '2':
                selected++;
                if (selected >= total) selected = 0;
                break;
            case ' ':
                if (selected == vis_count) return; /* Back */
                if (vis_items[selected] == ITEM_SOULS) {
                    open_upgrade_txt();
                } else if (inventory[vis_items[selected]] > 0) {
                    use_item(vis_items[selected]);
                    printf("  Used %s!\n", item_defs[vis_items[selected]].name);
                    if (flash_type == 1) printf("  Refreshing water!\n");
                    else printf("  Healing energy!\n");
                    flash_type = 0;
                }
                break;
        }
    }
}

/* ── High score / death helpers (text mode) --------------------------- */

static void show_high_scores_txt(void) {
    HighScoreEntry entries[MAX_HIGHSCORES];
    int n = load_high_scores(entries);
    if (n == 0) {
        printf("  (no high scores yet)\n");
        return;
    }
    printf("\n  --- HIGH SCORES ---\n");
    for (int i = 0; i < n; i++) {
        printf("  %2d. %-15s %6d (Lvl %d)\n", i + 1,
               entries[i].name, entries[i].score, entries[i].level);
    }
}

static void check_high_score_txt(void) {
    int score = get_current_score();
    if (!is_high_score(score)) return;

    printf("\n  NEW HIGH SCORE! (%d points)\n", score);
    printf("  Enter your name: ");
    fflush(stdout);
    char name[HS_NAME_LEN] = {0};
    if (fgets(name, HS_NAME_LEN, stdin)) {
        char *nl = strchr(name, '\n');
        if (nl) *nl = '\0';
        if (name[0]) add_high_score(name, score, p_level, p_steps);
    }
    show_high_scores_txt();
}

/* ── Help text ----------------------------------------------------- */

static void help_text(void) {
    printf("\n--- HELP ---\n");
    printf("Story: A noble hero is trapped in an eternal maze inside the\n");
    printf("mind of a powerful necromancer. The only way to break free is\n");
    printf("to defeat the foul necromancer himself.\n");
    printf("The exit is a skull-shaped switch in the maze wall —\n");
    printf("press 'a' to use it when in front.\n");
    printf("\nStats:\n");
    printf("  ATK — how hard thou strikest\n");
    printf("  DEF — chance to dodge and reduce damage\n");
    printf("  STA — increases max HP & potion capacity\n");
    printf("  END — increases max water & potion capacity\n");
    printf("  LCK — improves hit & drop chance\n");
    printf("\nControls:\n");
    printf("  f/b  — move forward/backward\n");
    printf("  l/r  — turn left/right\n");
    printf("  a    — attack / use\n");
    printf("  i    — inventory / upgrade menu\n");
    printf("  h    — high scores\n");
    printf("  ?    — this help screen\n");
    printf("  q    — quit\n");
}

/* ── Core text-mode game loop (callable from maze3d.c) ----------------- */

typedef struct {
    int start_level;
    int atk, def, tgh, sta, end, lck;
    int test_enemy;
} TextConfig;

static TextConfig text_cfg = { .start_level = 1, .test_enemy = -1 };

void run_text_mode(int start_level, int start_stats[6], int test_enemy) {
    signal(SIGINT, handle_signal);
    text_cfg.start_level = start_level;
    text_cfg.test_enemy = test_enemy;

    srand(time(0));
    map = malloc(MAP_W * MAP_H);
    if (!map) { fprintf(stderr, "malloc failed\n"); return; }

    /* Apply stat overrides before init_game */
    int orig_stats[6] = {0};
#define APPLY_STAT(sname, sidx) \
    do { if (start_stats[sidx] > 0) { \
        orig_stats[sidx] = p_##sname; \
        p_##sname = start_stats[sidx]; \
    } } while(0)
    /* These are set before init_game, but init_game reinitialises them.
       We'll apply after init_game below. */
    (void)orig_stats;

    /* Run the game loop once (no restart) */
    running = 1;
    game_won = 0;

    while (running) {
        init_game();
        /* Apply stat overrides after init_game resets them */
        if (start_stats[0] > 0) p_attack    = start_stats[0];
        if (start_stats[1] > 0) p_defense   = start_stats[1];
        if (start_stats[2] > 0) p_toughness = start_stats[2];
        if (start_stats[3] > 0) p_stamina   = start_stats[3];
        if (start_stats[4] > 0) p_endurance = start_stats[4];
        if (start_stats[5] > 0) p_luck      = start_stats[5];
        recalc_player_max();
        p_health = p_max_health;
        p_water  = p_max_water;
        if (start_level > 1) p_level = start_level;

        while (running) {
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
            if (text_cfg.test_enemy >= 0 && text_cfg.test_enemy < NUM_ENEMY_TYPES) {
                int fx = (int)player.x + (int)player.dir_x;
                int fy = (int)player.y + (int)player.dir_y;
                if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H && get_map(fx, fy) == 0) {
                    int map_cell = MAP_ENEMY_SKELE;
                    if (text_cfg.test_enemy >= ENEMY_ORC_L1 && text_cfg.test_enemy <= ENEMY_ORC_L12)
                        map_cell = MAP_ENEMY_ORC;
                    else if (text_cfg.test_enemy >= ENEMY_CULTIST_L1)
                        map_cell = MAP_ENEMY_CULTIST;
                    set_map(fx, fy, map_cell);
                    add_sprite(fx, fy, SPRITE_ENEMY, 0.4, text_cfg.test_enemy);
                    log_msg("A %s bars thy path!", enemy_info[text_cfg.test_enemy].name);
                }
            }

            log_msg("Thou dost enter level %d of the labyrinth", p_level);
            printf("\n=== Level %d ===\n", p_level);
            printf("Stats: ATK=%d DEF=%d STA=%d END=%d LCK=%d\n",
                   p_attack, (p_defense + p_toughness) >> 1, p_stamina, p_endurance, p_luck);
            printf("Thou art in a dark maze. Find the exit!\n");
            flush_log_stdio();

            while (running) {
                describe_view();
                printf("\n[f]wd [b]wd [l]eft [r]ight [a]ttack [i]nventory [h]scores [?]help [q]uit: ");
                fflush(stdout);

                char line[64];
                if (!fgets(line, sizeof(line), stdin)) { running = 0; break; }
                char cmd = line[0];

                int r = 0;
                switch (cmd) {
                    case 'f':
                        r = do_move((int)player.dir_x, (int)player.dir_y);
                        flush_log_stdio();
                        if (r == 3) { running = 0; goto victory; }
                        break;
                    case 'b':
                        r = do_move(-(int)player.dir_x, -(int)player.dir_y);
                        flush_log_stdio();
                        if (r == 3) { running = 0; goto victory; }
                        break;
                    case 'l':
                        do_turn(1);
                        flush_log_stdio();
                        break;
                    case 'r':
                        do_turn(0);
                        flush_log_stdio();
                        break;
                    case 'a':
                        r = do_attack();
                        flush_log_stdio();
                        if (r == 3) { running = 0; goto victory; }
                        break;
                    case 'i':
                    case 'I':
                        open_inventory_txt();
                        flush_log_stdio();
                        break;
                    case 'h':
                    case 'H':
                        show_high_scores_txt();
                        flush_log_stdio();
                        break;
                    case 'q':
                    case 'Q':
                        running = 0;
                        break;
                    case '?':
                        help_text();
                        break;
                    default:
                        printf("  Unknown command: %c\n", cmd);
                        break;
                }
                /* Handle level advance and death */
                if (r == 2) {
                    p_level++;
                    p_score += 50;
                    steps = 75 + (p_level - 1) * 15;
                    if (steps > 200000) steps = 200000;
                    generate_maze(steps);
                    player.x = MIDDLE_W + 0.5;
                    player.y = MIDDLE_H + 0.5;
                    log_msg("Thou dost enter level %d of the labyrinth", p_level);
                    printf("\n=== Level %d ===\n", p_level);
                    printf("Thou dost enter a new level of the labyrinth!\n");
                }
                if (p_health <= 0 && running == 0) {
                    /* Death */
                    flush_log_stdio();
                    printf("\nGAME OVER!\n");
                    printf("Level %d, %d steps\n", p_level, p_steps);
                    printf("Score: %d\n", get_current_score());
                    check_high_score_txt();
                    printf("\nWouldst thou try again? [y/N]: ");
                    fflush(stdout);
                    if (!fgets(line, sizeof(line), stdin)) return;
                    if (line[0] == 'y' || line[0] == 'Y') {
                        running = 1;
                        goto death_restart;
                    }
                    return;
                }
            }
            if (!running) goto done;
        }
        if (game_won) goto victory;
        return;

    death_restart:
        ;
    }

    if (game_won) goto victory;
    return;

victory:
    printf("\n");
    printf("  ╔══════════════════════════════════════════╗\n");
    printf("  ║                                        ║\n");
    printf("  ║   HARK! VICTORY!                       ║\n");
    printf("  ║   Thou hast defeated the Necromancer!  ║\n");
    printf("  ║                                        ║\n");
    printf("  ╚══════════════════════════════════════════╝\n");
    printf("\n");
    printf("  Victory achieved at level %d!\n", p_level);
    printf("  Total steps: %d\n", p_steps);
    printf("  Score: %d\n", get_current_score());
    check_high_score_txt();
    return;

done:
    return;
}


