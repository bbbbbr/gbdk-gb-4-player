#ifndef _GAMEPLAY_H
#define _GAMEPLAY_H

#include "common.h"

#define PLAYER_STATUS_INGAME     0u
#define PLAYER_STATUS_LOST       1u
#define PLAYER_STATUS_WON        2u
#define PLAYER_STATUS_ALL_LOST   3u
#define PLAYER_STATUS_GAME_OVER  4u

#define PLAYER_COUNT_SINGLE_PLAYER_MODE  1u

// Loading tile and map data at start of gameplay takes about 6 frames,
// so ignore that many packets plus a few more as wiggle room.
//
// This is to avoid dumping serial packet data in an uncontrolled way
// if the RX buffer goes too long without being serviced. The alternate
// is to make the RX packet buffer deeper (RX_BUF_NUM_PACKETS), but that
// uses more memory.
#define RX_BUF_INITIAL_PACKET_IGNORE_COUNT   8u

void gameplay_run(void);
static void gameplay_init(void);

#define GAMEPLAY_SET_THIS_PLAYER_STATUS(status)  game_this_player_status = status
#define GAMEPLAY_GET_THIS_PLAYER_STATUS()        (game_this_player_status)

#ifdef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    void gameplay_run_local_only(void);
#endif

extern uint8_t game_this_player_status;

#endif