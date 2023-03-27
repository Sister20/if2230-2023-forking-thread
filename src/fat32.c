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
  read_blocks(ptr, logical_block_address, block_count);
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

    // Check if the entry matches the requested name and is not empty. If it's not satisfied, skip.
    if (!(is_directory_empty(entry) &&
          is_dir_name_same(entry, request)))
    {
      continue;
    }

    // Return error when entry is not a folder
    if (!is_subdirectory(entry))
    {
      return 1;
    }

    // Return error when the buffer size is insufficient
    if (request.buffer_size < entry->filesize)
    {
      return -1;
    }

    // Enough size, read the cluster to the buffer
    read_clusters(request.buf, entry->cluster_high, 1);
    return 0;
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

    // Check if the entry isn't empty and matches the requested name and attributes. If it's not satisfied, skip.
    if (!(!is_dir_empty(entry) && is_dir_ext_name_same(entry, request)))
    {
      continue;
    }

    // Return error when entry is a folder
    if (is_subdirectory(entry))
    {
      return 1;
    }
    // Return error when not enough buffer size
    if (request.buffer_size < entry->filesize)
    {
      return -1;
    }

    // Buffer size sufficient, reading the content
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

  // If no matching directory entry was found, return error
  return 2;
}

int8_t write(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  bool check_empty = FALSE;
  uint32_t new_cluster_number = 0;

  // Iterate through the directory entries and find empty cluster
  for (int i = 0; i < CLUSTER_MAP_SIZE && !check_empty; i++)
  {
    // Check if the cluster empty, if yes target the cluster
    check_empty = driver_state.fat_table.cluster_map[i] == 0;
    if (check_empty)
    {
      new_cluster_number = i;
    }
  }

  // If no matching directory entry was found, return error
  if (!check_empty)
  {
    return 2;
  }

  // Determine whether we're creating a file or a folder
  bool is_creating_directory = request.buffer_size == 0;

  // Iterate through the directory entries and find the same folder/file. Return early if file with the same name already exist.
  for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
       i++)
  {
    struct FAT32DirectoryEntry *entry =
        &(driver_state.dir_table_buf.table[i]);
    if (!is_dir_empty(entry) && (is_creating_directory ? is_dir_name_same(entry, request) : is_dir_ext_name_same(entry, request)))
    {
      return 1;
    }
  }

  // Iterate through the directory entries and find empty entry
  bool found_empty_entry = FALSE;
  uint8_t index_of_empty_entry = 0;
  for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) && !found_empty_entry;
       i++)
  {

    struct FAT32DirectoryEntry *entry =
        &(driver_state.dir_table_buf.table[i]);

    // Skip attempting to write if it's not empty
    found_empty_entry = is_dir_empty(entry);
    if (found_empty_entry)
    {
      index_of_empty_entry = i;
    }
  }

  if (!found_empty_entry)
  {
    return -1;
  }

  struct FAT32DirectoryEntry *target_entry =
      &(driver_state.dir_table_buf.table[index_of_empty_entry]);

  // Create a directory
  if (is_creating_directory)
  {
    driver_state.fat_table.cluster_map[new_cluster_number] = FAT32_FAT_END_OF_FILE;
    memcpy(target_entry->name, request.name, 8);
    target_entry->filesize = request.buffer_size;
    target_entry->cluster_high = (uint16_t)new_cluster_number >> 16;
    target_entry->cluster_low = (uint16_t)new_cluster_number & 0x0000FFFF;
    target_entry->attribute = (uint8_t)ATTR_SUBDIRECTORY;
    target_entry->user_attribute = (uint8_t)UATTR_NOT_EMPTY;
    struct FAT32DirectoryTable new_directory;
    init_directory_table(&new_directory, request.name,
                         request.parent_cluster_number);

    // Write the new directory into the cluster
    write_clusters(&new_directory, new_cluster_number, 1);

    // Update the FAT table in storage
    write_clusters(&driver_state.fat_table, 1, 1);

    // Update directory table of the parent
    write_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
    return 0;
  }

  // Create a file
  int required_clusters = request.buffer_size / CLUSTER_SIZE;
  required_clusters += required_clusters % 2 == 0 ? 0 : 1;

  for (int i = 0; i < required_clusters; i++)
  {
    write_clusters(request.buf, new_cluster_number, 1);
    uint32_t old_cluster_number = new_cluster_number;
    for (int j = i + 1; j < CLUSTER_MAP_SIZE; j++)
    {
      // Check if the cluster is empty
      if (driver_state.fat_table.cluster_map[j] == 0)
      {
        new_cluster_number = j;
        break;
      }
    }
    driver_state.fat_table.cluster_map[old_cluster_number] =
        new_cluster_number;
    memcpy(target_entry->name, request.name, 8);
    target_entry->filesize = request.buffer_size;
    target_entry->cluster_high = new_cluster_number >> 16;
    target_entry->cluster_low = new_cluster_number & 0x0000FFFF;
    target_entry->user_attribute = UATTR_NOT_EMPTY;
  }
  write_clusters(&driver_state.fat_table, 1, 1);
  return 0;
}

int8_t delete(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++)
  {
    struct FAT32DirectoryEntry *entry = &(driver_state.dir_table_buf.table[i]);

    // Check if the entry matches the requested name and attributes. If not, skip.
    if (!(!is_dir_empty(entry) && is_dir_ext_name_same(entry, request)))
    {
      continue;
    }
    // Found a matching directory entry, check if subdirectory empty or not
    if (is_subdirectory(entry))
    {
      struct FAT32DirectoryTable subdir_table;
      read_clusters(&subdir_table, entry->cluster_low, 1);
      for (uint8_t j = 1; j < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); j++)
      {
        struct FAT32DirectoryEntry *sub_entry = &(subdir_table.table[j]);
        // Check if the entry not empty
        if (!is_dir_empty(sub_entry))
        {
          return 2;
        }
      }

      // Folder is empty and can be deleted
      entry->user_attribute = 0;
      driver_state.fat_table.cluster_map[entry->cluster_low] = FAT32_FAT_END_OF_FILE;
      write_clusters(&driver_state.fat_table, 1, 1);
      return 0;
    }

    // Not a folder
    uint16_t now_cluster_number = entry->cluster_low;
    do
    {
      uint16_t next_cluster_number = driver_state.fat_table.cluster_map[now_cluster_number] & 0xFFFF;
      driver_state.fat_table.cluster_map[now_cluster_number] = 0;
      now_cluster_number = driver_state.fat_table.cluster_map[next_cluster_number] & 0xFFFF;
    } while (now_cluster_number != 0xFFFF);
    entry->user_attribute = 0;
    write_clusters(&driver_state.fat_table, 1, 1);
    return 0;
  }
  return 1;
}

bool is_dir_empty(struct FAT32DirectoryEntry *entry)
{
  return entry->user_attribute != UATTR_NOT_EMPTY;
}

bool is_dir_name_same(struct FAT32DirectoryEntry *entry, struct FAT32DriverRequest req)
{
  return memcmp(entry->name, req.name, 8) == 0;
};

bool is_dir_ext_same(struct FAT32DirectoryEntry *entry, struct FAT32DriverRequest req)
{
  return memcmp(entry->ext, req.ext, 3) == 0;
};

bool is_dir_ext_name_same(struct FAT32DirectoryEntry *entry, struct FAT32DriverRequest req)
{
  return is_dir_name_same(entry, req) && is_dir_ext_same(entry, req);
};

bool is_subdirectory(struct FAT32DirectoryEntry *entry)
{
  return entry->attribute == ATTR_SUBDIRECTORY;
};
