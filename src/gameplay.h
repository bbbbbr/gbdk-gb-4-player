#ifndef _GAMEPLAY_H
#define _GAMEPLAY_H

#include "common.h"

void gameplay_run(void);
void gameplay_init(void);

#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    void gameplay_run_local_only(void);
#endif

#endif