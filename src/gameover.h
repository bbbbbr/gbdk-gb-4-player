#ifndef _GAMEOVER_H
#define _GAMEOVER_H

#include "common.h"

void gameover_run(void);
void gameover_show_end_game_message(void);

#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    void gameover_run_local_only(void);
#endif

#endif