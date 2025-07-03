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
  - The **Last** Speed and Size values sent before the 0xAA switch to Transmission mode command
    - determine what the speed and size will be in Transmission mode

  - Missing that all Players will receive 4 x 0xCC signal during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xAA to initiate the switch from Ping -> Transmission

  - Any Player can send the 0xAA mode switch request, not just Player 1
    - Not certain, but 0xAA command may need to be aligned
      - such that the first 0xAA is in reply to the Ping header (0xFE)
    - Sending 4 x 0xAA command should be aligned
      - such that it starts being loaded as the next reply immediately after receiving the header packet
        - which means they start being sent one byte after the header for four bytes

  - "It’s possible to restart the ping phase while operating in the transmission phase. To do so, the master Game Boy should send 4 or more bytes (FF FF FF FF, it’s possible fewer $FF bytes need to be sent, but this has not been extensively investigated yet). "
    - It appears sufficient to send as few as 3 x 0xFF to restart Ping mode
    - The 4 x 0xFF command to switch back to Ping mode appears to be more reliable and have less adverse affects
      - when aligned as the first reply bytes to the start of a data packet
        - if this isn't done it can result in a state where the adapter sends garbled data and an altered speed
    - Switching back to Ping mode with 4 x 0xFF seems to always result in a larger gap between bytes (i.e. change in overall transmission rate)
  - Missing that all other Players (usually 2-4) will receive 4 x 0xFF during Ping/Ping phase
    - when (usually) Player 1 sends 4 x 0xFF during Transmission mode to restart Ping mode
      - However Player 1 MUST be connected for any other player to start transmission mode
        - UPDATE!!: NOT 4 x 0xFF, it will be PACKET_BYTES x 0xFF (as in, entire packet is 0xFF)
          - This avoids the false positive situation where a DMG-07 with some consoles unplugged
            - may send periodic 4 x 0xFF even though a mode switch is not being performed
          - Note: This behavior breaks if requesting 5 x 4 = 20 byte packets (the size seems to not be supported)
            - It sends intermittent FF's in groups, but not the full count


  - Power is supplied to the adapter from the Player 1 port
    - Losing power on that port will cause a reset of the device

  - It's possible that any repeating 3(?) byte TX sequence could trigger a reset?
    - seems to be happening when sending tx data byte + 3 x 0xA5 or 3 x 0x01

  - GBs are ALWAYS external clock
    - so when docs say "The protocol is simple: Each Game Boy sends a packet to the DMG-07 simultaneously"
      - They mean "WHEN the GB RXs a packet, it should reply with 


  - The transmission rate is not continuous
    - There seems to be some kind of larger interval between groups of transmitted bytes
      - The size looks variable based on the speed (large gap with faster speed, smaller gap with slower speed)
    - Speed 0x00 -> 13 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 13 = 0.86 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 0) + 512)) ÷ 8)) = 0.977 msec
    - Speed 0xFF -> 23 scanlines between RX bytes -> ((1000 ÷ 59.7) ÷ 254) × 23 = 1.57 msec
      - docs calc: msec per byte: (1000 / ((4194304 ÷ ((6 × 255) + 512)) ÷ 8)) = 3.895 msec

  - With F1 race the SPEED setting seems to apply *INSTANTLY* in Ping mode
    - On the first Ping reply it sets SPEED to 0x28
      - Which changes the interval between Ping packets from 12.3ms to 20.2ms

  - Some notes about the the `RATE` byte in the Ping reply, during Ping phase.  (SPEED)
    - The RATE change applies __immediately__ upon next Ping packet
    At least for Ping phase, this formula for RATE at least *does not* apply:
    > `DMG-07 Bits-Per-Second --> 4194304 / ((6 * RATE) + 512)`The lowest setting (RATE = 0) runs the DMG-07 at the normal speed DMGs usually transfer data (1KB/s), while setting it to $FF runs it close to the slowest speed (2042 bits-per-second).
    - Instead RATE controls: **Interval Between Packets** = `(12.2 msec) + ((RATE & 0x0F) * 1 msec)`
    - Yielding a min interval of `12.2 msec` and max of ~`27.21 msec`
    - The timing for a Ping phase packet is broken down like this:
      - **Interval Between Packet Bytes**: __Always__ `1.43 msec` (i.e. between 1->2, 2->3, 3->4)
      - **Clock Period** for packet bytes (while transmitting): __Always__ `15.95 usec` (62.66khz)
      - **Interval Between Packets**: `12.2 msec` - `27.21 msec`
    - I've seen the base value for RATE vary between `12.20 msec` and `12.23 msec`
    - `RATE == 0x00` seems to have behavior where it may not always change the speed, so if one wanted the minimal speed then using a value of `0x10` for example would set that more reliably

  - RATE and Transmission phase timing
    - **Byte Xfer time**:  `~0.128 msec` (Always)
    - **Interval Between Packet Bytes**:  `((RATE >> 4) x .106 msec) + 0.887 msec`
    - **Total Packet Time**... __whichever is larger of the two__:
      - Precalc minimum: `((RATE & 0x0F) x 1 msec) + 17 msec`
      - Size Based Transfer time `(Byte Xfer + Interval Between Packet Bytes) x Byte Count) + (a value that fluctuates between .36 to 2.15 msec)`
        - Not sure how that trailing add-on amount gets calculated
        - This scenario where Size Based Transfer time exceeds the precalc minimum mostly shows up in 16 byte packet settings, or smaller packet sizes when the *Interval Between Packet Bytes* is at the largest setting.
*/


// Implementation for using the OEM DMG-07 4-Player adapter

// TODO: figure out why speed changes on -> revert to ping -> resume transmit, and esp when initiated by Player 1 vs other player

// TODO: filter briefly unstable connection status values when link cable unplugged?
// TODO: deal with sio isr exiting at unsafe times and causing video glitches - benchmarking shows it's prob ok to burn a line here and there to exit at safe times
// TODO: instrument display of num dropped packets, how long processing is taking, etc
// TODO: OPTIONAL: add a send queue


uint8_t _4p_mode;                    // Current mode: [Ping] -> [Switching to Transmission(Xfer)] -> [Transmission(Xfer)]
uint8_t _4p_mode_state;              // Sub-state within current mode
uint8_t _4p_connect_status;          // 4-Player connection status
uint8_t _4p_rate;                    // Controls Packet timing and interval setting (see extensive _4P_REPLY_PING_RATE_DEFAULT notes in header)

uint8_t _4p_switchmode_cmd_rx_count; // Track whether 4 x 0xCC or 0xFF have been received in a row (signal commands for switching between Ping and Transmission(Xfer) modes)
bool    _4p_request_switch_to_xfer_mode;

// Transmission(Xfer) mode vars
uint8_t _4P_xfer_tx_data;                      // Data to send in tx state
uint8_t _4p_xfer_count;                        // Tracks number of bytes handled in the various states of Transmission(Xfer) mode
// RX Buffer and controls
//
// RX is set up to use a ring buffer with RX_WRITE, RX_READ markers and a RX_COUNT of bytes in the buffer
// - RX_WRITE fills the buffer and increments RX_COUNT
// - RX_READ reads from the buffer
//   - ONLY decrements RX_COUNT when done using the data it is reading
//     - Reads should only process data in multiples of PACKET SIZE, ignore other bytes
//   - ONLY dercrements RX_COUNT in multiples of PACKET SIZE
//     - to ensure RX_WRITE can always either write a FULL packet or NONE (no partials)
//   - The only locking is around reading the byte RX_COUNT and decrementing it from MAIN
//     - The ISR has no locking around RX_COUNT
//
uint8_t   _4p_rx_buf[RX_BUF_SZ];          // RX Buffer is double buffered to allow access of one while other is filling
uint8_t * _4p_rx_buf_WRITE_ptr;           // Write pointer to SIO RX buf for incoming data
uint8_t * _4p_rx_buf_READ_ptr;            // Read pointer to SIO RX buf for reading out the received data
uint8_t   _4p_rx_buf_count;               // Number of bytes currently in SIO RX buf
#define                                    RX_BUF_PTR_END_WRAP_ADDR (_4p_rx_buf + RX_BUF_SZ)  // Used by ISR for pointer wraparound
const uint8_t * _4p_rx_buf_end_wrap_addr = RX_BUF_PTR_END_WRAP_ADDR;                          // Used by Main for pointer wraparound

uint8_t sio_keepalive;               // Monitor SIO rx count to detect disconnects



// =================== State change functions ===================


// May be called either from Main code or ISR, so no critical section
// (main caller expected to wrap it in critical)
inline void four_player_reset_to_ping_no_critical(void) {
    _4p_mode                    = _4P_STATE_PING;
    _4p_mode_state              = _4P_PING_STATE_WAIT_HEADER;
    _4p_connect_status          = _4P_CONNECT_NONE;
    _4p_switchmode_cmd_rx_count = _4P_START_XFER_COUNT_RESET;
    sio_keepalive               = SIO_KEEPALIVE_RESET;
    _4p_request_switch_to_xfer_mode = false;
}


void four_player_init(void) {
    CRITICAL {
        four_player_reset_to_ping_no_critical();
    }
    // Set default rate just once on power up
    _4p_rate = _4P_REPLY_PING_RATE_DEFAULT;
}


bool four_player_request_change_to_xfer_mode(void) {
    uint8_t result = false;  // Default to failed

    CRITICAL {
        if (_4p_mode == _4P_STATE_PING) {
            _4p_request_switch_to_xfer_mode = true;
            result = true; // Signal success
        }
    }

    return result;
}


// Gets called within ISR to change modes once there is
// the right alignment for sending the command
static void four_player_begin_send_xfer_mode_cmd(void) {
    _4p_mode       = _4P_STATE_START_XFER;
    _4p_mode_state = _4P_START_XFER_STATE_SENDING1;
    _4p_request_switch_to_xfer_mode = false;
}


// Gets called within ISR to change modes to Transmission(Xfer), no critical section
static void four_player_init_xfer_mode(void) {
    _4p_mode                    = _4P_STATE_XFER;
    _4p_mode_state              = _4P_XFER_STATE_TX_LOCAL_DATA_AND_RX;
    _4p_switchmode_cmd_rx_count = _4P_RESTART_PING_COUNT_RESET;

    _4p_xfer_count              = _4P_XFER_COUNT_RESET;
    // Buffer and control: Reset read and write pointers to base of buffer, zero count
    _4p_rx_buf_WRITE_ptr        = _4p_rx_buf;
    _4p_rx_buf_READ_ptr         = _4p_rx_buf;
    _4p_rx_buf_count            = 0u;
}


// Called from Main, requires critical section
// should only be called when in Transmission(Xfer) mode
//
// A result of true is "success", but that only means
// the request was successfully submitted, not that the
// mode changed- that needs to be monitored in _4p_mode
bool four_player_request_change_to_ping_mode(void) {
    uint8_t result = false;  // Default to failed

    CRITICAL {
        if (_4p_mode == _4P_STATE_XFER) {
            // This is a sub-state of Transmission(Xfer) mode
            // that tries to wait until the right packet alignment
            // before sending the switch to Ping mode command
            _4p_mode_state = _4P_XFER_STATE_WAIT_ALIGN_RESTART_PING_CMD;
            result = true; // Signal success
        }
    }

    return result;
}


// Gets called within ISR to change modes once there is
// the right alignment for sending the command
static void four_player_begin_send_ping_mode_cmd(void) {
        _4p_mode       = _4P_STATE_RESTART_PING;
        _4p_mode_state = _4P_RESTART_PING_STATE_SENDING1;
}


// =================== Data interface functions ===================


void four_player_set_xfer_data(uint8_t tx_byte) {
    // Don't need a critical section here since it's read-only from the ISR
    // CRITICAL {
        _4P_xfer_tx_data = tx_byte;
    // }
}


uint8_t four_player_rx_buf_get_num_packets_ready(void) {

    // Don't need a critical section here since Main will
    // only use data if it's >= RX_BUF_PACKET_SZ.
    //
    // If the ISR writes more bytes during this read
    // that's ok. The ISR NEVER decrements it, only Main
    // does that using a Critical section.
    return _4p_rx_buf_count / RX_BUF_PACKET_SZ;
}


// Removes N bytes from the RX buffer, with N locked to a multiple of packet size
// That keeps alignment and write calculations easier
void four_player_rx_buf_remove_n_packets(uint8_t packet_count_to_remove) {

    uint8_t byte_count_to_remove = packet_count_to_remove * RX_BUF_PACKET_SZ;
    CRITICAL {
        if (byte_count_to_remove <= _4p_rx_buf_count)
            _4p_rx_buf_count -= byte_count_to_remove;
    }
}


// Configures RATE sent to the adapter during Ping mode
// - 0x00 is a SPECIAL value that takes NO EFFECT, do not use it
// 
void _4p_set_rate(uint8_t new_rate) {
    // Don't need a critical section here since it's read-only from the ISR
    _4p_rate = new_rate;
}

// =================== Interrupt Functions ===================

void four_player_vbl_isr(void) {
    if (sio_keepalive) {
        sio_keepalive--;

        // Keepalive timed out, probably a disconnect so reset vars
        if (sio_keepalive == SIO_KEEPALIVE_TIMEOUT)
            four_player_reset_to_ping_no_critical();
            sio_keepalive == SIO_KEEPALIVE_RESET;
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
        // Monitor for 4 x 0xCC received in a row which means a switch to Transmission(Xfer) mode
        if (sio_byte == _4P_START_XFER_INDICATOR) {
            _4p_switchmode_cmd_rx_count++;

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
                SB_REG = _4P_REPLY_PING_ACK2;       // Pre-load Status2 reply (Ack2)
                _4p_connect_status = sio_byte;      // Save player connection data
                _4p_mode_state++;
                break;

            case _4P_PING_STATE_STATUS2:
                SB_REG = _4p_rate;                 // Pre-load Status3 reply (Speed)
                _4p_connect_status = sio_byte;      // Save player connection data
                _4p_mode_state++;
                break;

            case _4P_PING_STATE_STATUS3:
                SB_REG = _4P_REPLY_PING_PAKCET_SZ;  // Pre-load Header reply (Packet Size)
                _4p_connect_status = sio_byte;      // Save player connection data
                _4p_mode_state = _4P_PING_HEADER;   // Packet should wrap around to header again

                // When switching to Transmission(Xfer mode) the switch needs
                // begin a complete transfer (here) so that the last speed and size
                // data are accurate instead of random or other values. Takes
                // effect on the *next* received sio byte
                if (_4p_request_switch_to_xfer_mode) {
                        four_player_begin_send_xfer_mode_cmd();
                }
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
static void sio_handle_mode_start_xfer(void) {
    SB_REG = _4P_REPLY_START_XFER_CMD;

    // Once the fourth byte has been sent the state should be change into Transmission
    _4p_mode_state++;
    if (_4p_mode_state == _4P_START_XFER_STATE_DONE) {
        // Don't immediately switch to xfer mode, wait for the signal (revert to Ping state in mean time)
        // Revert to Ping state while waiting for ping to transmission mode signal
        _4p_mode       = _4P_STATE_PING;
        _4p_mode_state = _4P_PING_STATE_WAIT_HEADER;
        _4p_switchmode_cmd_rx_count = _4P_START_XFER_COUNT_RESET;
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
    if (_4p_xfer_count == 0) {
        SB_REG = _4P_xfer_tx_data;
         // Clear tx data now that it's transmitted to a specified value
         // (in this case 0x00, meaning no data transmitted)
        _4P_xfer_tx_data = _4P_XFER_CLEAR_TX_AFTER_TRANSMIT_VALUE;
    }
    else
        SB_REG = _4P_XFER_TX_PAD_VALUE;

    // Save the RX data and wrap pointer to start of buffer if needed
    if (_4p_rx_buf_count != RX_BUF_SZ) { 
        // byte count increment and pointer wraparound are done below
        *_4p_rx_buf_WRITE_ptr++ = sio_byte;
    }
    // else TODO: OPTIONAL: could count dropped bytes here

    _4p_xfer_count++;
    // Check for completion of RX state, if so return to initial TX state
    if (_4p_xfer_count == _4P_XFER_RX_SZ) {
        _4p_xfer_count = _4P_XFER_COUNT_RESET;
        
        // Due to how the buffer is set up and used, we can assume
        // the byte Count and WRITE pointer won't overflow at any time during 
        // a single packet write, and so can consolidate the buffer-end
        // wraparound test and Count increment to the end of the packet
        _4p_rx_buf_count += RX_BUF_PACKET_SZ;
        if (_4p_rx_buf_WRITE_ptr == RX_BUF_PTR_END_WRAP_ADDR)
            _4p_rx_buf_WRITE_ptr = _4p_rx_buf;

        // Now ready if trying to switch back to ping mode which seems to
        // work better when aligned to the start of the packet phase
        // (this is end of packet, so *next* transfer is when the first
        //  ping restart command gets loaded as the reply to the first packet byte)
        if (_4p_mode_state == _4P_XFER_STATE_WAIT_ALIGN_RESTART_PING_CMD) {
            four_player_begin_send_ping_mode_cmd();
        }
    }
}


// Try to revert to Ping mode from Transmission(Xfer) mode by sending 4 x 0xFF
static void sio_handle_mode_restart_ping(void) {

    // Send restart ping command bytes regardless of what is being received
    SB_REG = _4P_REPLY_RESTART_PING_CMD;
    _4p_mode_state++;

    // Once the fourth byte has been sent the state should be reset to Ping
    if (_4p_mode_state == _4P_RESTART_PING_STATE_DONE) {
            four_player_reset_to_ping_no_critical();
    }
}


void four_player_sio_isr(void) CRITICAL INTERRUPT {

    #ifdef DISPLAY_SIO_ISR_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif

    // Serial keep alive
    sio_keepalive = SIO_KEEPALIVE_RESET;
    uint8_t sio_byte = SB_REG;


    if (_4p_mode == _4P_STATE_PING)
        sio_handle_mode_ping(sio_byte);
    else if (_4p_mode == _4P_STATE_START_XFER)
        sio_handle_mode_start_xfer();
    else if (_4p_mode == _4P_STATE_XFER)
        sio_handle_mode_xfer(sio_byte);
    else if (_4p_mode == _4P_STATE_RESTART_PING)
        sio_handle_mode_restart_ping();

    // Return to listening
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;

    #ifdef DISPLAY_SIO_ISR_DURATION_IN_BGP
        BGP_REG = ~BGP_REG;
    #endif
}
// Install direct interrupt handler
ISR_VECTOR(VECTOR_SERIAL, four_player_sio_isr)


// =================== General Enable/Disable Commands ===================


void four_player_enable(void) {
    #ifdef ENABLE_SIO_KEEPALIVE
        CRITICAL {
            // Avoid double-install by trying to remove first
            remove_VBL(four_player_vbl_isr);
            add_VBL(four_player_vbl_isr);
        }
    #endif

    set_interrupts(VBL_IFLAG | SIO_IFLAG);

    // Enable Serial RX to start
    SC_REG = SIOF_XFER_START | SIOF_CLOCK_EXT;
}


void four_player_disable(void) {
    #ifdef ENABLE_SIO_KEEPALIVE
        CRITICAL {
            remove_VBL(four_player_vbl_isr);
        }
    #endif

    set_interrupts(VBL_IFLAG & ~SIO_IFLAG);

    // Disable Serial RX
    SC_REG = SIOF_CLOCK_EXT;
}
