#include "lib-header/stdtype.h"
#include "lib-header/stdmem.h"
#include "lib-header/gdt.h"
#include "lib-header/framebuffer.h"
#include "lib-header/kernel_loader.h"
#include "lib-header/interrupt.h"
#include "lib-header/idt.h"
#include "lib-header/keyboard.h"
#include "lib-header/cmosrtc.h"
#include "lib-header/disk.h"

void kernel_setup(void)
{
    enter_protected_mode(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_fat32();
    keyboard_state_activate();

    uint32_t FTtime = get_FTTimestamp_time();
    char time[32];
    int i;
    for (i = 0; i < 32; i++)
    {
        time[0] = (FTtime & 1);
        FTtime = FTtime >> 1;
    }
    framebuffer_write_row(0,0,time,0,0);
}