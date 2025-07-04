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


#define GAME_SPEED_START         1u
#define PLAYER_X_HEAD_START_STEP (DEVICE_SCREEN_PX_WIDTH / (PLAYER_NUM_MAX + 1u))
#define PLAYER_Y_HEAD_START      (DEVICE_SCREEN_PX_HEIGHT / 2u)

#define PLAYER_DIR_NONE          (0u)
#define PLAYER_DIR_UP            (J_UP)
#define PLAYER_DIR_DOWN          (J_DOWN)
#define PLAYER_DIR_LEFT          (J_LEFT)
#define PLAYER_DIR_RIGHT         (J_RIGHT)

// Check for 8 bit tile alignment before turning in N direction
#define PLAYER_ALIGNED_DIR_CHANGE(pixel_coord) ((pixel_coord & 0x07u) == 0)

uint8_t game_tick;
uint8_t player_speed;
uint8_t player_dir[PLAYER_NUM_MAX];
uint8_t player_dir_next[PLAYER_NUM_MAX];

uint8_t player_x_head[PLAYER_NUM_MAX];
uint8_t player_y_head[PLAYER_NUM_MAX];

// uint8_t player_x_tail[PLAYER_NUM_MAX];
// uint8_t player_y_tail[PLAYER_NUM_MAX];


static void game_vars_reset(void) {

    game_tick    = 0;
    player_speed = GAME_SPEED_START;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {
        player_dir[c]      = PLAYER_DIR_UP;
        player_dir_next[c] = PLAYER_DIR_NONE;

        player_x_head[c] = PLAYER_X_HEAD_START_STEP * (c + 1);
        player_y_head[c] = PLAYER_Y_HEAD_START;
    }
}


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

    game_vars_reset();

    // Set up player sprites
    uint8_t my_player_num = WHICH_PLAYER_AM_I();
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {
            // Use different sprite tile for this player vs others
            uint8_t tile_id = (my_player_num == (c + 1)) ? SPR_SNAKE_TILE_YOUR_PLYR_HEAD : SPR_SNAKE_TILE_OTHER_PLYR_HEAD;
            set_sprite_tile(c, tile_id);

            move_sprite(c, player_x_head[c], player_y_head[c]);
        }
        else hide_sprite(c);

        player_id_bit <<= 1;
    }
    hide_sprites_range(PLAYER_NUM_MAX, MAX_HARDWARE_SPRITES);

    rx_packet_ignore_count = RX_BUF_INITIAL_PACKET_IGNORE_COUNT;
}


static void packet_process_player_data(void) {
    static uint8_t c;

    // One game tick per 4 Player data packet
    game_tick++;

    uint8_t player_id_bit = _4P_PLAYER_1;

    for (c = 0; c < _4P_XFER_RX_SZ; c++) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            // Check for player commands
            uint8_t value = _4p_rx_buf_READ_ptr[c];
            if ((value & _SIO_CMD_MASK) == _SIO_CMD_DPAD) {

                uint8_t dir_request = value & _SIO_DATA_MASK;
                if (dir_request) player_dir_next[c] = dir_request;
            }

            // TODO: could also cache player_dir[] -> dir
            // Cached copy to remove duplicate array lookup
            uint8_t dir_next = player_dir_next[c];

            // If tile aligned on X or Y then do check for direction change
            if (dir_next) {

// TODO: block change of dir to opposite of current dir
// dir_next != dir_reverse[ player_dir[c] ]
// const dir_reverse[] = {PLAYER_DIR_DOWN, 
// - Maybe dir from bits to 0-3? (could be X = even, Y = odd)

// TODO: Lock initial X/Y positions to tile alignment

                // Y changes must be applied on a X tile alignment
                if  PLAYER_ALIGNED_DIR_CHANGE(player_x_head[c]) {
                    // Apply dir change if requested and zero out request
                    if (dir_next & (PLAYER_DIR_UP | PLAYER_DIR_DOWN)) {
                        player_dir[c]      = dir_next;
                        player_dir_next[c] = PLAYER_DIR_NONE;
                    }
                }
                // X changes must be applied on a Y tile alignment
                if  PLAYER_ALIGNED_DIR_CHANGE(player_y_head[c]) {
                    // Apply dir change if requested and zero out request
                    if (dir_next & (PLAYER_DIR_LEFT | PLAYER_DIR_RIGHT)) {
                        player_dir[c]      = dir_next;
                        player_dir_next[c] = PLAYER_DIR_NONE;
                    }
                }
            }

            // Apply movement
            if ((game_tick & 0x03u) == 0u) {
                switch (player_dir[c]) {
                    case PLAYER_DIR_UP:    player_y_head[c] -= player_speed; break;
                    case PLAYER_DIR_DOWN:  player_y_head[c] += player_speed; break;
                    case PLAYER_DIR_LEFT:  player_x_head[c] -= player_speed; break;
                    case PLAYER_DIR_RIGHT: player_x_head[c] += player_speed; break;
                }
                move_sprite(c, player_x_head[c], player_y_head[c]);
            }
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
            packet_process_player_data();

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
