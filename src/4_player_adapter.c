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

*/


// Implementation for using the OEM DMG-07 4-Player adapter



uint8_t _4p_state;           // Master state (Enumerate vs Xfer)
uint8_t _4p_enumerate_state; // Packet state within Enumeration
uint8_t _4p_connect_status;  // 4-Player connection status

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
            _4p_enumerate_state = _4P_ENUM_STATE_RESET;
        }
    }
}


void four_player_sio_isr(void) {
    // Serial keepalive
    sio_watchdog = SIO_WATCHDOG_RESET;

    uint8_t sio_byte = SB_REG;
    
    // // Debugging (no overflow checking)
    // sio_rx[sio_rx_head++] = sio_byte;
    // sio_rx_count++;
    
    if (_4p_state == _4P_STATE_ENUMERATE) {
        // In Enumeration phase 0xFE only ever means HEADER,
        // Status packets should never have that value
        if (sio_byte == _4P_ENUM_HEADER) {
            _4p_enumerate_state = _4P_ENUM_STATE_STATUS1;
            SB_REG = _4P_ENUM_REPLY_ACK1;
        }
        else {
            switch (_4p_enumerate_state) {

                case _4P_ENUM_STATE_STATUS1:
                    SB_REG = _4P_ENUM_REPLY_ACK2; // _4P_ENUM_REPLY_SPEED;      // Pre-load Status2 reply (Speed)
                    _4p_connect_status = sio_byte;      // Save player connection data
                    _4p_enumerate_state++;
                    break;

                case _4P_ENUM_STATE_STATUS2:
                    SB_REG = _4P_ENUM_REPLY_SPEED; // _4P_ENUM_REPLY_PAKCET_SZ;  // Pre-load Status3 reply (Packet Size)
                    _4p_connect_status = sio_byte;      // Save player connection data
                    _4p_enumerate_state++;
                    break;

                case _4P_ENUM_STATE_STATUS3:
                    SB_REG = _4P_ENUM_REPLY_PAKCET_SZ; // _4P_ENUM_REPLY_ACK1;       // Pre-load Header reply (Ack1)
                    _4p_connect_status = sio_byte;      // Save player connection data
                    _4p_enumerate_state = _4P_ENUM_HEADER; // Packet should wrap around to header again
                    break;
            }

        }
    }
    else // (_4p_state == _4P_STATE_XFER) 
    {
    }

// DEBUG pre-load next
// SB_REG = 0x88u;

    // Return to listening
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_init(void) {
    _4p_state           = _4P_STATE_ENUMERATE;
    _4p_connect_status  = _4P_CONNECT_NONE;
    _4p_enumerate_state = _4P_ENUM_STATE_RESET;

    // SB_REG = 0x88u; // TODO: change back to 0x00?

    // sio_rx_head   = 0;
    // sio_rx_count = 0; 
    sio_watchdog = SIO_WATCHDOG_RESET;   
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
