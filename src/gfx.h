#ifndef _GFX_H
#define _GFX_H

#include <stdint.h>

#include "title_bg.h"
#include "checkbox.h"
#include "font_nums.h"

#include "you_arrow_spr.h"

#include "snake_tiles.h"
#include "board_tiles.h"
#include "game_over.h"

#define BG_TITLE_BG_TILES_START      0u
#define BG_CHECKBOX_TILES_START      (BG_TITLE_BG_TILES_START + title_bg_TILE_COUNT)
  #define BG_CHECKBOX_TILE_UNCHECKED (BG_CHECKBOX_TILES_START + 0)
  #define BG_CHECKBOX_TILE_CHECKED   (BG_CHECKBOX_TILES_START + 1)
#define BG_FONT_NUMS_TILES_START     (BG_CHECKBOX_TILES_START + checkbox_TILE_COUNT)

#define BLANK_TILE (BG_TITLE_BG_TILES_START)


#define SPR_YOU_ARROW_SPR_TILES_START  0u


// Tile shared between sprites and BKG
#define SPR_BKG_SHARED_TILE_START     128u

#define TILES_PER_SNAKE ((snake_tiles_TILE_COUNT / 2u))

#define SNAKE_TILES_START             (SPR_BKG_SHARED_TILE_START)
  #define SNAKE_TILE_YOURS_HEAD        (SNAKE_TILES_START + 0u)       // Order is R,L,U,D
  #define SNAKE_TILE_YOURS_TAIL        (SNAKE_TILE_YOURS_HEAD + 4u)   // Order is R,L,U,D
  #define SNAKE_TILE_YOURS_BODY        (SNAKE_TILE_YOURS_HEAD + 8u)   // Order is R/L, U/D
  #define SNAKE_TILE_YOURS_TURN        (SNAKE_TILE_YOURS_HEAD + 10u)  // Order is R,L,U,D

  #define SNAKE_TILE_OTHERS_HEAD        (SNAKE_TILES_START + TILES_PER_SNAKE) // Order is R,L,U,D
  #define SNAKE_TILE_OTHERS_TAIL        (SNAKE_TILE_OTHERS_HEAD + 4u)   // Order is R,L,U,D
  #define SNAKE_TILE_OTHERS_BODY        (SNAKE_TILE_OTHERS_HEAD + 8u)   // Order is R/L, U/D
  #define SNAKE_TILE_OTHERS_TURN        (SNAKE_TILE_OTHERS_HEAD + 10u)  // Order is R,L,U,D

    #define TILE_RIGHT 0u
    #define TILE_LEFT  1u
    #define TILE_UP    2u
    #define TILE_DOWN  3u

    #define TILE_LEFT_RIGHT 0u
    #define TILE_UP_DOWN    1u

    #define TILE_OTHERS_OFFSET  (TILES_PER_SNAKE)

    // (moving from) Vertical movement mapping of turns
    #define TILE_DOWN_RIGHT 0u
    #define TILE_UP_RIGHT   1u
    #define TILE_DOWN_LEFT  2u
    #define TILE_UP_LEFT    3u

    // (moving from) Horizontal movement mapping of turns
    #define TILE_LEFT_UP    0u
    #define TILE_LEFT_DOWN  1u
    #define TILE_RIGHT_UP   2u
    #define TILE_RIGHT_DOWN 3u

#define BOARD_TILES_START       (SNAKE_TILES_START + (uint8_t)snake_tiles_TILE_COUNT)
    #define BOARD_TILE_HEART    (BOARD_TILES_START + 0u)

#define GAME_OVER_TILES_START       (BOARD_TILES_START + board_tiles_TILE_COUNT)

#endif // _GFX_H


