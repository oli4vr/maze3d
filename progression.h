#ifndef PROGRESSION_H
#define PROGRESSION_H

/* ── Player base stats ───────────────────────────────────────── */
#define PLAYER_BASE_HEALTH    50
#define PLAYER_BASE_WATER     50
#define PLAYER_BASE_ATTACK     3
#define PLAYER_BASE_DEFENSE    4
#define PLAYER_BASE_TOUGHNESS  0
#define PLAYER_BASE_ENDURANCE  0
#define PLAYER_BASE_STAMINA    0
#define PLAYER_BASE_LUCK       0

#define SOULS_UPGRADE_COST    3

/* ── Derived formulas ──────────────────────────────────────────── */
/* max_health = base_health + (10 * stamina)
   max_water  = base_water  + (10 * endurance)                   */

/* ── Combat formulas ───────────────────────────────────────────── */
/* tohit = attacker_attack + (rand() % 9) + clamp(luck_diff, -3, 3)
 *        >= defender_defense
 * damage = attacker_attack - (defender_toughness >> 1)  (min 1 on hit)
 * TGH is halved to prevent it from completely negating enemy attacks.
 * The range is ALWAYS 0-8 (9 values), luck gives ±1 per point
 * capped at ±3.  This prevents luck from shrinking the random
 * range and making enemies unhittable.                         */

/* ── Monster stat tables ─────────────────────────────────────── */
#define MSTAT(att,def,tgh,end,sta,lck,bhp,souls) \
    { att, def, tgh, end, sta, lck, bhp, souls }

/* All enemies have lck=0.  The old system let enemy luck grow
   to 30+, which shrank the player's rand() range to 1 and made
   high-tier enemies literally unhittable.  Luck is removed from
   all non-boss enemies entirely.                               */

/* ── Skeleton family (levels 1-12) ────────────────────────────── */
/* All-rounder, moderate stats.  ATK increased ~20% from original
   to keep them threatening against TGH-stacking players.  Most
   players should die before reaching level 40.                     */
#define SKEL_L1  MSTAT( 2, 6, 0, 0, 0, 0, 15,  3)
#define SKEL_L2  MSTAT( 6, 9, 1, 0, 0, 0, 22,  5)
#define SKEL_L3  MSTAT(10,12, 2, 0, 0, 0, 30,  6)
#define SKEL_L4  MSTAT(13,15, 3, 0, 0, 0, 38,  7)
#define SKEL_L5  MSTAT(17,19, 4, 0, 0, 0, 46,  8)
#define SKEL_L6  MSTAT(21,21, 5, 0, 0, 0, 55,  9)
#define SKEL_L7  MSTAT(25,25, 6, 0, 0, 0, 64, 10)
#define SKEL_L8  MSTAT(28,28, 8, 0, 0, 0, 74, 11)
#define SKEL_L9  MSTAT(32,32, 9, 0, 0, 0, 84, 13)
#define SKEL_L10 MSTAT(35,36,10, 0, 0, 0, 94, 14)
#define SKEL_L11 MSTAT(38,40,12, 0, 0, 0, 88, 16)
#define SKEL_L12 MSTAT(42,44,14, 0, 0, 0, 82, 18)

/* ── Orc family (levels 1-12) ─────────────────────────────────── */
/* Tougher than skeletons: higher ATK, DEF, and more HP.
   Introduced from level 5.  ATK increased ~20% to keep them
   dangerous even against TGH-stacking players.                    */
#define ORC_L1   MSTAT( 5, 7, 2, 0, 0, 0, 22,  5)
#define ORC_L2   MSTAT( 9,10, 3, 0, 0, 0, 30,  7)
#define ORC_L3   MSTAT(12,13, 4, 0, 0, 0, 38,  8)
#define ORC_L4   MSTAT(16,17, 5, 0, 0, 0, 46, 10)
#define ORC_L5   MSTAT(20,21, 6, 0, 0, 0, 56, 12)
#define ORC_L6   MSTAT(24,25, 7, 0, 0, 0, 66, 15)
#define ORC_L7   MSTAT(27,29, 9, 0, 0, 0, 78, 18)
#define ORC_L8   MSTAT(32,32,11, 0, 0, 0, 90, 20)
#define ORC_L9   MSTAT(34,35,13, 0, 0, 0,100, 22)
#define ORC_L10  MSTAT(37,38,17, 0, 0, 0,130, 24)
#define ORC_L11  MSTAT(41,41,20, 0, 0, 0,160, 27)
#define ORC_L12  MSTAT(46,45,25, 0, 0, 0,200, 30)

/* ── Cultist family (levels 1-12) ─────────────────────────────── */
/* Glass cannons: extreme ATK, paper DEF, low HP.
   All can use magic (10% on counter, 75% on death strike).
   Magic formula: atk or atk+atk/4 — always > regular atk-tgh.
   A race: kill them in 1-2 rounds or face devastating damage.
   Introduced from level 10.  ATK restored to original values
   (was reduced 20% after overzealous balance pass).              */
#define CULTIST_L1  MSTAT(24, 4, 0, 0, 0, 0, 22,  5)
#define CULTIST_L2  MSTAT(30, 7, 1, 0, 0, 0, 28,  8)
#define CULTIST_L3  MSTAT(36,10, 2, 0, 0, 0, 35, 10)
#define CULTIST_L4  MSTAT(42,13, 3, 0, 0, 0, 40, 12)
#define CULTIST_L5  MSTAT(48,17, 4, 0, 0, 0, 45, 14)
#define CULTIST_L6  MSTAT(54,20, 5, 0, 0, 0, 51, 16)
#define CULTIST_L7  MSTAT(58,24, 6, 0, 0, 0, 57, 19)
#define CULTIST_L8  MSTAT(62,27, 7, 0, 0, 0, 63, 21)
#define CULTIST_L9  MSTAT(66,28, 8, 0, 0, 0, 69, 23)
#define CULTIST_L10 MSTAT(70,31, 9, 0, 0, 0, 80, 25)
#define CULTIST_L11 MSTAT(74,34,10, 0, 0, 0, 96, 28)
#define CULTIST_L12 MSTAT(78,37,11, 0, 0, 0,110, 30)

/* ── Boss (Necromancer) ───────────────────────────────────────── */
/* Spawns at BOSS_LEVEL.  DEF=40 gives the player a realistic hit
   chance.  ATK=55 (restored from earlier reduction).  With level
   bonus + TGH halving, the player must be well-prepared. 500 HP
   is a slog that demands prep and luck.                           */
#define BOSS_STAT    MSTAT(55, 40, 18, 0, 0, 5, 600, 150)
#define BOSS_LEVEL   40
#define BOSS_BONUS   5000

/* ── Spawn formulas (unchanged) ───────────────────────────────── */
/* base_skeleton_type = 1 + (game_level * 11) / 39
   base_orc_type      = (game_level >= 5)
                          ? (1 + ((game_level - 5) * 11) / 34) : 0
   base_cultist_type  = (game_level >= 10)
                          ? (1 + ((game_level - 10) * 11) / 29) : 0
   Then clamp each to [0, 12].
   Only families with base_* > 0 are candidates.
   50/50 chance to spawn 1 tier below base (min 1)               */

/* ── Consumable item effects ───────────────────────────────────── */
#define POTION_HP_BASE      30
#define POTION_HP_RANDOM    20
#define POTION_WATER_RANDOM  5

#define WATER_BOTTLE_HP_RANDOM    5
#define WATER_BOTTLE_WATER_BASE  30
#define WATER_BOTTLE_WATER_RANDOM 20

/* ── Inventory limits ─────────────────────────────────────────── */
#define MAX_ITEM_STACK        9       /* base; increased by STA+END */
#define MAX_SOULS_STACK     999
/* Max potions/water bottles = base + (stamina + endurance) / 3,
   hard-capped at 30 for balance.  At L40 with STA≈35, END≈9:
   9 + 44/3 = 23.  A maxed STA/END build reaches ~29.             */
#define ITEM_MAX_STACK(sta, end)                             \
    ( (MAX_ITEM_STACK + ((sta) + (end)) / 3) < 30           \
      ? (MAX_ITEM_STACK + ((sta) + (end)) / 3) : 30 )

/* ── End turn costs (thirst) ──────────────────────────────────── */
#define TURN_WATER_COST       4
#define TURN_HP_DRAIN         2    /* when water == 0 */

/* ── Global enemy level bonus ──────────────────────────────────── */
/* Added to enemy ATK (both counter and death strike) per game
   level.  Increased to `lvl/3` so levels 30-40 become
   unforgivably difficult.  Old value `(lvl*2)/9` made late-game
   too gentle.  L1-2:0 L3-5:1 L6-8:2 L9-11:3 L12-14:4
   L15-17:5 L18-20:6 L21-23:7 L24-26:8 L27-29:9
   L30-32:10 L33-35:11 L36-38:12 L39-40:13                    */
#define ENEMY_ATK_LEVEL_BONUS(lvl)  ((lvl) / 3)

#endif
