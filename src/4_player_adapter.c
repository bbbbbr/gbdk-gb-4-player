#include <gbdk/platform.h>
#include <stdint.h>
#include <gb/isr.h>

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

  - Missing that all Players will receive 4 x 0xCC signal during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xAA to initiate the switch from Ping -> Transmission

  - Any Player can send the 0xAA mode switch request, not just Player 1
    - Not certain, but 0xAA command may need to be aligned
      - such that the first 0xAA is in reply to the Ping header (0xFE)

  - Missing that all other Players (usually 2-4) will receive 4 x 0xFF during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xFF during Transmission mode to restart Ping mode
      - However Player 1 MUST be connected for any other player to start transmission mode

  - "It’s possible to restart the ping phase while operating in the transmission phase. To do so, the master Game Boy should send 4 or more bytes (FF FF FF FF, it’s possible fewer $FF bytes need to be sent, but this has not been extensively investigated yet). "
    - It appears sufficient to send as few as 3 x 0xFF to restart Ping mode
      - Sometimes the 4 x 0xFF doesn't seem to work and needs another retry

  - Power is supplied to the adapter from the Player 1 port
    - Losing power on that port will cause a reset of the device

  - It's possible that any repeating 3(?) byte TX sequence could trigger a reset?
    - seems to be happening when sending tx data byte + 3 x 0xA5 or 3 x 0x01

  - GBs are ALWAYS external clock
    - so when docs say "The protocol is simple: Each Game Boy sends a packet to the DMG-07 simultaneously"
      - They mean "WHEN the GB RXs a packet, it should reply with 

  - Given some unknown conditions, starting Transmission(Xfer) mode can begin broadcasting random garbage data to all platers
    - The Restart-ping and Start-Transmission modes have no effect when the adapter is in this state

  - The transmission rate is not continuous
    - There seems to be some kind of larger interval between groups of transmitted bytes
      - The size looks variable based on the speed (large gap with faster speed, smaller gap with slower speed)
    - Speed 0x00 -> 13 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 13 = 0.86 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 0) + 512)) ÷ 8)) = 0.977 msec
    - Speed 0xFF -> 23 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 23 = 1.57 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 255) + 512)) ÷ 8)) = 3.895 msec

*/


// Implementation for using the OEM DMG-07 4-Player adapter

// TODO: filter briefly unstable connection status values when link cable unplugged
// TODO: filter out first (couple?) packet(s?) in xfer mode
// TODO: implement loopback alignment check? (i.e.  know what last tx was, if next own rx doesn't match try skipping by +1 packet to re-align->repeat until working)
// TODO: block start xfer mode when already in xfer mode , maybe throw error
// TODO: deal with sio isr exiting at unsafe times and causing video glitches - benchmarking shows it's prob ok to burn a line here and there to exit at safe times
// TODO: current method of claiming the sio buffer does not guarantee a complete buffer, needs to be "claim next buffer" which triggers on completion of a full xfer


uint8_t _4p_mode;                    // Current mode: [Ping] -> [Switching to Transmission(Xfer)] -> [Transmission(Xfer)]
uint8_t _4p_mode_state;              // Sub-state within current mode
uint8_t _4p_connect_status;          // 4-Player connection status
uint8_t _4p_switchmode_cmd_rx_count; // Track whether 4 x 0xCC or 0xFF have been received in a row (signal commands for switching between Ping and Transmission(Xfer) modes)

// Transmission(Xfer) mode vars
uint8_t _4P_xfer_tx_data;                      // Data to send in tx state
uint8_t _4p_xfer_count;                        // Tracks number of bytes handled in the various states of Transmission(Xfer) mode
uint8_t _4p_xfer_rx_buf_sio_active;            // Which of the xfer rx double buffers is active for SIO receiving
uint8_t   _4p_xfer_rx_buf[2][_4P_XFER_RX_SZ];  // RX Buffer is double buffered to allow access of one while other is filling
uint8_t * _4p_xfer_sio_rx_buf_ptr;             // Pointer to slice of RX double buffer in use by sio
bool      _4p_xfer_rx_buf_ready_for_main[2];   // Indicates when a RX buffer is ready for main

uint8_t sio_keepalive;               // Monitor SIO rx count to detect disconnects


#ifdef SIO_CAPTURE_ENABLED
    bool    capture_enabled;
    bool    capture_ready;
    uint8_t capture_count;
    uint8_t capture_rx_buf[CAPTURE_SIZE];
    uint8_t capture_tx_buf[CAPTURE_SIZE];

    void capture_reset(void) {
        capture_ready = false;
        capture_count = 0;
        capture_enabled = false;
    }

    void capture_dump(void) {
        capture_ready = false;
        uint8_t c;

        gotoxy(1, 4);
        for (c = 0; c < CAPTURE_SIZE; c++) {
            if (((c + 1) % 3) == 0)
                printf("\n");
            // Cursed unfixed var args casting SDCC Bug #3172
            // https://sourceforge.net/p/sdcc/bugs/3172/
            uint8_t rx = (uint8_t)capture_rx_buf[c];
            uint8_t tx = (uint8_t)capture_tx_buf[c];
            printf("%hx,%hx|", (uint8_t)rx, (uint8_t)tx);
        }
    }
#endif


// == State change functions ==



// May be called either from Main code or ISR, so no critical section
// (main caller expected to wrap it in critical)
inline void four_player_reset_to_ping_no_critical(void) {
        _4p_mode                    = _4P_STATE_PING;
        _4p_connect_status          = _4P_CONNECT_NONE;
        _4p_mode_state              = _4P_PING_STATE_WAIT_HEADER;
        _4p_switchmode_cmd_rx_count = _4P_START_XFER_COUNT_RESET;
        sio_keepalive               = SIO_KEEPALIVE_RESET;
}


void four_player_init(void) {
    #ifdef SIO_CAPTURE_ENABLED
        capture_reset();
    #endif

    CRITICAL {
        four_player_reset_to_ping_no_critical();
    }
}


void four_player_request_change_to_xfer_mode(void) {

    #ifdef SIO_CAPTURE_ENABLED
        #ifdef SIO_CAPTURE_AND_DUMP_ON_RX_REQUEST_START_XFER
            capture_reset();
            capture_enabled = true;
        #endif
    #endif

    CRITICAL {
        _4p_mode       = _4P_STATE_START_XFER;
        _4p_mode_state = _4P_START_XFER_STATE_WAIT_HEADER;
    }
}


// Only called from ISR, no critical section
void four_player_init_xfer_mode(void) {
    _4p_mode                    = _4P_STATE_XFER;
    _4p_mode_state              = _4P_XFER_STATE_TX_LOCAL_DATA; // TODO: no sub-state in this mode
    _4p_switchmode_cmd_rx_count = _4P_RESTART_PING_COUNT_RESET;

    _4p_xfer_count              = _4P_XFER_COUNT_RESET;
    _4p_xfer_rx_buf_sio_active  = 0u;
    _4p_xfer_sio_rx_buf_ptr     = _4p_xfer_rx_buf[_4p_xfer_rx_buf_sio_active];
    _4p_xfer_rx_buf_ready_for_main[0u] = false;
    _4p_xfer_rx_buf_ready_for_main[1u] = false;
}


void four_player_restart_ping_mode(void) {
    CRITICAL {
        _4p_mode       = _4P_STATE_RESTART_PING;
        _4p_mode_state = _4P_RESTART_PING_STATE_SENDING1;
    }
}


void four_player_set_xfer_data(uint8_t tx_byte) {
    // Don't really need a critical section here since it's read-only from the ISR
    // CRITICAL {
        _4P_xfer_tx_data = tx_byte;
    // }
}


void four_player_claim_active_sio_buffer_for_main(void) {
    CRITICAL {
        // TODO: convert _4p_xfer_rx_buf_ready_for_main[] from array to just two vars selected by _4p_xfer_rx_buf_sio_active
        // Clear ready flag for the buffer claimed by main
        _4p_xfer_rx_buf_ready_for_main[_4p_xfer_rx_buf_sio_active] = false;
        
        // Flip which buffer is active so sio writes into the one not claimed by main
        _4p_xfer_rx_buf_sio_active = !_4p_xfer_rx_buf_sio_active;
        _4p_xfer_sio_rx_buf_ptr    = _4p_xfer_rx_buf[_4p_xfer_rx_buf_sio_active];
    }
}


// == Interrupt Functions ==

void four_player_vbl_isr(void) {
    if (sio_keepalive) {
        sio_keepalive--;

        // Keepalive timed out, probably a disconnect so reset vars
        if (sio_keepalive == SIO_KEEPALIVE_TIMEOUT)
            four_player_reset_to_ping_no_critical();
    }
}


static void sio_handle_mode_ping(uint8_t sio_byte) {
    // In Ping phase 0xFE only ever means HEADER,
    // Status packets should never have that value
    if (sio_byte == _4P_PING_HEADER) {
        _4p_mode_state = _4P_PING_STATE_STATUS1;
        SB_REG = _4P_REPLY_PING_ACK1;
    }
    else {
        // TODO: Maybe move this to the main ISR body so it can be shared for all?
        // Monitor for 4 x 0xCC received in a row which means a switch to Transmission(Xfer) mode
        if (sio_byte == _4P_START_XFER_INDICATOR) {
            _4p_switchmode_cmd_rx_count++;

            #ifdef SIO_CAPTURE_ENABLED
                #ifdef SIO_AUTO_CAPTURE_AND_DUMP_ON_RX_START_XFER
                    // DEBUG: trigger a capture whenever possibly switching into Xfer mode
                    if ((_4p_switchmode_cmd_rx_count == 2) && (capture_enabled == false)  && (capture_ready == false))  {
                        capture_count = 0;
                        capture_enabled = true;
                    }
                #endif
            #endif

            // When final byte of sequence is received, switch to Transmission(Xfer) mode
            if (_4p_switchmode_cmd_rx_count == _4P_START_XFER_COUNT_DONE) {
                four_player_init_xfer_mode();
                SB_REG = _4P_XFER_TX_PAD_VALUE;
                // Put a zero in the reply byte for this transfer and break out of processing
                return;
            }
            SB_REG = _4P_XFER_TX_PAD_VALUE;
        } else _4p_switchmode_cmd_rx_count = _4P_START_XFER_COUNT_RESET;

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

            // Implied non-action: case _4P_PING_STATE_WAIT_HEADER -> load zero as TX placeholder
            default:
                SB_REG = _4P_XFER_TX_PAD_VALUE;
        }

    }
}


// Once program requests switching from Ping to Transmission(Xfer) mode
// then send four 0xAA bytes after receiving a header to prompt
// the DMG-07 to switch over
//
// This should only be called while the hardware is in PING mode
static void sio_handle_mode_start_xfer(uint8_t sio_byte) {
    // Wait for Ping header to align start of command transmission (not sure this is required)
    if (sio_byte == _4P_PING_HEADER) {
        _4p_mode_state = _4P_START_XFER_STATE_SENDING1;
        SB_REG = _4P_REPLY_START_XFER_CMD;
    }
    else if (_4p_mode_state != _4P_START_XFER_STATE_WAIT_HEADER) {

        SB_REG = _4P_REPLY_START_XFER_CMD;

        // Once the fourth byte has been sent the state should be change into Transmission
        if (++_4p_mode_state == _4P_START_XFER_STATE_SENDING4) {            
            // Don't immediately switch to xfer mode, wait for the signal (revert to Ping state in mean time)
            // Revert to Ping state while waiting for ping to transmission mode signal
            _4p_mode       = _4P_STATE_PING;
            _4p_mode_state = _4P_PING_STATE_WAIT_HEADER;
            _4p_switchmode_cmd_rx_count = _4P_START_XFER_COUNT_RESET;
        }
    }
}


// Try to revert to Ping mode from Transmission(Xfer) mode by sending 4 x 0xFF
static void sio_handle_mode_restart_ping(void) {

    // Send restart ping command bytes regardless of what is being received
    SB_REG = _4P_REPLY_RESTART_PING_CMD;

    // Once the fourth byte has been sent the state should be reset to Ping
    if (++_4p_mode_state == _4P_RESTART_PING_STATE_SENDING4) {
            four_player_reset_to_ping_no_critical();
    }
}


// Data Transmission(Xfer) state
static void sio_handle_mode_xfer(uint8_t sio_byte) {

    // Monitor for 4 x 0xFF received in a row which means a switch to Transmission(Xfer) mode
    if (sio_byte == _4P_RESTART_PING_INDICATOR) {
        _4p_switchmode_cmd_rx_count++;
        if (_4p_switchmode_cmd_rx_count == _4P_RESTART_PING_COUNT_DONE) {
            four_player_reset_to_ping_no_critical();
            // Put a zero in the reply byte for this transfer and break out of processing
           SB_REG = _4P_XFER_TX_PAD_VALUE;
           return;
        }
    }
    else _4p_switchmode_cmd_rx_count = _4P_RESTART_PING_COUNT_RESET;


    // Load local data to send for next reply
    // During TX padding phase just send zeros (optional, but recommended)
    //
    // For transfers larger than 1 byte, it would be: if (_4p_xfer_count <= _4P_XFER_TX_SZ) { SB_REG = <whatever data>; }
    if (_4p_xfer_count == 0)
        SB_REG = _4P_xfer_tx_data;
    else
        SB_REG = _4P_XFER_TX_PAD_VALUE;

    // Save the RX data
    _4p_xfer_sio_rx_buf_ptr[_4p_xfer_count++] = sio_byte;

    // Check for completion of RX state, if so return to initial TX state
    if (_4p_xfer_count == _4P_XFER_RX_SZ) {
        _4p_xfer_count = _4P_XFER_COUNT_RESET;
        _4p_xfer_rx_buf_ready_for_main[_4p_xfer_rx_buf_sio_active] = true;
    }
}


void four_player_sio_isr(void) CRITICAL INTERRUPT {

    #ifdef DISPLAY_SIO_ISR_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    // Serial keep alive
    sio_keepalive = SIO_KEEPALIVE_RESET;
    uint8_t sio_byte = SB_REG;


    // More complex mode handlers first
    if (_4p_mode == _4P_STATE_PING)
        sio_handle_mode_ping(sio_byte);
    else if (_4p_mode == _4P_STATE_XFER)
        sio_handle_mode_xfer(sio_byte);
    else if (_4p_mode == _4P_STATE_START_XFER)
        sio_handle_mode_start_xfer(sio_byte);
    else if (_4p_mode == _4P_STATE_RESTART_PING)
        sio_handle_mode_restart_ping();

    // DEBUG, capture RX Incoming and TX Reply data, signal to dump buf when full
    #ifdef SIO_CAPTURE_ENABLED
        if (capture_enabled) {
            if (capture_count < CAPTURE_SIZE) {
                capture_rx_buf[capture_count] = sio_byte;
                capture_tx_buf[capture_count] = SB_REG;
                capture_count++;
                if (capture_count == CAPTURE_SIZE) {
                    capture_enabled = false;
                    capture_ready = true;
                }
            }
        }
    #endif
    // Return to listening
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;

    #ifdef DISPLAY_SIO_ISR_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif
}

// Install direct interrupt handler
ISR_VECTOR(VECTOR_SERIAL, four_player_sio_isr)


void four_player_enable(void) {
    CRITICAL {
        // Avoid double-install by trying to remove first
        remove_VBL(four_player_vbl_isr);

        add_VBL(four_player_vbl_isr);
    }
    set_interrupts(VBL_IFLAG | SIO_IFLAG);

    // Enable Serial RX to start
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_disable(void) {
    CRITICAL {
        // remove_SIO(four_player_sio_isr); // TODO: Change this to a fixed ISR vector for lower overhead
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
}

