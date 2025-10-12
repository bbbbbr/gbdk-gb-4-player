#include <gbdk/platform.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "input.h"
#include "fade.h"
#include "util.h"

#ifdef DEBUG_SELECT_BUTTON_SCREEN_MEMDUMP

    #include "debug_font_00_to_FF.h"

    #define MEM_COPY_SIZE     128u
    #define MEM_RENDER_WIDTH  (16u)
    #define MEM_RENDER_HEIGHT (MEM_COPY_SIZE / MEM_RENDER_WIDTH)

    #define BOTTOM_DUMP_START_Y (DEVICE_SCREEN_HEIGHT - MEM_RENDER_HEIGHT)

    uint8_t request_dump_mem_on_ready;

    void debug_dump_mem(void) {

        uint8_t temp_buf[MEM_COPY_SIZE];
        // Load font num tiles 0x00 - 0xFF (two digits per tile) aligned with tile 0 = 0x00
        set_bkg_data(0u, debug_font_00_to_FF_TILE_COUNT, debug_font_00_to_FF_tiles);

        // Fill screen with 0x01 as default with border of 0x00 (0x00 is inverted in the font)
        fill_bkg_rect(0u, 0u, DEVICE_SCREEN_WIDTH, DEVICE_SCREEN_HEIGHT, 0x01u);
        fill_bkg_rect(MEM_RENDER_WIDTH + 1, 0u, DEVICE_SCREEN_WIDTH - MEM_RENDER_WIDTH, DEVICE_SCREEN_HEIGHT, 0x00u);
        // fill_bkg_rect(0u, 0u, MEM_RENDER_WIDTH + 1u, MEM_RENDER_HEIGHT + 1u, 0x00u);
        // fill_bkg_rect(0u, BOTTOM_DUMP_START_Y - 1u, MEM_RENDER_WIDTH + 1u, DEVICE_SCREEN_HEIGHT, 0x00u);

        // Dump first 128 bytes of WRAM to 0,0
        memcpy(temp_buf, (const uint8_t *)0xC0A0u, MEM_COPY_SIZE);
        set_bkg_tiles(0u, 0u, MEM_RENDER_WIDTH, MEM_RENDER_HEIGHT, temp_buf);

        // Dump another 128 bytes of WRAM to bottom of screen
        memcpy(temp_buf, (const uint8_t *)0xC200u, MEM_COPY_SIZE);
        set_bkg_tiles(0u, BOTTOM_DUMP_START_Y, MEM_RENDER_WIDTH, MEM_RENDER_HEIGHT, temp_buf);

        fade_in();

        waitpadup();
        waitpad(J_ANY);
    }

#endif