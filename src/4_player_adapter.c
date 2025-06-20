#include <gbdk/platform.h>
#include <stdint.h>

#include <stdio.h>
#include <gbdk/console.h>

#include "common.h"

#include "4_player_adapter.h"

// Observations vs Pandocs / DanDocs / etc
/*

- Does GB need be connected to Player 1 for DMG-07 to work, since that is where it draws power
  - Or can it draw power from any 3rd party link cable with power wired up? connector?

- The "ping" (ping) diagram for what replies get sent is ambiguous
  - it can be read as ACK1 should be in SB_REG when the Header byte comes in
    - so that it's loaded and ready to be exchanged when the header byte is received
      - but that isn't how it seems to work, instead
        - ACK1 should be loaded into SB_REG *after* receiving Header (0xFE)
          - so that it's received at the same time STATUS_1
            - and so on

  - Missing that all other Players (usually 2-4) will receive 4 x 0xCC during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xAA to initiate the switch from Ping -> Transmission

  - Also, inaccurate in that ANY Player can send the 0xAA mode switch request, not just Player 1

  - Missing that all other Players (usually 2-4) will receive 4 x 0xFF during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xFF during Transmission mode to restart Ping mode

  - "It’s possible to restart the ping phase while operating in the transmission phase. To do so, the master Game Boy should send 4 or more bytes (FF FF FF FF, it’s possible fewer $FF bytes need to be sent, but this has not been extensively investigated yet). "
    - It appears sufficient to send as few as 3 x 0xFF to restart Ping mode

*/


// Implementation for using the OEM DMG-07 4-Player adapter

// TODO: filter briefly unstable connection status values when link cable unplugged



uint8_t _4p_mode;                     // Current mode: Ping -> Switch to Transmission(Xfer) -> Transmission(Xfer)
uint8_t _4p_mode_state;               // Sub-state within mode
uint8_t _4p_connect_status;           // 4-Player connection status
uint8_t _4p_switch_mode_rx_counter; // Track whether 4 x 0xCC has been received in a row (for switching to Transmission(Xfer) mode)
uint8_t sio_keepalive;                // Monitor SIO rx count to detect disconnects


// == State change functions ==

// May be called either from Main code or ISR, so no critical section
// (main caller expected to wrap it in critical)
inline void four_player_reset_no_critical(void) {
    CRITICAL {
        _4p_mode                   = _4P_STATE_PING;
        _4p_connect_status         = _4P_CONNECT_NONE;
        _4p_mode_state             = _4P_PING_STATE_WAIT_HEADER;
        _4p_switch_mode_rx_counter = _4P_START_XFER_COUNT_RESET;
        sio_keepalive              = SIO_KEEPALIVE_RESET;
    }
}


// Should be executed wrapped with a critical section
void four_player_init(void) {
    CRITICAL {
        four_player_reset_no_critical();
    }
}


// Should be executed wrapped with a critical section
void four_player_change_to_xfer_mode(void) {
    CRITICAL {
        _4p_mode       = _4P_STATE_START_XFER;
        _4p_mode_state = _4P_START_XFER_STATE_WAIT_HEADER;
    }
}


// Only called from ISR, no critical section
void four_player_enter_xfer_mode(void) {
    _4p_mode                   = _4P_STATE_XFER;
    _4p_mode_state             = _4P_XFER_STATE_SEND;
    _4p_switch_mode_rx_counter = _4P_RESTART_PING_COUNT_RESET;
}


// Should be executed wrapped with a critical section
void four_player_restart_ping_mode(void) {
    CRITICAL {
        _4p_mode       = _4P_STATE_RESTART_PING;
        _4p_mode_state = _4P_RESTART_PING_STATE_SENDING1;
    }
}


// == Interrupt Functions ==

void four_player_vbl_isr(void) {
    if (sio_keepalive) {
        sio_keepalive--;

        // Keepalive timed out, probably a disconnect so reset vars
        if (sio_keepalive == SIO_KEEPALIVE_TIMEOUT)
            four_player_reset_no_critical();
    }
}


static inline void sio_handle_mode_ping(uint8_t sio_byte) {
    // In Ping phase 0xFE only ever means HEADER,
    // Status packets should never have that value
    if (sio_byte == _4P_PING_HEADER) {
        _4p_mode_state = _4P_PING_STATE_STATUS1;
        SB_REG = _4P_REPLY_PING_ACK1;
    }
    else {
        // Monitor for 4 x 0xCC received in a row which means a switch to Transmission(Xfer) mode
        if (sio_byte == _4P_START_XFER_INDICATOR) {
            _4p_switch_mode_rx_counter++;
            if (_4p_switch_mode_rx_counter == _4P_START_XFER_COUNT_DONE) {
                four_player_enter_xfer_mode();
                gotoxy(1,3);
                printf("4xCC\n");
            }
        }
        else _4p_switch_mode_rx_counter = _4P_START_XFER_COUNT_RESET;

        // Otherwise continue normal ping handling
        switch (_4p_mode_state) {

            case _4P_PING_STATE_STATUS1:
                SB_REG = _4P_REPLY_PING_ACK2;   // _4P_REPLY_PING_SPEED;      // Pre-load Status2 reply (Speed)
                _4p_connect_status = sio_byte;  // Save player connection data
                _4p_mode_state++;
                break;

            case _4P_PING_STATE_STATUS2:
                SB_REG = _4P_REPLY_PING_SPEED;  // _4P_REPLY_PING_PAKCET_SZ;  // Pre-load Status3 reply (Packet Size)
                _4p_connect_status = sio_byte;  // Save player connection data
                _4p_mode_state++;
                break;

            case _4P_PING_STATE_STATUS3:
                SB_REG = _4P_REPLY_PING_PAKCET_SZ; // _4P_REPLY_PING_ACK1;       // Pre-load Header reply (Ack1)
                _4p_connect_status = sio_byte;     // Save player connection data
                _4p_mode_state = _4P_PING_HEADER;   // Packet should wrap around to header again
                break;
        }

    }
}


// Once program requests switching from Ping to Transmission(Xfer) mode
// then send four 0xAA bytes after receiving a header to prompt
// the DMG-07 to switch over
static inline void sio_handle_mode_start_xfer(uint8_t sio_byte) {

    // In Ping phase 0xFE only ever means HEADER,
    // Status packets should never have that value
    if (sio_byte == _4P_PING_HEADER) {
        _4p_mode_state = _4P_START_XFER_STATE_SENDING1;
        SB_REG = _4P_REPLY_START_XFER_CMD;

        gotoxy(1,1);
        printf("ST-AA\n");
    }
    else if (_4p_mode_state != _4P_START_XFER_STATE_WAIT_HEADER) {

        SB_REG = _4P_REPLY_START_XFER_CMD;

        // Once the fourth byte has been sent the state should be change into Transmission
        if (++_4p_mode_state == _4P_START_XFER_STATE_SENDING4) {
            // TODO: make function handle_switch_to_xfer_mode();
            // TODO: Reset XFER counter vars
            _4p_mode       = _4P_STATE_XFER;
            _4p_mode_state = _4P_XFER_STATE_SEND;
            gotoxy(1,2);
            printf("4xAA\n");
        }
    }
}


// Try to revert to Ping mode from Transmission(Xfer) mode by sending 4 x 0xFF
static inline void sio_handle_mode_restart_ping(void) {

    // Send restart ping command bytes regardless of what is being received
    SB_REG = _4P_REPLY_RESTART_PING_CMD;

    // Once the fourth byte has been sent the state should be reset to Ping
    if (++_4p_mode_state == _4P_RESTART_PING_STATE_SENDING4) {
            gotoxy(1,4);
            printf("4xFF\n");
            four_player_reset_no_critical();
    }
     // else if (++_4p_mode_state == (_4P_RESTART_PING_STATE_SENDING4 + 32))
}


// Data Transmission(Xfer) state
static inline void sio_handle_mode_xfer(uint8_t sio_byte) {

    // Monitor for 4 x 0xFF received in a row which means a switch to Transmission(Xfer) mode
    if (sio_byte == _4P_RESTART_PING_INDICATOR) {
        _4p_switch_mode_rx_counter++;
        if (_4p_switch_mode_rx_counter == _4P_RESTART_PING_COUNT_DONE) {
            four_player_reset_no_critical();
            gotoxy(1,5);
            printf("4xFF\n");
        }
    }
    else _4p_switch_mode_rx_counter = _4P_RESTART_PING_COUNT_RESET;

    // if (_4p_mode_state == _4P_XFER_STATE_SEND) {

    //     SB_REG = // Send buffer from queue
    //     sio_byte = // Save to some buffer
    //     _4p_mode_state == _4P_START_XFER_STATE_WAIT_HEADER;
    //     sio_rx_counter = N * 4 // Expect to received 4 x N bytes
    // } else {
    // }
}


void four_player_sio_isr(void) {
    // Serial keepalive
    sio_keepalive = SIO_KEEPALIVE_RESET;
    uint8_t sio_byte = SB_REG;

    switch (_4p_mode) {
        case _4P_STATE_PING:
            sio_handle_mode_ping(sio_byte); break;

        case _4P_STATE_START_XFER:
            sio_handle_mode_start_xfer(sio_byte); break;

        case _4P_STATE_XFER:
            sio_handle_mode_xfer(sio_byte); break;

        case _4P_STATE_RESTART_PING:
            sio_handle_mode_restart_ping(); break;
    }


    // Return to listening
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_enable(void) {
    CRITICAL {
        // Avoid double-install by trying to remove first
        remove_SIO(four_player_sio_isr);
        remove_VBL(four_player_vbl_isr);

        add_SIO(four_player_sio_isr);
        add_VBL(four_player_vbl_isr);
    }
    set_interrupts(VBL_IFLAG | SIO_IFLAG);

    // Enable Serial RX to start
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_disable(void) {
    CRITICAL {
        remove_SIO(four_player_sio_isr);
        remove_VBL(four_player_vbl_isr);
    }
    set_interrupts(VBL_IFLAG & ~SIO_IFLAG);

    // Disable Serial RX
    SC_REG = SIOF_CLOCK_EXT;
}


uint16_t print_count = 0u;
void four_player_log(void) {

    gotoxy(1,1);
    printf("%hx\n", _4p_connect_status);

    // // unsigned overflow wraparound intentional
    // CRITICAL {
    //     uint8_t rx_tail = (sio_rx_head - sio_rx_count);
    //     while(sio_rx_count >= 4) {
    //         sio_rx_count--;
    //         // if (print_count < (340 / 3)) {
    //             uint8_t print_byte = sio_rx[rx_tail++];
    //             if (print_byte == _4P_PING_HEADER) printf("\n");
    //             printf("%hx,", print_byte);
    //             // print_count++;
    //         // }
    //     }
    // }
}

