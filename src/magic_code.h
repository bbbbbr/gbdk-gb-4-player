#ifndef _MAGIC_CODE_H
#define _MAGIC_CODE_H


#define MAGIC_CODE_STATE_RESET     0x00u
// NOTE: There are multiple counter states between
//       these two upper and lower bound #defines
#define MAGIC_CODE_STATE_ACTIVATED 0xFFu // This should be higher than the index of the last button sequence entry

extern uint8_t magic_code_state;

#define IS_MAGIC_CODE_ACTIVE (magic_code_state == MAGIC_CODE_STATE_ACTIVATED)

void magic_code_reset(void);
bool magic_code_update(void);
void magic_code_sound_chime(void);

#endif // _MAGIC_CODE_H