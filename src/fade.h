#ifndef _FADE_H
#define _FADE_H

#include "common.h"

#define FADE_DELAY_NORM       (70u)
#define FADE_DELAY_HELP_IN    (40u)
#define FADE_DELAY_HELP_OUT   (120u)
#define FADE_DELAY_INTRO      (40u)

#define BG_PAL_TITLE 0
#define BG_PAL_BOARD 1

void fade_in(void);
void fade_out(void);

#endif // _FADE_H
