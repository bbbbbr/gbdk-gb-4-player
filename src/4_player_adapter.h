#ifndef _4_PLAYER_ADATER_H
#define _4_PLAYER_ADATER_H

#include "common.h"


#define SIO_KEEPALIVE_RESET   60u
#define SIO_KEEPALIVE_TIMEOUT 0u

#define _4P_XFER_SZ 1 // Use 1 Byte as total data size in Transmission(Xfer) mode
// #define _4P_XFER_SZ  2 // 4

#if (_4P_XFER_SZ  > 1)
    #error "COMPILE FAIL: _4P_XFER_SZ TX data size larger than 1 byte not supported, to do that add multiple/buffer tx support sio_handle_mode_xfer()"
#endif

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
    // SPEED:
    // DMG-07 Bits-Per-Second --> 4194304 / ((6 * SPEED) + 512)
    // Rough Bytes per frame: (((4194304 ÷ ((6 × SPEED) + 512)) ÷ 8) ÷ 59.7)
    // Rough MSec per byte:   (1000 / ((4194304 ÷ ((6 × SPEED) + 512)) ÷ 8))
    //
    // 0x00 = Fastest, 0xFF = slowest

    #define _4P_REPLY_PING_SPEED      0u  // 255u // ~256.7 bytes per second // 85u  // ~513 bytes per second // 0x00u // 1024 bytes per second
    // SIZE sets the length of packets exchanged between all Game Boys. Nothing fancy, just the number of bytes in each packet. It probably shouldn’t be set to zero.
    #define _4P_REPLY_PING_PAKCET_SZ (_4P_XFER_SZ) // Reply

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

    // Buffer sizing for Transmission(Xfer) mode
    #define RX_BUF_PACKET_SZ      (_4P_XFER_RX_SZ)   // Size in bytes of total RX transfer (i.e TX Size x 4 Players)
    #define RX_BUF_NUM_PACKETS    3u                 // Tripple buffer for RX packets 
    #define RX_BUF_SZ             (RX_BUF_PACKET_SZ * RX_BUF_NUM_PACKETS)

// Restart Ping mode
    #define _4P_REPLY_RESTART_PING_CMD       0xFFu  // Sent x 4 by any Player to change from Transmission(Xfer) to Ping mode
    #define _4P_RESTART_PING_INDICATOR       0xFFu  // Received x 4 by *any* (non-sending?) Players indicating a change from Transmission(Xfer) back to Ping mode
    #define _4P_RESTART_PING_COUNT_DONE      4u     // How many consecutive 0xFF in a row triggers the switch from Ping to Transmission(Xfer) mode
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

void four_player_set_xfer_data(uint8_t tx_byte);
uint8_t four_player_rx_buf_get_num_packets_ready(void);
void four_player_rx_buf_remove_n_packets(uint8_t packet_count_to_remove);

extern uint8_t _4p_mode;
extern uint8_t _4p_connect_status;
extern uint8_t _4P_xfer_tx_data;

extern uint8_t _4p_rx_buf[RX_BUF_SZ];
extern uint8_t * _4p_rx_buf_READ_ptr;
extern uint8_t   _4p_rx_buf_count;
extern const uint8_t * _4p_rx_buf_end_wrap_addr;

extern uint8_t packet_counter;
extern uint8_t sio_counter;

// PLAYER_ID_BIT should be one of _4P_PLAYER_1/2/3/4
#define IS_PLAYER_CONNECTED(PLAYER_ID_BIT)  (_4p_connect_status & PLAYER_ID_BIT)
#define WHICH_PLAYER_AM_I()                 (_4p_connect_status & _4P_PLAYER_ID_MASK)
#define IS_PLAYER_DATA_READY()              (four_player_rx_buf_get_num_packets_ready() != 0u)
#define GET_CURRENT_MODE()                  (_4p_mode)

// Due to how the buffer is set up and used, we can assume
// the READ pointer won't overflow at any time during a single packet read,
// and so can consolidate the buffer-end wraparound test and
// count increment to the end of the packet.
inline void _4p_rx_buf_packet_increment_read_ptr(void) {
    _4p_rx_buf_READ_ptr += RX_BUF_PACKET_SZ;
    if (_4p_rx_buf_READ_ptr == _4p_rx_buf_end_wrap_addr)
        _4p_rx_buf_READ_ptr = _4p_rx_buf;
}

#endif // _4_PLAYER_ADATER_H
