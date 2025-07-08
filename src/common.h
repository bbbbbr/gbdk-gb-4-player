#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdbool.h>

#define ARRAY_LEN(A)  sizeof(A) / sizeof(A[0])

#define FAST_RAND_MODULO_8(range_size)   ( (uint8_t) ( ((uint16_t)rand() * range_size) >> 8 ))


#define DISPLAY_SIO_ISR_DURATION_IN_BGP
#define DISPLAY_USE_SIO_DATA_DURATION_IN_BGP

// #define DEBUG_LOCAL_SINGLE_PLAYER_ONLY

#ifndef DEBUG_LOCAL_SINGLE_PLAYER_ONLY
    #define ENABLE_SIO_KEEPALIVE
#endif

// GB Sound macros
#define AUDTERM_ALL_LEFT  (AUDTERM_4_LEFT | AUDTERM_3_LEFT | AUDTERM_2_LEFT | AUDTERM_1_LEFT)
#define AUDTERM_ALL_RIGHT (AUDTERM_4_RIGHT | AUDTERM_3_RIGHT | AUDTERM_2_RIGHT | AUDTERM_1_RIGHT)

// SIO/Serial commands (Upper Nibble bits only)
#define _SIO_CMD_MASK  0xF0u
#define _SIO_DATA_MASK 0x0Fu

#define _SIO_CMD_DPAD    0x80u
#define _SIO_CMD_BUTTONS 0x90u


#endif // _COMMON_H


