#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"

#include "4_player_adapter.h"

#include "title_screen.h"



static void main_init(void) {


    HIDE_BKG;
    HIDE_SPRITES;
    DISPLAY_ON;
    SPRITES_8x16;

    // fade_out(FADE_DELAY_NORM, BG_PAL_TITLE);
    SHOW_BKG;
    SHOW_SPRITES;
    // fade_in(FADE_DELAY_NORM, BG_PAL_TITLE);
    BGP_REG  = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);
    OBP0_REG = DMG_PALETTE(DMG_WHITE, DMG_LITE_GRAY, DMG_DARK_GRAY, DMG_BLACK);

	UPDATE_KEYS();
}


void main(void){

    main_init();

    four_player_init();
    four_player_enable();

    while (1) {
        title_screen_run();
    }
}
