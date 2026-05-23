/* engine.h — Public interface for the 2.5D raycasting engine
 *
 * Defines map dimensions, texture/sprite constants, the Player/Sprite
 * data types, and declares all engine functions and global state.
 * Used by maze3d.c (interactive game) and maze3d-demo.c (auto-pilot). */

#ifndef ENGINE_H
#define ENGINE_H

#include <stdint.h>
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif
#include <locale.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Fixed-point arithmetic ────────────────────────────────────── */
typedef int64_t fp_t;
#define FP_SHIFT 20
#define FP_SCALE (1LL << FP_SHIFT)
#define FP_ONE   (1LL << FP_SHIFT)
#define int_to_fp(x)  ((fp_t)(x) << FP_SHIFT)
#define fp_to_int(x)  ((int)((x) >> FP_SHIFT))
#define double_to_fp(x) ((fp_t)((x) * FP_SCALE))

/* ── Performance: cast 1 ray per RAY_STEP columns ─────────────── */
#define RAY_STEP 1

/* ── Animation timings (milliseconds) ──────────────────────────── */
/* Controls how long each animation takes regardless of FPS.
   Tune to taste — shorter = snappier, longer = smoother.          */
#define ANIM_MOVE_MS    250   /* forward/backward move (ms)       */
#define ANIM_TURN_MS    200   /* 90-degree turn (ms)              */
#define ATTACK_MS       150   /* attack lunge (ms)                */
#define AUTO_MOVE_MS    250   /* demo-mode move (ms)              */
#define AUTO_TURN_MS    200   /* demo-mode turn (ms)              */

int64_t get_time_ms(void);   /* monotonic milliseconds since boot */

/* ── Map dimensions ───────────────────────────────────────────── */
#define MAP_W 512          /* world width  in cells               */
#define MAP_H 512          /* world height in cells               */
#define MIDDLE_W (MAP_W/2) /* centre column (player spawn)        */
#define MIDDLE_H (MAP_H/2) /* centre row    (player spawn)        */
#define MAZE_STEPS 200000  /* random-walk carving budget           */

/* ── Texture resolution & count ───────────────────────────────── */
#define TEX_W 256          /* wall texture width  (pixels)        */
#define TEX_H 256          /* wall texture height (pixels)        */
#define NUM_TEX 40         /* 4 colour × 2 style × 5 variations   */

/* ── Texture encoding constants ───────────────────────────────── */
#define NUM_COLOUR_TYPES   4  /* grey, blue, red, purple          */
#define NUM_TEXTURE_STYLES 2  /* 0=stone, 1=brick                 */
#define VARIANTS_PER_TYPE  5  /* 4 normal + 1 skull variation     */

/* ── Sprite dimensions ────────────────────────────────────────── */
#define SPRITE_W 32        /* small sprite width  (water/potion)  */
#define SPRITE_H 32        /* small sprite height (water/potion)  */
#define SKELE_W 96         /* skeleton sprite width               */
#define SKELE_H 128        /* skeleton sprite height              */
#define MAX_SPRITES 4096   /* maximum concurrent sprites          */

/* ── Sprite type constants ────────────────────────────────────── */
#define SPRITE_WATER        0  /* blue water puddle               */
#define SPRITE_POTION_BLUE  1  /* blue  potion (restores water)   */
#define SPRITE_POTION_RED   2  /* red   potion (restores health)  */
#define SPRITE_POTION_PINK  3  /* pink  potion (restores health)  */
#define SPRITE_POTION_GREEN 4  /* green potion (restores health)  */
#define SPRITE_WORM_BLUE    5
#define SPRITE_WORM_RED     6
#define SPRITE_WORM_PINK    7
#define SPRITE_WORM_GREEN   8
#define SPRITE_WORM_FIRST   SPRITE_WORM_BLUE
#define SPRITE_WORM_LAST    SPRITE_WORM_GREEN
#define SPRITE_ENEMY        9  /* generic enemy (type in level field) */
#define NUM_SPRITE_TYPES 10

/* ── Map-cell value constants (41-50) ─────────────────────────── */
/* These values live in the map[] array alongside wall codes 1-40.
   1-40 = wall texture index (zone*VARIANTS_PER_TYPE + 1 + var)   */
#define MAP_WATER       41  /* floor cell containing a water puddle  */
#define MAP_POTION      42  /* floor cell containing a potion        */
#define MAP_ENEMY_SKELE 43  /* floor cell containing a skeleton      */
#define MAP_ENEMY_ORC   44  /* floor cell containing an orc          */
#define MAP_ENEMY_CULTIST 45 /* floor cell containing a cultist      */
#define MAP_ENEMY_BOSS  46  /* floor cell containing the necromancer */
#define MAP_IS_ENEMY(v) ((v) >= MAP_ENEMY_SKELE && (v) <= MAP_ENEMY_BOSS)
#define MAP_EXIT     47  /* (unused; exit uses skull wall 1-40)   */

/* ── Player type ──────────────────────────────────────────────── */
/* Position, direction vector, and camera plane for the DDA
   raycasting algorithm.  The camera plane is perpendicular to
   dir_x/dir_y and its length determines the FOV (~76° here).     */
typedef struct {
    double x, y;          /* world position (cells)               */
    double dir_x, dir_y;  /* unit direction vector                */
    double plane_x, plane_y; /* camera-plane vector (perp to dir) */
} Player;

/* ── Sprite type ──────────────────────────────────────────────── */
/* Position, vertical offset (0 = on floor, 0.5 = at eye level),
   type index into the sprite texture tables, and enemy variant.   */
typedef struct {
    double x, y;          /* world position (cells)               */
    double z;             /* 0 = on floor, 0.5 = eye level        */
    int type;             /* SPRITE_* constant                    */
    int level;            /* enemy level (0=boss, 1-6), 0=non-enemy */
} Sprite;

/* ── Macros for map access ────────────────────────────────────── */
#define set_map(x, y, v) map[(y) * MAP_W + (x)] = (v)
#define get_map(x, y) map[(y) * MAP_W + (x)]

/* ── Global state (extern, defined in engine.c) ───────────────── */
extern volatile int running;       /* becomes 0 on SIGINT or Quit */
extern unsigned char *map;         /* the MAP_W×MAP_H world grid  */
extern Player player;
extern int xterm_pair[256];        /* maps xterm256 index → ncurses colour pair */
extern int ceil_pair, floor_pair;  /* ceiling/floor colour pairs  */
extern int panel_pair;             /* HUD panel colour pair       */
extern int flash_pair;             /* red full-screen flash pair  */
extern int green_pair, blue_pair, grey_pair;  /* solid-colour overlay pairs  */
extern int health_pair, water_pair;/* green/blue bar colour pairs */
extern Sprite sprites[MAX_SPRITES];
extern int num_sprites;
extern int sprite_tex[NUM_SPRITE_TYPES][SPRITE_H][SPRITE_W];

/* ── Trig lookup tables (filled by init_luts()) ───────────────── */
#define MAX_ANIM_FRAMES 30         /* enough for original or reduced */
extern double cos_lut[MAX_ANIM_FRAMES + 1];
extern double sin_lut[MAX_ANIM_FRAMES + 1];

/* ── Engine API ───────────────────────────────────────────────── */
void init_tex(void);               /* generate all wall + sprite textures */
void init_luts(void);              /* generate trig + other lookup tables */
void init_player(void);            /* place player at centre of map       */
void create_lab(int num);          /* carve maze + scatter sprites        */
void init_ncurses(void);           /* setup terminal, colours, input      */
void cleanup(void);                /* restore terminal                   */
void handle_signal(int sig);       /* SIGINT → running = 0               */
void render_frame(void);           /* DDA raycast + sprite drawing       */
int  can_move_to(double nx, double ny); /* collision check with margin   */
int  rgb_to_xterm(unsigned char r, unsigned char g, unsigned char b); /* colour quantisation */

#endif
