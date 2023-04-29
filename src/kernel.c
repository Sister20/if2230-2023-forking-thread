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

void kernel_setup(void)
{
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
    allocate_single_user_page_frame((uint8_t *)0);

    // Initiation for testing
    struct FAT32DriverRequest req = {
        .buf = (uint8_t *)0,
        .name = "f1\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size = 0,
    };
    write(req);

    memcpy(req.name, "f2", 2);
    write(req);

    memcpy(req.name, "f3", 2);
    write(req);

    // move parent to f1
    req.parent_cluster_number = 0xa;

    memcpy(req.name, "f4", 2);
    write(req);

    memcpy(req.name, "f5", 2);
    write(req);

    // move parent to f1
    req.parent_cluster_number = 0xd;

    memcpy(req.name, "f6", 2);
    write(req);

    req.parent_cluster_number = 0xf;
    memcpy(req.name, "f7", 2);
    write(req);

    // add new file
    memcpy(req.name, "file1", 5);
    memcpy(req.ext, "txt", 3);
    struct ClusterBuffer cl = {
        .buf = "freestar freestar Lorem ipsum dolor sit amet, consectetur adipiscing elit.Ut efficitur dui magna, nec tincidunt risus malesuada sit amet.Nam ligula mi, lacinia sed ornare sit amet, imperdiet ac sem.Nunc ac sapien dignissim, mollis augue sed, gravida dolor.Aenean blandit libero et massa gravida, eu tristique urna congue.Aenean quis tempor nisl, efficitur efficitur ligula.Sed consectetur iaculis risus et tempor.Aenean fringilla consectetur urna,        ut dapibus est rhoncus nec.Cras accumsan ut justo vel hendrerit.Duis imperdiet quam ac malesuada hendrerit.Morbi eget tortor faucibus,elementum elit sit amet, blandit nunc.Maecenas rutrum facilisis lorem, in ultricies nisl molestie et.Proin sit amet ipsum at mi luctus pellentesque dapibus quis mi.Integer aliquet velit sit amet odio aliquet posuere.Ut ultrices ac magna fermentum malesuada.Proin nec rhoncus enim.Etiam id metus id nibh convallis venenatis.Nunc quis purus vulputate sem volutpat porttitor et non metus.Vestibulum commodo luctus neque ut congue.Etiam consectetur urna ut lectus maximus rhoncus.Vivamus a leo ut nisl feugiat pulvinar eget et diam.Nullam vitae ultrices arcu.Proin non sapien quis velit vulputate congue.Nulla pellentesque feugiat tempor.",
    };
    req.buf = &cl;
    req.buffer_size = CLUSTER_SIZE;
    req.parent_cluster_number = ROOT_CLUSTER_NUMBER;
    write(req);

    // Write shell into memory
    struct FAT32DriverRequest request = {
        .buf = (uint8_t *)0,
        .name = "shell",
        .ext = "\0\0\0",
        .parent_cluster_number = ROOT_CLUSTER_NUMBER,
        .buffer_size = 0x100000,
    };
    read(request);

    // Set TSS $esp pointer and jump into shell
    set_tss_kernel_current_stack();
    kernel_execute_user_program((uint8_t *)0);

    while (TRUE)
        ;
}
