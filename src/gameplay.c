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

    snakes_reset_and_draw();
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);

    rx_packet_ignore_count = RX_BUF_INITIAL_PACKET_IGNORE_COUNT;
}


static bool process_packets(void) {

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    // Read number of packets ready and cache it locally (it may change during processing)
    uint8_t packets_ready = four_player_rx_buf_get_num_packets_ready();

    for (uint8_t packet = 0; packet < packets_ready; packet++) {

        // Skip initial packets if requested
        if (rx_packet_ignore_count)
            rx_packet_ignore_count--;
        else {
            if (snakes_process_packet_input_and_tick_game() == false) {
                #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
                    BGP_REG = ~BGP_REG;
                #endif
                return false;
            }
        }

        // Move to next packet RX Bytes                
        _4p_rx_buf_packet_increment_read_ptr();
    }

    // free up the rx buf space now that it's done being used
    four_player_rx_buf_remove_n_packets(packets_ready);

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    return true;
}


void gameplay_run(void){

    gameplay_init();

    while (1) {
        UPDATE_KEYS();
        wait_vsync_or_sio_4P_packet_data_ready();

        if (IS_PLAYER_DATA_READY()) {
            if (process_packets() == false) {
                // Handle Game Over
            }
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


#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY

    extern       uint8_t _4P_xfer_tx_data;
    extern const uint8_t * _4p_rx_buf_end_wrap_addr;
    extern uint8_t * _4p_rx_buf_WRITE_ptr;
    extern uint8_t * _4p_rx_buf_READ_ptr;

    void _4p_mock_init_xfer_buffers(void) {
        // Buffer and control: Reset read and write pointers to base of buffer, zero count
        _4p_rx_buf_WRITE_ptr        = _4p_rx_buf;
        _4p_rx_buf_READ_ptr         = _4p_rx_buf;
        _4p_rx_buf_count            = 0u;
    }


    void _4p_mock_packet(void) {

        if (_4p_rx_buf_count != RX_BUF_SZ) {

            *_4p_rx_buf_WRITE_ptr++ = _4P_xfer_tx_data;
            *_4p_rx_buf_WRITE_ptr++ = 0x00u;
            *_4p_rx_buf_WRITE_ptr++ = 0x00u;
            *_4p_rx_buf_WRITE_ptr++ = 0x00u;

            _4p_rx_buf_count += RX_BUF_PACKET_SZ;

            // Handle wraparound
            if (_4p_rx_buf_WRITE_ptr == _4p_rx_buf_end_wrap_addr)
                _4p_rx_buf_WRITE_ptr = _4p_rx_buf;
        }
    }


    void gameplay_run_local_only(void){

        // Debug, set only local player present
        // _4p_connect_status = _4P_PLAYER_1 | PLAYER_1;
        // _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_3) | PLAYER_1;
        _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_2 | _4P_PLAYER_3 | _4P_PLAYER_4) | PLAYER_1;

        gameplay_init();
        _4p_mock_init_xfer_buffers();

        while (1) {
            UPDATE_KEYS();
            vsync();

            if (IS_PLAYER_DATA_READY()) {
                if (process_packets() == false) {
                    // Handle Game Over
                }                
            }

            // Load D-Pad TX data for next frame
            // Only send data when there is a discrete event

            uint8_t dpad_ticked = GET_KEYS_TICKED(J_DPAD);
            if (dpad_ticked != 0u) {
                four_player_set_xfer_data(_SIO_CMD_DPAD | dpad_ticked);
            } 
            else {
                // clear tx data, this would normally be done in the serial ISR as it transmitted
                _4P_xfer_tx_data = 0x00;
            }
            // four_player_set_xfer_data(keys);

            // Mock a packet, one per frame
            _4p_mock_packet();
        }
    }
#endif
