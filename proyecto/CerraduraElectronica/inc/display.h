#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "sapi.h"
#include "teclado.h"
#include "MEF.h"
#include "teclado.h"
#include <string.h>

#define SSD1306_I2C_ADDR 0x3C

extern unsigned char pinIndex;

void display_init(void);
void display_clear(void);
void display_update(void);

// ... (otros includes)
void display_printAt(const char *str, unsigned char x, unsigned char y);
void display_println(const char *str, unsigned char y);

#endif
