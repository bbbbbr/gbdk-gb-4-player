#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"
#include "4_player_adapter.h"

#include "gfx.h"


#define CHECKBOX_ROW 14u
// Checkbox positions for players 1 - 4
const uint8_t checkbox_col[] = {3u, 7u, 12u, 16u};

#define YOU_SPR_Y  (131u + DEVICE_SPRITE_PX_OFFSET_Y)
// Sprite arrow positions for players 1 - 4
const uint8_t you_spr_x[] = {
    (8u *  3u) + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    (8u *  7u) + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    (8u * 12u) + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
    (8u * 16u) + (uint8_t)DEVICE_SPRITE_PX_OFFSET_X,
};


void title_screen_init(void) {

    // Load map and tiles
    set_bkg_data(BG_TITLE_BG_TILES_START, title_bg_TILE_COUNT, title_bg_tiles);
    set_bkg_tiles(0,0, (title_bg_WIDTH / title_bg_TILE_W),
                       (title_bg_HEIGHT / title_bg_TILE_H), title_bg_map);

    set_bkg_data((uint8_t)BG_CHECKBOX_TILES_START, checkbox_TILE_COUNT, checkbox_tiles);


    set_sprite_data(SPR_YOU_ARROW_SPR_TILES_START, you_arrow_spr_TILE_COUNT, you_arrow_spr_tiles);

    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
    SHOW_SPRITES;
    fade_in();
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

    // Update "your player" icon
    uint8_t next_sprite_id = 0u;
    uint8_t my_player_num = WHICH_PLAYER_AM_I();

    if ((my_player_num >= PLAYER_NUM_MIN) &&
        (my_player_num <= PLAYER_NUM_MAX)) {
        // If valid player number, highlight which player the current one is
        next_sprite_id = move_metasprite(you_arrow_spr_metasprites[0], SPR_YOU_ARROW_SPR_TILES_START,
                                         next_sprite_id, you_spr_x[my_player_num - 1], (uint8_t)YOU_SPR_Y);
    }
    hide_sprites_range(next_sprite_id, MAX_HARDWARE_SPRITES);
}


void title_screen_run(void){

    title_screen_init();

    while (1) {
        UPDATE_KEYS();
        vsync_or_sio_4P_mode_change();

        // Exit title screen once mode is switched to XFER
        if (GET_CURRENT_MODE() == _4P_STATE_XFER)
            return;

        update_connection_display();

        if (KEY_TICKED(J_START)) {
            // Any player can start the game
            // if (WHICH_PLAYER_AM_I() == PLAYER_1) {
                four_player_request_change_to_xfer_mode();
            //}
        }
    }
}
