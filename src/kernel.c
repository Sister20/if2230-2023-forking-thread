#include "lib-header/stdtype.h"
#include "lib-header/stdmem.h"
#include "lib-header/gdt.h"
#include "lib-header/framebuffer.h"
#include "lib-header/kernel_loader.h"
#include "lib-header/interrupt.h"
#include "lib-header/idt.h"
#include "lib-header/keyboard.h"
#include "lib-header/fat32.h"
#include "lib-header/disk.h"
#include "lib-header/paging.h"

void kernel_setup(void) {
    enter_protected_mode(&_gdt_gdtr);
    pic_remap();
    initialize_idt();
    activate_keyboard_interrupt();
    framebuffer_clear();
    framebuffer_set_cursor(0, 0);
    initialize_filesystem_fat32();
    gdt_install_tss();
    set_tss_register();

    // Allocate first 4 MiB virtual memory
    allocate_single_user_page_frame((uint8_t*) 0);

    // Initiation for testing
    struct FAT32DriverRequest req = {
        .buf                   = (uint8_t*) 0,
        .name                  = "f1\0\0\0\0\0\0",
        .ext                   = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = 0,
    };
    write(req);

    memcpy(req.name, "f2", 2);
    write(req);

    memcpy(req.name, "f3", 2);
    write(req);

    // move parent to f1
    req.parent_cluster_number = 0x9;
    
    memcpy(req.name, "f4", 2);
    write(req);

    memcpy(req.name, "f5", 2);
    write(req);

    // move parent to f1
    req.parent_cluster_number = 0xc;
    
    memcpy(req.name, "f6", 2);
    write(req);

    req.parent_cluster_number = 0xe;
    memcpy(req.name, "f7", 2);
    write(req);

    // Write shell into memory
    struct FAT32DriverRequest request = {
        .buf                   = (uint8_t*) 0,
        .name                  = "shell",
        .ext                   = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = 0x100000,
    };
    read(request);

    // Set TSS $esp pointer and jump into shell 
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t*) 0);

    while (TRUE);
}
