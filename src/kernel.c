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
        .buf                   = cbuf,
        .name                  = "ikanaide",
        .ext                   = "uwu",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size           = 0,
    } ;

    write(request);  // Create folder "ikanaide"
    memcpy(request.name, "kano1\0\0\0", 8);
    write(request);  // Create folder "kano1"
    memcpy(request.name, "ikanaide", 8);
    memcpy(request.ext, "\0\0\0", 3);
    delete(request); // Delete first folder, thus creating hole in FS

    memcpy(request.name, "daijoubu", 8);
    request.buffer_size = 5*CLUSTER_SIZE;
    write(request);  // Create fragmented file "daijoubu"

    struct ClusterBuffer readcbuf;
    read_clusters(&readcbuf, ROOT_CLUSTER_NUMBER+1, 1); 
    // If read properly, readcbuf should filled with 'a'

    request.buffer_size = CLUSTER_SIZE;
    read(request);   // Failed read due not enough buffer size
    request.buffer_size = 5*CLUSTER_SIZE;
    read(request);   // Success read on file "daijoubu"

    // Make request for new directory with chaining
    struct FAT32DriverRequest request2 = {
        .buf = cbuf,
        .name = "ikanaide",
        .ext = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size = 0,
    };

    int temp = write(request2);

    request2.parent_cluster_number = 9;
    request2.buffer_size = CLUSTER_SIZE * 5;

    memcpy(request2.ext, "uwu", 3);
    for (int i = 1; i <= 70; i++)
    {

        int name_number = i + 33;
        char name[8] = {
            name_number,
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
        };
        memcpy(request2.name, name, 8);
        uint8_t ret = write(request2);
        if (ret != 0)
        {
            framebuffer_write(0, 0, 'F', 0x7, 0x0);
            framebuffer_write(1, 0, i + 33, 0x7, 0x0);
            framebuffer_write(2, 0, ret + 97, 0x7, 0x0);
            break;
        }
    }
    
    request2.buffer_size = 5 * CLUSTER_SIZE;
    memcpy(request2.name, "ikanaide", 8);
    memcpy(request2.ext, "\0\0\0", 3);
    request2.parent_cluster_number = ROOT_CLUSTER_NUMBER;

    // Try to read directory
    temp = read_directory(request2);

    memcpy(request2.ext, "uwu", 3);
    request2.parent_cluster_number = 9;

    // Delete all files in directory with chaining
    for (int i = 1; i <= 70; i++)
    {

        int name_number = i + 33;
        char name[8] = {
            name_number,
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
            '\0',
        };
        memcpy(request2.name, name, 8);
        uint8_t ret = delete(request2);
        if (ret != 0)
        {
            framebuffer_write(0, 0, 'F', 0x7, 0x0);
            framebuffer_write(1, 0, i + 33, 0x7, 0x0);
            framebuffer_write(2, 0, ret + 97, 0x7, 0x0);
            break;
        }
    }

    request2.parent_cluster_number = ROOT_CLUSTER_NUMBER;

    // Try to delete directory with chaining
    memcpy(request2.ext, "\0\0\0", 3);
    memcpy(request2.name, "ikanaide", 8);
    
    delete(request2);
    while (TRUE);
}