#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "gfx.h"

#include "print.h"

uint8_t * print_vram_addr = 0u;

// Move printing cursor on screen
void print_gotoxy(uint8_t x, uint8_t y, uint8_t target) {

    if (target == PRINT_BKG)
        print_vram_addr = get_bkg_xy_addr(x,y);
    else
        print_vram_addr = get_win_xy_addr(x,y);
}

// Print a string on background or window at location set by print_gotoxy()
void print_str(const char * txt) {

    uint8_t src_chr;
    uint8_t letter;
    uint8_t line_wrap_x_addr = (((uint8_t)print_vram_addr) & 0x1Fu);

    while (1) {
        src_chr = *txt++; // Next character
        if (!src_chr) return;

        if (src_chr == '\n') {
            // Mask out X location, reset it to line wrap location, move to next line
            print_vram_addr = (uint16_t *)(((uint16_t)print_vram_addr & 0xFFE0u) + (line_wrap_x_addr + 0x20u));
            continue;
        }
        else if (src_chr >= 'A' && src_chr <= 'Z') {
            letter = BG_FONT_ALPHA_TILES_START + (unsigned char)(src_chr - 'A');
        } else if (src_chr >= 'a' && src_chr <= 'z') {
            letter = BG_FONT_ALPHA_TILES_START + (unsigned char)(src_chr - 'a');
        } else if (src_chr >= '0' && src_chr <= '9') {
            letter = BG_FONT_NUMS_TILES_START + (unsigned char)(src_chr - '0');
        } else {
            switch (src_chr) {

                case '+': letter = BOARD_TILE_HEART; break;
                case '@': letter = BOARD_TILE_SKULL_DARK; break;

                // Default is blank tile for Space or any other unknown chars
                default:  letter = BLANK_TILE; break;
            }
        }

        set_vram_byte(print_vram_addr++, letter);
    }
}
