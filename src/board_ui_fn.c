#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "util.h"
#include "4_player_adapter.h"

#include "gfx.h"
#include "snakes.h"

#define MAX_PRINT_VAL            99u  // Cap print size to two decimal digits

#define PLAYER_LEN_PRINT_WIDTH  2u
#define PLAYER_LEN_PRINT_HEIGHT 1u

static const uint8_t ui_print_x[4u] = {
    BOARD_UI_PRINT_X_P1,
    BOARD_UI_PRINT_X_P2,
    BOARD_UI_PRINT_X_P3,
    BOARD_UI_PRINT_X_P4
};


// Render a given snake's length to the UI region at the bottom of the screen
void board_ui_print_snake_size(uint8_t p_num, uint8_t digit_lo, uint8_t digit_hi) {

    uint8_t tile_id_base;
    // Use outlined font if the update is for this player, otherwise non-outlined font
    if (WHICH_PLAYER_AM_I_ZERO_BASED() == p_num) tile_id_base = BG_FONT_NUMS_TILES_START;
    else                                         tile_id_base = BG_FONT_NUMS_NO_OUTLINE_TILES_START;

    // Print the Left digit
    uint8_t * p_vram = set_bkg_tile_xy(ui_print_x[p_num], BOARD_UI_PRINT_Y, digit_hi + tile_id_base);
    // Move to next tile and print the right digit (X increments, Y stays the same)
    p_vram++;
    set_vram_byte(p_vram, digit_lo + tile_id_base);
}

// Render a given snake's length to the UI region at the bottom of the screen
void board_ui_print_snake_size_dead(uint8_t p_num) {

    uint8_t tile_id;
    // Use outlined font if the update is for this player, otherwise non-outlined font
    if (WHICH_PLAYER_AM_I_ZERO_BASED() == p_num) tile_id = BG_FONT_NUMS_TILES_START + BG_FONT_X;
    else                                         tile_id = BG_FONT_NUMS_NO_OUTLINE_TILES_START + BG_FONT_X_NO_OUTLINE;

    // Print the Left digit
    uint8_t * p_vram = set_bkg_tile_xy(ui_print_x[p_num], BOARD_UI_PRINT_Y, tile_id);
    // Move to next tile and print the right digit (X increments, Y stays the same)
    p_vram++;
    set_vram_byte(p_vram, tile_id);
}
