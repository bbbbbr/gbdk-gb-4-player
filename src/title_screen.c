#include <gbdk/platform.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <gbdk/console.h>

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
                                         next_sprite_id, you_spr_x[my_player_num - 1], (uint8_t)YOU_SPR_Y);
    }
    hide_sprites_range(next_sprite_id, MAX_HARDWARE_SPRITES);
}

// uint8_t local_player_data[_4P_XFER_RX_SZ];
// uint8_t local_player_data_alt[_4P_XFER_RX_SZ];

static void handle_player_data(void) {

    uint8_t c;
/*    // Copy data out of the buffer, THEN use it
    // TODO: this is double buffered so sio writes into the other, but probably needs a locking mechanism with a critical section to toggle it
    // (i.e. this end sets which buf is free and sio just writes to the other and doesn't control it - that might result in more latency though vs some kind of "ready" signal)
    // memcpy(local_player_data, _4p_xfer_rx_buf[GET_INACTIVE_RX_BUF()], _4P_XFER_RX_SZ);
    memcpy(local_player_data, _4p_xfer_rx_buf, _4P_XFER_RX_SZ);
    memcpy(local_player_data_alt, _4p_xfer_rx_buf_alt, _4P_XFER_RX_SZ);
*/
    // This also releases 
    four_player_claim_active_sio_buffer_for_main();

    gotoxy(1, 4);
    printf("clm=%hu\n", (uint8_t)_4p_xfer_rx_buf_sio_active);

    for (c = 0; c < _4P_XFER_RX_SZ; c++) {
        if ((c & 0x3) == 0)
            printf("\n");
        printf("%hx,", (uint8_t)_4p_xfer_rx_buf[0][c]);
    }

    printf("\n");
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {
        if ((c & 0x3) == 0)
            printf("\n");
        printf("%hx,", (uint8_t)_4p_xfer_rx_buf[1][c]);
    }

}


static void start_data_mode(void) {
    four_player_request_change_to_xfer_mode();
}


static void restart_ping_mode(void) {
    four_player_restart_ping_mode();
}


void title_screen_run(void){

    title_screen_init();

    while (1) {
        UPDATE_KEYS();
        vsync();

        if (GET_CURRENT_MODE() == _4P_STATE_PING) {
            update_connection_display();
        }
        else if (GET_CURRENT_MODE() == _4P_STATE_XFER) {
            // Load TX data for next frame first
            four_player_set_xfer_data(keys & J_DPAD);  // TODO: lightweight checksum or etc on transmitted and received data

            gotoxy(1, 0);
            printf("%hx,%hx", (uint8_t)sio_counter, (uint8_t)packet_counter);

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
            cls();
            restart_ping_mode();
        }
    }
}
