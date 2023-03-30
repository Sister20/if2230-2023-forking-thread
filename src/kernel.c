#include "lib-header/disk.h"
#include "lib-header/fat32.h"
#include "lib-header/framebuffer.h"
#include "lib-header/gdt.h"
#include "lib-header/idt.h"
#include "lib-header/interrupt.h"
#include "lib-header/kernel_loader.h"
#include "lib-header/keyboard.h"
#include "lib-header/stdmem.h"
#include "lib-header/stdtype.h"

void kernel_setup(void) {
  enter_protected_mode(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  activate_keyboard_interrupt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  initialize_filesystem_fat32();
  keyboard_state_activate();

  struct ClusterBuffer cbuf[5];
  for (uint32_t i = 0; i < 5; i++)
    for (uint32_t j = 0; j < CLUSTER_SIZE; j++)
      cbuf[i].buf[j] = i + 'a';

  struct FAT32DriverRequest request = {
      .buf = cbuf,
      .name = "ikanaide",
      .ext = "\0\0\0",
      .parent_cluster_number = ROOT_CLUSTER_NUMBER + 1,
      .buffer_size = 5,
  };

  int temp = write(request); // Create folder

  if (temp == 0) {};
  // memcpy(request.name, "bruh", 8);
  // memcpy(request.ext, "uwu", 3);
  // request.parent_cluster_number = ROOT_CLUSTER_NUMBER + 1;
  // request.buffer_size = 9 * CLUSTER_SIZE;
  // framebuffer_write(0, 0, write(request) + 97, 0x7, 0x0);

  while (TRUE);
}
