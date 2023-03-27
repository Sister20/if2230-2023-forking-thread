#include "lib-header/keyboard.h"
#include "lib-header/portio.h"
#include "lib-header/framebuffer.h"
#include "lib-header/stdmem.h"
#include "lib-header/stdtype.h"

const char keyboard_scancode_1_to_ascii_map[256] = {
      0, 0x1B, '1', '2', '3', '4', '5', '6',  '7', '8', '9',  '0',  '-', '=', '\b', '\t',
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',  'o', 'p', '[',  ']', '\n',   0,  'a',  's',
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',   0, '\\',  'z', 'x',  'c',  'v',
    'b',  'n', 'm', ',', '.', '/',   0, '*',    0, ' ',   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0, '-',    0,    0,   0,  '+',    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
      0,    0,   0,   0,   0,   0,   0,   0,    0,   0,   0,    0,    0,   0,    0,    0,
};

static struct KeyboardDriverState keyboard_state = {
    .read_extended_mode = FALSE,
    .keyboard_input_on = FALSE,
    .buffer_index = 0x0,
    .keyboard_buffer = {},
};

/* -- Driver Interfaces -- */

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void){
    activate_keyboard_interrupt();
    keyboard_state.keyboard_input_on = TRUE;
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void){
    keyboard_state.keyboard_input_on = FALSE;
}

// Get keyboard buffer values - @param buf Pointer to char buffer, recommended size at least KEYBOARD_BUFFER_SIZE
void get_keyboard_buffer(char *buf){
    memcpy(buf, keyboard_state.keyboard_buffer, KEYBOARD_BUFFER_SIZE);
}

// Check whether keyboard ISR is active or not - @return Equal with keyboard_input_on value
bool is_keyboard_blocking(void){
    return keyboard_state.keyboard_input_on;
}

/* -- Keyboard Interrupt Service Routine -- */

/**
 * Handling keyboard interrupt & process scancodes into ASCII character.
 * Will start listen and process keyboard scancode if keyboard_input_on.
 * 
 * Will only print printable character into framebuffer.
 * Stop processing when enter key (line feed) is pressed.
 * 
 * Note that, with keyboard interrupt & ISR, keyboard reading is non-blocking.
 * This can be made into blocking input with `while (is_keyboard_blocking());` 
 * after calling `keyboard_state_activate();`
 */
void keyboard_isr(void) {
    if (!is_keyboard_blocking()){
      keyboard_state.buffer_index = 0;
    } else {
        uint8_t  scancode    = in(KEYBOARD_DATA_PORT);
        char     mapped_char = keyboard_scancode_1_to_ascii_map[scancode];
        // TODO : Implement scancode processing

        // Getting position cursor
        uint16_t position_cursor = get_cursor_position();
        uint8_t row = position_cursor / 80;
        uint8_t col = position_cursor % 80;

        // Validate if its a printable character + avoid break interrupt
        if(mapped_char != '\0'){
          // If backspace is pressed
          if(mapped_char == '\b'){
            // Getting current framebuffer address
            volatile uint16_t *location;
            location = (volatile uint16_t *)MEMORY_FRAMEBUFFER + (row * 80 + col);

            // If buffer index hasn't reached 0
            if(keyboard_state.buffer_index != 0){
              // Remove buffer element
              keyboard_state.buffer_index--;
              keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = 0x00;

              // Make sure to delete the previous printable character
              while(!(*location & 0x00FF)){
                if(col > 0){
                  col--;
                } else if(row > 0){
                  row--;
                  col = 79;
                } else{
                  break;
                }
                location--;
              }

              // Delete output and move cursor backwards
              framebuffer_write(row, col, 0x00, 0x7, 0x0);
              framebuffer_set_cursor(row, col);
            }
          } else if (mapped_char == '\n'){
            // If enter is pressed
            if(row < 24){
              // Move cursor to newline
              framebuffer_set_cursor(row + 1, 0);
            }

            // Deactivate keyboard input
            keyboard_state_deactivate();
          } else {
            // If any other printable character is pressed

            // Save to buffer and write to framebuffer
            keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = mapped_char;
            framebuffer_write(row, col, mapped_char, 0xF, 0);

            // Increment keyboard buffer index
            keyboard_state.buffer_index++;

            // Handle wrapping behaviour
            if(col < 79){
              framebuffer_set_cursor(row, col + 1);
            } else if(row < 24) {
              framebuffer_set_cursor(row + 1, 0);
            } else {
              // Do Nothing
            }
          }
        }
    }
    pic_ack(IRQ_KEYBOARD);
}
