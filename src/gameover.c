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


void gameover_init(void) {

    set_bkg_based_tiles( ((DEVICE_SCREEN_PX_WIDTH - game_over_WIDTH) / game_over_TILE_W) / 2u,
                         ((DEVICE_SCREEN_PX_HEIGHT - game_over_HEIGHT) / game_over_TILE_H) / 2u,
                         (game_over_WIDTH / game_over_TILE_W),
                         (game_over_HEIGHT / game_over_TILE_H),
                         game_over_map, (uint8_t)GAME_OVER_TILES_START);


    hide_sprites_range(0, MAX_HARDWARE_SPRITES);
}


static bool process_packet_check_button_press(void) {

    static uint8_t c;

    uint8_t player_id_bit = _4P_PLAYER_1;

    // Loop through all bytes in the packet.
    // Each byte in the packet represents input/commands
    // from one of the players (0..3)
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            // Check for player commands
            uint8_t value = _4p_rx_buf_READ_ptr[c];

            // D-Pad input type command
            if ((value & _SIO_CMD_MASK) == _SIO_CMD_BUTTONS) {

                // Save D-Pad button press for when the snake is next able to turn
                // Block snake turning back onto itself
                if (value & _SIO_DATA_MASK) return true;
            }
        }
    }

    return false;
}



static bool process_packets(void) {


    // Read number of packets ready and cache it locally (it may change during processing)
    uint8_t packets_ready = four_player_rx_buf_get_num_packets_ready();

    for (uint8_t packet = 0; packet < packets_ready; packet++) {

        // If a button was pressed then time to exit Game Over screen
        if (process_packet_check_button_press() == true)
            return false;

        // Move to next packet RX Bytes                
        _4p_rx_buf_packet_increment_read_ptr();
    }

    // free up the rx buf space now that it's done being used
    four_player_rx_buf_remove_n_packets(packets_ready);

    return true;
}


void gameover_run(void){

    gameover_init();

    while (1) {
        UPDATE_KEYS();
        wait_vsync_or_sio_4P_packet_data_ready();

        if (IS_PLAYER_DATA_READY()) {
            if (process_packets() == false) {
                // Handle exit Game Over screen
                four_player_request_change_to_ping_mode();
                return;
            }
        }

        // Load button ticked TX data for next frame
        // Only send data when there is a discrete event
        uint8_t buttons_ticked = (GET_KEYS_TICKED(J_START | J_A | J_B | J_SELECT)) >> 4;
        if (buttons_ticked != 0u) {
            four_player_set_xfer_data(_SIO_CMD_BUTTONS | buttons_ticked);
        }
    }
}


#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY

    extern       uint8_t _4P_xfer_tx_data;
    extern const uint8_t * _4p_rx_buf_end_wrap_addr;
    extern uint8_t * _4p_rx_buf_WRITE_ptr;
    extern uint8_t * _4p_rx_buf_READ_ptr;

    static void _4p_mock_init_xfer_buffers(void) {
        // Buffer and control: Reset read and write pointers to base of buffer, zero count
        _4p_rx_buf_WRITE_ptr        = _4p_rx_buf;
        _4p_rx_buf_READ_ptr         = _4p_rx_buf;
        _4p_rx_buf_count            = 0u;
    }


    static void _4p_mock_packet(void) {

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


    void gameover_run_local_only(void){

        // Debug, set only local player present
        // _4p_connect_status = _4P_PLAYER_1 | PLAYER_1;
        // _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_3) | PLAYER_1;
        _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_2 | _4P_PLAYER_3 | _4P_PLAYER_4) | PLAYER_1;

        gameover_init();
        _4p_mock_init_xfer_buffers();

        while (1) {
            UPDATE_KEYS();
            vsync();

            if (IS_PLAYER_DATA_READY()) {
                if (process_packets() == false) {
                    // Handle exit Game Over screen                    
                    return;
                }
            }

            // Load button ticked TX data for next frame
            // Only send data when there is a discrete event
            uint8_t buttons_ticked = (GET_KEYS_TICKED(J_START | J_A | J_B | J_SELECT)) >> 4;
            if (buttons_ticked != 0u) {
                four_player_set_xfer_data(_SIO_CMD_BUTTONS | buttons_ticked);
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
