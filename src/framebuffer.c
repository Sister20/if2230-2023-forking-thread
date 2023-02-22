#include "lib-header/framebuffer.h"
#include "lib-header/stdtype.h"
#include "lib-header/stdmem.h"
#include "lib-header/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // Set cursor to specified location. Row and column starts from 0
    uint16_t pos = r * 80 + c;
	out(0x3D4, 0x0F);
	out(0x3D5, (uint8_t) (pos & 0xFF));
	out(0x3D4, 0x0E);
	out(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // Set framebuffer character and color with corresponding parameter values.
    uint16_t attrib = (bg << 4) | (fg & 0x0F);
    volatile uint16_t * location;
    location = (volatile uint16_t *)MEMORY_FRAMEBUFFER + (row * 80 + col);
    *location = c | (attrib << 8);
}

void framebuffer_clear(void) {
    // Set all cell in framebuffer character to 0x00 (empty character)
    // and color to 0x07 (gray character & black background)
    for(int i = 0; i < 25; i++){
        for(int j = 0; j < 80; j++){
            framebuffer_write(i, j, 0x00, 0x7, 0x0);
        }
    }
}
