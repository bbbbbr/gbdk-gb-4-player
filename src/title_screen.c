#include <gbdk/platform.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "input.h"
#include "fade.h"

#include "4_player_adapter.h"

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
    set_bkg_data((uint8_t)BG_FONT_NUMS_TILES_START, font_nums_TILE_COUNT, font_nums_tiles);

    set_sprite_data(SPR_YOU_ARROW_SPR_TILES_START, you_arrow_spr_TILE_COUNT, you_arrow_spr_tiles);
    set_sprite_data(SPR_SNAKE_TILES_START, snake_tiles_spr_TILE_COUNT, snake_tiles_spr_tiles);
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
                                         next_sprite_id, you_spr_x[my_player_num - 1], (uint8_t)YOU_SPR_Y);
    }
    hide_sprites_range(next_sprite_id, MAX_HARDWARE_SPRITES);
}


#define DISPLAY_BUF_SZ (_4P_XFER_RX_SZ * 3)
uint8_t display_buf[DISPLAY_BUF_SZ];

static void handle_player_data(void) {

    four_player_claim_active_sio_buffer_for_main();

    static uint8_t c;

    // Display hex readout for all players
    uint8_t * p_display_buf = display_buf;
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {

        // Print hex bytes separated by spaces
        uint8_t value = _4p_xfer_rx_buf[0][c];
        *p_display_buf++ = (value >> 4) + BG_FONT_NUMS_TILES_START;
        *p_display_buf++ = (value & 0x0F) + BG_FONT_NUMS_TILES_START;
        *p_display_buf++ = BLANK_TILE;
    }
    set_bkg_tiles(0,0, DISPLAY_BUF_SZ, 1, display_buf);

    // Move a sprite for all players
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (c = 0; c < _4P_XFER_RX_SZ; c++) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            uint8_t value = _4p_xfer_rx_buf[0][c];

            int8_t x_mv = 0;
            if      (value & J_LEFT)  x_mv = -1;
            else if (value & J_RIGHT) x_mv = 1;

            int8_t y_mv = 0;
            if      (value & J_UP)   y_mv = -1;
            else if (value & J_DOWN) y_mv = 1;

            scroll_sprite(c, x_mv, y_mv);
        }
        player_id_bit <<= 1;
    }

}


// Temp for now, ultimately in separate file
static void handle_game_screen_init(void) {

    // Cover title screen info
    fill_bkg_rect(0,0,DEVICE_SCREEN_WIDTH, DEVICE_SCREEN_HEIGHT, BLANK_TILE);

    // Set up player sprites
    uint8_t my_player_num = WHICH_PLAYER_AM_I();
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {
            // Use different sprite tile for this player vs others
            uint8_t tile_id = (my_player_num == (c + 1)) ? SPR_SNAKE_TILE_YOUR_PLYR_HEAD : SPR_SNAKE_TILE_OTHER_PLYR_HEAD;
            set_sprite_tile(c, tile_id);

            uint8_t xinit = (c + 1) * (DEVICE_SCREEN_PX_WIDTH  / (PLAYER_NUM_MAX + 1));
            uint8_t yinit =     (DEVICE_SCREEN_PX_HEIGHT / 2u);
            move_sprite(c, xinit, yinit);
        }
        else hide_sprite(c);

        player_id_bit <<= 1;
    }
    hide_sprites_range(PLAYER_NUM_MAX, MAX_HARDWARE_SPRITES);
}


static void start_data_mode(void) {
    four_player_request_change_to_xfer_mode();
}


static void restart_ping_mode(void) {
    // TODO: do something with the result (bool) if it fails
    four_player_request_change_to_ping_mode();
}


void title_screen_run(void){

    static bool next_ping_mode_redraw = true;
    static bool next_xfer_mode_redraw = true;
    title_screen_init();

    while (1) {
        UPDATE_KEYS();
        vsync();

        if (GET_CURRENT_MODE() == _4P_STATE_PING) {
            if (next_ping_mode_redraw == true) {
                // Redraw the title screen
                title_screen_init();
                next_ping_mode_redraw = false;
                next_xfer_mode_redraw = true;
            }
            update_connection_display();
        }
        else if (GET_CURRENT_MODE() == _4P_STATE_XFER) {
            if (next_xfer_mode_redraw == true) {
                handle_game_screen_init();
                next_ping_mode_redraw = true;
                next_xfer_mode_redraw = false;
            }

            // Load TX data for next frame first
            four_player_set_xfer_data(0x80u | (keys & J_DPAD));  // TODO: maybe lightweight checksum or etc on transmitted and received data
            // four_player_set_xfer_data(0x80u | WHICH_PLAYER_AM_I());

            if (IS_PLAYER_DATA_READY()) {
                handle_player_data();
            }
        }



        if (KEY_TICKED(J_START)) {
            if (WHICH_PLAYER_AM_I() == PLAYER_1) {
                start_data_mode();
            }
        }
        else if (KEY_TICKED(J_SELECT)) {
            // Try to change mode even if this console isn't Player 1
            // (allowed, but probably not recommended)
            start_data_mode();
        }
        else if (KEY_TICKED(J_B)) {
            // Try to restart Ping mode
            restart_ping_mode();
        }
    }
}
