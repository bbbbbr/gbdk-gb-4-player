#include <gbdk/platform.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"

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

#define RX_BUF_INITIAL_PACKET_IGNORE_COUNT   4   // Number of initial packets to ignore, works for TX size of 1. May need research for larger values
uint8_t rx_packet_ignore_count;

static void gameplay_init(void) {

    // // Load map and tiles
    // set_bkg_data(BG_TITLE_BG_TILES_START, title_bg_TILE_COUNT, title_bg_tiles);
    // set_bkg_tiles(0,0, (title_bg_WIDTH / title_bg_TILE_W),
    //                    (title_bg_HEIGHT / title_bg_TILE_H), title_bg_map);

    // set_bkg_data((uint8_t)BG_CHECKBOX_TILES_START, checkbox_TILE_COUNT, checkbox_tiles);
    // set_bkg_data((uint8_t)BG_FONT_NUMS_TILES_START, font_nums_TILE_COUNT, font_nums_tiles);

    // set_sprite_data(SPR_YOU_ARROW_SPR_TILES_START, you_arrow_spr_TILE_COUNT, you_arrow_spr_tiles);
    // set_sprite_data(SPR_SNAKE_TILES_START, snake_tiles_spr_TILE_COUNT, snake_tiles_spr_tiles);
    // hide_sprites_range(0, MAX_HARDWARE_SPRITES);

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

    rx_packet_ignore_count = RX_BUF_INITIAL_PACKET_IGNORE_COUNT;
}


#define DISPLAY_BUF_SZ (_4P_XFER_RX_SZ * 3)
uint8_t display_buf[DISPLAY_BUF_SZ];

static void display_player_data(void) {

    static uint8_t c;

    // Display hex readout for all players
    uint8_t * p_display_buf = display_buf;
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {

        // Print hex bytes separated by spaces
        uint8_t value = _4p_rx_buf_READ_ptr[c];
        *p_display_buf++ = (value >> 4) + BG_FONT_NUMS_TILES_START;
        *p_display_buf++ = (value & 0x0F) + BG_FONT_NUMS_TILES_START;
        *p_display_buf++ = BLANK_TILE;
    }
    set_bkg_tiles(0,0, DISPLAY_BUF_SZ, 1, display_buf);

    // Move a sprite for all players
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (c = 0; c < _4P_XFER_RX_SZ; c++) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            uint8_t value = _4p_rx_buf_READ_ptr[c];

            int8_t x_mv = 0;
            if      (value & J_LEFT)  x_mv = -1;
            else if (value & J_RIGHT) x_mv = 1;

            int8_t y_mv = 0;
            if      (value & J_UP)    y_mv = -1;
            else if (value & J_DOWN)  y_mv = 1;

            scroll_sprite(c, x_mv, y_mv);            
        }
        player_id_bit <<= 1;
    }
}


static void handle_player_data(void) {

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    // Read number of packets ready and cache it locally (it may change during processing)
    uint8_t packets_ready = four_player_rx_buf_get_num_packets_ready();

    for (uint8_t packet = 0; packet < packets_ready; packet++) {

        // Skip initial packets if requested
        if (rx_packet_ignore_count)
            rx_packet_ignore_count--;
        else
            display_player_data();

        // Move to next packet RX Bytes                
        _4p_rx_buf_packet_increment_read_ptr();
    }

    // free up the rx buf space now that it's done being used
    four_player_rx_buf_remove_n_packets(packets_ready);

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif
}


void gameplay_run(void){

    gameplay_init();

    while (1) {
        UPDATE_KEYS();
        vsync_or_sio_4P_packet_data_ready();

        if (IS_PLAYER_DATA_READY()) {
            handle_player_data();
        }

        // Exit title screen once mode is switched to PING
        if (GET_CURRENT_MODE() == _4P_STATE_PING)
            return;

        // Load D-Pad TX data for next frame
        // Only send data when there is a discrete event
        uint8_t dpad_ticked = GET_KEYS_TICKED(J_DPAD);
        if (dpad_ticked != 0u) {
            four_player_set_xfer_data(_SIO_CMD_DPAD | dpad_ticked);
        }


        if (KEY_TICKED(J_B)) {
            four_player_request_change_to_ping_mode();
        }
    }
}
