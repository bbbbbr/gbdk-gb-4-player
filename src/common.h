#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define ARRAY_LEN(A)  (sizeof(A) / sizeof(A[0]))

#define FAST_RAND_MODULO_8(range_size)   ( (uint8_t) ( ((uint16_t)rand() * range_size) >> 8 ))


// #define DISPLAY_SIO_ISR_DURATION_IN_BGP
// #define DISPLAY_USE_SIO_DATA_DURATION_IN_BGP

// #define DEBUG_LOCAL_SINGLE_PLAYER_ONLY

// #define DEBUG_SINGLE_STEP
    // #define DEBUG_SHOW_CHECKSUM  // This requires on single-step to be enabled
                                    // The checksum ONLY counts accepted bytes and packets that have been received
    // #define DEBUG_PRINT_PLAYER_READY_STATUS // Same as above, requires single-step to be enabled

#define DEBUG_B_BUTTON_TO_REQUEST_PING_MODE

#define DEBUG_SELECT_BUTTON_SCREEN_MEMDUMP

#ifndef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    #define ENABLE_SIO_KEEPALIVE
#endif

#define ENABLE_SPAWN_HAZARD_AFTER_EATING

// Buttons are downshifted here since they are transmitted in
// the lower 4 bits via the _SIO_CMD_BUTTONS packet type
#define BUTTON_START   ((J_START) >> 4)
#define BUTTON_SELECT  ((J_SELECT) >> 4)
#define BUTTON_A       ((J_A) >> 4)
#define BUTTON_B       ((J_B) >> 4)


// GB Sound macros
#define AUDTERM_ALL_LEFT  (AUDTERM_4_LEFT | AUDTERM_3_LEFT | AUDTERM_2_LEFT | AUDTERM_1_LEFT)
#define AUDTERM_ALL_RIGHT (AUDTERM_4_RIGHT | AUDTERM_3_RIGHT | AUDTERM_2_RIGHT | AUDTERM_1_RIGHT)

// SIO/Serial commands (Upper Nibble bits only)
#define _SIO_CMD_MASK  0xF0u
#define _SIO_DATA_MASK 0x0Fu

#define _SIO_CMD_DPAD      0x80u
#define _SIO_CMD_BUTTONS   0x90u
#define _SIO_CMD_READY     0xA0u
#define _SIO_CMD_HEARTBEAT 0xB0u


#endif // _COMMON_H


