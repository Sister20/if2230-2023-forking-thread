#include "lib-header/portio.h"
#include "lib-header/stdtype.h"

/** x86 inb/outb:
 * @param dx target port
 * @param al input/output byte
 */

void out(uint16_t port, uint8_t data) {
  __asm__("outb %0, %1"
          : // <Empty output operand>
          : "a"(data), "Nd"(port));
}

uint8_t in(uint16_t port) {
  uint8_t result;
  __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
  return result;
}

// just skeleteon
void out16(uint16_t port, uint16_t data) {
  __asm__("outb %0, %1"
          : // <Empty output operand>
          : "a"(data), "Nd"(port));
}

uint16_t in16(uint16_t port) {
  uint16_t result;
  __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
  return result;
}
