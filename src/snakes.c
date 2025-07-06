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

uint8_t game_tick;
uint8_t game_tick_speed;
uint8_t player_dir[PLAYER_NUM_MAX];
uint8_t player_dir_next[PLAYER_NUM_MAX];

uint8_t player_x_head[PLAYER_NUM_MAX];
uint8_t player_y_head[PLAYER_NUM_MAX];

uint8_t player_x_tail[PLAYER_NUM_MAX];
uint8_t player_y_tail[PLAYER_NUM_MAX];

uint8_t game_board[BOARD_WIDTH][BOARD_HEIGHT];

// TODO: change for individual bits for directions so this and other things can be more compact
// Lookup to prevent snake from turning back on top of itself
const uint8_t dir_opposite[16u] = {
    PLAYER_DIR_NONE,
    PLAYER_DIR_LEFT,  // 0x01  J_RIGHT
    PLAYER_DIR_RIGHT, // 0x02  J_LEFT
    PLAYER_DIR_NONE,  // 0x03

    PLAYER_DIR_DOWN,  // 0x04  J_UP
    PLAYER_DIR_NONE,  // 0x05
    PLAYER_DIR_NONE,  // 0x06
    PLAYER_DIR_NONE,  // 0x07

    PLAYER_DIR_UP,    // 0x08  J_DOWN
};


static void player_head_increment(uint8_t move_dir, uint8_t player_id);
static void player_tail_increment(uint8_t move_dir, uint8_t player_id);



void snakes_reset(void) {

    game_tick       = 0;
    game_tick_speed = GAME_TICK_SPEED_START;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {
        player_dir[c]      = PLAYER_DIR_UP;
        player_dir_next[c] = PLAYER_DIR_NONE;

        player_x_head[c] = PLAYER_X_HEAD_START_STEP * (c + 1);
        player_y_head[c] = PLAYER_Y_HEAD_START;
    }
}


// void game_board_redraw_all(void) {
//
//     for (uint8_t y = 0; y < DEVICE_SCREEN_WIDTH; x++) {
//         for (uint8_t x = 0; x < DEVICE_SCREEN_WIDTH; x++) {                        
//         }
//     }
// }

void snakes_redraw_all(void) {
    // Set up player sprites
    uint8_t my_player_num = WHICH_PLAYER_AM_I();
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {
            // Use different sprite tile for this player vs others
            uint8_t tile_id = (my_player_num == (c + 1)) ? SPR_SNAKE_TILE_YOUR_PLYR_HEAD : SPR_SNAKE_TILE_OTHER_PLYR_HEAD;

            set_bkg_tile_xy(player_x_head[c] * TILE_WH, player_y_head[c], tile_id);
            // set_sprite_tile(c, tile_id);
            // move_sprite(c, player_x_head[c] * TILE_WH, player_y_head[c] * TILE_WH);
        }
        // else hide_sprite(c);

        player_id_bit <<= 1;
    }
    hide_sprites_range(PLAYER_NUM_MAX, MAX_HARDWARE_SPRITES);
}

/*
static uint8_t snake_head_increment(uint8_t move_dir, uint8_t player_id) {

    // Current head position
    uint8_t head_x = player_x_tail[player_id];
    uint8_t head_y = player_y_tail[player_id];

    // Store final move_dir in current position
    // (which will soon become "last position")
    uint8_t board_player_data = TAIL_BITS_TO_BOARD(next_dir) | (player_id & BOARD_PLAYER_BITS);
    game_board[head_x][head_y] = board_player_data;

    // Calc new X,Y for head
    int8_t new_x = head_x + dir_x[move_dir];
    int8_t new_y = head_y + dir_y[move_dir];

    // Handle screen edge wraparound
         if (new_x > BOARD_X_MAX) new_x = 0u;
    else if (new_x < 0u)          new_x = BOARD_X_MAX;

         if (new_y > BOARD_Y_MAX) new_y = 0u;
    else if (new_y < 0u)          new_y = BOARD_Y_MAX;

    // Check for collision, otherwise check for snake food or empty
    uint8_t board_data_new = game_board[new_x][new_y];

    if (board_data_new & BOARD_COLLISION_BIT) {
        // Failure, do NOT update head to new position
        return HEAD_INC_COLLIDED;
    else {
        // Set new location with player and preliminary dir bits
        game_board[new_x][new_y] = board_player_data;

        // Draw new snake tile based on whether it is this player or another console
        uint8_t tile_id = (my_player_num == (c + 1)) ? SPR_SNAKE_TILE_YOUR_PLYR_HEAD : SPR_SNAKE_TILE_OTHER_PLYR_HEAD;
        set_bkg_tile_xy(new_x, new_y, tile_id);

        // Save new head position
        player_x_tail[player_id] = new_x;
        player_y_tail[player_id] = new_y;

        if (board_data_new & BOARD_FOOD_BIT)
            return HEAD_INC_GROW_SNAKE; // Success, and grow snake (i.e. don't increment tail)
        else
            return HEAD_INC_OK; // Success, no collision
    }
}
*/
/*
static void player_tail_increment(uint8_t move_dir, uint8_t player_id) {

    // Current tail position
    int8_t tail_x = player_x_tail[player_id];
    int8_t tail_y = player_y_tail[player_id];

    // Read and cache current tail position data
    uint8_t tail_cur_data = game_board[tail_x][tail_y];

    // Clear board at current (soon to be former) tail position
    game_board[tail_x][tail_y] = BOARD_CLEAR;
    set_bkg_tile_xy(new_x, new_y, BLANK_TILE);

    // Calc new X,Y for tail
    int8_t new_x = tail_x + dir_x[move_dir];
    int8_t new_y = tail_y + dir_y[move_dir];

    // Handle screen edge wraparound
         if (new_x > BOARD_X_MAX) new_x = 0u;
    else if (new_x < 0u)          new_x = BOARD_X_MAX;

         if (new_y > BOARD_Y_MAX) new_y = 0u;
    else if (new_y < 0u)          new_y = BOARD_Y_MAX;

    // Save new tail position
    player_x_tail[player_id] = new_x;
    player_y_tail[player_id] = new_y;

}

*/

// In addition to processing input and events from the
// data packets, this function also "ticks" the game.
//
// Game ticks are triggered by packets so that the game
// state and mechanics on each separate console effectively
// runs in parallel identically (some display timing aside).
//
// To support this the game *never* uses local input directly,
// it always echos local input through the 4-player adapter
// so that it appears on each console at the same exact time
// and in the same exact order.
//
void snakes_process_packet_input_and_tick_game(void) {
    static uint8_t c;

    // One game tick per 4 Player data packet
    game_tick++;

    uint8_t player_id_bit = _4P_PLAYER_1;

    // Loop through all bytes in the packet.
    // Each byte in the packet represents input/commands
    // from one of the players (0..3)
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            // Check for player commands
            uint8_t value = _4p_rx_buf_READ_ptr[c];

            // D-Pad input type command
            if ((value & _SIO_CMD_MASK) == _SIO_CMD_DPAD) {
                // Save D-Pad button press for when the snake is next able to turn
                // Block snake turning back onto itself
                uint8_t dir_request = value & _SIO_DATA_MASK;
                if (dir_request && (player_dir[c] != dir_opposite[dir_request]))
                    player_dir_next[c] = dir_request;
            }
            
            // // All direction changes must only be applied when on an exact tile alignment
            // if (PLAYER_ALIGNED_DIR_CHANGE(player_x_head[c], player_y_head[c])) {

                // Apply movement
                if ((game_tick & 0x0Fu) == 0u) {  // TODO: more granular movement, make a counter that resets and counts down

                    // Check for direction change request
                    if (player_dir_next[c]) {
                        player_dir[c]      = player_dir_next[c];
                        player_dir_next[c] = PLAYER_DIR_NONE;
                    }

                    // set_bkg_tile_xy(player_x_head[c], player_y_head[c], BLANK_TILE);
                    set_bkg_tile_xy(player_x_head[c], player_y_head[c], SPR_SNAKE_TILE_YOUR_PLYR_BODY);

                    switch (player_dir[c]) {
                        case PLAYER_DIR_UP:    player_y_head[c] --; break;
                        case PLAYER_DIR_DOWN:  player_y_head[c] ++; break;
                        case PLAYER_DIR_LEFT:  player_x_head[c] --; break;
                        case PLAYER_DIR_RIGHT: player_x_head[c] ++; break;
                    }                  

                    // Draw a tile at existing location
                    set_bkg_tile_xy(player_x_head[c], player_y_head[c], SPR_SNAKE_TILE_YOUR_PLYR_HEAD);
                }
            // }
        }
        player_id_bit <<= 1;
    }
}

