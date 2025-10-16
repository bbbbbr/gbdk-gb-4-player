#include <gbdk/platform.h>
#include <stdint.h>
#include <stdbool.h>

#include "common.h"
#include "input.h"

#include "magic_code.h"

const uint8_t magic_code_seq[] = {J_UP, J_UP, J_DOWN, J_DOWN, J_LEFT, J_RIGHT, J_LEFT, J_RIGHT, J_B, J_A};
uint8_t magic_code_state = MAGIC_CODE_STATE_RESET;


void magic_code_reset(void) {
    magic_code_state = MAGIC_CODE_STATE_RESET;
}


bool magic_code_update(void) {

    // Only check button input if the mode has not already been activated
    if (magic_code_state != MAGIC_CODE_STATE_ACTIVATED) {

        // Check to see if the next button press in the sequence
        // happened during the current frame
        if ( KEY_TICKED( magic_code_seq[ magic_code_state ] ) ) {
            magic_code_state++;

            // If the end of the sequence has been reached
            // then set the code as activated and play a sound
            if (magic_code_state == ARRAY_LEN(magic_code_seq)) {

                magic_code_state = MAGIC_CODE_STATE_ACTIVATED;
                return true;
            }
        }
    }
    return false;
}


void magic_code_sound_chime(void) {

    NR52_REG = 0x80u; // Enables sound, always set this first
    NR51_REG = 0xFFu; // Enables all channels (left and right)
    NR50_REG = 0x77u; // Max volume    

    NR10_REG = 0x55u;
    NR11_REG = 0x80u;
    NR12_REG = 0xF7u;
    NR13_REG = 0x02u;
    NR14_REG = 0x86u;
}