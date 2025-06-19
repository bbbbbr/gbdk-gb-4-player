#ifndef _4_PLAYER_ADATER_H
#define _4_PLAYER_ADATER_H

#include "common.h"

// #define SIO_BUF_SZ 16u
#define SIO_BUF_SZ 256u

#define SIO_WATCHDOG_RESET     60u
#define SIO_WATCHDOG_TRIGGERED 0u

#define _4P_XFER_SZ  1u  // 1 Byte replies in Transfer phase

#define PLAYER_NUM_MIN 1u
#define PLAYER_NUM_MAX 4u

// 4-Player adapter communication states
enum {
    _4P_STATE_ENUMERATE,   // Initial state where connection is determined ("Ping")
    _4P_STATE_START_XFER,  // Transitional Enumeration state where start sequence is sent to switch the DMG-07 into Transfer state
    _4P_STATE_XFER,        // Operational state data packets are broadcast ("Transmission")
};


// Enumeration Phase
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
//     Game Boy RX       What Game Boy should load to SB Reg for next transfer
//     ----------------------------
//     HEADER     -->    ACK1 = 0x88   (HEADER BYTE == 0xFE)
//     STATUS_1   -->    ACK2 = 0x88   
//     STATUS_2   -->    RATE = Link Cable Speed 
//     STATUS_3   -->    SIZE = Packet Size


// Enumeration Data
    #define _4P_ENUM_HEADER          0xFEu

    #define _4P_REPLY_ENUM_ACK1      0x88u
    #define _4P_REPLY_ENUM_ACK2      0x88u
    // SPEED:
    // DMG-07 Bits-Per-Second --> 4194304 / ((6 * RATE) + 512)
    // 0x00 = Fastest, 0xFF = slowest
    #define _4P_REPLY_ENUM_SPEED 0x00u
    // SIZE sets the length of packets exchanged between all Game Boys. Nothing fancy, just the number of bytes in each packet. It probably shouldn’t be set to zero.
    #define _4P_REPLY_ENUM_PAKCET_SZ (_4P_XFER_SZ) // Reply

// Start XFer Data
    #define _4P_REPLY_START_XFER_CMD       0xAAu  // Sent x 4 by Player 1 to change from Enumeration to Transfer state
    #define _4P_START_XFER_INDICATOR       0xCCu  // Received x 4 by Players 2-4 indicating a change from Enumeration to Transfer state
    #define _4P_START_XFER_COUNT_THRESHOLD 4u     // How many consecutive received 0xCC in a row triggers the switch from Enumeration to Transfer state
    #define _4P_START_XFER_COUNT_RESET     0u 

// Restart Enumeration Data
    #define _4P_REPLY_RESTART_ENUM_CMD       0xFFu  // Sent x 4 by Player 1 to change from Transfer to Enumeration state
    //#define _4P_RESTART_ENUM_INDICATOR            // ?? TODO: Not sure if an indicator exists for Players 2-4 of the mode change back
    #define _4P_RESTART_ENUM_COUNT_THRESHOLD 4u     // How many consecutive 0xFF in a row triggers the switch from Enumeration to Transfer state
    #define _4P_RESTART_ENUM_COUNT_RESET     0u     //


// Enumeration Packet States
    enum {                           // Given State N what reply byte should be loaded for the *NEXT* transfer?
        _4P_ENUM_STATE_WAIT_HEADER,
        _4P_ENUM_STATE_HEADER0,      // Load reply Ack1
        _4P_ENUM_STATE_STATUS1,      // Load reply Ack2
        _4P_ENUM_STATE_STATUS2,      // Load reply Speed
        _4P_ENUM_STATE_STATUS3,      // Load reply Packet Size
    };

// Switching mode states
    enum {
        _4P_START_XFER_STATE_WAIT_HEADER,        
        _4P_START_XFER_STATE_SENDING1, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA) 
        _4P_START_XFER_STATE_SENDING2, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA) 
        _4P_START_XFER_STATE_SENDING3, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA) 
        _4P_START_XFER_STATE_SENDING4, // Set once header is received, thereafter reply with 4 x _4P_REPLY_START_XFER_CMD (0xAA) 
    };

// Transfer states
    enum {
        _4P_XFER_STATE_SEND,        
        _4P_XFER_STATE_SEND_PADDING,        
        _4P_XFER_STATE_RECEIVE,        
    };


// Player connectivity bits
#define _4P_PLAYER_ID_MASK 7u
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
void four_player_request_change_to_transfer_mode(void);

// PLAYER_ID_BIT should be one of _4P_PLAYER_1/2/3/4
#define IS_PLAYER_CONNECTED(PLAYER_ID_BIT)  (_4p_connect_status & PLAYER_ID_BIT)
#define WHICH_PLAYER_AM_I()                 (_4p_connect_status & _4P_PLAYER_ID_MASK)

extern uint8_t _4p_connect_status;




#endif // _4_PLAYER_ADATER_H
