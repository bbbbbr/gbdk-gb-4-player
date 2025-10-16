#ifndef _GAMEPLAY_H
#define _GAMEPLAY_H

#include "common.h"

#define PLAYER_STATUS_INGAME     0u
#define PLAYER_STATUS_LOST       1u
#define PLAYER_STATUS_WON        2u
#define PLAYER_STATUS_ALL_LOST   3u
#define PLAYER_STATUS_GAME_OVER  4u

#define PLAYER_COUNT_SINGLE_PLAYER_MODE  1u

#define GAME_MODE_STANDARD  0u
#define GAME_MODE_SNAFU     1u
#define GAME_MODE_DEFAULT (GAME_MODE_STANDARD)

#define GAME_MODE_GET()      (game_mode)
#define GAME_MODE_SET(mode)  (game_mode = mode)

void gameplay_run(void);
void gameplay_reset_mode(void);
static void gameplay_init(void);

#define GAMEPLAY_SET_THIS_PLAYER_STATUS(status)  game_this_player_status = status
#define GAMEPLAY_GET_THIS_PLAYER_STATUS()        (game_this_player_status)

#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    void gameplay_run_local_only(void);
#endif

extern uint8_t game_this_player_status;

extern bool    game_players_all_signaled_ready;
extern uint8_t game_players_ready_expected;
extern uint8_t game_players_ready_status;

extern uint8_t game_mode_requested;
extern uint8_t game_mode;

#endif