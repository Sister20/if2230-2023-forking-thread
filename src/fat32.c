#include "lib-header/fat32.h"
#include "lib-header/stdmem.h"
#include "lib-header/stdtype.h"
#include "lib-header/framebuffer.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C',
    'o',
    'u',
    'r',
    's',
    'e',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'D',
    'e',
    's',
    'i',
    'g',
    'n',
    'e',
    'd',
    ' ',
    'b',
    'y',
    ' ',
    ' ',
    ' ',
    ' ',
    ' ',
    'L',
    'a',
    'b',
    ' ',
    'S',
    'i',
    's',
    't',
    'e',
    'r',
    ' ',
    'I',
    'T',
    'B',
    ' ',
    ' ',
    'M',
    'a',
    'd',
    'e',
    ' ',
    'w',
    'i',
    't',
    'h',
    ' ',
    '<',
    '3',
    ' ',
    ' ',
    ' ',
    ' ',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '-',
    '2',
    '0',
    '2',
    '3',
    '\n',
    [BLOCK_SIZE - 2] = 'O',
    [BLOCK_SIZE - 1] = 'k',
};

uint32_t cluster_to_lba(uint32_t cluster) { return cluster * CLUSTER_BLOCK_COUNT; }

void create_fat32(void)
{
    framebuffer_write_row(0, 1, "Entering create FAT", 0xF, 0);
    write_blocks(fs_signature, BOOT_SECTOR, 1);
    framebuffer_write_row(0, 1, "Exiting create FAT", 0xF, 0);

    //   if (is_empty_storage()) {
    //     write_blocks(&fs_signature, BOOT_SECTOR, 1);
    //     return;
    //   }

    //
}

void initialize_filesystem_fat32(void)
{
    framebuffer_write_row(0, 1, "Entering initialization", 0xF, 0);
    create_fat32();
    framebuffer_write_row(0, 1, "Exiting initialization", 0xF, 0);
}

bool is_empty_storage()
{
    char buf[BLOCK_SIZE];
    read_blocks(buf, 0, 1);
    return !(memcmp(buf, fs_signature, BLOCK_SIZE) == 0);
}

void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    uint32_t logical_block_address = cluster_to_lba(cluster_number);
    uint8_t block_count = cluster_to_lba(cluster_count);
    write_blocks(ptr, logical_block_address, block_count);
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    uint32_t logical_block_address = cluster_to_lba(cluster_number);
    uint8_t block_count = cluster_to_lba(cluster_count);
    write_blocks(ptr, logical_block_address, block_count);
}