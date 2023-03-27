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

static struct FAT32DriverState driver_state;

uint32_t cluster_to_lba(uint32_t cluster) { return cluster * CLUSTER_BLOCK_COUNT + BOOT_SECTOR; }

void create_fat32(void)
{
    // Copy fs_signature
    write_blocks(fs_signature, BOOT_SECTOR, 1);

    // Copy reserved clusters
    driver_state.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
    driver_state.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
    driver_state.fat_table.cluster_map[2] = FAT32_FAT_END_OF_FILE;

    // Empty the remaining entry in the fat table
    for (int i = 3; i < CLUSTER_MAP_SIZE; i++)
    {
        driver_state.fat_table.cluster_map[i] = 0;
    }
    write_clusters(&driver_state.fat_table, 1, 1);
}

void initialize_filesystem_fat32(void)
{
    if (is_empty_storage())
    {
        create_fat32();
        return;
    }

    // Move the FAT table from storage to the driver state
    read_clusters(&driver_state.fat_table, 1, 1);
}

bool is_empty_storage()
{
    uint8_t boot_sector[BLOCK_SIZE];
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return !(memcmp(boot_sector, fs_signature, BLOCK_SIZE) == 0);
}

void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    uint32_t logical_block_address = cluster_to_lba(cluster_number);
    uint8_t block_count = cluster_count * CLUSTER_BLOCK_COUNT;
    write_blocks(ptr, logical_block_address, block_count);
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count)
{
    uint32_t logical_block_address = cluster_to_lba(cluster_number);
    uint8_t block_count = cluster_count * CLUSTER_BLOCK_COUNT;
    write_blocks(ptr, logical_block_address, block_count);
}

void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name, uint32_t parent_dir_cluster)
{
    memcpy(dir_table->table[0].name, name, 8);
    dir_table->table[0].cluster_high = parent_dir_cluster >> 16;
    dir_table->table[0].cluster_low = parent_dir_cluster & 0xFFFF;
}
