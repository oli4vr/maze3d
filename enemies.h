#ifndef ENEMIES_H
#define ENEMIES_H

#include "engine.h"

/* Enemy type constants */
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

/* Enemy display info (used in maze3d.c for name display) */
typedef struct {
    const char *name;     /* e.g. "Skeleton Level 3", "Necromancer" */
    int level;            /* 0 = boss, 1-6 = level */
} EnemyInfo;

typedef struct {
    const char *name;
    int level;
    unsigned char (*rgb)[96 * 3];
    unsigned char (*alpha)[96];
} EnemySpriteData;

extern const EnemySpriteData enemy_sprite_data[NUM_ENEMY_TYPES];
extern const EnemyInfo enemy_info[NUM_ENEMY_TYPES];
extern int enemy_tex[NUM_ENEMY_TYPES][128][96];

#endif
