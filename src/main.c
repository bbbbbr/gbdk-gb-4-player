#include <gbdk/platform.h>
#include <stdint.h>

#include "common.h"
#include "input.h"
#include "fade.h"


static void main_init(void) {

    HIDE_BKG;
    HIDE_SPRITES;
    DISPLAY_ON;
	UPDATE_KEYS();

    fade_out(FADE_DELAY_NORM, BG_PAL_TITLE);
    SHOW_BKG;
    SHOW_SPRITES;
}


void main(void){

    main_init();

    while (1) {
        vsync();
    }
}
