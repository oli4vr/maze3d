/* gentex.h — Procedural texture and sprite generation API
 *
 * All textures are generated at runtime using pseudo-random noise,
 * block composition, and embedded sprite masks (potion.h, worm.h).
 * The generated images are returned as flat 24-bit RGB arrays.       */

#ifndef GENTEX_H
#define GENTEX_H

#define GEN_W 256          /* generated texture width  (pixels)     */
#define GEN_H 256          /* generated texture height (pixels)     */
#define SPRITE_W 32        /* sprite output width                  */
#define SPRITE_H 32        /* sprite output height                 */

/* gen_texture: Generate a 256×256 stone wall texture.
 * `img` must be GEN_W × GEN_H × 3 bytes.
 * `colour_type` selects the colour scheme:
 *   0=grey, 1=blue, 2=red, 3=purple.                                */
void gen_texture(unsigned char *img, int colour_type);

/* gen_brick_texture: Generate a 256×256 brick wall texture.
 * Same dimensions and colour_type convention as gen_texture.
 * Uses deterministic seeding for seam bricks to ensure tileability. */
void gen_brick_texture(unsigned char *img, int colour_type);

/* gen_water_sprite: Generate a 32×32 blue water-puddle sprite
 * with an offset drop shadow.  Transparent pixels are (0,0,0).       */
void gen_water_sprite(unsigned char *img);

/* gen_potion_texture: Generate a 32×32 potion sprite.
 * `colour_type`: 0=blue, 1=red, 2=pink, 3=green (see gentex.c).    */
void gen_potion_texture(unsigned char *img, int colour_type);

/* gen_worm_texture: Generate a 32×32 worm sprite.
 * Same colour_type convention as gen_potion_texture.                 */
void gen_worm_texture(unsigned char *img, int colour_type);

#endif
