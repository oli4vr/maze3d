#include "gameplay.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* ── Game state ───────────────────────────────────────────────── */
int p_health, p_water, p_steps, p_level, p_score;
int p_upgrade_cnt[NUM_STATS];
int p_dir, anim_new_dir;
int is_attack = 0;
int flash_type = 0;
int exit_x, exit_y;
int game_won = 0;

/* ── Status effects ──────────────────────────────────────────── */
int p_poisoned = 0;
int p_spirit_turns = 0;
int move_was_step = 0;

/* ── Player RPG stats ────────────────────────────────────────── */
int p_attack, p_defense, p_toughness;
int p_endurance, p_stamina, p_luck;
int p_max_health, p_max_water;

/* ── Animation state ──────────────────────────────────────────── */
int anim_rem, anim_type, queued;
double a_ox, a_oy, a_odx, a_ody, a_opx, a_opy;
double a_nx, a_ny, a_ndx, a_ndy, a_npx, a_npy;

/* ── Drop popup ───────────────────────────────────────────────── */
int drop_popup_item = -1;
const char *drop_popup_name = NULL;

/* ── Direction vectors ────────────────────────────────────────── */
const double dir_vec[4][2] = {{1,0},{0,1},{-1,0},{0,-1}};

/* ── Inventory / Item definitions ─────────────────────────────── */
int inventory[NUM_ITEM_TYPES] = {0};

const consumable item_defs[NUM_ITEM_TYPES] = {
    { ITEM_HEALING_POTION, "Healing Potion",       "Use",   POTION_HP_BASE,       0, POTION_HP_RANDOM,   POTION_WATER_RANDOM },
    { ITEM_BOTTLE_OF_WATER, "Bottle of Water",     "Drink", 0, WATER_BOTTLE_WATER_BASE, WATER_BOTTLE_HP_RANDOM, WATER_BOTTLE_WATER_RANDOM },
    { ITEM_SOULS,          "Souls",               "Consume",   0, 0, 0, 0 },
    { ITEM_SPIRIT_LUCK,    "Spirit 'o Luck",       "Use",   0, 0, 10, 10 },
    { ITEM_ANTIDOTE,       "Antidote",             "Use",   0, 0, 10, 10 },
};

int sprite_to_item_map[NUM_SPRITE_TYPES] = {
    [SPRITE_POTION_BLUE]  = ITEM_BOTTLE_OF_WATER,
    [SPRITE_POTION_GREEN] = ITEM_ANTIDOTE,
    [SPRITE_POTION_RED]   = ITEM_HEALING_POTION,
    [SPRITE_POTION_PINK]  = ITEM_SPIRIT_LUCK,
};

/* ── Monster stat table ──────────────────────────────────────── */
const MonsterStats monster_stats[NUM_ENEMY_TYPES] = {
    [ENEMY_SKELETON_L1]  = SKEL_L1,
    [ENEMY_SKELETON_L2]  = SKEL_L2,
    [ENEMY_SKELETON_L3]  = SKEL_L3,
    [ENEMY_SKELETON_L4]  = SKEL_L4,
    [ENEMY_SKELETON_L5]  = SKEL_L5,
    [ENEMY_SKELETON_L6]  = SKEL_L6,
    [ENEMY_SKELETON_L7]  = SKEL_L7,
    [ENEMY_SKELETON_L8]  = SKEL_L8,
    [ENEMY_SKELETON_L9]  = SKEL_L9,
    [ENEMY_SKELETON_L10] = SKEL_L10,
    [ENEMY_SKELETON_L11] = SKEL_L11,
    [ENEMY_SKELETON_L12] = SKEL_L12,
    [ENEMY_ORC_L1]       = ORC_L1,
    [ENEMY_ORC_L2]       = ORC_L2,
    [ENEMY_ORC_L3]       = ORC_L3,
    [ENEMY_ORC_L4]       = ORC_L4,
    [ENEMY_ORC_L5]       = ORC_L5,
    [ENEMY_ORC_L6]       = ORC_L6,
    [ENEMY_ORC_L7]       = ORC_L7,
    [ENEMY_ORC_L8]       = ORC_L8,
    [ENEMY_ORC_L9]       = ORC_L9,
    [ENEMY_ORC_L10]      = ORC_L10,
    [ENEMY_ORC_L11]      = ORC_L11,
    [ENEMY_ORC_L12]      = ORC_L12,
    [ENEMY_CULTIST_L1]   = CULTIST_L1,
    [ENEMY_CULTIST_L2]   = CULTIST_L2,
    [ENEMY_CULTIST_L3]   = CULTIST_L3,
    [ENEMY_CULTIST_L4]   = CULTIST_L4,
    [ENEMY_CULTIST_L5]   = CULTIST_L5,
    [ENEMY_CULTIST_L6]   = CULTIST_L6,
    [ENEMY_CULTIST_L7]   = CULTIST_L7,
    [ENEMY_CULTIST_L8]   = CULTIST_L8,
    [ENEMY_CULTIST_L9]   = CULTIST_L9,
    [ENEMY_CULTIST_L10]  = CULTIST_L10,
    [ENEMY_CULTIST_L11]  = CULTIST_L11,
    [ENEMY_CULTIST_L12]  = CULTIST_L12,
    [ENEMY_BOSS]         = BOSS_STAT,
};

/* ── Enemy current HP (parallel to sprites[]) ────────────────── */
int enemy_hp[MAX_SPRITES];

/* ── Log system ──────────────────────────────────────────────── */
char log_buf[LOG_LINES][LOG_LEN];
int log_head = 0;

static int prev_log_mark = 0;

void log_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(log_buf[log_head], LOG_LEN, fmt, ap);
    va_end(ap);
    log_head = (log_head + 1) % LOG_LINES;
}

/* Print any log entries since last call to stdout (for text mode). */
void flush_log_stdio(void) {
    while (prev_log_mark != log_head) {
        if (log_buf[prev_log_mark][0])
            printf("  %s\n", log_buf[prev_log_mark]);
        prev_log_mark = (prev_log_mark + 1) % LOG_LINES;
    }
}

/* ── Stat names ──────────────────────────────────────────────── */
const char *stat_names[NUM_STATS] = {
    "Attack", "Defense", "Toughness", "Endurance", "Stamina", "Luck",
};

void recalc_player_max(void) {
    p_max_health = PLAYER_BASE_HEALTH + (10 * p_stamina);
    p_max_water  = PLAYER_BASE_WATER  + (10 * p_endurance);
}

void upgrade_stat(int stat_idx) {
    if (stat_idx < 0 || stat_idx >= NUM_STATS) return;
    int cost = SOULS_UPGRADE_COST + p_upgrade_cnt[stat_idx];

    /* Unified DEF uses combined upgrade count for cost */
    if (stat_idx == STAT_DEFENSE)
        cost = SOULS_UPGRADE_COST + p_upgrade_cnt[STAT_DEFENSE];

    if (inventory[ITEM_SOULS] < cost) return;

    inventory[ITEM_SOULS] -= cost;

    if (stat_idx == STAT_DEFENSE) {
        p_upgrade_cnt[STAT_DEFENSE]++;
        p_upgrade_cnt[STAT_TOUGHNESS]++;
        p_defense++;
        p_toughness++;
    } else {
        p_upgrade_cnt[stat_idx]++;
        switch (stat_idx) {
            case STAT_ATTACK:    p_attack++;    break;
            case STAT_ENDURANCE: p_endurance++; break;
            case STAT_STAMINA:   p_stamina++;   break;
            case STAT_LUCK:      p_luck++;      break;
            default: break;
        }
    }

    recalc_player_max();
    int val = stat_idx == STAT_DEFENSE ? p_defense + p_toughness :
              stat_idx == STAT_ATTACK    ? p_attack :
              stat_idx == STAT_ENDURANCE ? p_endurance :
              stat_idx == STAT_STAMINA   ? p_stamina : p_luck;
    log_msg("Thy %s is now but %d!", stat_names[stat_idx], val);
}

/* ── Helper: find sprite index for an enemy at a map cell ────── */
int get_enemy_idx_at(int fx, int fy) {
    for (int i = 0; i < num_sprites; i++)
        if ((int)sprites[i].x == fx && (int)sprites[i].y == fy
            && sprites[i].type == SPRITE_ENEMY)
            return i;
    return -1;
}

/* ── Enemy type selection based on game level ────────────────── */
int pick_enemy_type(void) {
    int base_skel = 1 + (p_level * 11) / 39;
    int base_orc  = (p_level >= 5) ? (1 + ((p_level - 5) * 11) / 34) : 0;
    int base_cult = (p_level >= 10) ? (1 + ((p_level - 10) * 11) / 29) : 0;

    if (base_skel > 12) base_skel = 12;
    if (base_orc  > 12) base_orc  = 12;
    if (base_cult > 12) base_cult = 12;

    int families[3], nf = 0;
    if (base_skel > 0) families[nf++] = 0;
    if (base_orc  > 0) families[nf++] = 1;
    if (base_cult > 0) families[nf++] = 2;

    int family = families[rand() % nf];
    int base_type;
    switch (family) {
        case 0:  base_type = base_skel; break;
        case 1:  base_type = base_orc;  break;
        default: base_type = base_cult; break;
    }

    int tier = base_type;
    if (tier > 1 && (rand() & 1)) tier--;   /* 50% chance one tier lower */
    if (tier > 12) tier = 12;

    switch (family) {
        case 0:  return ENEMY_SKELETON_L1 + (tier - 1);
        case 1:  return ENEMY_ORC_L1      + (tier - 1);
        default: return ENEMY_CULTIST_L1  + (tier - 1);
    }
}

/* ── Player direction ────────────────────────────────────────── */
void set_player_dir(int d) {
    p_dir = d;
    player.dir_x = dir_vec[d][0];
    player.dir_y = dir_vec[d][1];
    player.plane_x = dir_vec[d][1] * 0.767;
    player.plane_y = -dir_vec[d][0] * 0.767;
}

/* ── Sprite helpers ──────────────────────────────────────────── */
void add_sprite(int cx, int cy, int type, double z, int level) {
    if (num_sprites >= MAX_SPRITES) return;
    sprites[num_sprites].x = cx + 0.5;
    sprites[num_sprites].y = cy + 0.5;
    sprites[num_sprites].z = z;
    sprites[num_sprites].type = type;
    sprites[num_sprites].level = level;
    if (type == SPRITE_ENEMY && level >= 0 && level < NUM_ENEMY_TYPES)
        enemy_hp[num_sprites] = monster_stats[level].base_health
                                + (10 * monster_stats[level].stamina);
    num_sprites++;
}

int get_sprite_type_at(int cx, int cy) {
    for (int i = 0; i < num_sprites; i++)
        if ((int)sprites[i].x == cx && (int)sprites[i].y == cy)
            return sprites[i].type;
    return -1;
}

void remove_sprite_at(int cx, int cy) {
    for (int i = 0; i < num_sprites; i++)
        if ((int)sprites[i].x == cx && (int)sprites[i].y == cy) {
            if (i < num_sprites - 1)
                enemy_hp[i] = enemy_hp[num_sprites - 1];
            sprites[i] = sprites[--num_sprites];
            return;
        }
}

/* ── Combat ──────────────────────────────────────────────────── */

/* Resolve one round: player attacks the enemy at (fx,fy).       */
/* Returns 1 if enemy died, 0 otherwise. Logs messages.          */
void attack_enemy(int fx, int fy) {
    int si = get_enemy_idx_at(fx, fy);
    if (si < 0) return;

    int etype = sprites[si].level;
    if (etype < 0 || etype >= NUM_ENEMY_TYPES) return;

    const MonsterStats *m = &monster_stats[etype];
    const char *ename = enemy_info[etype].name;

    /* Apply Spirit 'o Luck buff: double luck and defense */
    int eff_luck = p_spirit_turns > 0 ? p_luck * 2 : p_luck;
    int eff_def  = p_spirit_turns > 0 ? p_defense * 2 : p_defense;

    /* --- Player attacks enemy --- */
    int lck_mod = eff_luck - m->luck;
    if (lck_mod < -3) lck_mod = -3;
    if (lck_mod > 3) lck_mod = 3;
    int tohit = p_attack + (rand() % 9) + lck_mod;
    if (tohit >= m->defense) {
        int dmg = p_attack - m->toughness;
        if (dmg < 0) dmg = 0;
        if (dmg == 0) dmg = 1;   /* always at least 1 on hit */

        enemy_hp[si] -= dmg;
        log_msg("Thou dost strike %s for %d damage!", ename, dmg);

        if (enemy_hp[si] <= 0) {
            /* Enemy dies */
            int souls = m->souls_drop;
            inventory[ITEM_SOULS] += souls;
            if (inventory[ITEM_SOULS] > MAX_SOULS_STACK)
                inventory[ITEM_SOULS] = MAX_SOULS_STACK;
            log_msg("Thou hast vanquished the %s! +%d souls", ename, souls);
            p_score += souls * 5;
            /* Drop chance: 25% + luck, capped at 50% */
            int drop_chance = 25 + eff_luck;
            if (drop_chance > 50) drop_chance = 50;
            if ((rand() % 100) < drop_chance) {
                int item;
                int dr = rand() % 100;
                if (dr < 40) {
                    item = ITEM_HEALING_POTION;
                } else if (dr < 80) {
                    item = ITEM_BOTTLE_OF_WATER;
                } else if (dr < 90) {
                    item = ITEM_SPIRIT_LUCK;
                } else {
                    item = (p_level >= 9) ? ITEM_ANTIDOTE : ITEM_SPIRIT_LUCK;
                }
                if (inventory[item] < ITEM_MAX_STACK(p_stamina, p_endurance)) {
                    inventory[item]++;
                    log_msg("%s hath dropped a %s", ename, item_defs[item].name);
                    drop_popup_item = item;
                    drop_popup_name = (const char *)item_defs[item].name;
                }
            }
            /* Death strike: enemy lashes out as it dies */
            log_msg("%s lashes out in its death throes!", ename);
            {
                int e_atk = m->attack + ENEMY_ATK_LEVEL_BONUS(p_level);
                int can_magic = (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12)
                                || etype == ENEMY_BOSS;
                if (can_magic && (rand() % 100) < 75) {
                    int edmg = e_atk + (e_atk >> 2) * (rand() & 1);
                    p_health -= edmg;
                    log_msg("A dark spell strikes thee for %d damage!", edmg);
                    /* Cultists may also poison on death strike */
                    if (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12)
                        p_poisoned = 1;
                } else {
                    int elck_mod = m->luck - eff_luck;
                    if (elck_mod < -3) elck_mod = -3;
                    if (elck_mod > 3) elck_mod = 3;
                    int etohit = e_atk + (rand() % 9) + elck_mod;
                    if (etohit >= eff_def) {
                        int edmg = e_atk - (p_toughness >> 1);
                        if (edmg < 0) edmg = 0;
                        if (edmg == 0) edmg = 1;
                        p_health -= edmg;
                        log_msg("It strikes thee for %d damage!", edmg);
                        /* Cultists may also poison on death strike */
                        if (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12)
                            p_poisoned = 1;
                    } else {
                        log_msg("It misses thee!");
                    }
                }
                if (p_health < 0) p_health = 0;
                /* Clear drop popup if player dies from death strike */
                if (p_health <= 0) {
                    drop_popup_item = -1;
                    drop_popup_name = NULL;
                }
            }
            if (etype == ENEMY_BOSS) {
                p_steps += BOSS_BONUS;
                p_score += BOSS_BONUS;
                log_msg("=== VICTORY! Thou hast defeated the Necromancer! ===");
                game_won = 1;
            }
            set_map(fx, fy, 0);
            remove_sprite_at(fx, fy);
            return;
        }
    } else {
        log_msg("Thou dost assail %s, but fail to land a blow", ename);
    }

    /* --- Enemy counter-attacks (if still alive) --- */
    int e_atk = m->attack + ENEMY_ATK_LEVEL_BONUS(p_level);
    int can_magic = (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12)
                    || etype == ENEMY_BOSS;
    int is_cultist = (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12);
    if (can_magic && ((rand() % 100) < (10 + m->luck))) {
        int edmg = e_atk + (e_atk >> 2) * (rand() & 1);
        p_health -= edmg;
        log_msg("%s hurls a dark spell for %d damage", ename, edmg);
        /* Cultists may poison on magical counter */
        if (is_cultist && (rand() & 1)) {
            p_poisoned = 1;
            log_msg("The foul magic doth poison thee!");
        }
    } else {
        int elck_mod = m->luck - eff_luck;
        if (elck_mod < -3) elck_mod = -3;
        if (elck_mod > 3) elck_mod = 3;
        int etohit = e_atk + (rand() % 9) + elck_mod;
        if (etohit >= eff_def) {
            int edmg = e_atk - (p_toughness >> 1);
            if (edmg < 0) edmg = 0;
            if (edmg == 0) edmg = 1;
            p_health -= edmg;
            log_msg("%s strikes thee for %d damage", ename, edmg);
            /* Cultist poison stab */
            if (is_cultist && (rand() & 1)) {
                p_poisoned = 1;
                log_msg("The cultist's blade doth poison thee!");
            }
        } else {
            log_msg("%s assails thee, but misses", ename);
        }
    }
    if (p_health < 0) p_health = 0;
    if (p_health <= 0) return;
}

/* Legacy wrapper: called on lunge-completion / space while
   queuing.  Attacks the enemy directly in front of the player.   */
void attack_enemies(void) {
    int fx = (int)player.x + (int)player.dir_x;
    int fy = (int)player.y + (int)player.dir_y;
    if (fx >= 0 && fx < MAP_W && fy >= 0 && fy < MAP_H) {
        int v = get_map(fx, fy);
        if (MAP_IS_ENEMY(v)) {
            attack_enemy(fx, fy);
        }
    }
    p_steps++;
    p_score++;
}

/* ── Maze generation helpers ─────────────────────────────────── */
int borders_ok(int x, int y) {
    if (x < 1 || x >= MAP_W - 1 || y < 1 || y >= MAP_H - 1) return 0;
    if (get_map(x+1,y)==0 && get_map(x,y+1)==0 && get_map(x+1,y+1)==0) return 0;
    if (get_map(x+1,y)==0 && get_map(x,y-1)==0 && get_map(x+1,y-1)==0) return 0;
    if (get_map(x-1,y)==0 && get_map(x,y-1)==0 && get_map(x-1,y-1)==0) return 0;
    if (get_map(x-1,y)==0 && get_map(x,y+1)==0 && get_map(x-1,y+1)==0) return 0;
    return 1;
}

void generate_maze(int steps) {
    for (int sy = 0; sy < MAP_H; sy += 8)
        for (int sx = 0; sx < MAP_W; sx += 8) {
            int ct = rand() % NUM_COLOUR_TYPES;
            int ts = rand() % NUM_TEXTURE_STYLES;
            int zone = ct * NUM_TEXTURE_STYLES + ts;
            for (int dy = 0; dy < 8; dy++)
                for (int dx = 0; dx < 8; dx++) {
                    int var = rand() % 4;
                     set_map(sx + dx, sy + dy, zone * VARIANTS_PER_TYPE + 1 + var);
                }
        }

    for (int sy = MIDDLE_H - 4; sy < MIDDLE_H + 4; sy++)
        for (int sx = MIDDLE_W - 4; sx < MIDDLE_W + 4; sx++) {
            int var = rand() % 4;
             set_map(sx, sy, 3 * VARIANTS_PER_TYPE + 1 + var);
        }

    int x = MIDDLE_W, y = MIDDLE_H;
    for (int n = 0; n < steps; n++) {
        if (get_map(x, y) > 0) set_map(x, y, 0);
        int nx = x, ny = y;
        switch (rand() & 3) {
            case 0: nx = x + 1; break;
            case 1: nx = x - 1; break;
            case 2: ny = y + 1; break;
            case 3: ny = y - 1; break;
        }
        if (borders_ok(nx, ny)) { x = nx; y = ny; }
    }

    num_sprites = 0;
    int best_dist = 0;
    exit_x = MIDDLE_W; exit_y = MIDDLE_H;

    for (int my = 0; my < MAP_H; my++)
        for (int mx = 0; mx < MAP_W; mx++) {
            if (get_map(mx, my) != 0) continue;
            int dist = abs(mx - MIDDLE_W) + abs(my - MIDDLE_H);
            if (dist > best_dist) { best_dist = dist; exit_x = mx; exit_y = my; }

           /* Keep a 5×5 clear zone around the player start */
           if (abs(mx - MIDDLE_W) <= 2 && abs(my - MIDDLE_H) <= 2) continue;

           int r = rand();
             /* Potion: ~8% overall (single roll, branching sub-types) */
             if (((r >> 16) & 3) == 0 && ((r >> 1) & 3) == 0) {
                 int sp_type;
                 if ((rand() % 16) == 0) {
                     sp_type = SPRITE_POTION_PINK;   /* Spirit 'o Luck */
                 } else if (p_level >= 9 && (rand() % 8) == 0) {
                     sp_type = SPRITE_POTION_GREEN;  /* Antidote */
                 } else {
                     sp_type = (rand() & 1) ? SPRITE_POTION_BLUE : SPRITE_POTION_RED;
                 }
                 set_map(mx, my, MAP_POTION);
                 add_sprite(mx, my, sp_type, 0.0, 0);
             } else if ((r % 9) == 0) {
                 set_map(mx, my, MAP_WATER);
                 add_sprite(mx, my, SPRITE_WATER, 0.0, 0);
               } else if (((r >> 4) & 7) == 0) {
                  /* Don't place enemy adjacent to another enemy */
                  int blocked = 0;
                  int x1 = mx > 0 ? mx - 1 : mx;
                  int x2 = mx < MAP_W - 1 ? mx + 1 : mx;
                  int y1 = my > 0 ? my - 1 : my;
                  int y2 = my < MAP_H - 1 ? my + 1 : my;
                  for (int dy = y1; dy <= y2 && !blocked; dy++)
                      for (int dx = x1; dx <= x2 && !blocked; dx++)
                          if (MAP_IS_ENEMY(get_map(dx, dy)))
                              blocked = 1;
                  if (blocked) continue;
                  int etype = pick_enemy_type();
                 int emap = MAP_ENEMY_SKELE;
                 if (etype >= ENEMY_ORC_L1 && etype <= ENEMY_ORC_L12) emap = MAP_ENEMY_ORC;
                 else if (etype >= ENEMY_CULTIST_L1 && etype <= ENEMY_CULTIST_L12) emap = MAP_ENEMY_CULTIST;
                 else if (etype == ENEMY_BOSS) emap = MAP_ENEMY_BOSS;
                 set_map(mx, my, emap);
                 add_sprite(mx, my, SPRITE_ENEMY, 0.4, etype);
             }
        }

   remove_sprite_at(exit_x, exit_y);
   if (p_level >= BOSS_LEVEL) {
       /* Boss level — place necromancer at exit instead of skull wall */
       set_map(exit_x, exit_y, MAP_ENEMY_BOSS);
       add_sprite(exit_x, exit_y, SPRITE_ENEMY, 0.4, ENEMY_BOSS);
   } else {
       int zone = -1;
       for (int i = 0; i < 4; i++) {
           int rx = exit_x + (rand() % 5) - 2;
           int ry = exit_y + (rand() % 5) - 2;
           if (rx >= 0 && rx < MAP_W && ry >= 0 && ry < MAP_H) {
               int v = get_map(rx, ry);
               if (v >= 1 && v <= NUM_TEX) {
                   zone = (v - 1) / VARIANTS_PER_TYPE;
                   break;
               }
           }
       }
       if (zone < 0) {
           int ct = rand() % NUM_COLOUR_TYPES;
           int ts = rand() % NUM_TEXTURE_STYLES;
           zone = ct * NUM_TEXTURE_STYLES + ts;
       }
       set_map(exit_x, exit_y, zone * VARIANTS_PER_TYPE + 1 + 4);
   }
  }

/* ── Game lifecycle ───────────────────────────────────────────── */

void init_game(void) {
    p_health = PLAYER_BASE_HEALTH;
    p_water = PLAYER_BASE_WATER;
    p_steps = 0;
    p_level = 1;
    p_score = 0;
    for (int i = 0; i < NUM_STATS; i++) p_upgrade_cnt[i] = 0;
    p_dir = 0;

    p_attack    = PLAYER_BASE_ATTACK;
    p_defense   = PLAYER_BASE_DEFENSE;
    p_toughness = PLAYER_BASE_TOUGHNESS;
    p_endurance = PLAYER_BASE_ENDURANCE;
    p_stamina   = PLAYER_BASE_STAMINA;
    p_luck      = PLAYER_BASE_LUCK;
    recalc_player_max();

    p_poisoned = 0;
    p_spirit_turns = 0;
    move_was_step = 0;

    memset(inventory, 0, sizeof(inventory));
    log_head = 0;
    prev_log_mark = 0;
    game_won = 0;
    for (int i = 0; i < LOG_LINES; i++) log_buf[i][0] = '\0';
    log_msg("Thou hast awoken...");
    log_msg("within the necromancer's labyrinth");
}

static int skip_next_drain = 0;  /* skip water drain on next end_turn() */

void consume_item(int mx, int my) {
     int v = get_map(mx, my);
     if (v == MAP_WATER) {
         int hp_heal = rand() % 11;  /* 0-10 HP */
         int wtr_gain = 15 + (rand() & 15);
         p_health += hp_heal;
         if (p_health > p_max_health) p_health = p_max_health;
         p_water += wtr_gain;
         if (p_water > p_max_water) p_water = p_max_water;
         skip_next_drain = 1;
         set_map(mx, my, 0);
         remove_sprite_at(mx, my);
         flash_type = 1;
         log_msg("Thou dost drink from a murky puddle (+%d HP, +%d WTR)", hp_heal, wtr_gain);
     } else if (v == MAP_POTION) {
          int sp_type = get_sprite_type_at(mx, my);
          int item_id = (sp_type >= 0 && sp_type < NUM_SPRITE_TYPES)
                        ? sprite_to_item_map[sp_type] : -1;
           int maxst = (item_id == ITEM_SOULS) ? MAX_SOULS_STACK : ITEM_MAX_STACK(p_stamina, p_endurance);
          if (item_id >= 0 && item_id < NUM_ITEM_TYPES && inventory[item_id] < maxst) {
              inventory[item_id]++;
              set_map(mx, my, 0);
              remove_sprite_at(mx, my);
              flash_type = 3; /* grey flash for pickup */
              log_msg("Thou hast picked up %s", item_defs[item_id].name);
          }
     }
  }

void use_item(int item_id) {
    if (item_id < 0 || item_id >= NUM_ITEM_TYPES) return;
    if (item_id == ITEM_SOULS) return;  /* souls handled via upgrade menu */

    if (inventory[item_id] <= 0) return;

    const consumable *def = &item_defs[item_id];
    int hp_gain = def->hp_base + (rand() % (def->hp_plus_random + 1));
    int water_gain = def->water_base + (rand() % (def->water_plus_random + 1));

    p_health += hp_gain;
    if (p_health > p_max_health) p_health = p_max_health;
    p_water += water_gain;
    if (p_water > p_max_water) p_water = p_max_water;

    if (water_gain > 0) skip_next_drain = 1;

    /* Apply special effects */
    if (item_id == ITEM_SPIRIT_LUCK) {
        p_spirit_turns = SPIRIT_DURATION(p_luck);
        log_msg("Spirit 'o Luck emboldens thee for %d turns!", p_spirit_turns);
    }
    if (item_id == ITEM_ANTIDOTE) {
        if (p_poisoned) {
            p_poisoned = 0;
            log_msg("The Antidote cleanseth the poison from thy veins!");
        }
    }

    inventory[item_id]--;
    flash_type = (item_id == ITEM_BOTTLE_OF_WATER) ? 1 :
                 (item_id == ITEM_HEALING_POTION) ? 2 :
                 (item_id == ITEM_SPIRIT_LUCK) ? 3 :
                 (item_id == ITEM_ANTIDOTE) ? 2 : 3;
    log_msg("Used %s (+%d HP, +%d WTR)", def->name, hp_gain, water_gain);
}

int end_turn(void) {
     /* Poison damage on any step (only forward/backward set move_was_step) */
     if (p_poisoned && move_was_step) {
         int pdmg = POISON_TICK_MIN + (rand() % (POISON_TICK_MAX - POISON_TICK_MIN + 1));
         p_health -= pdmg;
         log_msg("Poison wracks thy frame for %d HP!", pdmg);
         if (p_health <= 0) { p_health = 0; log_msg("The poison hath claimed thee!"); return 1; }
     }
     move_was_step = 0;

     /* Decay spirit turns */
     if (p_spirit_turns > 0) {
         p_spirit_turns--;
         if (p_spirit_turns == 0)
             log_msg("The Spirit 'o Luck fades away");
     }

     if (skip_next_drain) {
         skip_next_drain = 0;
     } else if (p_water == 0) {
         p_health -= TURN_HP_DRAIN;
         flash_type = 4;
         log_msg("Thou dost lose %d HP to unquenchable thirst", TURN_HP_DRAIN);
     } else p_water -= TURN_WATER_COST;
     if (p_water < 0) p_water = 0;
     if (p_health < 0) p_health = 0;
     p_steps++;
     p_score++;
     return (p_health == 0) ? 1 : 0;
 }

/* ── Enemy name lookup ────────────────────────────────────────── */

const char *get_enemy_name_at(int fx, int fy) {
    if (fx < 0 || fx >= MAP_W || fy < 0 || fy >= MAP_H) return NULL;
    if (!MAP_IS_ENEMY(get_map(fx, fy))) return NULL;
    for (int i = 0; i < num_sprites; i++)
        if ((int)sprites[i].x == fx && (int)sprites[i].y == fy) {
            int e = sprites[i].level;
            if (e >= 0 && e < NUM_ENEMY_TYPES)
                return enemy_info[e].name;
            return NULL;
        }
    return NULL;
}

/* ── Movement & animation setup ───────────────────────────────── */

int try_move(int dx, int dy, int attack) {
     int nx = (int)player.x + dx, ny = (int)player.y + dy;
     int v = get_map(nx, ny);
   if (v >= 1 && v <= NUM_TEX) return 0;
       if (MAP_IS_ENEMY(v)) return -1;
      is_attack = attack;
      move_was_step = 1;  /* forward/backward step for poison ticking */
     a_ox = player.x; a_oy = player.y;
    a_nx = nx + 0.5; a_ny = ny + 0.5;
    a_odx = a_ndx = player.dir_x;
    a_ody = a_ndy = player.dir_y;
    a_opx = a_npx = player.plane_x;
    a_opy = a_npy = player.plane_y;
    anim_rem = ANIM_FRAMES; anim_type = 0; queued = 0;
    return 1;
}

void try_attack(void) {
    a_ox = player.x;
    a_oy = player.y;
    a_nx = player.x + player.dir_x * 0.25;
    a_ny = player.y + player.dir_y * 0.25;
    a_odx = a_ndx = player.dir_x;
    a_ody = a_ndy = player.dir_y;
    a_opx = a_npx = player.plane_x;
    a_opy = a_npy = player.plane_y;
    anim_rem = ATTACK_FRAMES; anim_type = 2; queued = 0;
}

int is_opposite(int a, int b) {
    return (a == 1 && b == 2) || (a == 2 && b == 1) ||
           (a == 3 && b == 4) || (a == 4 && b == 3);
}

void try_turn(int left) {
    move_was_step = 0; /* turning does not trigger poison */
    a_ox = player.x; a_oy = player.y;
    a_nx = player.x; a_ny = player.y;
    a_odx = player.dir_x; a_ody = player.dir_y;
    a_opx = player.plane_x; a_opy = player.plane_y;
    anim_new_dir = (p_dir + (left ? 1 : 3)) & 3;
    a_ndx = dir_vec[anim_new_dir][0];
    a_ndy = dir_vec[anim_new_dir][1];
    a_npx = dir_vec[anim_new_dir][1] * 0.767;
    a_npy = -dir_vec[anim_new_dir][0] * 0.767;
    anim_rem = ANIM_FRAMES; anim_type = 1; queued = 0;
}

/* ── Animation update ─────────────────────────────────────────-- */

void update_anim(void) {
    if (anim_rem <= 0) return;
    anim_rem--;
    double t = 1.0 - (double)anim_rem / ANIM_FRAMES;
    if (anim_type == 2) {
        double tt = 1.0 - (double)anim_rem / ATTACK_FRAMES;
        double bt = (tt < 0.5) ? tt * 2.0 : 2.0 - tt * 2.0;
        player.x = a_ox + (a_nx - a_ox) * bt;
        player.y = a_oy + (a_ny - a_oy) * bt;
    } else {
        player.x = a_ox + (a_nx - a_ox) * t;
        player.y = a_oy + (a_ny - a_oy) * t;
        if (anim_type == 1) {
            double a = t * M_PI / 2;
            if (a_ndx == a_ody && a_ndy == -a_odx) a = -a;
            double ca = cos(a), sa = sin(a);
            player.dir_x = a_odx * ca - a_ody * sa;
            player.dir_y = a_odx * sa + a_ody * ca;
            player.plane_x = a_opx * ca - a_opy * sa;
            player.plane_y = a_opx * sa + a_opy * ca;
        }
    }
}

/* ── High scores ──────────────────────────────────────────────── */

#define HS_DIR  ".maze3d"
#define HS_FILE "maze3d.keep"
#define HS_XOR_KEY 0xAB
#define HS_MAGIC   0xFEEDFACE

static void hs_path(char *buf, size_t sz) {
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/%s/%s", home, HS_DIR, HS_FILE);
}

static void hs_xor(unsigned char *data, int len) {
    for (int i = 0; i < len; i++) data[i] ^= HS_XOR_KEY;
}

int get_current_score(void) {
    return p_score;
}

int is_high_score(int score) {
    HighScoreEntry entries[MAX_HIGHSCORES];
    memset(entries, 0, sizeof(entries));
    int n = load_high_scores(entries);
    if (n < MAX_HIGHSCORES) return 1;
    for (int i = 0; i < n; i++)
        if (score > entries[i].score) return 1;
    return 0;
}

int add_high_score(const char *name, int score, int level, int steps) {
    HighScoreEntry entries[MAX_HIGHSCORES];
    memset(entries, 0, sizeof(entries));
    int n = load_high_scores(entries);

    /* Find insertion position */
    int pos = n;
    for (int i = 0; i < n; i++) {
        if (score > entries[i].score) { pos = i; break; }
    }
    if (pos >= MAX_HIGHSCORES) return 0;

    /* Shift later entries down */
    int to_move = MAX_HIGHSCORES - pos - 1;
    if (to_move > 0)
        memmove(&entries[pos + 1], &entries[pos], to_move * sizeof(HighScoreEntry));
    if (n < MAX_HIGHSCORES) n++;

    /* Insert new entry */
    strncpy(entries[pos].name, name, HS_NAME_LEN - 1);
    entries[pos].name[HS_NAME_LEN - 1] = '\0';
    entries[pos].score  = score;
    entries[pos].level  = level;
    entries[pos].steps  = steps;

    return save_high_scores(entries);
}

static const char *default_names[MAX_HIGHSCORES] = {
    "Arnold", "Silvester", "Bruce", "Jackie", "Clint"
};
static const int default_scores[MAX_HIGHSCORES] = { 5000, 4000, 3000, 2000, 1000 };

int load_high_scores(HighScoreEntry *entries) {
    char path[512];
    hs_path(path, sizeof(path));
    FILE *f = fopen(path, "rb");
    if (!f) {
        /* File doesn't exist — create defaults */
        HighScoreEntry def[MAX_HIGHSCORES];
        memset(def, 0, sizeof(def));
        for (int i = 0; i < MAX_HIGHSCORES; i++) {
            strncpy(def[i].name, default_names[i], HS_NAME_LEN - 1);
            def[i].name[HS_NAME_LEN - 1] = '\0';
            def[i].score  = default_scores[i];
            def[i].level  = MAX_HIGHSCORES - i;
            def[i].steps  = default_scores[i] - (MAX_HIGHSCORES - i - 1) * 100;
        }
        save_high_scores(def);
        memcpy(entries, def, sizeof(def));
        return MAX_HIGHSCORES;
    }

    /* Read raw data */
    unsigned char raw[sizeof(HighScoreEntry) * MAX_HIGHSCORES + 8];
    int rd = fread(raw, 1, sizeof(raw), f);
    fclose(f);
    if (rd < 8) return 0;

    hs_xor(raw, rd);

    /* Check magic */
    unsigned int magic;
    memcpy(&magic, raw, 4);
    if (magic != HS_MAGIC) {
        /* Invalid file — re-create with defaults */
        HighScoreEntry def[MAX_HIGHSCORES];
        memset(def, 0, sizeof(def));
        for (int i = 0; i < MAX_HIGHSCORES; i++) {
            strncpy(def[i].name, default_names[i], HS_NAME_LEN - 1);
            def[i].name[HS_NAME_LEN - 1] = '\0';
            def[i].score  = default_scores[i];
            def[i].level  = MAX_HIGHSCORES - i;
            def[i].steps  = default_scores[i] - (MAX_HIGHSCORES - i - 1) * 100;
        }
        save_high_scores(def);
        memcpy(entries, def, sizeof(def));
        return MAX_HIGHSCORES;
    }

    int count = raw[4];
    if (count > MAX_HIGHSCORES) count = MAX_HIGHSCORES;

    memcpy(entries, raw + 8, count * sizeof(HighScoreEntry));
    return count;
}

int save_high_scores(const HighScoreEntry *entries) {
    char path[512];
    hs_path(path, sizeof(path));

    /* Ensure directory exists */
    char dir[512];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(dir, sizeof(dir), "%s/%s", home, HS_DIR);
    mkdir(dir, 0700);

    unsigned char raw[8 + sizeof(HighScoreEntry) * MAX_HIGHSCORES];
    unsigned int magic = HS_MAGIC;
    memcpy(raw, &magic, 4);
    raw[4] = (unsigned char)MAX_HIGHSCORES;
    memset(raw + 5, 0, 3);
    memcpy(raw + 8, entries, sizeof(HighScoreEntry) * MAX_HIGHSCORES);

    int total = 8 + sizeof(HighScoreEntry) * MAX_HIGHSCORES;
    hs_xor(raw, total);

    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int wrote = fwrite(raw, 1, total, f);
    fclose(f);
    return wrote == total;
}
