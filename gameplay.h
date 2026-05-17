#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include "engine.h"
#include "enemies.h"
#include "progression.h"

/* ── Animation constants ──────────────────────────────────────── */
#define ANIM_FRAMES 22
#define ATTACK_FRAMES 16

/* ── Game state ───────────────────────────────────────────────── */
extern int p_health, p_water, p_steps, p_level, p_score;
extern int p_dir, anim_new_dir;
extern int is_attack;
extern int flash_type;
extern int exit_x, exit_y;
extern int game_won;

/* ── Player RPG stats (persist across levels) ─────────────────── */
extern int p_attack, p_defense, p_toughness;
extern int p_endurance, p_stamina, p_luck;
extern int p_max_health, p_max_water;

/* ── Derived stat recalc ──────────────────────────────────────── */
void recalc_player_max(void);

/* ── Animation state ──────────────────────────────────────────── */
extern int anim_rem, anim_type, queued;
extern double a_ox, a_oy, a_odx, a_ody, a_opx, a_opy;
extern double a_nx, a_ny, a_ndx, a_ndy, a_npx, a_npy;

/* ── Direction vectors ────────────────────────────────────────── */
extern const double dir_vec[4][2];

/* ── Inventory / Item system ──────────────────────────────────── */
typedef struct {
    unsigned char id;
    unsigned char name[20];
    unsigned char action_text[8];
    int hp_base;
    int water_base;
    int hp_plus_random;
    int water_plus_random;
} consumable;

enum {
    ITEM_HEALING_POTION = 0,
    ITEM_BOTTLE_OF_WATER,
    ITEM_SOULS,
    NUM_ITEM_TYPES
};

extern int inventory[NUM_ITEM_TYPES];
extern const consumable item_defs[NUM_ITEM_TYPES];
extern int sprite_to_item_map[NUM_SPRITE_TYPES];

void use_item(int item_id);

/* ── Monster stat table ───────────────────────────────────────── */
typedef struct {
    int attack, defense, toughness, endurance, stamina, luck;
    int base_health;
    int souls_drop;
} MonsterStats;

extern const MonsterStats monster_stats[NUM_ENEMY_TYPES];

/* ── Enemy HP tracking ────────────────────────────────────────── */
extern int enemy_hp[MAX_SPRITES];

int  get_enemy_idx_at(int fx, int fy);

/* ── Combat ───────────────────────────────────────────────────── */
void attack_enemies(void);   /* reworked: RPG combat vs enemy in front */
void attack_enemy(int fx, int fy);  /* attack specific cell           */
int  enemy_counterattack(int fx, int fy);  /* enemy strikes back      */

/* ── Spawn helpers ────────────────────────────────────────────── */
int  pick_enemy_type(void);

/* ── Game lifecycle ───────────────────────────────────────────── */
int  borders_ok(int x, int y);
void generate_maze(int steps);
void init_game(void);
void consume_item(int mx, int my);
int  end_turn(void);

/* ── Drop popup ───────────────────────────────────────────────── */
extern int drop_popup_item;
extern const char *drop_popup_name;

/* ── Log system ───────────────────────────────────────────────── */
#define LOG_LINES 8
#define LOG_LEN 80
extern char log_buf[LOG_LINES][LOG_LEN];
extern int log_head;

void log_msg(const char *fmt, ...);
void flush_log_stdio(void);

/* ── Stat upgrade ─────────────────────────────────────────────── */
enum {
    STAT_ATTACK = 0,
    STAT_DEFENSE,
    STAT_TOUGHNESS,
    STAT_ENDURANCE,
    STAT_STAMINA,
    STAT_LUCK,
    NUM_STATS
};

extern const char *stat_names[NUM_STATS];
extern int p_upgrade_cnt[NUM_STATS];
void upgrade_stat(int stat_idx);

/* ── Lookup helpers (used by both modes) ──────────────────────── */
const char *get_enemy_name_at(int fx, int fy);

/* ── Movement & animation ─────────────────────────────────────── */
int  try_move(int dx, int dy, int attack);
void try_attack(void);
int  is_opposite(int a, int b);
void try_turn(int left);
void set_player_dir(int d);
void add_sprite(int cx, int cy, int type, double z, int level);
int  get_sprite_type_at(int cx, int cy);
void remove_sprite_at(int cx, int cy);
void update_anim(void);

/* ── High scores (binary, stored in ~/.maze3d/.maze3d.keep) ──── */
#define MAX_HIGHSCORES 5
#define HS_NAME_LEN    11

typedef struct {
    char name[HS_NAME_LEN];
    int  score;
    int  level;
    int  steps;
} HighScoreEntry;

int  is_high_score(int score);
int  add_high_score(const char *name, int score, int level, int steps);
int  load_high_scores(HighScoreEntry *entries);
int  save_high_scores(const HighScoreEntry *entries);
int  get_current_score(void);

/* ── Text mode runner (implemented in maze3d-text.c) ──────────── */
void run_text_mode(int start_level, int start_stats[6], int test_enemy);

#endif
