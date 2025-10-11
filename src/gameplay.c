#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"
#include "4_player_adapter.h"

#include "gfx.h"
#include "snakes.h"
#include "gameplay.h"


uint8_t game_this_player_status;

bool    game_players_all_signaled_ready;
uint8_t game_players_ready_expected;
uint8_t game_players_ready_status;

static void gameplay_init(void) {

    // Takes at least 6 frames with screen on

    // Load map and tiles
    set_sprite_data(SNAKE_TILES_START, snake_tiles_TILE_COUNT, snake_tiles_tiles);
    set_sprite_data(BOARD_TILES_START, board_tiles_TILE_COUNT, board_tiles_tiles);

    set_sprite_data(BOARD_UI_TILES_START, board_ui_TILE_COUNT, board_ui_tiles);

    set_bkg_data((uint8_t)BG_FONT_NUMS_TILES_START, font_nums_TILE_COUNT, font_nums_tiles);
    set_bkg_data((uint8_t)BG_FONT_NUMS_NO_OUTLINE_TILES_START, font_nums_no_outline_TILE_COUNT, font_nums_no_outline_tiles);
    set_bkg_data((uint8_t)BG_FONT_ALPHA_TILES_START, font_alpha_TILE_COUNT, font_alpha_tiles);

    // Cover title screen info
    fill_bkg_rect(0u, 0u, DEVICE_SCREEN_WIDTH, DEVICE_SCREEN_HEIGHT, BLANK_TILE);
    set_bkg_based_tiles(BOARD_UI_X_START, BOARD_UI_Y_START, board_ui_WIDTH / board_ui_TILE_W, board_ui_HEIGHT / board_ui_TILE_H, board_ui_map, BOARD_UI_TILES_START);

    // Set up "Paused" sprites - they won't be displayed until later when needed (via hiding/showing ALL sprites)
    HIDE_SPRITES;
    hide_sprites_range(0, MAX_HARDWARE_SPRITES);    
    set_sprite_data(SPR_PAUSED_SPR_TILES_START, paused_spr_TILE_COUNT, paused_spr_tiles);    
    move_metasprite(paused_spr_metasprites[0], SPR_PAUSED_SPR_TILES_START,
                    0u, (uint8_t)PAUSED_SPR_X, (uint8_t)PAUSED_SPR_Y);

    snakes_init_and_draw();
    fade_in();

    GAMEPLAY_SET_THIS_PLAYER_STATUS(PLAYER_STATUS_INGAME);
    game_players_ready_status       = _4P_CONNECT_NONE;
    game_players_ready_expected     = _4P_CONNECT_NONE;
    game_players_all_signaled_ready = false;
}


#ifdef DEBUG_SHOW_CHECKSUM
    uint16_t sio_checksum;
#endif

static bool process_packets(void) {

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    // Read number of packets ready and cache it locally (it may change during processing)
    uint8_t packets_ready = four_player_rx_fifo_get_num_packets_ready();

    for (uint8_t packet = 0; packet < packets_ready; packet++) {

        #ifdef DEBUG_SHOW_CHECKSUM
            sio_checksum++;
        #endif

        // Process player input (direction, pause, console ready status, etc)
        snakes_process_packet_input();

        // Only tick the game once all player consoles have yet signaled readiness
        if (game_players_all_signaled_ready) {
            if (snakes_tick_game() == false) {
                #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
                    BGP_REG = ~BGP_REG;
                #endif
                return false;
            }
        }

        // Move to next packet RX Bytes                
        _4p_rx_fifo_packet_increment_read_ptr();
    }

    // free up the rx buf space now that it's done being used
    four_player_rx_fifo_remove_n_packets(packets_ready);

    #ifdef DISPLAY_USE_SIO_DATA_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    return true;
}


void gameplay_run(void){

    gameplay_init();

    // Save number of connected players for checking
    // whether all players have signaled readiness
    game_players_ready_expected = _4p_connect_status >> _4P_CONNECT_BITS_DOWNSHIFT;

    #ifdef DEBUG_SHOW_CHECKSUM
        sio_checksum = 0u;
    #endif

    // Wait for packet discarding to complete
    while (four_player_get_packet_discard_count() > 0u);
    // Send ready signal with player bit
    four_player_set_xfer_data(_SIO_CMD_READY | (PLAYER_1 << WHICH_PLAYER_AM_I_ZERO_BASED()) );

    while (1) {
        UPDATE_KEYS();
        wait_vsync_or_sio_4P_packet_data_ready();

        if (IS_PLAYER_DATA_AVAILABLE()) {
            if (process_packets() == false) {
                // Handle Game Over: back to main loop
                return;
            }
        }

        // Exit title screen once mode is switched to PING
        if (GET_CURRENT_MODE() == _4P_STATE_PING)
            return;

        // Load D-Pad TX data for next frame
        // Only send data when there is a discrete event
        uint8_t dpad_ticked    = GET_KEYS_TICKED(J_DPAD);
        uint8_t buttons_ticked = GET_KEYS_TICKED(J_BUTTONS) >> 4;
        if (dpad_ticked != 0u) {
            four_player_set_xfer_data(_SIO_CMD_DPAD | dpad_ticked);
        }
        else if (GET_KEYS_TICKED(J_START)) {
            four_player_set_xfer_data(_SIO_CMD_BUTTONS | buttons_ticked);
        }

        #ifdef DEBUG_B_BUTTON_TO_REQUEST_PING_MODE
            if (KEY_TICKED(J_B)) {
                four_player_request_change_to_ping_mode();
            }
        #endif
    }
}


#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY

    extern       uint8_t _4P_xfer_tx_data;
    extern const uint8_t * _4p_rx_fifo_end_wrap_addr;
    extern uint8_t * _4p_rx_fifo_WRITE_ptr;
    extern uint8_t * _4p_rx_fifo_READ_ptr;

    static void _4p_mock_init_xfer_buffers(void) {
        // Buffer and control: Reset read and write pointers to base of buffer, zero count
        _4p_rx_fifo_WRITE_ptr        = _4p_rx_fifo;
        _4p_rx_fifo_READ_ptr         = _4p_rx_fifo;
        _4p_rx_fifo_count            = 0u;
    }


    static void _4p_mock_packet(void) {

        if (_4p_rx_fifo_count != RX_FIFO_SZ) {

            *_4p_rx_fifo_WRITE_ptr++ = _4P_xfer_tx_data;
            *_4p_rx_fifo_WRITE_ptr++ = 0x00u;
            *_4p_rx_fifo_WRITE_ptr++ = 0x00u;
            *_4p_rx_fifo_WRITE_ptr++ = 0x00u;

            _4p_rx_fifo_count += RX_FIFO_PACKET_SZ;

            // Handle wraparound
            if (_4p_rx_fifo_WRITE_ptr == _4p_rx_fifo_end_wrap_addr)
                _4p_rx_fifo_WRITE_ptr = _4p_rx_fifo;
        }
    }


    void gameplay_run_local_only(void){

        // Debug, set only local player present
        // _4p_connect_status = _4P_PLAYER_1 | PLAYER_1;
        // _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_3) | PLAYER_1;
        _4p_connect_status = (_4P_PLAYER_1 | _4P_PLAYER_2 | _4P_PLAYER_3 | _4P_PLAYER_4) | PLAYER_1;

        gameplay_init();

        _4p_mock_init_xfer_buffers();
        game_players_all_signaled_ready = true;

        while (1) {
            UPDATE_KEYS();
            vsync();

            if (IS_PLAYER_DATA_AVAILABLE()) {
                if (process_packets() == false) {
                    // Handle Game Over: back to main loop
                    return;
                }
            }

            // Load D-Pad TX data for next frame
            // Only send data when there is a discrete event

            // Load D-Pad TX data for next frame
            // Only send data when there is a discrete event
            uint8_t dpad_ticked    = GET_KEYS_TICKED(J_DPAD);
            uint8_t buttons_ticked = GET_KEYS_TICKED(J_BUTTONS) >> 4;
            if (dpad_ticked != 0u) {
                four_player_set_xfer_data(_SIO_CMD_DPAD | dpad_ticked);
            }
            else if (GET_KEYS_TICKED(J_START)) {
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
