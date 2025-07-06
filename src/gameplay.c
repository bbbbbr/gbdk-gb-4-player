#include <gbdk/platform.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"
#include "4_player_adapter.h"

#include "gfx.h"
#include "snakes.h"


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

    snakes_reset();
    snakes_redraw_all();

    rx_packet_ignore_count = RX_BUF_INITIAL_PACKET_IGNORE_COUNT;
}


static void process_packets(void) {

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
            snakes_process_packet_input_and_tick_game();

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
            process_packets();
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
