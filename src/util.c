#include <gbdk/platform.h>
#include <stdint.h>

#include "4_player_adapter.h"

// Wait in HALT until vblank ISRs done or a 4 Player mode change
//
// NOTE: To wake up more often than VBlank this expects the serial ISR to be running
void vsync_or_sio_4P_mode_change(void) {

    const uint8_t start_4p_mode = GET_CURRENT_MODE();
    const uint8_t start_systime = *((uint8_t *)&sys_time);

    do {
        __asm \
            HALT    ; // Wait for any interrupt
            NOP     ; // HALT sometimes skips the next instruction
        __endasm;
    } while ((GET_CURRENT_MODE() == start_4p_mode) && (start_systime == *((uint8_t *)&sys_time)) );
}

// Wait in HALT until vblank ISRs done or a 4 Player packet is ready
//
// NOTE: To wake up more often than VBlank this expects the serial ISR to be running
void wait_vsync_or_sio_4P_packet_data_ready(void) {

    const uint8_t start_systime = *((uint8_t *)&sys_time);

    do {
        __asm \
            HALT    ; // Wait for any interrupt
            NOP     ; // HALT sometimes skips the next instruction
        __endasm;
    } while ( ((_4p_rx_buf_count / RX_BUF_PACKET_SZ) == 0u) && (start_systime == *((uint8_t *)&sys_time)) );
}

