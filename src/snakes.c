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
uint8_t player_dir_prev[PLAYER_NUM_MAX];

uint8_t player_x_head[PLAYER_NUM_MAX];
uint8_t player_y_head[PLAYER_NUM_MAX];

uint8_t player_x_tail[PLAYER_NUM_MAX];
uint8_t player_y_tail[PLAYER_NUM_MAX];

uint8_t game_board[BOARD_WIDTH][BOARD_HEIGHT];

// TODO: change for individual bits for directions so this and other things can be more compact
// Lookup to prevent snake from turning back on top of itself
const uint8_t dir_opposite[] = {
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

const uint8_t snake_tile_head_lut[] = {
    [J_RIGHT] = SNAKE_TILE_YOURS_HEAD + TILE_RIGHT,
    [J_LEFT]  = SNAKE_TILE_YOURS_HEAD + TILE_LEFT,
    [J_UP]    = SNAKE_TILE_YOURS_HEAD + TILE_UP,
    [J_DOWN]  = SNAKE_TILE_YOURS_HEAD + TILE_DOWN,
};

const uint8_t snake_tile_body_vert_lut[] = {
    // Single Left Right segments
    [J_UP   | J_UP]   = SNAKE_TILE_YOURS_BODY + TILE_UP_DOWN,
    [J_DOWN | J_DOWN] = SNAKE_TILE_YOURS_BODY + TILE_UP_DOWN,

    // Corner turn segments [prev | current]
    [J_DOWN   | J_RIGHT] = SNAKE_TILE_YOURS_TURN + TILE_DOWN_RIGHT,
    [J_UP     | J_RIGHT] = SNAKE_TILE_YOURS_TURN + TILE_UP_RIGHT,
    [J_DOWN   | J_LEFT]  = SNAKE_TILE_YOURS_TURN + TILE_DOWN_LEFT,
    [J_UP     | J_LEFT]  = SNAKE_TILE_YOURS_TURN + TILE_UP_LEFT,
};

const uint8_t snake_tile_body_horiz_lut[] = {
    // Left Right segments
    [J_LEFT  | J_LEFT]  = SNAKE_TILE_YOURS_BODY + TILE_LEFT_RIGHT,
    [J_RIGHT | J_RIGHT] = SNAKE_TILE_YOURS_BODY + TILE_LEFT_RIGHT,

    // Corner turn segments [prev | current]
    [J_LEFT   | J_UP]    = SNAKE_TILE_YOURS_TURN + TILE_LEFT_UP,
    [J_LEFT   | J_DOWN]  = SNAKE_TILE_YOURS_TURN + TILE_LEFT_DOWN,
    [J_RIGHT  | J_UP]    = SNAKE_TILE_YOURS_TURN + TILE_RIGHT_UP,
    [J_RIGHT  | J_DOWN]  = SNAKE_TILE_YOURS_TURN + TILE_RIGHT_DOWN,
};


static void player_head_increment(uint8_t move_dir, uint8_t player_id);
static void player_tail_increment(uint8_t move_dir, uint8_t player_id);



void snakes_board_reset(void) {

    for (uint8_t y = 0; y < BOARD_HEIGHT; y++) {
        for (uint8_t x = 0; x < BOARD_WIDTH; x++) {                        
            game_board[x][y] = BOARD_CLEAR;
        }
    }
}


void snakes_reset(void) {

    game_tick       = 0u;
    game_tick_speed = GAME_TICK_SPEED_START;

    snakes_board_reset();

    for (uint8_t c = 0u; c < PLAYER_NUM_MAX; c++) {
        player_dir[c]      = PLAYER_DIR_UP;
        player_dir_next[c] = PLAYER_DIR_UP;
        player_dir_next[c] = PLAYER_DIR_NONE;

        player_x_head[c] = PLAYER_X_HEAD_START_STEP * (c + 1u);
        player_y_head[c] = PLAYER_Y_HEAD_START;

        // TODO: tail
        // player_x_tail[c] = PLAYER_X_HEAD_START_STEP * (c + 1u);
        // player_y_tail[c] = PLAYER_Y_HEAD_START + 1u;
    }    
}


void snakes_redraw_all(void) {
    // Set up player sprites
    uint8_t my_player_num = WHICH_PLAYER_AM_I_ZERO_BASED();
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            uint8_t head_x = player_x_head[c];
            uint8_t head_y = player_y_head[c];

            // Draw tile for player
            // Use different tile for this player vs others
            uint8_t tile_id = snake_tile_head_lut[ player_dir[c] ];
            if (my_player_num =! c) tile_id += TILE_OTHERS_OFFSET;

            set_bkg_tile_xy(head_x, head_y, tile_id);

            // Record data to board
            uint8_t board_player_data = TAIL_BITS_TO_BOARD(player_dir[c]) | (c & BOARD_PLAYER_BITS); // TODO make a macro out of this
            game_board[head_x][head_y] = board_player_data;

        }
        player_id_bit <<= 1;
    }
    hide_sprites_range(PLAYER_NUM_MAX, MAX_HARDWARE_SPRITES);
}

/*
static uint8_t snake_head_increment(uint8_t player_num, bool is_your_player) {

    // Current head position
    uint8_t move_dir = player_dir[player_num];
    int8_t head_x = player_x_head[player_num];
    int8_t head_y = player_y_head[player_num];

    // Store final move_dir in current position
    // (which will soon become previous position)    
    uint8_t board_player_data = TAIL_BITS_TO_BOARD(move_dir) | (player_num & BOARD_PLAYER_BITS); // TODO make a macro out of this
    game_board[head_x][head_y] = board_player_data;

    switch (move_dir) {
        case PLAYER_DIR_UP:    head_y--; break;
        case PLAYER_DIR_DOWN:  head_y++; break;
        case PLAYER_DIR_LEFT:  head_x--; break;
        case PLAYER_DIR_RIGHT: head_x++; break;
    }                  

    // // Calc new X,Y for head
    // int8_t new_x = head_x + dir_x[move_dir];
    // int8_t new_y = head_y + dir_y[move_dir];

    // Handle screen edge wraparound
         if (head_x > BOARD_X_MAX) head_x = 0;
    else if (head_x < 0)          head_x = BOARD_X_MAX;

         if (head_y > BOARD_Y_MAX) head_y = 0;
    else if (head_y < 0)          head_y = BOARD_Y_MAX;

    // Check for collision, otherwise check for snake food or empty
    uint8_t board_data_new = game_board[head_x][head_y];

    // if (board_data_new & BOARD_COLLISION_BIT) {
    //     // Failure, do NOT update head to new position
    //     return HEAD_INC_COLLIDED;
    // }
    // else
     {
        // Set new location with player and preliminary dir bits
        game_board[head_x][head_y] = board_player_data;

        // Draw head tile at new location
        uint8_t head_tile_id = snake_tile_head_lut[ player_dir[player_num] ];
        if (is_your_player) head_tile_id += TILE_OTHERS_OFFSET;

        set_bkg_tile_xy(player_x_head[player_num], player_y_head[player_num], head_tile_id);

        // Save new head position
        player_x_tail[player_num] = head_x;
        player_y_tail[player_num] = head_y;

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

    uint8_t my_player_num = WHICH_PLAYER_AM_I_ZERO_BASED();
    uint8_t player_id_bit = _4P_PLAYER_1;

    // Loop through all bytes in the packet.
    // Each byte in the packet represents input/commands
    // from one of the players (0..3)
    for (c = 0; c < _4P_XFER_RX_SZ; c++) {

        bool is_your_player = (my_player_num == c);

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
            
            // Apply movement
            if ((game_tick & 0x0Fu) == 0u) {  // TODO: more granular movement, make a counter that resets and counts down

                // Cache current dur as prev for calculating turns
                player_dir_prev[c] = player_dir[c];

                // Check for direction change request
                if (player_dir_next[c]) {
                    player_dir[c]      = player_dir_next[c];
                    player_dir_next[c] = PLAYER_DIR_NONE;
                }

                // Draw body segment at current location (straight or corner) based on movement
                // dir into current tile (dir_prev[]) and movement dir into next tile (dir[])
                uint8_t body_tile_id;
                if (player_dir_prev[c] & (PLAYER_DIR_LEFT | PLAYER_DIR_RIGHT))
                    body_tile_id = snake_tile_body_horiz_lut[ player_dir_prev[c] | player_dir[c] ];
                else
                    body_tile_id = snake_tile_body_vert_lut[ player_dir_prev[c] | player_dir[c] ];
                if (is_your_player) body_tile_id += TILE_OTHERS_OFFSET;
                set_bkg_tile_xy(player_x_head[c], player_y_head[c], body_tile_id);

                // Move head
                switch (player_dir[c]) {
                    case PLAYER_DIR_UP:    player_y_head[c]--;
                                           if (player_y_head[c] == BOARD_Y_WRAP_MIN) player_y_head[c] = BOARD_Y_MAX;
                                           break;
                    case PLAYER_DIR_DOWN:  player_y_head[c]++;
                                           if (player_y_head[c] > BOARD_Y_MAX) player_y_head[c] = BOARD_Y_MIN;
                                           break;
                    case PLAYER_DIR_LEFT:  player_x_head[c]--;
                                           if (player_x_head[c] == BOARD_X_WRAP_MIN) player_x_head[c] = BOARD_X_MAX;
                                           break;
                    case PLAYER_DIR_RIGHT: player_x_head[c]++;
                                           if (player_x_head[c] > BOARD_X_MAX) player_x_head[c] = BOARD_X_MIN;
                                           break;
                }                  

                // Draw head tile at new location
                uint8_t head_tile_id = snake_tile_head_lut[ player_dir[c] ];
                if (is_your_player) head_tile_id += TILE_OTHERS_OFFSET;

                set_bkg_tile_xy(player_x_head[c], player_y_head[c], head_tile_id);

                // uint8_t try_move = snake_head_increment(c, is_your_player);
                // if (try_move == HEAD_INC_COLLIDED) {
                //     // Game over, propagate result upward
                // }
                // else if (try_move == HEAD_INC_GROW_SNAKE) {
                // }
                // else {
                //     // snake_head_increment(player_dir[c], c, is_your_player);
                // }
            }
        }
        player_id_bit <<= 1;
    }
}

