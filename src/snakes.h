#ifndef _SNAKES_H
#define _SNAKES_H


#define TILE_WH                  8u  // Tile Width and Height in pixels

#define GAME_TICK_SPEED_START         1u

// #define PLAYER_GAME_TICK_SPEED_START         1u

#define PLAYER_X_HEAD_START_STEP ((DEVICE_SCREEN_WIDTH / (PLAYER_NUM_MAX + 1u))) // * TILE_WH)
#define PLAYER_Y_HEAD_START      ((DEVICE_SCREEN_HEIGHT / 2u)) // * TILE_WH)

#define PLAYER_DIR_NONE          (0u)
#define PLAYER_DIR_UP            (J_UP)
#define PLAYER_DIR_DOWN          (J_DOWN)
#define PLAYER_DIR_LEFT          (J_LEFT)
#define PLAYER_DIR_RIGHT         (J_RIGHT)

// Check for 8 bit tile alignment before turning in N direction
// #define PLAYER_ALIGNED_DIR_CHANGE(pixel_coord) ((pixel_coord & 0x07u) == 0)

#define PLAYER_ALIGNED_DIR_CHANGE(x,y) (((x | y) & 0x07u) == 0)


#define BOARD_WIDTH         (DEVICE_SCREEN_WIDTH)
#define BOARD_HEIGHT        (DEVICE_SCREEN_HEIGHT)
#define BOARD_X_MAX         (BOARD_WIDTH - 1u)
#define BOARD_Y_MAX         (BOARD_HEIGHT - 1u)
#define BOARD_X_MIN         (0u)
#define BOARD_Y_MIN         (0u)
#define BOARD_X_WRAP_MIN    ((uint8_t)-1)
#define BOARD_Y_WRAP_MIN    ((uint8_t)-1)

// Bits stored per board/tile entry
// 7..4 : direction
// 3    : wall collision
// 2    : snake food
// 1..0 : player number

#define BOARD_CLEAR          0x00u
#define BOARD_TAIL_DIR_BITS  0xF0u
#define BOARD_COLLISION_BIT  0x08u
#define BOARD_FOOD_BIT       0x04u
#define BOARD_PLAYER_BITS    0x03u

#define TAIL_BITS_FROM_BOARD(board_byte)  ((board_byte & BOARD_TAIL_DIR_BITS) >> 4)
#define TAIL_BITS_TO_BOARD(dir)           ((dir & BOARD_TAIL_DIR_BITS) >> 4)

#define HEAD_INC_COLLIDED     0u
#define HEAD_INC_GROW_SNAKE   1u
#define HEAD_INC_OK           2u


void snakes_reset(void);
void snakes_redraw_all(void);

void snakes_process_packet_input_and_tick_game(void);

#endif // _SNAKES_H


