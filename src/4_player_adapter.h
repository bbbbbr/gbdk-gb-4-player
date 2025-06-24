#ifndef _4_PLAYER_ADATER_H
#define _4_PLAYER_ADATER_H

#include "common.h"


#define SIO_KEEPALIVE_RESET   60u
#define SIO_KEEPALIVE_TIMEOUT 0u

// #define _4P_XFER_SZ 1 // Use 1 Byte as total data size in Transmission(Xfer) mode
#define _4P_XFER_SZ  2

// #if (_4P_XFER_SZ  > 1)
//     #error "COMPILE FAIL: _4P_XFER_SZ TX data size larger than 1 byte not supported, to do that add tx buffer support in state _4P_XFER_STATE_TX_LOCAL_DATA of sio_handle_mode_xfer()"
// #endif

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
    // DMG-07 Bits-Per-Second --> 4194304 / ((6 * RATE) + 512)
    // 0x00 = Fastest, 0xFF = slowest
    #define _4P_REPLY_PING_SPEED      0x00u
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

// Restart Ping mode
    #define _4P_REPLY_RESTART_PING_CMD       0xFFu  // Sent x 4 by any Player to change from Transmission(Xfer) to Ping mode
    #define _4P_RESTART_PING_INDICATOR       0xFFu  // Received x 4 by *any* (non-sending?) Players indicating a change from Transmission(Xfer) back to Ping mode
    #define _4P_RESTART_PING_COUNT_DONE      4u     // How many consecutive 0xFF in a row triggers the switch from Ping to Transmission(Xfer) mode
    #define _4P_RESTART_PING_COUNT_RESET     0u


// Ping mode atates
    enum {                           // Given State N what reply byte should be loaded for the *NEXT* transfer?
        _4P_PING_STATE_WAIT_HEADER,
        _4P_PING_STATE_HEADER0,      // Load reply Ack1
        _4P_PING_STATE_STATUS1,      // Load reply Ack2
        _4P_PING_STATE_STATUS2,      // Load reply Speed
        _4P_PING_STATE_STATUS3,      // Load reply Packet Size
    };

// Switching to Transmission(Xfer) mode states
    enum {
        _4P_START_XFER_STATE_WAIT_HEADER,
        _4P_START_XFER_STATE_SENDING1, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_START_XFER_STATE_SENDING2, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_START_XFER_STATE_SENDING3, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_START_XFER_STATE_SENDING4, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
    };

// Transmission(Xfer) modes
// No sub-states for this mode, just synchronous RX and TX (where TX data will become available during the *NEXT* RX set)
    enum {
        _4P_XFER_STATE_TX_LOCAL_DATA,  // TODO: During this stage are all RX Bytes 0?
        _4P_XFER_STATE_TX_PADDING,     // TODO: During this stage are all RX Bytes 0?
        _4P_XFER_STATE_RX_REMOTE_DATA, // TODO: During this stage should all tx bytes be 0?
    };

// Reset from Ping back to Transmission(Xfer) mode states
    enum {
        _4P_RESTART_PING_STATE_SENDING1, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_RESTART_PING_STATE_SENDING2, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_RESTART_PING_STATE_SENDING3, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
        _4P_RESTART_PING_STATE_SENDING4, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA)
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
void four_player_log(void);
void four_player_request_change_to_xfer_mode(void);
void four_player_restart_ping_mode(void);

void four_player_set_xfer_data(uint8_t tx_byte);
void four_player_claim_active_sio_buffer_for_main(void);  // TODO: May be better as a queue count instead of bool, in cases where there are 2x buffers ready and filled?


extern uint8_t _4p_mode;
extern uint8_t _4p_connect_status;
extern uint8_t _4P_xfer_tx_data;
extern uint8_t _4p_xfer_rx_buf_sio_active;
extern uint8_t _4p_xfer_rx_buf[2][_4P_XFER_RX_SZ];
// extern uint8_t _4p_xfer_rx_buf[_4P_XFER_RX_SZ];
// extern uint8_t _4p_xfer_rx_buf_alt[_4P_XFER_RX_SZ];
extern bool    _4p_xfer_rx_buf_ready_for_main[2];

extern uint8_t packet_counter;
extern uint8_t sio_counter;

// PLAYER_ID_BIT should be one of _4P_PLAYER_1/2/3/4
#define IS_PLAYER_CONNECTED(PLAYER_ID_BIT)  (_4p_connect_status & PLAYER_ID_BIT)
#define WHICH_PLAYER_AM_I()                 (_4p_connect_status & _4P_PLAYER_ID_MASK)
#define IS_PLAYER_DATA_READY()              (_4p_xfer_rx_buf_ready_for_main[_4p_xfer_rx_buf_sio_active] == true)
#define GET_CURRENT_MODE()                  (_4p_mode)
// #define GET_INACTIVE_RX_BUF()               (!_4p_xfer_rx_buf_active)  // Use the OPPOSITE one that SIO is currently using


#endif // _4_PLAYER_ADATER_H
