#ifndef _PRINT_H
#define _PRINT_H

#define PRINT_BKG 0
#define PRINT_WIN 1

void print_gotoxy(uint8_t x, uint8_t y, uint8_t target);
void print_str(const char * txt);

#endif // _PRINT_H
