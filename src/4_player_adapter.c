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

- The "ping" (enumeration) diagram for what replies get sent is ambiguous
  - it can be read as ACK1 should be in SB_REG when the Header byte comes in
    - so that it's loaded and ready to be exchanged when the header byte is received
      - but that isn't how it seems to work, instead
        - ACK1 should be loaded into SB_REG *after* receiving Header (0xFE)
          - so that it's received at the same time STATUS_1
            - and so on

  - Missing part about how all other Players (usually 2-4) will receive 4 x 0xCC during Enumeration/Ping phase
    - when (usually) Player 1 sends 4 x 0xAA to initiate the switch from Enumeration -> Transmission

  - Also, inaccurate in that ANY Player can send the 0xAA mode switch request, not just Player 1

    TODO: naming -> Mode or Phase instead of State
*/


// Implementation for using the OEM DMG-07 4-Player adapter

// TODO: filter briefly unstable connection status values when link cable unplugged



uint8_t _4p_state;                      // Master state (Enumerate vs Xfer)
uint8_t _4p_sub_state;                  // Packet state within Enumeration
uint8_t _4p_connect_status;             // 4-Player connection status

uint8_t _4p_switch_to_transfer_counter; // Track whether 4 x 0xCC has been received in a row (for switching to transfer mode)

// uint8_t sio_rx_head;
// uint8_t sio_rx_count;
// uint8_t sio_rx[SIO_BUF_SZ];

uint8_t sio_watchdog;


void four_player_vbl_isr(void) {
    if (sio_watchdog) {
        sio_watchdog--;
        // Serial watchdog counter no longer being updated, 
        // probably disconnect so reset vars
        if (sio_watchdog == SIO_WATCHDOG_TRIGGERED) {
            _4p_state           = _4P_STATE_ENUMERATE;
            _4p_connect_status  = _4P_CONNECT_NONE;
            _4p_sub_state = _4P_ENUM_STATE_WAIT_HEADER;
        }
    }
}


static inline void sio_handle_state_enumerate(uint8_t sio_byte) {
    // In Enumeration phase 0xFE only ever means HEADER,
    // Status packets should never have that value
    if (sio_byte == _4P_ENUM_HEADER) {
        _4p_sub_state = _4P_ENUM_STATE_STATUS1;
        SB_REG = _4P_REPLY_ENUM_ACK1;
    }
    else {
        // Monitor for 4 x 0xCC received in a row which means Player 1 changed us to 
        if (sio_byte == _4P_START_XFER_INDICATOR) {
            _4p_switch_to_transfer_counter++;
            if (_4p_switch_to_transfer_counter == _4P_START_XFER_COUNT_THRESHOLD) {
                // TODO: make function handle_switch_to_xfer_mode();
                // TODO: Reset XFER counter vars
                _4p_state     = _4P_STATE_XFER;
                _4p_sub_state = _4P_XFER_STATE_SEND;
                gotoxy(1,3);
                printf("4xCCA\n");
            }
        }
        else _4p_switch_to_transfer_counter = _4P_START_XFER_COUNT_RESET;
                
        // Otherwise continue normal enumeration handling
        switch (_4p_sub_state) {

            case _4P_ENUM_STATE_STATUS1:
                SB_REG = _4P_REPLY_ENUM_ACK2;   // _4P_REPLY_ENUM_SPEED;      // Pre-load Status2 reply (Speed)
                _4p_connect_status = sio_byte;  // Save player connection data
                _4p_sub_state++;
                break;

            case _4P_ENUM_STATE_STATUS2:
                SB_REG = _4P_REPLY_ENUM_SPEED;  // _4P_REPLY_ENUM_PAKCET_SZ;  // Pre-load Status3 reply (Packet Size)
                _4p_connect_status = sio_byte;  // Save player connection data
                _4p_sub_state++;
                break;

            case _4P_ENUM_STATE_STATUS3:
                SB_REG = _4P_REPLY_ENUM_PAKCET_SZ; // _4P_REPLY_ENUM_ACK1;       // Pre-load Header reply (Ack1)
                _4p_connect_status = sio_byte;     // Save player connection data
                _4p_sub_state = _4P_ENUM_HEADER;   // Packet should wrap around to header again
                break;
        }

    }    
}


// Once program requests switching from Enumeration to Xfer state
// then send four 0xAA bytes after receiving a header to prompt
// the DMG-07 to switch over
static inline void sio_handle_state_start_xfer(uint8_t sio_byte) {

    // In Enumeration phase 0xFE only ever means HEADER,
    // Status packets should never have that value
    if (sio_byte == _4P_ENUM_HEADER) {
        _4p_sub_state = _4P_START_XFER_STATE_SENDING1;
        SB_REG = _4P_REPLY_START_XFER_CMD;
        
        gotoxy(1,1);
        printf("ST-AA\n");        
    }
    else if (_4p_sub_state != _4P_START_XFER_STATE_WAIT_HEADER) {

        SB_REG = _4P_REPLY_START_XFER_CMD;

        // Once the fourth byte has been sent the state should be change into Xfer
        if (++_4p_sub_state == _4P_START_XFER_STATE_SENDING4) {
            // TODO: make function handle_switch_to_xfer_mode();
            // TODO: Reset XFER counter vars
            _4p_state     = _4P_STATE_XFER;
            _4p_sub_state = _4P_XFER_STATE_SEND;
            gotoxy(1,2);
            printf("4xAA\n");
        }
    }    
}

// Data transfer state
static inline void sio_handle_state_xfer(uint8_t sio_byte) {

    sio_byte;
    // if (_4p_sub_state == _4P_XFER_STATE_SEND) {

    //     SB_REG = // Send buffer from queue
    //     sio_byte = // Save to some buffer
    //     _4p_sub_state == _4P_START_XFER_STATE_WAIT_HEADER;
    //     sio_rx_counter = N * 4 // Expect to received 4 x N bytes
    // } else {
    // }
}


// // Debugging (no overflow checking)
// sio_rx[sio_rx_head++] = sio_byte;
// sio_rx_count++;

void four_player_sio_isr(void) {
    // Serial keepalive
    sio_watchdog = SIO_WATCHDOG_RESET;
    uint8_t sio_byte = SB_REG;
    
    if (_4p_state == _4P_STATE_ENUMERATE) {
        sio_handle_state_enumerate(sio_byte);
    }
    else if (_4p_state == _4P_STATE_START_XFER) {
        sio_handle_state_start_xfer(sio_byte);
    }
    else // implied: if (_4p_state == _4P_STATE_XFER) 
    {
        sio_handle_state_xfer(sio_byte);
    }

    // Return to listening
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_init(void) {
    _4p_state           = _4P_STATE_ENUMERATE;
    _4p_connect_status  = _4P_CONNECT_NONE;
    _4p_sub_state       = _4P_ENUM_STATE_WAIT_HEADER;
    _4p_switch_to_transfer_counter = 0;

    sio_watchdog = SIO_WATCHDOG_RESET; // TODO: rename timeout?

    // sio_rx_head   = 0;
    // sio_rx_count = 0; 
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
    //             if (print_byte == _4P_ENUM_HEADER) printf("\n");
    //             printf("%hx,", print_byte);
    //             // print_count++;
    //         // }
    //     }
    // }
}

// Should be executed wrapped with a critical section 
void four_player_request_change_to_transfer_mode(void) {
    CRITICAL {
        _4p_state     = _4P_STATE_START_XFER; 
        _4p_sub_state = _4P_START_XFER_STATE_WAIT_HEADER;
    }
}


