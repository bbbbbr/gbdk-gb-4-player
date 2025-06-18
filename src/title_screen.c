#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"

#include "4_player_adapter.h"

#include "title_bg.h"
#include "checkbox.h"
#include "you_arrow_spr.h"

#define BG_TITLE_BG_TILES_START      0u
#define BG_CHECKBOX_TILES_START      (BG_TITLE_BG_TILES_START + title_bg_TILE_COUNT)
  #define BG_CHECKBOX_TILE_UNCHECKED (BG_CHECKBOX_TILES_START + 0)
  #define BG_CHECKBOX_TILE_CHECKED   (BG_CHECKBOX_TILES_START + 1)

#define SPR_YOU_ARROW_SPR_TILES_START  0u


#define CHECKBOX_ROW 12u
const uint8_t checkbox_col[] = {3u, 7u, 12u, 16u};

#define YOU_SPR_Y  (118u + DEVICE_SPRITE_PX_OFFSET_Y)
const uint8_t you_spr_x[] = {
    24u  + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    56u  + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    90u  + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    124u + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X
};


static void title_screen_init(void) {

    // Load map and tiles
    set_bkg_data(BG_TITLE_BG_TILES_START, title_bg_TILE_COUNT, title_bg_tiles);
    set_bkg_tiles(0,0, (title_bg_WIDTH / title_bg_TILE_W),
                       (title_bg_HEIGHT / title_bg_TILE_H), title_bg_map);

    set_bkg_data((uint8_t)BG_CHECKBOX_TILES_START, checkbox_TILE_COUNT, checkbox_tiles);

    set_sprite_data(SPR_YOU_ARROW_SPR_TILES_START, you_arrow_spr_TILE_COUNT, you_arrow_spr_tiles);
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
}


static void update_connection_display(void) {

    // Update per-player connection check-boxes
    uint8_t idx = 0;
    uint8_t player_id_bit = _4P_PLAYER_1;

    while (idx < 4u) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {
            // Checked
            set_bkg_tile_xy(checkbox_col[idx], CHECKBOX_ROW, (uint8_t)BG_CHECKBOX_TILE_CHECKED);
        } else {
            // Unchecked
            set_bkg_tile_xy(checkbox_col[idx], CHECKBOX_ROW, (uint8_t)BG_CHECKBOX_TILE_UNCHECKED);
        }
        player_id_bit <<= 1;
        idx++;
    }

    // Update your player icon
    uint8_t next_sprite_id = 0u;
    uint8_t my_player_num = WHICH_PLAYER_AM_I();

    if ((my_player_num >= PLAYER_NUM_MIN) && 
        (my_player_num <= PLAYER_NUM_MAX)) {
        // If valid player number, highlight which player the current one is
        next_sprite_id = move_metasprite(you_arrow_spr_metasprites[0], SPR_YOU_ARROW_SPR_TILES_START,
                                         next_sprite_id, you_spr_x[my_player_num - 1], YOU_SPR_Y);
    }
    hide_sprites_range(next_sprite_id, MAX_HARDWARE_SPRITES);
}


void title_screen_run(void){

    title_screen_init();
    UPDATE_KEYS();

    while (1) {
        vsync();
        update_connection_display();
    }
}
