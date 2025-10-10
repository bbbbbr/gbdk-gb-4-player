#ifndef _SNAKES_H
#define _SNAKES_H


#define TILE_WH                  8u  // Tile Width and Height in pixels

#define GAME_TICK_SPEED_START    1u

// #define PLAYER_GAME_TICK_SPEED_START         1u

#define PLAYER_X_HEAD_START_STEP ((DEVICE_SCREEN_WIDTH / (PLAYER_NUM_MAX + 1u))) // * TILE_WH)
#define PLAYER_Y_HEAD_START      ((DEVICE_SCREEN_HEIGHT / 2u)) // * TILE_WH)

#define PLAYER_LEN_START         3u

#define PLAYER_DIR_NONE          (0u)
#define PLAYER_DIR_UP            (J_UP)    // 0x04U
#define PLAYER_DIR_DOWN          (J_DOWN)  // 0x08U
#define PLAYER_DIR_LEFT          (J_LEFT)  // 0x02U
#define PLAYER_DIR_RIGHT         (J_RIGHT) // 0x01U

// Check for 8 bit tile alignment before turning in N direction
// #define PLAYER_ALIGNED_DIR_CHANGE(pixel_coord) ((pixel_coord & 0x07u) == 0)

#define PLAYER_ALIGNED_DIR_CHANGE(x,y) (((x | y) & 0x07u) == 0)


#define BOARD_WIDTH         (DEVICE_SCREEN_WIDTH)
#define BOARD_HEIGHT        (DEVICE_SCREEN_HEIGHT - 2)
#define BOARD_X_MAX         (BOARD_WIDTH - 1u)
#define BOARD_Y_MAX         (BOARD_HEIGHT - 1u)
#define BOARD_X_MIN         (0u)
#define BOARD_Y_MIN         (0u)
#define BOARD_X_WRAP_MIN    ((uint8_t)-1)
#define BOARD_Y_WRAP_MIN    ((uint8_t)-1)

#define BOARD_INDEX(x,y)    ((y * BOARD_WIDTH) + x)

#define BOARD_UI_X_START    (0u)
#define BOARD_UI_Y_START    (BOARD_Y_MAX + 1u)

#define BOARD_UI_PRINT_Y    (BOARD_UI_Y_START + 1u)
#define BOARD_UI_PRINT_X_P1 ( 4u)
#define BOARD_UI_PRINT_X_P2 ( 8u)
#define BOARD_UI_PRINT_X_P3 (12u)
#define BOARD_UI_PRINT_X_P4 (16u)

// Bits stored per board/tile entry
// 7..4 : direction
// 3    : wall collision
// 2    : snake food
// 1..0 : player number
#define BOARD_DIR_BITSHIFT      4

#define BOARD_CLEAR             0x00u
#define BOARD_TAIL_DIR_BITS     0xF0u  // WARNING, DON'T MAKE THIS packed to 0..3 in 2 bits since it would break collision when ((Player == 0) && (Dir==0)), relies on at least 1 bit set for any player presence
#define BOARD_COLLISION_BIT     0x08u
#define BOARD_FOOD_BIT          0x04u
#define BOARD_PLAYER_BITS       0x03u  // 0 .. 3
#define BOARD_EXCEPT_FOOD_MASK (BOARD_TAIL_DIR_BITS | BOARD_COLLISION_BIT | BOARD_PLAYER_BITS)

#define FOOD_SPAWNED_NONE          0u
#define FOOD_SPAWNED_MAX           4u  // Max number of food that can be on the board at a time
#define FOOD_TIMER_COUNT_DONE      0u
#define FOOD_SPAWN_TIMER_MIN     (60u * 2u)  // 2 seconds Min
#define FOOD_SPAWN_TIMER_RANGE   0xFFu // 256, which is about 4 seconds - to avoid more expensive modulo calc// (60u * 5u)  // 0-5 seconds range

#define HAZARD_SPAWN_VALUE       0x00u  // Spawn a hazard if the random roll returns this value
#define HAZARD_SPAWN_MASK        0x07u  // 1 in 7 chance of spawning a hazard instead of food

#define DIR_BITS_FROM_BOARD(board_byte)     (board_byte >> 4)
// #define DIR_BITS_TO_BOARD(dir)           ((dir & BOARD_TAIL_DIR_BITS) >> 4)
#define PLAYER_AND_DIR_BITS_TO_BOARD(p_num)         ((snakes[p_num].dir << BOARD_DIR_BITSHIFT) | p_num)

#define HEAD_INC_COLLIDED     0u
#define HEAD_INC_GROW_SNAKE   1u
#define HEAD_INC_OK           2u

void snakes_init_and_draw(void);
// void snakes_reset(void);
// void snakes_redraw_all(void);

bool snakes_process_packet_input_and_tick_game(void);

#ifdef DEBUG_SINGLE_STEP
    void debug_print_info(uint8_t p_num, uint8_t tick);
    void debug_print_checksum(void);
#endif

#endif // _SNAKES_H


