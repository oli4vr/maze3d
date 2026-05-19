#ifndef ENEMIES_H
#define ENEMIES_H

#include "engine.h"

/* Enemy type constants (37 types: 12 skel + 12 orc + 12 cult + 1 boss) */
enum {
    ENEMY_SKELETON_L1 = 0,
    ENEMY_SKELETON_L2,
    ENEMY_SKELETON_L3,
    ENEMY_SKELETON_L4,
    ENEMY_SKELETON_L5,
    ENEMY_SKELETON_L6,
    ENEMY_SKELETON_L7,
    ENEMY_SKELETON_L8,
    ENEMY_SKELETON_L9,
    ENEMY_SKELETON_L10,
    ENEMY_SKELETON_L11,
    ENEMY_SKELETON_L12,
    ENEMY_ORC_L1,
    ENEMY_ORC_L2,
    ENEMY_ORC_L3,
    ENEMY_ORC_L4,
    ENEMY_ORC_L5,
    ENEMY_ORC_L6,
    ENEMY_ORC_L7,
    ENEMY_ORC_L8,
    ENEMY_ORC_L9,
    ENEMY_ORC_L10,
    ENEMY_ORC_L11,
    ENEMY_ORC_L12,
    ENEMY_CULTIST_L1,
    ENEMY_CULTIST_L2,
    ENEMY_CULTIST_L3,
    ENEMY_CULTIST_L4,
    ENEMY_CULTIST_L5,
    ENEMY_CULTIST_L6,
    ENEMY_CULTIST_L7,
    ENEMY_CULTIST_L8,
    ENEMY_CULTIST_L9,
    ENEMY_CULTIST_L10,
    ENEMY_CULTIST_L11,
    ENEMY_CULTIST_L12,
    ENEMY_BOSS,
    NUM_ENEMY_TYPES
};

/* 19 base greyscale sprites (6 skel + 6 orc + 6 cult + 1 boss) */
#define NUM_ENEMY_SPRITES 19

enum {
    ENEMY_SPR_SKEL_L1,  ENEMY_SPR_SKEL_L2,  ENEMY_SPR_SKEL_L3,
    ENEMY_SPR_SKEL_L4,  ENEMY_SPR_SKEL_L5,  ENEMY_SPR_SKEL_L6,
    ENEMY_SPR_ORC_L1,   ENEMY_SPR_ORC_L2,   ENEMY_SPR_ORC_L3,
    ENEMY_SPR_ORC_L4,   ENEMY_SPR_ORC_L5,   ENEMY_SPR_ORC_L6,
    ENEMY_SPR_CULT_L1,  ENEMY_SPR_CULT_L2,  ENEMY_SPR_CULT_L3,
    ENEMY_SPR_CULT_L4,  ENEMY_SPR_CULT_L5,  ENEMY_SPR_CULT_L6,
    ENEMY_SPR_BOSS,
};

/* Enemy display info */
typedef struct {
    const char *name;
    int level;
} EnemyInfo;

/* Map enemy type → greyscale sprite index (0-18) */
extern const unsigned char enemy_sprite_idx[NUM_ENEMY_TYPES];

/* Map enemy type → color profile index (0-6) for tinting */
extern const unsigned char enemy_color_ramp_idx[NUM_ENEMY_TYPES];

/* 4 per-profile × 9 xterm-256 colors (index 0 unused, 1-8 = grey levels)
   All profiles use the grey ramp. */
extern int enemy_profile_colors[4][9];

extern const EnemyInfo enemy_info[NUM_ENEMY_TYPES];

#endif
