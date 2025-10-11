#ifndef _4_PLAYER_ADATER_H
#define _4_PLAYER_ADATER_H

#include "common.h"

// Control whether the serial handler is installed as a bare vector
// or if it needs to be installed via add_SIO()
// The bare vector is faster, but cannot be installed
#define FOUR_PLAYER_BARE_ISR_VECTOR
#define FOUR_PLAYER_BARE_ISR_SAFE_VRAM_EXIT

#define SIO_KEEPALIVE_RESET   60u
#define SIO_KEEPALIVE_TIMEOUT 0u


// Console TX SIZE: controls how many bytes are sent from each console. 
// - Min: 1 -> x 4 = 4  byte total Packer Size
// - Max: 4 -> x 4 = 16 byte total Packet Size
//
// - Observation: Ping mode restart seems to malfunction when testing
//   with a SIZE of 5, so 4 may be the functional maximum
//
// TODO: OPTIONAL: make size runtime configurable

#define _4P_XFER_SZ 1u           // Send N Bytes per Player in Transmission(Xfer) mode
                                 // Valid values: 1u, 2u, 3u, 4u
                                 // Resulting total RX transfer from DMG-07 will be 4 Players x _4P_XFER_SZ

#define RX_FIFO_NUM_PACKETS  8u   // Buffering capacity for up to N RX packets

// ALSO ADJUSTABLE: RX_FIFO_NUM_PACKETS for controlling how many packets the rx buffer can hold

#if (_4P_XFER_SZ  > 1)
    #error "COMPILE FAIL: _4P_XFER_SZ TX data size larger than 1 byte not supported, to do that add multiple/buffer tx support sio_handle_mode_xfer()"
#endif


// == RATE value setting for Ping and Transmission(Xfer) intervals and timing ==
//
#define _4P_REPLY_PING_RATE_DEFAULT      0x10u
//
// - This setting is set in Ping Mode and applies immediately upon the next packet
// - The value applied for Transmission(Xfer) mode will be from the last packet sent in Ping mode by Player 1
// - It applies to Ping and Transmission(Xfer) modes, but affects them differently per below
// 
// == RATE in Ping Mode ==
//
//   - The RATE change applies __immediately__ upon next Ping packet
//     - Value Range
//       - Use Lower Nibble to: Set min possible ` time (i.e. delay between packets)
//
//       - Interval Between Packets      = (12.2 msec) + ((RATE & 0x0F) * 1 msec)
//         - Range: 12.2 msec -> 27.21 msec
//         - Min: 0x00 =  12.2 msec
//         - Max: 0x0F = ~27.2 msec
//         - WARNING! The value 0x00 is special and does nothing (no rate/timing change),
//                    so for min Ping rate use a value of 0x10 to access the "0" setting
//
//    - These values are *fixed and do not change* in Ping mode
//     - Interval After Each Packet Byte = 1.43 msec
//     - Active Byte Transfer Time       = ~0.128 msec
//       - S-CLK Clock Period            = ~15.95 usec (62.66khz)
//
//
// == RATE in Transmission(Xfer) Mode ==
//
//   - For this mode RATE is set by the LAST value transmitted in Ping mode by Player 1
//     - Value Range
//       - Use Upper Nibble to: Set time between packet bytes
//       - Use Lower Nibble to: Set min possible packet time (i.e. delay between packets)
//       - WARNING! The value 0x00 is special and does nothing (no rate/timing change),
//                  so consider either 0x01 or 0x10 to achieve "faster" settings
//
//       - Interval Between Packet Bytes = ((RATE >> 4) x .106 msec) + 0.887 msec
//       - Total Packet Time =
//         - Whichever of the following is larger:
//           - Precalc Minimum           = ((RATE & 0x0F) x 1 msec) + 17 msec
//              - Range: 17 msec -> 32 msec
//              - Min: 0x00 = ~17 msec
//              - Max: 0x0F = ~32 msec
//           - Bytes Transfer Time       = (Byte Transfer Time[0.128 msec] + Interval Between Packet Bytes) x Byte Count) + (a value somewhere between .36 to 2.15 msec)
//             - Not sure how the trailing add-on amount is determined
//             - This scenario where Bytes Transfer Time exceeds the Precalc Minimum
//               mostly shows up in 16 byte packet settings, or smaller packet sizes
//               when the *Interval Between Packet Bytes* is at the largest setting.    
//
//    - These values are *fixed and do not change* in Transmission mode
//     - Active Byte Transfer Time       = ~0.128 msec
//       - S-CLK Clock Period            = ~15.95 usec (62.66khz)
//
//    - Some examples for Transmission Mode to illustrate:
//
//      - Shortest/Smallest possible packet
//        - **This is a special case where RATE 0x10 x 4 Bytes is fastest transfer since RATE 0x00 is not available**
//         - Packet Bytes:4, RATE=0x10 = Total Packet Time = ~17 msec
//           - Interval Between Packet Bytes = ((0x10 >> 4) x .106 msec) + 0.887 msec = ~0.993 msec
//           - Total Transfer time           = 17 msec (see below)
//             - Precalc Minimum             = ((0x10 & 0x0F) x 1 msec) + 17 msec     = 17 msec   <-- Larger of the two, so this is the used value
//             - Bytes Transfer Time         = (0.128 msec + 0.993 msec) x 4 bytes    = 4.48 msec <-- Smaller than Minimum, so **ignored**
//
//      - Another
//         - Packet Bytes:4, RATE=0x01 = Total Packet Time = ~18 msec
//           - Interval Between Packet Bytes = ((0x01 >> 4) x .106 msec) + 0.887 msec = ~0.887 msec
//           - Total Transfer time           = 18 msec (see below)
//             - Precalc Minimum             = ((0x01 & 0x0F) x 1 msec) + 17 msec     = 18 msec   <-- Larger of the two, so this is the used value
//             - Bytes Transfer Time         = (0.128 msec + 0.887 msec) x 4 bytes    = 4.06 msec <-- Smaller than Minimum, so **ignored**
//
//      - Longest/Largest possible packet
//         - Packet Bytes:16, RATE=0xFF = Total Packet Time = ~41.6 msec
//           - Interval Between Packet Bytes = ((0xFF >> 4) x .106 msec) + 0.887 msec = ~2.477 msec
//           - Total Transfer time           = ~41.6 msec (see below)
//             - Precalc Minimum             = ((0xFF & 0x0F) x 1 msec) + 17 msec     = 32 msec    <-- Smaller of the two, so **ignored**
//             - Bytes Transfer Time         = (0.128 msec + 2.477 msec) x 16 bytes   = 41.68 msec <-- Larger than Minimum, so this is the used value
//
//


#define PLAYER_NUM_MIN 1u
#define PLAYER_NUM_MAX 4u

// 4-Player adapter communication states
enum {
    _4P_STATE_PING,         // Initial mode where connection is determined ("Ping")
    _4P_STATE_START_XFER,   // Switching from Ping -> Transmission(Xfer) mode by sending 4 x _4P_REPLY_START_XFER_CMD
    _4P_STATE_XFER,         // Operational state data packets are broadcast ("Transmission")
    _4P_STATE_RESTART_PING, // Switching from Transmission(Xfer) -> Ping mode by sending 4 x _4P_REPLY_RESTART_PING_CMD
};


// Ping Phase
// * 4 bytes sent by adapter: <0xFE> <STATUS1> <STATUS2> <STATUS3>
//   FE is the packet header and does not change
// * With received STATUS composed of the following bits:
//   2..0: Recipient's Player Slot/ID, value 0-3
//   7..4: Connection status for all Slots/Players
//         .4 = Slot/Player 1 ... .7 = Slot/Player 4

// For each of the bytes received above, a reply
// should be sent (pre-loaded) as follows
//
// The (currected) pandocs list the order as follows:
//
//     ----------------------------
//     DMG-07 Sends -->  What Game Boy loads SB Reg with as reply for *next* transfer
//     ----------------------------
//     HEADER       -->  ACK1 = 0x88   (HEADER BYTE == 0xFE)
//     STATUS_1     -->  ACK2 = 0x88
//     STATUS_2     -->  RATE = Link Cable Speed
//     STATUS_3     -->  SIZE = Packet Size


// Ping mode
    #define _4P_PING_HEADER           0xFEu
    #define _4P_REPLY_PING_ACK1       0x88u
    #define _4P_REPLY_PING_ACK2       0x88u
    #define _4P_REPLY_PING_PAKCET_SZ (_4P_XFER_SZ)

// Start Transmission(Xfer) mode
    #define _4P_REPLY_START_XFER_CMD         0xAAu  // Sent x 4 by *any* Player to change from Ping to Transmission(Xfer) mode
    #define _4P_START_XFER_INDICATOR         0xCCu  // Received x 4 by *any* (non-sending?) Players indicating a change from Ping to Transmission(Xfer) mode
    #define _4P_START_XFER_COUNT_DONE        4u     // How many consecutive received 0xCC in a row triggers the switch from Ping to Transmission(Xfer) mode
    #define _4P_START_XFER_COUNT_RESET       0u

// Transmission(Xfer) mode
    #define _4P_XFER_RX_SZ                   (_4P_XFER_SZ * 4u)                 // Total number of bytes to receive from all connected devices
    #define _4P_XFER_TX_SZ                   (_4P_XFER_SZ)                      // Number of bytes to send during broadcast stage
    #define _4P_XFER_TX_PADDING_SZ           (_4P_XFER_RX_SZ - _4P_XFER_TX_SZ)  // Number of bytes to send during Send byte count set in Ping stage
    #define _4P_XFER_COUNT_RESET             0u
    #define _4P_XFER_TX_PAD_VALUE            0x00u
    #define _4P_XFER_CLEAR_TX_AFTER_TRANSMIT_VALUE 0x00u
    #define NO_PACKETS_TO_DISARD             0u

    // Buffer sizing for Transmission(Xfer) mode
    #define RX_FIFO_PACKET_SZ      (_4P_XFER_RX_SZ)   // Size in bytes of total RX transfer (i.e TX Size x 4 Players)
    #define RX_FIFO_SZ            (RX_FIFO_PACKET_SZ * RX_FIFO_NUM_PACKETS)

#if (RX_FIFO_SZ  > 256)
    #error "COMPILE FAIL: RX_FIFO_PACKET_SZ * RX_FIFO_NUM_PACKETS cannot be larger than 256 bytes"
#endif


// Restart Ping mode
    #define _4P_REPLY_RESTART_PING_CMD       0xFFu  // Sent x 4 by any Player to change from Transmission(Xfer) to Ping mode
    #define _4P_RESTART_PING_INDICATOR       0xFFu  // Received x 4 by *any* (non-sending?) Players indicating a change from Transmission(Xfer) back to Ping mode
    #define _4P_RESTART_PING_COUNT_DONE      (_4P_XFER_RX_SZ)  // How many consecutive 0xFF in a row triggers the switch from Ping to Transmission(Xfer) mode
    #define _4P_RESTART_PING_COUNT_RESET     0u


// Ping mode states
    enum {                           // Given State N what reply byte should be loaded for the *NEXT* transfer?
        _4P_PING_STATE_WAIT_HEADER,
        _4P_PING_STATE_HEADER0,      // Load reply Ack1
        _4P_PING_STATE_STATUS1,      // Load reply Ack2
        _4P_PING_STATE_STATUS2,      // Load reply Speed
        _4P_PING_STATE_STATUS3,      // Load reply Packet Size
    };

// Switching to Transmission(Xfer) mode states
    enum {
//        _4P_START_XFER_STATE_WAIT_HEADER,
        _4P_START_XFER_STATE_SENDING1, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_START_XFER_STATE_SENDING2,
        _4P_START_XFER_STATE_SENDING3,
        _4P_START_XFER_STATE_SENDING4,
        _4P_START_XFER_STATE_DONE,
    };

// Transmission(Xfer) modes
// Only substates in this mode are TX+RX (default) or align waiting to send the command to switch back to ping mode
    enum {
        _4P_XFER_STATE_TX_LOCAL_DATA_AND_RX,
        _4P_XFER_STATE_WAIT_ALIGN_RESTART_PING_CMD
    };

// Reset from Ping back to Transmission(Xfer) mode states
    enum {
        // _4P_RESTART_PING_STATE_WAIT_ALIGN,
        _4P_RESTART_PING_STATE_SENDING1,
        _4P_RESTART_PING_STATE_SENDING2,
        _4P_RESTART_PING_STATE_SENDING3,
        _4P_RESTART_PING_STATE_SENDING4,
        _4P_RESTART_PING_STATE_DONE,
    };


// Player connectivity bits
// Bit packing for player connection byte broadcasted by hardware (stored in _4p_connect_status) is:
// .7 .. .4 = Player connection status bits, one bit per player
// .3 .. .0 = "Your" player number, range 1-4
#define _4P_PLAYER_ID_MASK 0x07u
#define _4P_CONNECT_NONE   0u
#define _4P_PLAYER_1   (1u << 4)
#define _4P_PLAYER_2   (1u << 5)
#define _4P_PLAYER_3   (1u << 6)
#define _4P_PLAYER_4   (1u << 7)

#define PLAYER_1  1u
#define PLAYER_2  2u
#define PLAYER_3  3u
#define PLAYER_4  4u

void four_player_init(void);
void four_player_enable(void);
void four_player_disable(void);
bool four_player_request_change_to_xfer_mode(void);
bool four_player_request_change_to_ping_mode(void);

void four_player_set_packet_discard_count(uint8_t packets_to_discard);

void four_player_set_xfer_data(uint8_t tx_byte);
uint8_t four_player_rx_fifo_get_num_packets_ready(void);
void four_player_rx_fifo_remove_n_packets(uint8_t packet_count_to_remove);
void _4p_set_rate(uint8_t speed);

extern uint8_t _4p_mode;
extern uint8_t _4p_connect_status;
extern uint8_t _4P_xfer_tx_data;

extern uint8_t   _4p_rx_fifo[RX_FIFO_SZ];
extern uint8_t * _4p_rx_fifo_READ_ptr;
extern uint8_t   _4p_rx_fifo_count;
extern const uint8_t * _4p_rx_fifo_end_wrap_addr;

extern uint8_t _4p_rx_overflowed_bytes_count;

// PLAYER_ID_BIT should be one of _4P_PLAYER_1/2/3/4
#define IS_PLAYER_CONNECTED(PLAYER_ID_BIT)  (_4p_connect_status & PLAYER_ID_BIT)
#define WHICH_PLAYER_AM_I()                 (_4p_connect_status & _4P_PLAYER_ID_MASK)
#define WHICH_PLAYER_AM_I_ZERO_BASED()      ((_4p_connect_status & _4P_PLAYER_ID_MASK) - 1u)
#define IS_PLAYER_DATA_READY()              (four_player_rx_fifo_get_num_packets_ready() != 0u)
#define GET_CURRENT_MODE()                  (_4p_mode)

// Due to how the buffer is set up and used, we can assume
// the READ pointer won't overflow at any time during a single packet read,
// and so can consolidate the buffer-end wraparound test and
// count increment to the end of the packet.
inline void _4p_rx_fifo_packet_increment_read_ptr(void) {
    _4p_rx_fifo_READ_ptr += RX_FIFO_PACKET_SZ;
    if (_4p_rx_fifo_READ_ptr == _4p_rx_fifo_end_wrap_addr)
        _4p_rx_fifo_READ_ptr = _4p_rx_fifo;
}

#endif // _4_PLAYER_ADATER_H
