#include "display.h"

// Buffer de imagen 128x64 = 1024 bytes
static unsigned char buffer[1024];

static unsigned char cursorX = 0;
static unsigned char cursorY = 0;

// Variables para gestionar el ingreso del PIN
char pinBuffer[PIN_LENGTH + 1]; // +1 para el terminador nulo
unsigned char pinIndex = 0;

/********************************************
 *  COMUNICACIÓN BÁSICA CON SSD1306
 ********************************************/
static void ssd1306_cmd(unsigned char cmd){
    unsigned char data[2] = {0x00, cmd};     // 0x00 = comando
    i2cWrite(I2C0, SSD1306_I2C_ADDR, data, 2, TRUE);
}

static void ssd1306_data(unsigned char *data, uint16_t len){
    unsigned char temp[len+1];
    temp[0] = 0x40;                    // 0x40 = datos
    memcpy(&temp[1], data, len);
    i2cWrite(I2C0, SSD1306_I2C_ADDR, temp, len+1, TRUE);
}

/********************************************
 *  INICIALIZACIÓN
 ********************************************/
void display_init(void){

    i2cInit(I2C0, 100000);  // 100 kHz recomendado
    delay(100);

    ssd1306_cmd(0xAE);
    ssd1306_cmd(0x20);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0xB0);
    ssd1306_cmd(0xC8);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0x10);
    ssd1306_cmd(0x40);
    ssd1306_cmd(0x81);
    ssd1306_cmd(0x7F);
    ssd1306_cmd(0xA1);
    ssd1306_cmd(0xA6);
    ssd1306_cmd(0xA8);
    ssd1306_cmd(0x3F);
    ssd1306_cmd(0xD3);
    ssd1306_cmd(0x00);
    ssd1306_cmd(0xD5);
    ssd1306_cmd(0x80);
    ssd1306_cmd(0xD9);
    ssd1306_cmd(0x22);
    ssd1306_cmd(0xDA);
    ssd1306_cmd(0x12);
    ssd1306_cmd(0xDB);
    ssd1306_cmd(0x20);
    ssd1306_cmd(0x8D);
    ssd1306_cmd(0x14);
    ssd1306_cmd(0xAF);

    display_clear();
    display_update();

    // Limpiamos buffer inicial
    memset(pinBuffer, 0, sizeof(pinBuffer));

}

/********************************************
 *  LIMPIEZA DE PANTALLA
 ********************************************/
void display_clear(void){
    for(int i=0; i<1024; i++)
        buffer[i] = 0x00;
}

/********************************************
 *  ACTUALIZAR PANTALLA
 ********************************************/
void display_update(void){
    for(unsigned char page = 0; page < 8; page++){
        ssd1306_cmd(0xB0 + page);
        ssd1306_cmd(0x00);
        ssd1306_cmd(0x10);

        ssd1306_data(&buffer[page * 128], 128);
    }
}

/********************************************
 *  FUENTE 6x8
 ********************************************/
static const unsigned char font6x8[][6] = {
#include "font6x8.inc"
};

/********************************************
 *  TEXTO
 ********************************************/
static void display_setCursor(unsigned char x, unsigned char y){
    cursorX = x;
    cursorY = y;
}

static void display_print(const char *str){
    while(*str){
        char c = *str;
        if(c < 32 || c > 126) c = '?';

        for(unsigned char i=0; i<6; i++)
            buffer[cursorX + (cursorY/8)*128 + i] = font6x8[c-32][i];

        cursorX += 6;
        str++;
    }
}

// Esta funcion SOLO ubica y escribe en buffer, no refresca
void display_printAt(const char *str, unsigned char x, unsigned char y){
    display_setCursor(x, y);
    display_print(str);
}

//Esta funcion escribe centrado
void display_println(const char *str, unsigned char y){
    unsigned char len = 0;
    while(str[len]) len++; // Calculamos longitud del string manualmente o con strlen(str)

    // Ancho de pantalla 128px. Cada caracter son 6px de ancho.
    // Calculamos la posición X inicial
    int16_t x = (128 - (len * 6)) / 2;

    if(x < 0) x = 0; // Protección por si el texto es más ancho que la pantalla

    display_setCursor((unsigned char)x, y);
    display_print(str);
}
