#include <gbdk/platform.h>
#include <rand.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"
#include "4_player_adapter.h"

#include "gfx.h"
#include "board_ui_fn.h"

#include "snakes.h"
#include "gameplay.h"


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

    uint8_t size_digit_hi; // Upper digit 0-9 as 00 - 90
    uint8_t size_digit_lo; // Lower digit 0-9

    bool is_alive;
} snake_t;

snake_t snakes[PLAYER_NUM_MAX];

uint8_t snakes_player_count;
uint8_t snakes_alive_count;

// Laid out as Rows, so: (y * BOARD_WIDTH) + x -> BOARD_INDEX(x,y)
uint8_t game_board[BOARD_WIDTH * BOARD_HEIGHT];
const uint8_t * p_board_end = game_board + BOARD_INDEX(BOARD_WIDTH, BOARD_HEIGHT);

uint16_t food_spawn_timer;
uint8_t  food_currently_spawned_count;
uint8_t  food_eaten_total;
uint8_t  rand_seed_1;
bool     rand_initialized;

bool     game_toggle_pause_requested;
bool     game_is_paused;

#ifdef DEBUG_SINGLE_STEP
    bool input_found;
#endif

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

static uint8_t snake_calc_tile_head(uint8_t p_num);
static uint8_t snake_calc_tile_tail(uint8_t p_num, uint8_t dir);
static uint8_t snake_calc_tile_body(uint8_t p_num);

static void snake_board_reset(void);

static uint8_t snake_try_head_increment(uint8_t p_num);
static void snake_tail_increment(uint8_t p_num);
static void snake_length_increment_and_render(uint8_t p_num);
static bool snake_died_check_gameover(void);
static bool snake_handle_head_increment_result(uint8_t p_num, uint8_t try_move);
static bool snake_check_for_input(uint8_t p_num);


static void rand_and_food_reset(void) {
    // TODO: food_spawned_min -> allow spawning more than 1 food at a time, and increase it over time? (by getting "+" spawns that maybe are timed to vanish)
    food_eaten_total             = 0u;
    food_currently_spawned_count = FOOD_SPAWNED_NONE;
    food_spawn_timer   = FOOD_SPAWN_TIMER_MIN;
    rand_seed_1        = 0u;
    rand_initialized   = false;
}


// Rand init gets triggered based on broadcasted user input and game_tick
// which both run in exact parallel, so rand result should be identical
// for all snakes
static void rand_update_seed(void) {

    if (rand_seed_1 == 0u)
        rand_seed_1 = game_tick;
    else {
        rand_initialized = true;
        initrand( (uint16_t)rand_seed_1 << 8 | (uint16_t)game_tick);
    }
 }


static void food_spawn_single_in_center(void) {

    food_currently_spawned_count++;
    game_board[ BOARD_INDEX(BOARD_WIDTH / 2, BOARD_HEIGHT / 2) ] = BOARD_FOOD_BIT;
    set_bkg_tile_xy(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, BOARD_TILE_HEART);
}


static void food_spawn_new(void) {
    // TODO: Food eaten counter with "Win" threshold so this doesn't have to deal with an extremely cluttered board what would make this inefficient and possibly unbeatable

    food_currently_spawned_count++;

    // Start with one food pellet in the middle
    // Upshift by 1 to increase range
    food_spawn_timer = ((uint16_t)FAST_RAND_MODULO_8(FOOD_SPAWN_TIMER_RANGE) << 1)+ FOOD_SPAWN_TIMER_MIN;

    uint8_t spawn_x = FAST_RAND_MODULO_8(BOARD_WIDTH);
    uint8_t spawn_y = FAST_RAND_MODULO_8(BOARD_HEIGHT);

    uint8_t * p_board = game_board + BOARD_INDEX(spawn_x, spawn_y);
    while( *p_board != BLANK_TILE) {
        // Wrap around to the start if the end is reached
        p_board++;
        if (p_board == p_board_end)
            p_board = game_board;

        // Update x position and wrap if needed
        spawn_x++;
        if (spawn_x == BOARD_WIDTH) {
            spawn_x = BOARD_X_MIN;

            spawn_y++;
            if (spawn_y == BOARD_HEIGHT) {
                spawn_y = BOARD_Y_MIN;
            }
        }
    }

    *p_board = BOARD_FOOD_BIT;
    set_bkg_tile_xy(spawn_x, spawn_y, BOARD_TILE_HEART);
 }


static uint8_t snake_calc_tile_head(uint8_t p_num) {

    // Draw tile for player
    // Use different tile / color for this player vs others
    uint8_t tile_id = snake_tile_head_lut[ snakes[p_num].dir ];
    if (WHICH_PLAYER_AM_I_ZERO_BASED() != p_num) tile_id += TILE_OTHERS_OFFSET;

    return tile_id;
}


static uint8_t snake_calc_tile_tail(uint8_t p_num, uint8_t dir) {

    // Draw tile for player
    // Use different tile for this player vs others
    uint8_t tile_id = snake_tile_tail_lut[ dir ];
    if (WHICH_PLAYER_AM_I_ZERO_BASED() != p_num) tile_id += TILE_OTHERS_OFFSET;

    return tile_id;
}


static uint8_t snake_calc_tile_body(uint8_t p_num) {

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


static void snake_board_reset(void) {

    uint8_t * p_board = game_board;

    for (uint8_t y = 0; y < BOARD_HEIGHT; y++) {
        for (uint8_t x = 0; x < BOARD_WIDTH; x++) {
            *p_board++  = BOARD_CLEAR;
        }
    }

    // Start with one food pellet in the middle
    rand_and_food_reset();
    food_spawn_single_in_center();
}


void snakes_init_and_draw(void) {

    // Debug
    #ifdef DEBUG_SINGLE_STEP
        input_found = false;
    #endif

    snakes_alive_count    = 0u; // Reset to zero, increment +1 for every connected player
    game_tick             = 0u;
    game_tick_speed       = GAME_TICK_SPEED_START;
    game_is_paused        = false;

    snake_board_reset();

    uint8_t player_id_bit = _4P_PLAYER_1;
    for (uint8_t c = 0u; c < PLAYER_NUM_MAX; c++) {
        if (IS_PLAYER_CONNECTED(player_id_bit)) {

            snakes_alive_count++;
            snakes[c].is_alive = true;

            snakes[c].dir      = PLAYER_DIR_UP;
            snakes[c].dir_next = PLAYER_DIR_NONE;
            snakes[c].dir_prev = PLAYER_DIR_UP;

            // Have to set up and record the snake head/body/tail on the board before drawing
            uint8_t head_x = snakes[c].head_x = PLAYER_X_HEAD_START_STEP * (c + 1u);
            uint8_t head_y = snakes[c].head_y = PLAYER_Y_HEAD_START;

            // Record data to board
            uint8_t board_player_data = PLAYER_AND_DIR_BITS_TO_BOARD(c);
            uint8_t * p_board = game_board + BOARD_INDEX(head_x, head_y );

            for (uint8_t row = 0; row < PLAYER_LEN_START; row++) {
                *p_board = board_player_data;
                p_board += BOARD_WIDTH;
            }

            // Snake starts off 3 tiles long (head, body, tail)
            snakes[c].tail_x = PLAYER_X_HEAD_START_STEP * (c + 1u);
            snakes[c].tail_y = PLAYER_Y_HEAD_START + (PLAYER_LEN_START - 1u);

            snakes[c].size_digit_lo = PLAYER_LEN_START;
            snakes[c].size_digit_hi = 0u;
            board_ui_print_snake_size(c, snakes[c].size_digit_lo, snakes[c].size_digit_hi);

            // Now draw them
            set_bkg_tile_xy(head_x, head_y,     snake_calc_tile_head(c));
            set_bkg_tile_xy(head_x, head_y + 1, snake_calc_tile_body(c));
            set_bkg_tile_xy(head_x, head_y + 2, snake_calc_tile_tail(c, snakes[c].dir));
        }
        else {
            snakes[c].is_alive = false;
        }
        player_id_bit <<= 1;
    }

    // Save number of players at start
    snakes_player_count = snakes_alive_count;
}


// Render a snake as dead on the board
// Filling it's tiles with skulls marked as collide-able
void snake_render_dead( uint8_t p_num) {

    static bool  is_this_player;
    static uint8_t skull_icon;
    static uint8_t head_x, head_y;

    is_this_player = WHICH_PLAYER_AM_I_ZERO_BASED() == p_num;
    skull_icon = (is_this_player) ? BOARD_TILE_SKULL_DARK : BOARD_TILE_SKULL_LITE;

    head_x = snakes[p_num].head_x;
    head_y = snakes[p_num].head_y;

    uint8_t x = snakes[p_num].tail_x;
    uint8_t y = snakes[p_num].tail_y;

    // Traverse snake from tail to head
    while (1) {
        // Render a skull icon onto current snake tile, set it into board
        set_bkg_tile_xy(x, y, skull_icon);

        uint8_t * p_board = game_board + BOARD_INDEX(x, y);
        // Clear board at current (soon to be former) tail position
        uint8_t board_data = *p_board;
        *p_board = BOARD_COLLISION_BIT;

        // Exit loop when the head is reached and overwritten
        if ((x == head_x) && (y == head_y)) break;

        // Retrieve dir of tail from board
        // Increment tail and handle wraparound
        switch ( DIR_BITS_FROM_BOARD(board_data) ) {
            case PLAYER_DIR_UP:    y--;
                                   if (y == BOARD_Y_WRAP_MIN) y = BOARD_Y_MAX;
                                   break;
            case PLAYER_DIR_DOWN:  y++;
                                   if (y > BOARD_Y_MAX) y = BOARD_Y_MIN;
                                   break;
            case PLAYER_DIR_LEFT:  x--;
                                   if (x == BOARD_X_WRAP_MIN) x = BOARD_X_MAX;
                                   break;
            case PLAYER_DIR_RIGHT: x++;
                                   if (x > BOARD_X_MAX) x = BOARD_X_MIN;
                                   break;
        }
    }
}


// Try to move the snake to a new tile
static uint8_t snake_try_head_increment(uint8_t p_num) {

    uint8_t head_x;
    uint8_t head_y;
    uint8_t old_head_x = head_x = snakes[p_num].head_x;
    uint8_t old_head_y = head_y = snakes[p_num].head_y;

    // Store final move_dir in current head position (will soon become previous position)
    // Use same value below at (possible) new head position
    uint8_t board_bits_player_num_and_dir = PLAYER_AND_DIR_BITS_TO_BOARD(p_num);
    game_board[ BOARD_INDEX(head_x, head_y) ] = board_bits_player_num_and_dir;

    // Increment head and handle screen edge wraparound
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
        *p_board_new = board_bits_player_num_and_dir;

        uint8_t body_tile_id = snake_calc_tile_body(p_num);
        uint8_t head_tile_id = snake_calc_tile_head(p_num);

        // Update the tiles
        set_bkg_tile_xy(head_x, head_y, head_tile_id);
        set_bkg_tile_xy(old_head_x, old_head_y, body_tile_id);

        snakes[p_num].head_x = head_x;
        snakes[p_num].head_y = head_y;

        // Check for food at new location
        if (board_data_new & BOARD_FOOD_BIT)
            return HEAD_INC_GROW_SNAKE; // Success, and grow snake (i.e. don't increment tail)
        else
            return HEAD_INC_OK; // Success, no collision

    } else {
        // Collision! Do NOT update head to new position
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


// Called after a snake has collided with something and died
//
// Sets player game won/lost status if applicable, also flags game as ended
static bool snake_died_check_gameover(void) {

    // Single player mode ends when the snake dies
    if (snakes_player_count == PLAYER_COUNT_SINGLE_PLAYER_MODE) {
        // Single player mode gets a different message
        GAMEPLAY_SET_THIS_PLAYER_STATUS(PLAYER_STATUS_GAME_OVER);
        return true;  // Game Over
    }
    else {
        // Multi-player mode game ends when only one player is alive
        if (snakes_alive_count == 1u) {

            // If this player is still alive, it was the last and it won.
            // Otherwise all other players lost.
            if (snakes[ WHICH_PLAYER_AM_I_ZERO_BASED() ].is_alive)
                GAMEPLAY_SET_THIS_PLAYER_STATUS(PLAYER_STATUS_WON);
            else
                GAMEPLAY_SET_THIS_PLAYER_STATUS(PLAYER_STATUS_LOST);

            return true;  // Game Over
        }
        else if (snakes_alive_count == 0u) {
            // Or if all remaining players died in the same game tick -> special no-winners messsage
            // Single player mode gets a different message
            GAMEPLAY_SET_THIS_PLAYER_STATUS(PLAYER_STATUS_ALL_LOST);
            return true;  // Game Over
        }
    }

    return false; // Game DID NOT END
}


// Handle the result of a call to snake_try_head_increment()
// Returns: FALSE if game is over
static bool snake_handle_head_increment_result(uint8_t p_num, uint8_t try_move) {

    if (try_move == HEAD_INC_COLLIDED) {

        // Could do tail shrink and point loss when snakes collide
        snakes[p_num].is_alive = false;
        snakes_alive_count--;
        snake_render_dead(p_num);

        // Put X's over dead snake's length display
        // (except when in single player mode, so it remains visible after single player game ended)
        if (snakes_player_count != PLAYER_COUNT_SINGLE_PLAYER_MODE)
            board_ui_print_snake_size_dead(p_num);

        if (snake_died_check_gameover() == true)
            return false; // Signal Game Over
    }
    else if (try_move == HEAD_INC_GROW_SNAKE) {

        // Ate food, in order to grow the snake don't increment the tail
        snake_length_increment_and_render(p_num);
        if (food_currently_spawned_count) food_currently_spawned_count--;

        // TODO: maybe if food is skull snake shrinks and loses total food eaten?
        // TODO: implement/display total food eaten as score/etc
        food_eaten_total++;
    }
    else {
        // Implied: HEAD_INC_OK

        // No food eaten, snake stays same size
        snake_tail_increment(p_num);
    }

    return true;
}


// Increment bcd-ish snake length counter and show it on the overlay
static void snake_length_increment_and_render(uint8_t p_num) {

    // Increment upper digit if wrapping around
    if (snakes[p_num].size_digit_lo == 9u) {
        // Only increment if total is less than 99
        if (snakes[p_num].size_digit_hi != 9u) {
            snakes[p_num].size_digit_lo = 0u;
            snakes[p_num].size_digit_hi++;
        }
    }
    else {
        snakes[p_num].size_digit_lo++;
    }

    board_ui_print_snake_size(p_num, snakes[p_num].size_digit_lo, snakes[p_num].size_digit_hi);
}


// Check for input commands in the 4-Player RX buffer
static bool snake_check_for_input(uint8_t p_num) {

    uint8_t value   = _4p_rx_buf_READ_ptr[p_num];
    uint8_t cmd     =  value & _SIO_CMD_MASK;
    uint8_t payload =  value & _SIO_DATA_MASK;

    // D-Pad input type command
    if (cmd == _SIO_CMD_DPAD) {

        if (!rand_initialized) rand_update_seed();

        // Save D-Pad button press for when the snake is next able to turn
        // Block snake turning back onto itself
        uint8_t dir_request = payload;
        if (dir_request && (snakes[p_num].dir != dir_opposite[dir_request]))
            snakes[p_num].dir_next = dir_request;

        return true;
    }
    else if (cmd == _SIO_CMD_BUTTONS) {
        if (payload == BUTTON_START)
            game_toggle_pause_requested = true;
    }

    return false;
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
#ifdef DEBUG_SHOW_CHECKSUM
    extern uint16_t sio_checksum;
    extern uint16_t _4p_sio_packet_checksum;
#endif

bool snakes_process_packet_input_and_tick_game(void) {
    static uint8_t c;

    // One game tick per 4 Player data packet
    game_tick++;
    game_toggle_pause_requested = false;

    uint8_t player_id_bit = _4P_PLAYER_1;  // Start at lowest player bit


    #ifdef DEBUG_SINGLE_STEP
        bool this_tick_input_found = false;
        for (c = 0u; c < _4P_XFER_RX_SZ; c++) {
            uint8_t value = _4p_rx_buf_READ_ptr[c];
            // D-Pad input type command
            if ((value & _SIO_CMD_MASK) == _SIO_CMD_DPAD) {
                // debug_print_info(c, game_tick);
                this_tick_input_found = true;
                input_found = true;
            }
            #ifdef DEBUG_SHOW_CHECKSUM
            sio_checksum += value;
            #endif
        }
        #ifdef DEBUG_SHOW_CHECKSUM
            if (this_tick_input_found) {
                debug_print_info(4u, (uint8_t)((sio_checksum >> 8) & 0xFFu));
                debug_print_info(5u, (uint8_t)(sio_checksum & 0xFFu));
                debug_print_info(6u, (uint8_t)((_4p_sio_packet_checksum >> 8) & 0xFFu));
                debug_print_info(7u, (uint8_t)(_4p_sio_packet_checksum & 0xFFu));
            }
        #endif
    #endif

    // Loop through all bytes in the packet.
    // Each byte in the packet represents input/commands
    // from one of the players (0..3)
    for (c = 0u; c < _4P_XFER_RX_SZ; c++) {

        if (IS_PLAYER_CONNECTED(player_id_bit) && (snakes[c].is_alive)) {

            snake_check_for_input(c);

            if (game_is_paused == false) {

                // Apply movement if it's a game update tick
                if ((game_tick & 0x0Fu) == 0u) {  // TODO: more granular movement, make a counter that resets and counts down
                    // Cache current dir as prev for calculating turns
                    snakes[c].dir_prev = snakes[c].dir;

                    // Check for direction change request
                    if (snakes[c].dir_next) {

                        snakes[c].dir      = snakes[c].dir_next;
                        snakes[c].dir_next = PLAYER_DIR_NONE;
                    }

                    #ifdef DEBUG_SINGLE_STEP
                    if (input_found) {
                    #endif
                        // Try to move the snake, handle collisions and eating food (growing the snake)
                        // The game over player states get set here if applicable
                        uint8_t try_move     = snake_try_head_increment(c);
                        bool    game_is_over = (snake_handle_head_increment_result(c, try_move) == false);

                        if (game_is_over) return false;
                    #ifdef DEBUG_SINGLE_STEP
                    }
                    #endif
                }
            }
        }

        // Move to next snake
        player_id_bit <<= 1;
    }

    if (game_is_paused == false) {
        #ifdef DEBUG_SINGLE_STEP
        // Debug - flush queued input now that it has been used
        if ((game_tick & 0x0Fu) == 0u)
            input_found = false;
        #endif

        // Only spawn food when there is none
        if (food_currently_spawned_count == FOOD_SPAWNED_NONE) {
            if (food_spawn_timer == FOOD_TIMER_COUNT_DONE) {
                if (rand_initialized) food_spawn_new();
            }
            else food_spawn_timer--;
        }
    }

    // Handle pause toggle requests at the end so that multiple players
    // pressing pause at the same time doesn't cause the pause request
    // to be negated. Instead they are aggregated into a single request
    if (game_toggle_pause_requested) {
        // Invert pause state
        game_is_paused = !game_is_paused;

        // Add or remove the pause indicator, done by toggling ALL sprites on/off
        if (game_is_paused)  SHOW_SPRITES;
        else                 HIDE_SPRITES;
    }

    return true;
}


#ifdef DEBUG_SINGLE_STEP

static const uint8_t ui_print_x[] = {
    BOARD_UI_PRINT_X_P1,
    BOARD_UI_PRINT_X_P2,
    BOARD_UI_PRINT_X_P3,
    BOARD_UI_PRINT_X_P4,
    0u,  // 4u
    2u,
    16u,
    18u,
    6u,
};


    // Render a given snake's length to the UI region at the bottom of the screen
    void debug_print_info(uint8_t p_num, uint8_t tick) {

        uint8_t digit_hi = tick >> 4;
        uint8_t digit_lo = tick & 0x0Fu;

        if (digit_hi <= 9u) digit_hi += BG_FONT_NUMS_TILES_START;
        else               digit_hi += BG_FONT_ALPHA_TILES_START - 10u;

        if (digit_lo <= 9u) digit_lo += BG_FONT_NUMS_TILES_START;
        else               digit_lo += BG_FONT_ALPHA_TILES_START - 10u;


        // Print the Left digit
        uint8_t * p_vram = set_bkg_tile_xy(ui_print_x[p_num], BOARD_UI_PRINT_Y - 1u, digit_hi);
        // Move to next tile and print the right digit (X increments, Y stays the same)
        p_vram++;
        set_vram_byte(p_vram, digit_lo);
    }
#endif