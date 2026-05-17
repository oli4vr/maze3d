/* gentex.c — Procedural texture and sprite generation
 *
 * Generates stone/brick wall textures (256×256) using block-composition
 * with randomised shapes, rounded corners, bevel highlights, and drop
 * shadows.  Also generates 32×32 sprites (water, potions, worms) from
 * embedded alpha-masked templates.
 *
 * All textures are tileable by design.  Brick textures use a
 * deterministic hash for seam bricks so the pattern is consistent
 * across all 5 variations of the same colour type.                   */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gentex.h"
#include "potion.h"
#include "worm.h"

/* ── Stone texture constants ───────────────────────────────────── */
#define SEGS 8           /* grid segments per axis (8×8 = 64 blocks max) */
#define SPS 32           /* pixels per segment (256/8 = 32)             */

/* ── Colour type ───────────────────────────────────────────────── */
typedef struct { unsigned char r, g, b; } Color;

/* Available block shapes: each entry is {width, height} in segments */
static int shapes[][2] = {
    {1,2},{2,2},{2,1},{3,2},{2,3},{3,3},{3,1},{1,3}
};
#define NSHAPES 8

/* shuffle: Fisher-Yates shuffle for randomising shape selection order. */
static void shuffle(int *arr, int n) {
    for (int i = n-1; i > 0; i--) {
        int j = rand() % (i+1);
        int t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }
}

/* ── Block descriptor ──────────────────────────────────────────── */
/* Describes a single stone/brick block placed on the 8×8 segment grid.
 * Each block has randomised rounding, corner cuts, shadow/highlight
 * sizes, and a palette index.  Large blocks (≥3×3) may get a 1×1
 * cell cutout (like a missing brick) for visual variety.            */
typedef struct {
    int gx, gy, gw, gh;     /* grid position and size (in segments)    */
    int ss, hs;              /* shadow size, highlight size (pixels)   */
    int gap;                 /* mortar gap (pixels)                    */
    int stone_idx;           /* base palette index for the stone       */
    int round[4];            /* corner rounding radii (0 = not rounded)*/
    int cut[4];              /* corner cut depths (0 = no cut)         */
    int cell_cut;            /* corner index (0-3) for 1×1 cell cut on blocks ≥3×3, -1=none */
} Block;

/* ── gen_texture: Stone wall texture ──────────────────────────────── */

/* gen_texture: Procedural 256×256 stone wall texture.
 *
 * Algorithm:
 *   1. Build a 256-entry colour palette based on the colour type.
 *      Index 0 = black (unused), 1 = mortar, 2-255 = stone shades.
 *   2. Subdivide the 8×8 segment grid into randomly-shaped blocks
 *      (1×1 to 3×3 segments) using shuffled shape ordering.
 *   3. Fill the image with mortar colour (pal[1]).
 *   4. Render each block: apply noise, corner rounding, corner cuts,
 *      cell cutouts, right/bottom shadows, and top/left highlights.
 *   5. Quantise each pixel to the nearest palette entry (palette
 *      indexing ensures the final image uses few unique colours).
 *   6. Apply external drop shadows into the mortar gaps.
 *   7. Ensure tileability by slightly tweaking the last column.       */
void gen_texture(unsigned char *img, int ct) {
    /* ── Build palette ─────────────────────────────────────────── */
#define PAL_SIZE 256
    Color pal[PAL_SIZE];
    pal[0] = (Color){0,0,0};

    /* Mortar colour: dark grey, slightly randomised, 25% darker than stone base */
    int md = (int)((10 + rand() % 30) * 0.75f);
    pal[1] = (Color){md, md, md};

    /* Stone shades: 254 levels from dark (35) to lighter (115) with per-level noise */
    for (int i = 0; i < 254; i++) {
        float t = i / 253.0f;
        int v = 35 + (int)(t * 80);
        int n = (rand() % 5) - 2;
        switch (ct) {
        case 0: { /* grey */
            int gv = v + n;
            if (gv < 0) gv = 0;
            if (gv > 255) gv = 255;
            pal[2 + i] = (Color){gv, gv, gv};
            break;
        }
        case 1: { /* blue */
            int bv = v + n;
            if (bv < 0) bv = 0;
            if (bv > 255) bv = 255;
            pal[2 + i] = (Color){0, 0, bv};
            break;
        }
        case 2: { /* red */
            int rv = v + n;
            if (rv < 0) rv = 0;
            if (rv > 255) rv = 255;
            pal[2 + i] = (Color){rv, 0, 0};
            break;
        }
        case 3: { /* purple/pink — red-dominant with half blue */
            int rv = v + n;
            if (rv < 0) rv = 0;
            if (rv > 255) rv = 255;
            int bv = (int)(rv * 0.5f);
            if (bv < 0) bv = 0;
            if (bv > 255) bv = 255;
            pal[2 + i] = (Color){rv, 0, bv};
            break;
        }

        }
    }

    /* ── Block placement ───────────────────────────────────────── */
    int grid[SEGS][SEGS] = {0};
    Block blocks[SEGS * SEGS];
    int nb = 0;

    /* Greedy placement: try all 8 shapes in random order; if none
       fits, fall back to a 1×1 block at the current position.       */
    for (int y = 0; y < SEGS; y++) {
        for (int x = 0; x < SEGS; x++) {
            if (grid[y][x]) continue;
            int ord[NSHAPES];
            for (int i = 0; i < NSHAPES; i++) ord[i] = i;
            shuffle(ord, NSHAPES);
            int placed = 0;
            for (int si = 0; si < NSHAPES; si++) {
                int bw = shapes[ord[si]][0], bh = shapes[ord[si]][1];
                int ok = 1;
                for (int dy = 0; dy < bh && ok; dy++)
                    for (int dx = 0; dx < bw; dx++) {
                        int cx = (x + dx) % SEGS, cy = y + dy;
                        if (cy >= SEGS || grid[cy][cx]) { ok = 0; break; }
                    }
                if (!ok) continue;
                Block *b = &blocks[nb++];
                b->gx = x; b->gy = y; b->gw = bw; b->gh = bh;
                b->ss = 6 + rand() % 4;  /* shadow size: 6-9 px */
                b->hs = 4 + rand() % 3;  /* highlight size: 4-6 px */
                b->gap = 3 + rand() % 2; /* mortar gap: 3-4 px */
                b->stone_idx = 2 + rand() % 253;
                b->cell_cut = (b->gw >= 3 && b->gh >= 3 && rand() % 100 < 50) ? (rand() % 4) : -1;
                for (int i = 0; i < 4; i++) {
                    b->round[i] = rand() % 6;  /* corner rounding radius */
                    b->cut[i] = (rand() % 4 == 0) ? (2 + rand() % 3) : 0;
                }
                for (int dy = 0; dy < bh; dy++)
                    for (int dx = 0; dx < bw; dx++)
                        grid[y+dy][(x+dx)%SEGS] = nb;
                placed = 1; break;
            }
            /* Fallback: 1×1 block */
            if (!placed) {
                Block *b = &blocks[nb++];
                b->gx = x; b->gy = y; b->gw = 1; b->gh = 1;
                b->ss = 6 + rand() % 4;
                b->hs = 4 + rand() % 3;
                b->gap = 3 + rand() % 2;
                b->stone_idx = 2 + rand() % 253;
                b->cell_cut = -1;
                for (int i = 0; i < 4; i++) {
                    b->round[i] = rand() % 6;
                    b->cut[i] = (rand() % 4 == 0) ? (2 + rand() % 3) : 0;
                }
                grid[y][x] = nb;
            }
        }
    }

    /* ── Stage 1: background fill ──────────────────────────────── */
    /* Fill the entire image with the mortar colour. */
    for (int i = 0; i < GEN_W * GEN_H * 3; i += 3) {
        img[i] = pal[1].r; img[i+1] = pal[1].g; img[i+2] = pal[1].b;
    }

    /* ── Stage 2: render all blocks ────────────────────────────── */
    for (int bi = 0; bi < nb; bi++) {
        Block *b = &blocks[bi];
        int ss = b->ss, hs = b->hs, gap = b->gap;
        int bx0 = b->gx * SPS + gap, by0 = b->gy * SPS + gap;
        int bw = b->gw * SPS - gap * 2, bh = b->gh * SPS - gap * 2;
        if (bw < 1 || bh < 1) continue;

        /* Per-block pixel noise (for surface variation) */
        int *noise = (int *)malloc(bw * bh * sizeof(int));
        for (int i = 0; i < bw * bh; i++)
            noise[i] = (rand() % 5) - 2;

        for (int dy = 0; dy < bh; dy++) {
            for (int dx = 0; dx < bw; dx++) {
                int px = (bx0 + dx) % GEN_W, py = by0 + dy;

                int d_left = dx, d_right = bw - 1 - dx;
                int d_top = dy, d_bot = bh - 1 - dy;

                /* Corner rounding: skip pixels inside rounded corners */
                int rnd = 0;
                if (b->round[0] > 0 && sqrtf((float)(dx*dx + dy*dy)) < b->round[0] + 1) rnd = 1;
                if (b->round[1] > 0 && sqrtf((float)((bw-1-dx)*(bw-1-dx) + dy*dy)) < b->round[1] + 1) rnd = 1;
                if (b->round[2] > 0 && sqrtf((float)(dx*dx + (bh-1-dy)*(bh-1-dy))) < b->round[2] + 1) rnd = 1;
                if (b->round[3] > 0 && sqrtf((float)((bw-1-dx)*(bw-1-dx) + (bh-1-dy)*(bh-1-dy))) < b->round[3] + 1) rnd = 1;

                /* Corner cuts: triangular remove at corners */
                int cut = 0;
                if (b->cut[0] > 0 && dx + dy < b->cut[0]) cut = 1;
                if (b->cut[1] > 0 && (bw-1-dx) + dy < b->cut[1]) cut = 1;
                if (b->cut[2] > 0 && dx + (bh-1-dy) < b->cut[2]) cut = 1;
                if (b->cut[3] > 0 && (bw-1-dx) + (bh-1-dy) < b->cut[3]) cut = 1;

                /* 1×1 cell cutout (missing brick area) */
                int cell_cut = 0;
                if (b->cell_cut >= 0) {
                    if (b->cell_cut == 0 && dx < SPS && dy < SPS) cell_cut = 1;
                    else if (b->cell_cut == 1 && (bw-1-dx) < SPS && dy < SPS) cell_cut = 1;
                    else if (b->cell_cut == 2 && dx < SPS && (bh-1-dy) < SPS) cell_cut = 1;
                    else if (b->cell_cut == 3 && (bw-1-dx) < SPS && (bh-1-dy) < SPS) cell_cut = 1;
                }

                /* Skip pixel if any cutout/rounding applies (mortar shows through) */
                if (rnd || cut || cell_cut) continue;

                /* Base stone colour with noise */
                int nv = noise[dy * bw + dx];
                int idx = b->stone_idx + nv;
                if (idx < 2) idx = 2;
                if (idx > 255) idx = 255;
                Color col = pal[idx];

                /* Right edge shadow (darkens the right side) */
                if (d_right < ss) {
                    float f = 0.45f - 0.38f * (1.0f - (float)d_right / ss);
                    if (f < 0.08f) f = 0.08f;
                    col.r = (unsigned char)(col.r * f);
                    col.g = (unsigned char)(col.g * f);
                    col.b = (unsigned char)(col.b * f);
                }
                /* Bottom edge shadow */
                if (d_bot < ss) {
                    float f = 0.45f - 0.38f * (1.0f - (float)d_bot / ss);
                    if (f < 0.08f) f = 0.08f;
                    col.r = (unsigned char)(col.r * f);
                    col.g = (unsigned char)(col.g * f);
                    col.b = (unsigned char)(col.b * f);
                }

                /* Top edge highlight (brightens the top) */
                if (d_top < hs) {
                    float f = 0.28f * (1.0f - (float)d_top / hs);
                    col.r = (unsigned char)(col.r + (255 - col.r) * f);
                    col.g = (unsigned char)(col.g + (255 - col.g) * f);
                    col.b = (unsigned char)(col.b + (255 - col.b) * f);
                }
                /* Left edge highlight */
                if (d_left < hs) {
                    float f = 0.20f * (1.0f - (float)d_left / hs);
                    col.r = (unsigned char)(col.r + (255 - col.r) * f);
                    col.g = (unsigned char)(col.g + (255 - col.g) * f);
                    col.b = (unsigned char)(col.b + (255 - col.b) * f);
                }

                /* ── Nearest-colour palette quantisation ───────── */
                /* Find the closest palette entry to the final colour. */
                int best = 0, best_d = 999999;
                for (int pi = 0; pi < PAL_SIZE; pi++) {
                    int dr = (int)col.r - (int)pal[pi].r;
                    int dg = (int)col.g - (int)pal[pi].g;
                    int db = (int)col.b - (int)pal[pi].b;
                    int d2 = dr*dr + dg*dg + db*db;
                    if (d2 < best_d) { best_d = d2; best = pi; }
                }
                int off = (py * GEN_W + px) * 3;
                img[off] = pal[best].r;
                img[off+1] = pal[best].g;
                img[off+2] = pal[best].b;
            }
        }
        free(noise);
    }

    /* ── Stage 3: external drop shadows ────────────────────────── */
    /* Cast shadows from block edges into the mortar gap and into
       neighbouring blocks (the gap neighbours will be drawn on top
       later, but this creates nice deep shadow lines).               */
    for (int bi = 0; bi < nb; bi++) {
        Block *b = &blocks[bi];
        int ss = b->ss, gap = b->gap;
        int bx0 = b->gx * SPS + gap, by0 = b->gy * SPS + gap;
        int bw = b->gw * SPS - gap * 2, bh = b->gh * SPS - gap * 2;
        if (bw < 1 || bh < 1) continue;

        /* Right-side shadow from brick edge */
        int sx = (bx0 + bw) % GEN_W;
        for (int dy = 0; dy < bh && by0 + dy < GEN_H; dy++) {
            for (int ddx = 0; ddx < ss + gap; ddx++) {
                int px = (sx + ddx) % GEN_W;
                int py = by0 + dy;
                int off = (py * GEN_W + px) * 3;
                if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                    float str = 0.85f - 0.25f * (float)ddx / (ss + gap);
                    if (str < 0.3f) str = 0.3f;
                    if (str > 0.6f) {
                        img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                    } else {
                        img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                        img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                        img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                    }
                }
            }
        }

        /* Bottom-side shadow from brick edge */
        int sy = by0 + bh;
        for (int dx = 0; dx < bw && sy < GEN_H; dx++) {
            for (int ddy = 0; ddy < ss + gap; ddy++) {
                int px = (bx0 + dx) % GEN_W;
                int py = sy + ddy;
                int off = (py * GEN_W + px) * 3;
                if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                    float str = 0.85f - 0.25f * (float)ddy / (ss + gap);
                    if (str < 0.3f) str = 0.3f;
                    if (str > 0.6f) {
                        img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                    } else {
                        img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                        img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                        img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                    }
                }
            }
        }

        /* Bottom-right corner shadow (full black overlap) */
        for (int ddy = 0; ddy < ss + gap && sy + ddy < GEN_H; ddy++) {
            for (int ddx = 0; ddx < ss + gap; ddx++) {
                int px = (sx + ddx) % GEN_W;
                int py = sy + ddy;
                int off = (py * GEN_W + px) * 3;
                if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                    img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                }
            }
        }

        /* 1×1 cell cutout shadows: interior edges cast shadows into the cutout mortar */
        if (b->cell_cut >= 0) {
            int cut_x = bx0 + bw - SPS;
            int cut_y = by0 + bh - SPS;

            if (b->cell_cut == 1 || b->cell_cut == 3) {
                /* Rightward shadow from cutout's left edge (TR or BR corner) */
                for (int dy = 0; dy < SPS && by0 + dy < GEN_H; dy++) {
                    for (int ddx = 0; ddx < ss + gap; ddx++) {
                        int px = (cut_x + ddx) % GEN_W;
                        int py = by0 + dy;
                        int off = (py * GEN_W + px) * 3;
                        if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                            float str = 0.85f - 0.25f * (float)ddx / (ss + gap);
                            if (str < 0.3f) str = 0.3f;
                            if (str > 0.6f) {
                                img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                            } else {
                                img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                                img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                                img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                            }
                        }
                    }
                }
            }

            if (b->cell_cut == 2 || b->cell_cut == 3) {
                /* Downward shadow from cutout's top edge (BL or BR corner) */
                for (int dx = 0; dx < SPS; dx++) {
                    int bx = (bx0 + dx) % GEN_W;
                    for (int ddy = 0; ddy < ss + gap && cut_y + ddy < GEN_H; ddy++) {
                        int py = cut_y + ddy;
                        int off = (py * GEN_W + bx) * 3;
                        if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                            float str = 0.85f - 0.25f * (float)ddy / (ss + gap);
                            if (str < 0.3f) str = 0.3f;
                            if (str > 0.6f) {
                                img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                            } else {
                                img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                                img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                                img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                            }
                        }
                    }
                }
            }

            if (b->cell_cut == 3) {
                /* BR corner shadow overlap (darkest) */
                for (int ddy = 0; ddy < ss + gap && cut_y + ddy < GEN_H; ddy++) {
                    for (int ddx = 0; ddx < ss + gap; ddx++) {
                        int px = (cut_x + ddx) % GEN_W;
                        int py = cut_y + ddy;
                        int off = (py * GEN_W + px) * 3;
                        if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                            img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                        }
                    }
                }
            }
        }
    }

    /* ── Tileability fix ───────────────────────────────────────── */
    /* If the last column matches column 0, tweak it slightly so the
       texture doesn't have a visible seam when tiled.                */
    int last = GEN_W - 1;
    for (int y = 0; y < GEN_H; y++) {
        int off0 = (y * GEN_W + 0) * 3;
        int offL = (y * GEN_W + last) * 3;
        if (img[off0] == img[offL] && img[off0+1] == img[offL+1] && img[off0+2] == img[offL+2]) {
            int c = rand() % 3;
            int v = img[offL + c];
            img[offL + c] = (v > 128) ? v - 1 : v + 1;
        }
    }
}

/* ── Brick texture helpers ─────────────────────────────────────────── */

/* brick_seed: Deterministic hash function for brick seam properties.
 * Returns a pseudo-random integer derived from (colour_type, id)
 * so that all 5 variations of the same colour type share the same
 * seam brick pattern (ensuring the skull-variant exit wall blends in). */
static int brick_seed(int ct, int id) {
    unsigned int h = (unsigned int)(ct * 1234567 + id * 78901);
    h = (h * 1103515245 + 12345) & 0x7fffffff;
    return (int)h;
}

/* ── gen_brick_texture: Brick wall texture ─────────────────────────── */

/* gen_brick_texture: Procedural 256×256 brick wall texture.
 *
 * Algorithm:
 *   1. Same palette setup as gen_texture.
 *   2. Each 256px row is 8 segments (32px each).  Even rows have
 *      bricks at segment columns (0,1),(2,3),(4,5),(6,7).  Odd rows
 *      have bricks offset by 1: (1,2),(3,4),(5,6),(7,0) for the
 *      classic running-bond pattern.
 *   3. Each brick has randomised stone index, shadow/highlight sizes,
 *      and corner rounding.  Seam bricks (at the 7-0 wrap) use the
 *      deterministic brick_seed() for consistency.
 *   4. Anomalies: occasionally a brick is widened (3 or 4 segments)
 *      or a "1×2" brick extends into the next row (vertical anomaly).
 *   5. Drop shadows are applied in a second pass using the same
 *      segment layout (with matching anomalies).                      */
void gen_brick_texture(unsigned char *img, int ct) {
#define PAL_SIZE 256
    Color pal[PAL_SIZE];
    pal[0] = (Color){0,0,0};

    /* Palette: same scheme as stone textures */
    int md = (int)((10 + rand() % 30) * 0.75f);
    pal[1] = (Color){md, md, md};

    for (int i = 0; i < 254; i++) {
        float t = i / 253.0f;
        int v = 35 + (int)(t * 80);
        int n = (rand() % 5) - 2;
        switch (ct) {
        case 0: {
            int gv = v + n;
            if (gv < 0) gv = 0;
            if (gv > 255) gv = 255;
            pal[2 + i] = (Color){gv, gv, gv};
            break;
        }
        case 1: {
            int bv = v + n;
            if (bv < 0) bv = 0;
            if (bv > 255) bv = 255;
            pal[2 + i] = (Color){0, 0, bv};
            break;
        }
        case 2: {
            int rv = v + n;
            if (rv < 0) rv = 0;
            if (rv > 255) rv = 255;
            pal[2 + i] = (Color){rv, 0, 0};
            break;
        }
        case 3: {
            int rv = v + n;
            if (rv < 0) rv = 0;
            if (rv > 255) rv = 255;
            int bv = (int)(rv * 0.5f);
            if (bv < 0) bv = 0;
            if (bv > 255) bv = 255;
            pal[2 + i] = (Color){rv, 0, bv};
            break;
        }
        }
    }

    /* Mortar gap: deterministic from colour type for seam consistency */
    int gap = 3 + brick_seed(ct, 999) % 2;

    /* ── Background fill with mortar ───────────────────────────── */
    for (int i = 0; i < GEN_W * GEN_H * 3; i += 3) {
        img[i] = pal[1].r; img[i+1] = pal[1].g; img[i+2] = pal[1].b;
    }

#define BSEGS 8
#define BSPS 32

    /* ── Brick property tables ─────────────────────────────────── */
    /* Pre-compute properties for all 32 bricks (4 per row × 8 rows).
       Seam bricks (column index 3, wrapping from col 7→0) use the
       deterministic brick_seed() so the pattern is consistent.       */
    int brick_stone_idx[8][4];
    int brick_round[8][4][4];
    int brick_ss[8][4], brick_hs[8][4];

    for (int row = 0; row < BSEGS; row++) {
        for (int bc = 0; bc < 4; bc++) {
            int is_seam = (bc == 3);

            if (is_seam) {
                int s = brick_seed(ct, row * 4 + bc);
                brick_stone_idx[row][bc] = 2 + s % 253;
                if (s % 5 == 0) {
                    brick_stone_idx[row][bc] -= 2;
                    if (brick_stone_idx[row][bc] < 2) brick_stone_idx[row][bc] = 2;
                }
                brick_ss[row][bc] = 6 + brick_seed(ct, row * 4 + bc + 100) % 4;
                brick_hs[row][bc] = 4 + brick_seed(ct, row * 4 + bc + 200) % 3;
                for (int k = 0; k < 4; k++) {
                    int r = brick_seed(ct, row * 4 + bc + 300 + k * 100);
                    brick_round[row][bc][k] = (r % 3 == 0) ? (2 + r % 4) : 0;
                }
            } else {
                brick_stone_idx[row][bc] = 2 + rand() % 253;
                if (rand() % 5 == 0) {
                    brick_stone_idx[row][bc] -= 2;
                    if (brick_stone_idx[row][bc] < 2) brick_stone_idx[row][bc] = 2;
                }
                brick_ss[row][bc] = 6 + rand() % 4;
                brick_hs[row][bc] = 4 + rand() % 3;
                for (int k = 0; k < 4; k++)
                    brick_round[row][bc][k] = (rand() % 3 == 0) ? (2 + rand() % 4) : 0;
            }
        }
    }

    /* ── Render all bricks with anomalies ──────────────────────── */
    int vert_col = -1;  /* column taken by a 1×2 anomaly from row above */

    for (int row = 0; row < BSEGS; row++) {
        /* ── Build segment list for this row ────────────────────── */
        int s_col[6], s_w[6], s_bc[6], s_h[6];
        int nseg = 0;

        if (row % 2 == 0) {
            /* Even row: bricks at (0,1),(2,3),(4,5),(6,7) */
            for (int bc = 0; bc < 4; bc++) {
                int col = bc * 2;
                if (vert_col >= 0 && col == vert_col - 1) {
                    s_col[nseg] = col; s_w[nseg] = 1; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else {
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                }
            }
        } else {
            /* Odd row: bricks at (1,2),(3,4),(5,6),(7,0) */
            for (int bc = 0; bc < 4; bc++) {
                int col = (bc * 2 + 1) % BSEGS;
                int col2 = (col + 1) % BSEGS;
                if (bc == 3) {
                    /* Wrapping brick (cols 7,0) — always full 2-width */
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else if (vert_col == col2) {
                    s_col[nseg] = col; s_w[nseg] = 1; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else if (vert_col == col) {
                    s_col[nseg] = col + 1; s_w[nseg] = 1; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else {
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                }
            }
        }

        /* ── Apply anomaly (random brick pattern variation) ─────── */
        if (vert_col < 0 && row < 7 && nseg >= 4 && rand() % 4 == 0) {
            int pos = 1 + rand() % (nseg - 2);
            int type = rand() % 3;

            if (type == 0 && pos + 1 < nseg) {
                /* 3+1 split: widen a brick to 3 segments, neighbour shrinks to 1 */
                s_w[pos] = 3; s_w[pos + 1] = 1;
                for (int k = pos + 2; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
            } else if (type == 1 && pos + 1 < nseg) {
                /* Merge: two bricks become one 4-segment brick */
                s_w[pos] = 4;
                for (int k = pos + 1; k < nseg - 1; k++) {
                    s_col[k] = s_col[k + 1]; s_w[k] = s_w[k + 1];
                    s_bc[k] = s_bc[k + 1]; s_h[k] = s_h[k + 1];
                }
                nseg--;
                for (int k = pos + 1; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
            } else if (type == 2 && row % 2 == 0 && s_col[pos] >= 2 && s_col[pos] <= 4) {
                /* 1×2 vertical anomaly: brick extends into the next row */
                int left = s_col[pos];
                s_w[pos] = 1; s_h[pos] = 2;
                for (int k = nseg; k > pos + 1; k--) {
                    s_col[k] = s_col[k - 1]; s_w[k] = s_w[k - 1];
                    s_bc[k] = s_bc[k - 1]; s_h[k] = s_h[k - 1];
                }
                s_col[pos + 1] = left + 1; s_w[pos + 1] = 1;
                s_bc[pos + 1] = s_bc[pos]; s_h[pos + 1] = 1;
                nseg++;
                for (int k = pos + 2; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
                vert_col = left;
            }
        }

        /* ── Render each segment ───────────────────────────────── */
        for (int s = 0; s < nseg; s++) {
            int gx = s_col[s], gw = s_w[s], bc = s_bc[s], gh = s_h[s];
            int bx0 = gx * BSPS + gap, by0 = row * BSPS + gap;
            int bw = gw * BSPS - gap * 2, bh = gh * BSPS - gap * 2;
            if (bw < 1 || bh < 1) continue;

            int is_seam = (bc == 3);

            /* Noise: deterministic for seam bricks */
            int *noise = malloc(bw * bh * sizeof(int));
            if (is_seam) {
                for (int i = 0; i < bw * bh; i++) {
                    unsigned int h = brick_seed(ct, (row * 4 + bc) * 10000 + i);
                    noise[i] = (int)(h % 5) - 2;
                }
            } else {
                for (int i = 0; i < bw * bh; i++)
                    noise[i] = (rand() % 5) - 2;
            }

            int stone_idx = brick_stone_idx[row][bc];
            int bss = brick_ss[row][bc], bhs = brick_hs[row][bc];

            for (int dy = 0; dy < bh; dy++) {
                for (int dx = 0; dx < bw; dx++) {
                    int px = (bx0 + dx) % GEN_W, py = by0 + dy;

                    int d_left = dx, d_right = bw - 1 - dx;
                    int d_top = dy, d_bot = bh - 1 - dy;

                    /* Corner rounding */
                    int rnd = 0;
                    if (brick_round[row][bc][0] > 0 && sqrtf((float)(dx*dx + dy*dy)) < brick_round[row][bc][0] + 1) rnd = 1;
                    if (brick_round[row][bc][1] > 0 && sqrtf((float)(d_right*d_right + dy*dy)) < brick_round[row][bc][1] + 1) rnd = 1;
                    if (brick_round[row][bc][2] > 0 && sqrtf((float)(dx*dx + d_bot*d_bot)) < brick_round[row][bc][2] + 1) rnd = 1;
                    if (brick_round[row][bc][3] > 0 && sqrtf((float)(d_right*d_right + d_bot*d_bot)) < brick_round[row][bc][3] + 1) rnd = 1;
                    if (rnd) continue;

                    int nv = noise[dy * bw + dx];
                    int idx = stone_idx + nv;
                    if (idx < 2) idx = 2;
                    if (idx > 255) idx = 255;
                    Color col = pal[idx];

                    /* Right/bottom shadows, top/left highlights (same as stone) */
                    if (d_right < bss) {
                        float f = 0.45f - 0.38f * (1.0f - (float)d_right / bss);
                        if (f < 0.08f) f = 0.08f;
                        col.r = (unsigned char)(col.r * f);
                        col.g = (unsigned char)(col.g * f);
                        col.b = (unsigned char)(col.b * f);
                    }
                    if (d_bot < bss) {
                        float f = 0.45f - 0.38f * (1.0f - (float)d_bot / bss);
                        if (f < 0.08f) f = 0.08f;
                        col.r = (unsigned char)(col.r * f);
                        col.g = (unsigned char)(col.g * f);
                        col.b = (unsigned char)(col.b * f);
                    }
                    if (d_top < bhs) {
                        float f = 0.28f * (1.0f - (float)d_top / bhs);
                        col.r = (unsigned char)(col.r + (255 - col.r) * f);
                        col.g = (unsigned char)(col.g + (255 - col.g) * f);
                        col.b = (unsigned char)(col.b + (255 - col.b) * f);
                    }
                    if (d_left < bhs) {
                        float f = 0.20f * (1.0f - (float)d_left / bhs);
                        col.r = (unsigned char)(col.r + (255 - col.r) * f);
                        col.g = (unsigned char)(col.g + (255 - col.g) * f);
                        col.b = (unsigned char)(col.b + (255 - col.b) * f);
                    }

                    /* Nearest-colour palette quantisation */
                    int best = 0, best_d = 999999;
                    for (int pi = 0; pi < PAL_SIZE; pi++) {
                        int dr = (int)col.r - (int)pal[pi].r;
                        int dg = (int)col.g - (int)pal[pi].g;
                        int db = (int)col.b - (int)pal[pi].b;
                        int d2 = dr*dr + dg*dg + db*db;
                        if (d2 < best_d) { best_d = d2; best = pi; }
                    }
                    int off = (py * GEN_W + px) * 3;
                    img[off] = pal[best].r;
                    img[off+1] = pal[best].g;
                    img[off+2] = pal[best].b;
                }
            }
            free(noise);
        }

        /* Clear vert_col after the row below the 1×2 has been processed */
        if (vert_col >= 0 && row % 2 == 1) vert_col = -1;
    }

    /* ── Drop shadows (second pass, same segment layout) ───────── */
    vert_col = -1;

    for (int row = 0; row < BSEGS; row++) {
        /* Rebuild segments for shadows (matching the render layout) */
        int s_col[6], s_w[6], s_bc[6], s_h[6];
        int nseg = 0;

        if (row % 2 == 0) {
            for (int bc = 0; bc < 4; bc++) {
                int col = bc * 2;
                if (vert_col >= 0 && col == vert_col - 1) {
                    s_col[nseg] = col; s_w[nseg] = 1; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else {
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                }
            }
        } else {
            for (int bc = 0; bc < 4; bc++) {
                int col = (bc * 2 + 1) % BSEGS;
                if (bc == 3) {
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else if (vert_col >= 0 && (vert_col == col || vert_col == (col + 1) % BSEGS)) {
                    s_col[nseg] = col; s_w[nseg] = 1; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                } else {
                    s_col[nseg] = col; s_w[nseg] = 2; s_bc[nseg] = bc; s_h[nseg] = 1; nseg++;
                }
            }
        }

        /* Re-apply anomalies */
        if (vert_col < 0 && row < 7 && nseg >= 4 && rand() % 4 == 0) {
            int pos = 1 + rand() % (nseg - 2);
            int type = rand() % 3;
            if (type == 0 && pos + 1 < nseg) {
                s_w[pos] = 3; s_w[pos + 1] = 1;
                for (int k = pos + 2; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
            } else if (type == 1 && pos + 1 < nseg) {
                s_w[pos] = 4;
                for (int k = pos + 1; k < nseg - 1; k++) {
                    s_col[k] = s_col[k + 1]; s_w[k] = s_w[k + 1];
                    s_bc[k] = s_bc[k + 1]; s_h[k] = s_h[k + 1];
                }
                nseg--;
                for (int k = pos + 1; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
            } else if (type == 2 && row % 2 == 0 && s_col[pos] >= 2 && s_col[pos] <= 4) {
                int left = s_col[pos];
                s_w[pos] = 1; s_h[pos] = 2;
                for (int k = nseg; k > pos + 1; k--) {
                    s_col[k] = s_col[k - 1]; s_w[k] = s_w[k - 1];
                    s_bc[k] = s_bc[k - 1]; s_h[k] = s_h[k - 1];
                }
                s_col[pos + 1] = left + 1; s_w[pos + 1] = 1;
                s_bc[pos + 1] = s_bc[pos]; s_h[pos + 1] = 1;
                nseg++;
                for (int k = pos + 2; k < nseg; k++)
                    s_col[k] = s_col[k - 1] + s_w[k - 1];
                vert_col = left;
            }
        }

        /* Shadows for each segment */
        for (int s = 0; s < nseg; s++) {
            int gx = s_col[s], gw = s_w[s], bc = s_bc[s], gh = s_h[s];
            int bx0 = gx * BSPS + gap, by0 = row * BSPS + gap;
            int bw = gw * BSPS - gap * 2, bh = gh * BSPS - gap * 2;
            if (bw < 1 || bh < 1) continue;

            int bss = brick_ss[row][bc];

            /* Right shadow */
            int sx = (bx0 + bw) % GEN_W;
            for (int dy = 0; dy < bh && by0 + dy < GEN_H; dy++) {
                for (int ddx = 0; ddx < bss + gap; ddx++) {
                    int px = (sx + ddx) % GEN_W;
                    int py = by0 + dy;
                    int off = (py * GEN_W + px) * 3;
                    if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                        float str = 0.85f - 0.25f * (float)ddx / (bss + gap);
                        if (str < 0.3f) str = 0.3f;
                        if (str > 0.6f) {
                            img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                        } else {
                            img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                            img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                            img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                        }
                    }
                }
            }

            /* Bottom shadow */
            int sy = by0 + bh;
            for (int dx = 0; dx < bw && sy < GEN_H; dx++) {
                for (int ddy = 0; ddy < bss + gap; ddy++) {
                    int px = (bx0 + dx) % GEN_W;
                    int py = sy + ddy;
                    int off = (py * GEN_W + px) * 3;
                    if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                        float str = 0.85f - 0.25f * (float)ddy / (bss + gap);
                        if (str < 0.3f) str = 0.3f;
                        if (str > 0.6f) {
                            img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                        } else {
                            img[off]   = (unsigned char)(pal[1].r * (1.0f - str));
                            img[off+1] = (unsigned char)(pal[1].g * (1.0f - str));
                            img[off+2] = (unsigned char)(pal[1].b * (1.0f - str));
                        }
                    }
                }
            }

            /* BR corner shadow */
            for (int ddy = 0; ddy < bss + gap && sy + ddy < GEN_H; ddy++) {
                for (int ddx = 0; ddx < bss + gap; ddx++) {
                    int px = (sx + ddx) % GEN_W;
                    int py = sy + ddy;
                    int off = (py * GEN_W + px) * 3;
                    if (img[off] == pal[1].r && img[off+1] == pal[1].g && img[off+2] == pal[1].b) {
                        img[off] = 0; img[off+1] = 0; img[off+2] = 0;
                    }
                }
            }
        }

        if (vert_col >= 0 && row % 2 == 1) vert_col = -1;
    }
}

/* ── Sprite generation ───────────────────────────────────────────── */

/* gen_water_sprite: Blue elliptical water puddle with a dark shadow
 * offset to the top-left.  Transparent background = (0,0,0).
 * The shadow is drawn first, then the water ellipse on top.         */
void gen_water_sprite(unsigned char *img) {
    memset(img, 0, SPRITE_W * SPRITE_H * 3);

    int cx = SPRITE_W / 2;
    int ry = 2;
    int cy = SPRITE_H - 1 - ry;
    int rx = 10;

    /* Shadow: offset left/up from water ellipse */
    int shx = 2, shy = 2;
    for (int y = 0; y < SPRITE_H; y++)
        for (int x = 0; x < SPRITE_W; x++) {
            int sx = x - shx, sy = y - shy;
            float sdx = (float)(sx - cx) / rx;
            float sdy = (float)(sy - cy) / ry;
            if (sdx * sdx + sdy * sdy <= 1.0f) {
                int idx = (y * SPRITE_W + x) * 3;
                img[idx] = 8; img[idx+1] = 8; img[idx+2] = 8;
            }
        }

    /* Water puddle: darker blue ellipse with slight bright centre */
    for (int y = 0; y < SPRITE_H; y++)
        for (int x = 0; x < SPRITE_W; x++) {
            float dx = (float)(x - cx) / rx;
            float dy = (float)(y - cy) / ry;
            float d2 = dx * dx + dy * dy;
            if (d2 > 1.0f) continue;

            int idx = (y * SPRITE_W + x) * 3;
            float bright = 0.4f + 0.4f * (1.0f - d2);
            int r = (int)(12 * bright);
            int g = (int)(28 * bright);
            int b = (int)(64 * bright);
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            img[idx] = r;
            img[idx+1] = g;
            img[idx+2] = b;
        }
 }

 /* gen_potion_texture: Render a potion bottle sprite from the
  * embedded POTION_W × POTION_H alpha-masked template (potion.h).
  * The template is centred and placed at the bottom of the 32×32 grid.
  * colour_type: 0=blue, 1=red, 2=pink, 3=green.                    */
 void gen_potion_texture(unsigned char *img, int ct) {
    memset(img, 0, SPRITE_W * SPRITE_H * 3);
    int ox = (SPRITE_W - POTION_W) / 2;
    int oy = SPRITE_H - POTION_H;
    for (int py = 0; py < POTION_H; py++)
        for (int px = 0; px < POTION_W; px++) {
            int si = py * POTION_W + px;
            if (!potion_opaqueness[si]) continue;
            int v = potion_colour[si];
            int dx = ox + px, dy = oy + py;
            int idx = (dy * SPRITE_W + dx) * 3;
            switch (ct) {
            case 0: /* blue */
                img[idx] = 0; img[idx+1] = 0; img[idx+2] = v;
                break;
            case 1: /* red */
                img[idx] = v; img[idx+1] = 0; img[idx+2] = 0;
                break;
            case 2: /* pink */
                img[idx] = v; img[idx+1] = 0; img[idx+2] = (unsigned char)(v * 0.6f);
                break;
            case 3: /* green */
                img[idx] = 0; img[idx+1] = v; img[idx+2] = 0;
                break;
            }
        }
}

/* gen_worm_texture: Render a worm sprite from the embedded
 * WORM_W × WORM_H alpha-masked template (worm.h).
 * Same layout and colour_type convention as gen_potion_texture.     */
void gen_worm_texture(unsigned char *img, int ct) {
    memset(img, 0, SPRITE_W * SPRITE_H * 3);
    int ox = (SPRITE_W - WORM_W) / 2;
    int oy = SPRITE_H - WORM_H;
    for (int py = 0; py < WORM_H; py++)
        for (int px = 0; px < WORM_W; px++) {
            int si = py * WORM_W + px;
            if (!worm_opaqueness[si]) continue;
            int v = worm_colour[si];
            int dx = ox + px, dy = oy + py;
            int idx = (dy * SPRITE_W + dx) * 3;
            switch (ct) {
            case 0: img[idx] = v; img[idx+1] = v; img[idx+2] = v; break;
            case 1: img[idx] = 0; img[idx+1] = 0; img[idx+2] = v; break;
            case 2: img[idx] = v; img[idx+1] = 0; img[idx+2] = 0; break;
            case 3: img[idx] = v; img[idx+1] = 0; img[idx+2] = (unsigned char)(v * 0.6f); break;
            }
        }
}
