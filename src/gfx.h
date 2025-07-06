#ifndef _GFX_H
#define _GFX_H

#include <stdint.h>

#include "title_bg.h"
#include "checkbox.h"
#include "font_nums.h"

#include "you_arrow_spr.h"
#include "snake_tiles_spr.h"

#define BG_TITLE_BG_TILES_START      0u
#define BG_CHECKBOX_TILES_START      (BG_TITLE_BG_TILES_START + title_bg_TILE_COUNT)
  #define BG_CHECKBOX_TILE_UNCHECKED (BG_CHECKBOX_TILES_START + 0)
  #define BG_CHECKBOX_TILE_CHECKED   (BG_CHECKBOX_TILES_START + 1)
#define BG_FONT_NUMS_TILES_START     (BG_CHECKBOX_TILES_START + checkbox_TILE_COUNT)

#define BLANK_TILE (BG_TITLE_BG_TILES_START)


#define SPR_YOU_ARROW_SPR_TILES_START  0u
#define SPR_SNAKE_TILES_START         (SPR_YOU_ARROW_SPR_TILES_START + you_arrow_spr_TILE_COUNT)
  #define SPR_SNAKE_TILE_YOUR_PLYR_HEAD   (SPR_SNAKE_TILES_START + 0)
  #define SPR_SNAKE_TILE_OTHER_PLYR_HEAD  (SPR_SNAKE_TILES_START + 3)


#endif // _GFX_H


