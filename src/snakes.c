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

typedef struct {
    uint8_t dir;
    uint8_t dir_next;
    uint8_t dir_prev;

    uint8_t head_x;
    uint8_t head_y;

    uint8_t tail_x;
    uint8_t tail_y;
} snake_t;

snake_t snakes[PLAYER_NUM_MAX];

// Laid out as Rows, so: (y * BOARD_WIDTH) + x -> BOARD_INDEX(x,y)
uint8_t game_board[BOARD_WIDTH * BOARD_HEIGHT];

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

const uint8_t snake_tile_tail_lut[] = {
    [J_RIGHT] = SNAKE_TILE_YOURS_TAIL + TILE_RIGHT,
    [J_LEFT]  = SNAKE_TILE_YOURS_TAIL + TILE_LEFT,
    [J_UP]    = SNAKE_TILE_YOURS_TAIL + TILE_UP,
    [J_DOWN]  = SNAKE_TILE_YOURS_TAIL + TILE_DOWN,
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




uint8_t snake_calc_tile_head(uint8_t p_num) {

    // Draw tile for player
    // Use different tile for this player vs others
    uint8_t tile_id = snake_tile_head_lut[ snakes[p_num].dir ];
    if (WHICH_PLAYER_AM_I_ZERO_BASED() != p_num) tile_id += TILE_OTHERS_OFFSET;

    return tile_id;
}


uint8_t snake_calc_tile_tail(uint8_t p_num, uint8_t dir) {

    // Draw tile for player
    // Use different tile for this player vs others
    uint8_t tile_id = snake_tile_tail_lut[ dir ];
    if (WHICH_PLAYER_AM_I_ZERO_BASED() != p_num) tile_id += TILE_OTHERS_OFFSET;

    return tile_id;
}


uint8_t snake_calc_tile_body(uint8_t p_num) {

    // Draw body segment at current location (straight or corner) based on movement
    // dir into current tile (dir_prev[]) and movement dir into next tile (dir[])
    uint8_t tile_id;
    if (snakes[p_num].dir_prev & (PLAYER_DIR_LEFT | PLAYER_DIR_RIGHT))
        tile_id = snake_tile_body_horiz_lut[ snakes[p_num].dir_prev | snakes[p_num].dir ];
    else
        tile_id = snake_tile_body_vert_lut[ snakes[p_num].dir_prev | snakes[p_num].dir ];

    if (WHICH_PLAYER_AM_I_ZERO_BASED() != p_num) tile_id += TILE_OTHERS_OFFSET;

    return tile_id;
}


void snakes_board_reset(void) {

    uint8_t * p_board = game_board;

    for (uint8_t y = 0; y < BOARD_HEIGHT; y++) {
        for (uint8_t x = 0; x < BOARD_WIDTH; x++) {                        
            *p_board++  = BOARD_CLEAR;
        }
    }
}


void snakes_reset_and_draw(void) {

    game_tick             = 0u;
    game_tick_speed       = GAME_TICK_SPEED_START;
    uint8_t player_id_bit = _4P_PLAYER_1;

    snakes_board_reset();

    for (uint8_t c = 0u; c < PLAYER_NUM_MAX; c++) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            snakes[c].dir      = PLAYER_DIR_UP;
            snakes[c].dir_next = PLAYER_DIR_UP;
            snakes[c].dir_next = PLAYER_DIR_NONE;

            // Have to set up and record the snake head/body/tail on the board before drawing
            uint8_t head_x = snakes[c].head_x = PLAYER_X_HEAD_START_STEP * (c + 1u);
            uint8_t head_y = snakes[c].head_y = PLAYER_Y_HEAD_START;

            // Record data to board
            uint8_t board_player_data = PLAYER_BITS_TO_BOARD(c);
            uint8_t * p_board = game_board + BOARD_INDEX(head_x, head_y );

            for (uint8_t row = 0; row < PLAYER_LEN_START; row++) {
                *p_board = board_player_data;
                p_board += BOARD_WIDTH;
            }

            // Snake starts off 3 tiles long (head, body, tail)
            snakes[c].tail_x = PLAYER_X_HEAD_START_STEP * (c + 1u);
            snakes[c].tail_y = PLAYER_Y_HEAD_START + (PLAYER_LEN_START - 1u);

            // Now draw them
            set_bkg_tile_xy(head_x, head_y,      snake_calc_tile_head(c));
            set_bkg_tile_xy(head_x, head_y + 1, snake_calc_tile_body(c));
            set_bkg_tile_xy(head_x, head_y + 2, snake_calc_tile_tail(c, snakes[c].dir));
        }
        player_id_bit <<= 1;        
    }    
}


/*
void snakes_redraw_all(void) {
    // Set up player sprites
    uint8_t my_player_num = WHICH_PLAYER_AM_I_ZERO_BASED();
    uint8_t player_id_bit = _4P_PLAYER_1;

    for (uint8_t c = 0; c < PLAYER_NUM_MAX; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            uint8_t head_x = snakes[c].head_x;
            uint8_t head_y = snakes[c].head_y;

            // Draw tile for player
            // Use different tile for this player vs others
            uint8_t tile_id = snake_tile_head_lut[ snakes[c].dir ];
            if (my_player_num =! c) tile_id += TILE_OTHERS_OFFSET;

            set_bkg_tile_xy(head_x, head_y, tile_id);

            // Record data to board
            uint8_t board_player_data = PLAYER_BITS_TO_BOARD(c);
            uint8_t * p_board = game_board + BOARD_INDEX(head_x, head_y);
            for (uint8_t row = 0; row < PLAYER_X_LEN_START; row++) {
                *p_board = board_player_data;
                p_board += BOARD_WIDTH;
            }


        }
        player_id_bit <<= 1;
    }
}
*/


uint8_t snake_head_increment(uint8_t p_num) {

    uint8_t head_x;
    uint8_t head_y;
    uint8_t old_head_x = head_x = snakes[p_num].head_x;
    uint8_t old_head_y = head_y = snakes[p_num].head_y;

    // Store final move_dir in current position
    // (which will soon become previous position)    
    uint8_t board_player_data = snakes[p_num].dir | p_num; // TODO make a macro out of this
    game_board[ BOARD_INDEX(head_x, head_y) ] = PLAYER_BITS_TO_BOARD(p_num);

    // Increment tail and handle wraparound
    switch (snakes[p_num].dir) {
        case PLAYER_DIR_UP:    head_y--;
                               if (head_y == BOARD_Y_WRAP_MIN) head_y = BOARD_Y_MAX;
                               break;
        case PLAYER_DIR_DOWN:  head_y++;
                               if (head_y > BOARD_Y_MAX) head_y = BOARD_Y_MIN;
                               break;
        case PLAYER_DIR_LEFT:  head_x--;
                               if (head_x == BOARD_X_WRAP_MIN) head_x = BOARD_X_MAX;
                               break;
        case PLAYER_DIR_RIGHT: head_x++;
                               if (head_x > BOARD_X_MAX) head_x = BOARD_X_MIN;
                               break;
    }                  


    // Check for collision at new location, otherwise check for snake food or empty
    uint8_t * p_board_new = game_board + BOARD_INDEX(head_x, head_y);
    uint8_t   board_data_new = *p_board_new;

    // No collision
    if ((board_data_new & BOARD_EXCEPT_FOOD_MASK) == BOARD_CLEAR) {

        // Set new location with player and preliminary dir bits
        *p_board_new = board_player_data;

        uint8_t body_tile_id = snake_calc_tile_body(p_num);
        uint8_t head_tile_id = snake_calc_tile_head(p_num);

        // Update the tiles
        set_bkg_tile_xy(head_x, head_y, head_tile_id);
        set_bkg_tile_xy(old_head_x, old_head_y, body_tile_id);

        snakes[p_num].head_x = head_x;
        snakes[p_num].head_y = head_y;

        if (board_data_new & BOARD_FOOD_BIT)
            return HEAD_INC_GROW_SNAKE; // Success, and grow snake (i.e. don't increment tail)
        else
            return HEAD_INC_OK; // Success, no collision
    } else {
        // Failure, do NOT update head to new position
        return HEAD_INC_COLLIDED;
    }
}


static void snake_tail_increment(uint8_t p_num) {

    // Current tail position
    uint8_t tail_x;
    uint8_t tail_y;
    uint8_t old_tail_x = tail_x = snakes[p_num].tail_x;
    uint8_t old_tail_y = tail_y = snakes[p_num].tail_y;

    // Read and cache current tail position data
    uint8_t * p_board = game_board + BOARD_INDEX(tail_x, tail_y);
    uint8_t board_tail_data = *p_board;

    // Clear board at current (soon to be former) tail position
    *p_board = BOARD_CLEAR;

    // Retrieve dir of tail from board
    // Increment tail and handle wraparound
    switch ( DIR_BITS_FROM_BOARD(board_tail_data) ) {
        case PLAYER_DIR_UP:    tail_y--;
                               if (tail_y == BOARD_Y_WRAP_MIN) tail_y = BOARD_Y_MAX;
                               break;
        case PLAYER_DIR_DOWN:  tail_y++;
                               if (tail_y > BOARD_Y_MAX) tail_y = BOARD_Y_MIN;
                               break;
        case PLAYER_DIR_LEFT:  tail_x--;
                               if (tail_x == BOARD_X_WRAP_MIN) tail_x = BOARD_X_MAX;
                               break;
        case PLAYER_DIR_RIGHT: tail_x++;
                               if (tail_x > BOARD_X_MAX) tail_x = BOARD_X_MIN;
                               break;
    }                  

    // Save new tail position
    snakes[p_num].tail_x = tail_x;
    snakes[p_num].tail_y = tail_y;

    set_bkg_tile_xy(tail_x, tail_y, snake_calc_tile_tail(p_num, DIR_BITS_FROM_BOARD( game_board[ BOARD_INDEX(tail_x, tail_y) ] )) );
    set_bkg_tile_xy(old_tail_x, old_tail_y, BLANK_TILE);
}


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
bool snakes_process_packet_input_and_tick_game(void) {
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
                if (dir_request && (snakes[c].dir != dir_opposite[dir_request]))
                    snakes[c].dir_next = dir_request;
            }
            
            // Apply movement
            if ((game_tick & 0x0Fu) == 0u) {  // TODO: more granular movement, make a counter that resets and counts down

                // Cache current dur as prev for calculating turns
                snakes[c].dir_prev = snakes[c].dir;

                // Check for direction change request
                if (snakes[c].dir_next) {
                    snakes[c].dir      = snakes[c].dir_next;
                    snakes[c].dir_next = PLAYER_DIR_NONE;
                }

                uint8_t try_move = snake_head_increment(c);
                if (try_move == HEAD_INC_COLLIDED) {
                    return false;   // Game over
                }
                else if (try_move == HEAD_INC_GROW_SNAKE) {
                    // Ate food, in order to grow the snake don't increment the tail
                }
                else {
                    snake_tail_increment(c);
                }
            }
        }
        player_id_bit <<= 1;
    }

    return true;
}

