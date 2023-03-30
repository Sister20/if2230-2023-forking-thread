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

    struct ClusterBuffer cbuf[5];
    for (uint32_t i = 0; i < 5; i++)
        for (uint32_t j = 0; j < CLUSTER_SIZE; j++)
            cbuf[i].buf[j] = i + 'a';

    struct FAT32DriverRequest request = {
        .buf = cbuf,
        .name = "ikanaide",
        .ext = "uwu",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size = 0,
    };

    // write(request); // Create folder "ikanaide"
    // memcpy(request.name, "kano1\0\0\0", 8);
    // write(request); // Create folder "kano1"

    // memcpy(request.name, "bruh", 8);
    // memcpy(request.ext, "uwu", 3);
    // request.parent_cluster_number = ROOT_CLUSTER_NUMBER + 1;
    // request.buffer_size = 9 * CLUSTER_SIZE;
    // framebuffer_write(0, 0, write(request) + 97, 0x7, 0x0);

    // delete (request);

    request.parent_cluster_number = ROOT_CLUSTER_NUMBER;
    request.buffer_size = CLUSTER_SIZE * 2;

    // bool once_failed_to_write = FALSE;

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
        memcpy(request.name, name, 8);
        uint8_t ret = write(request);
        if (ret != 0)
        {
            // once_failed_to_write = TRUE;
            framebuffer_write(0, 0, 'F', 0x7, 0x0);
            framebuffer_write(1, 0, i + 33, 0x7, 0x0);
            framebuffer_write(3, 0, ret + 97, 0x7, 0x0);
            break;
        }
    }

    // memcpy(request.name, "ikanaide", 8);
    // memcpy(request.ext, "\0\0\0", 3);
    // // delete (request); // Delete first folder, thus creating hole in FS

    // memcpy(request.name, "daijoubu", 8);
    // request.buffer_size = 5 * CLUSTER_SIZE;
    // write(request); // Create fragmented file "daijoubu"

    // struct ClusterBuffer readcbuf;
    // read_clusters(&readcbuf, ROOT_CLUSTER_NUMBER + 1, 1);
    // // If read properly, readcbuf should filled with 'a'

    // request.buffer_size = CLUSTER_SIZE;
    // read(request); // Failed read due not enough buffer size
    // request.buffer_size = 5 * CLUSTER_SIZE;
    // read(request); // Success read on file "daijoubu"

    // memcpy(request.name, "kano1\0\0\0", 8);
    // memcpy(request.ext, "\0\0\0", 3);
    // delete (request);

    // memcpy(request.name, "bruh", 8);
    // memcpy(request.ext, "\0\0\0", 3);
    // request.parent_cluster_number = ROOT_CLUSTER_NUMBER + 1;
    // request.buffer_size = 9 * CLUSTER_SIZE;
    // framebuffer_write(0, 0, write(request) + 97, 0x7, 0x0);
    while (TRUE)
        ;
}