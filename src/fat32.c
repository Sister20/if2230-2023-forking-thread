#include "lib-header/fat32.h"
#include "lib-header/framebuffer.h"
#include "lib-header/stdmem.h"
#include "lib-header/stdtype.h"

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

uint32_t cluster_to_lba(uint32_t cluster)
{
  return cluster * CLUSTER_BLOCK_COUNT + BOOT_SECTOR;
}

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
    // Create FAT if it's empty
    create_fat32();
  }

  // Move the FAT table from storage to the driver state
  read_clusters(&driver_state.fat_table, 1, 1);
}

bool is_empty_storage()
{
  uint8_t boot_sector[BLOCK_SIZE];
  read_blocks(&boot_sector, BOOT_SECTOR, 1);
  return memcmp(boot_sector, fs_signature, BLOCK_SIZE);
}

void write_clusters(const void *ptr, uint32_t cluster_number,
                    uint8_t cluster_count)
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

void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name,
                          uint32_t parent_dir_cluster)
{
  memcpy(dir_table->table[0].name, name, 8);
  dir_table->table[0].cluster_high = parent_dir_cluster >> 16;
  dir_table->table[0].cluster_low = parent_dir_cluster & 0xFFFF;
}

int8_t read_directory(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
       i++)
  {
    struct FAT32DirectoryEntry *entry = &(driver_state.dir_table_buf.table[i]);

    // Check if the entry matches the requested name and is not empty
    if (entry->user_attribute == UATTR_NOT_EMPTY &&
        memcmp(entry->name, request.name, 8) == 0)
    {
      if (entry->attribute == ATTR_SUBDIRECTORY)
      {
        // The directory is a subdirectory
        if (request.buffer_size < entry->filesize)
        {
          // Size of the given buffer is not enough
          return -1;
        }
        else
        {
          // Enough size, read the cluster to the buffer
          read_clusters(request.buf, entry->cluster_high, 1);
          return 0;
        }
      }
      else
      {
        // Not a folder
        return 1;
      }
    }
  }

  // If no matching directory entry was found, return error
  return 2;
}

int8_t read(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
       i++)
  {
    struct FAT32DirectoryEntry *entry = &(driver_state.dir_table_buf.table[i]);

    // Check if the entry matches the requested name and attributes
    if (entry->user_attribute == UATTR_NOT_EMPTY &&
        memcmp(entry->name, request.name, 8) == 0 &&
        memcmp(entry->ext, request.ext, 3) == 0)
    {
      if (entry->attribute != ATTR_SUBDIRECTORY)
      {
        // Found a matching directory entry
        if (request.buffer_size < entry->filesize)
        {
          return -1;
        }
        else
        {
          uint16_t next_cluster_number = entry->cluster_low;
          do
          {
            read_clusters(request.buf + CLUSTER_SIZE * i, next_cluster_number,
                          1);
            next_cluster_number =
                driver_state.fat_table.cluster_map[next_cluster_number] &
                0x0000FFFF;
          } while (next_cluster_number != 0xFFFF);
          return 0;
        }
      }
      else
      {
        // Not a file
        return 1;
      }
    }
  }

  // If no matching directory entry was found, return error
  return 2;
}

int8_t write(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  bool checkEmpty = FALSE;
  uint32_t new_cluster_number = 0;

  // Iterate through the directory entries and find empty cluster
  for (int i = 0; i < CLUSTER_MAP_SIZE; i++)
  {
    // Check if the cluster empty
    if (driver_state.fat_table.cluster_map[i] == 0)
    {
      new_cluster_number = i;
      checkEmpty = TRUE;
      break;
    }
  }

  if (checkEmpty)
  {
    // Iterate through the directory entries and find the same folder/file
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
         i++)
    {
      struct FAT32DirectoryEntry *entry =
          &(driver_state.dir_table_buf.table[i]);
      if (entry->user_attribute == UATTR_NOT_EMPTY)
      {
        if (memcmp(entry->name, request.name, 8) == 0 &&
            memcmp(entry->ext, request.ext, 3) == 0)
        {
          return 1;
        }
      }
    }

    // Iterate through the directory entries and find empty entry
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
         i++)
    {
      struct FAT32DirectoryEntry *entry =
          &(driver_state.dir_table_buf.table[i]);
      if (entry->user_attribute != UATTR_NOT_EMPTY)
      {
        if (request.buffer_size == 0)
        {
          driver_state.fat_table.cluster_map[i] = FAT32_FAT_END_OF_FILE;
          memcpy(entry->name, request.name, 8);
          entry->filesize = request.buffer_size;
          entry->cluster_high = (uint16_t)new_cluster_number >> 16;
          entry->cluster_low = (uint16_t)new_cluster_number & 0x0000FFFF;
          entry->attribute = (uint8_t)ATTR_SUBDIRECTORY;
          entry->user_attribute = (uint8_t)UATTR_NOT_EMPTY;
          struct FAT32DirectoryTable new_directory;
          init_directory_table(&new_directory, request.name,
                               request.parent_cluster_number);
          write_clusters(&new_directory, new_cluster_number, 1);
          write_clusters(&driver_state.fat_table, 1, 1);
          return 0;
        }
        else
        {
          int threshold;
          if ((request.buffer_size / CLUSTER_SIZE) % 2 == 0)
          {
            threshold = request.buffer_size / CLUSTER_SIZE;
          }
          else
          {
            threshold = request.buffer_size / CLUSTER_SIZE + 1;
          }
          for (int i = 0; i < threshold; i++)
          {
            write_clusters(request.buf, new_cluster_number, 1);
            uint32_t old_cluster_number = new_cluster_number;
            for (int j = i + 1; j < CLUSTER_MAP_SIZE; j++)
            {
              // Check if the cluster empty
              if (driver_state.fat_table.cluster_map[j] == 0)
              {
                new_cluster_number = j;
                break;
              }
            }
            driver_state.fat_table.cluster_map[old_cluster_number] =
                new_cluster_number;
            memcpy(entry->name, request.name, 8);
            entry->filesize = request.buffer_size;
            entry->cluster_high = new_cluster_number >> 16;
            entry->cluster_low = new_cluster_number & 0x0000FFFF;
            entry->user_attribute = UATTR_NOT_EMPTY;
          }
          write_clusters(&driver_state.fat_table, 1, 1);
          return 0;
        }
      }
    }
  }

  // If no matching directory entry was found, return error
  return 2;
}
