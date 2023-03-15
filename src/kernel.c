#include "lib-header/stdtype.h"
#include "lib-header/stdmem.h"
#include "lib-header/gdt.h"
#include "lib-header/framebuffer.h"
#include "lib-header/kernel_loader.h"

void kernel_setup(void)
{
    enter_protected_mode(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    __asm__("int $0x4");
    while (TRUE);

    // enter_protected_mode(&_gdt_gdtr);
    // framebuffer_clear();
    // framebuffer_write_row(3, 8, "Hai!", 0, 0xF);
    // framebuffer_write_row(4, 8, "We are", 0, 0xF);
    // framebuffer_write_row(5, 8, "Michael Jonathan H.", 0xF, 0);
    // framebuffer_write_row(6, 8, "Fatih N.R.I.", 0xF, 0);
    // framebuffer_write_row(7, 8, "Johannes Lee", 0xF, 0);
    // framebuffer_write_row(8, 8, "Hari Sudewa", 0xF, 0);
    // framebuffer_write_row(9, 8, "Johann Christan Kandani", 0xF, 0);
    // framebuffer_write_row(10, 8, "And we'll ready to be forkin' all over the place", 0, 0xF);
    // framebuffer_set_cursor(3, 7);
    // while (TRUE)
        ;
}