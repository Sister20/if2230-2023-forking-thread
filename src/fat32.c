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

uint32_t cluster_to_lba(uint32_t cluster) {
  return cluster * CLUSTER_BLOCK_COUNT + BOOT_SECTOR;
}

void create_fat32(void) {
  // Copy fs_signature
  write_blocks(fs_signature, BOOT_SECTOR, 1);

  // Copy reserved clusters
  driver_state.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
  driver_state.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
  driver_state.fat_table.cluster_map[2] = FAT32_FAT_END_OF_FILE;

  // Empty the remaining entry in the fat table
  for (int i = 3; i < CLUSTER_MAP_SIZE; i++) {
    driver_state.fat_table.cluster_map[i] = 0;
  }
  write_clusters(&driver_state.fat_table, 1, 1);

  // Initialize root directory
  struct FAT32DirectoryTable root;
  init_directory_table(&root, "root", 2);
  write_clusters(&root, 2, 1);
}

void initialize_filesystem_fat32(void) {
  if (is_empty_storage()) {
    // Create FAT if it's empty
    create_fat32();
  }

  // Move the FAT table from storage to the driver state
  read_clusters(&driver_state.fat_table, 1, 1);
}

bool is_empty_storage() {
  uint8_t boot_sector[BLOCK_SIZE];
  read_blocks(&boot_sector, BOOT_SECTOR, 1);
  return memcmp(boot_sector, fs_signature, BLOCK_SIZE);
}

void write_clusters(const void *ptr, uint32_t cluster_number,
                    uint8_t cluster_count) {
  uint32_t logical_block_address = cluster_to_lba(cluster_number);
  uint8_t block_count = cluster_count * CLUSTER_BLOCK_COUNT;
  write_blocks(ptr, logical_block_address, block_count);
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count) {
  uint32_t logical_block_address = cluster_to_lba(cluster_number);
  uint8_t block_count = cluster_count * CLUSTER_BLOCK_COUNT;
  read_blocks(ptr, logical_block_address, block_count);
}

void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name,
                          uint32_t parent_dir_cluster) {
  memcpy(dir_table->table[0].name, name, 8);
  dir_table->table[0].cluster_high = parent_dir_cluster >> 16;
  dir_table->table[0].cluster_low = parent_dir_cluster & 0xFFFF;
  dir_table->n_of_entry = 1;
}

int8_t read_directory(struct FAT32DriverRequest request) {
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  struct FAT32DirectoryEntry *entry;
  bool found_matching_directory = FALSE;
  bool found_matching_file = FALSE;
  for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                      !found_matching_directory;
       i++) {
    entry = &(driver_state.dir_table_buf.table[i]);
    found_matching_file =
        !(is_dir_empty(entry)) && is_dir_name_same(entry, request);
    found_matching_directory = found_matching_file && is_subdirectory(entry);
  }

  // Return error when entry is not a folder
  if (!found_matching_directory && found_matching_file) {
    return 1;
  }

  if (!found_matching_directory) {
    return 2;
  }

  // Return error when the buffer size is insufficient
  if (request.buffer_size < entry->n_of_occupied_cluster * CLUSTER_SIZE) {
    return -1;
  }

  // Enough size, read the cluster to the buffer
  read_directory_by_entry(entry, request);
  return 0;
}

int8_t read(struct FAT32DriverRequest request) {
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  struct FAT32DirectoryEntry *entry;
  bool found_matching_file = FALSE;
  uint8_t index_of_matching_file;
  for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                      found_matching_file;
       i++) {
    entry = &(driver_state.dir_table_buf.table[i]);
    found_matching_file =
        !(is_dir_empty(entry)) && is_dir_name_ext_same(entry, request);
    if (found_matching_file) {
      index_of_matching_file = i;
    }
  }

  // Check if the entry isn't empty and matches the requested name and
  // attributes. If it's not satisfied, skip.
  if (found_matching_file) {
    return 2;
  }

  // Return error when entry is a folder
  if (is_subdirectory(entry)) {
    return 1;
  }
  // Return error when not enough buffer size
  if (request.buffer_size < entry->filesize) {
    return -1;
  }

  // Buffer size sufficient, reading the content
  read_directory_by_entry(entry, request);

  return 0;
}

int8_t write(struct FAT32DriverRequest request) {
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
  read_clusters(&driver_state.fat_table, 1, 1);

  bool check_empty = FALSE;
  uint32_t new_cluster_number = 0;

  // Iterate through the directory entries and find empty cluster
  for (int i = 3; i < CLUSTER_MAP_SIZE && !check_empty; i++) {
    // Check if the cluster empty, if yes target the cluster
    check_empty = driver_state.fat_table.cluster_map[i] == (uint32_t)0;
    if (check_empty) {
      new_cluster_number = i;
    }
  }

  // If no matching directory entry was found, return error
  if (!check_empty) {
    return 2;
  }

  // Determine whether we're creating a file or a folder
  bool is_creating_directory = request.buffer_size == 0;

  // Iterate through the directory entries and find the same folder/file. Return
  // early if file with the same name already exist.
  for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
       i++) {
    struct FAT32DirectoryEntry *entry = &(driver_state.dir_table_buf.table[i]);
    if (!is_dir_empty(entry) &&
        (is_creating_directory ? is_dir_name_same(entry, request)
                               : is_dir_ext_name_same(entry, request))) {
      return 1;
    }
  }

  // Iterate through the directory entries and find empty entry
  bool found_empty_entry = FALSE;
  struct FAT32DirectoryEntry *entry;
  for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                      !found_empty_entry;
       i++) {

    entry = &(driver_state.dir_table_buf.table[i]);

    // Skip attempting to write if it's not empty
    found_empty_entry = is_dir_empty(entry);
  }

  // If there are no empty directories, return error
  if (!found_empty_entry) {
    return -1;
  }

  // Create a directory
  if (is_creating_directory) {
    create_subdirectory_from_entry(new_cluster_number, entry, request);
    return 0;
  }

  // Create a file
  create_file_from_entry(new_cluster_number, entry, request);
  return 0;
}

int8_t delete(struct FAT32DriverRequest request) {
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Iterate through the directory entries and find the matching one
  bool found_directory = FALSE;
  struct FAT32DirectoryEntry *entry;
  for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                      !found_directory;
       i++) {
    entry = &(driver_state.dir_table_buf.table[i]);
    found_directory =
        !is_dir_empty(entry) &&
        (is_subdirectory(entry) ? is_dir_name_same(entry, request)
                                : is_dir_ext_name_same(entry, request));
  }

  // Failed to find any matching directory
  if (!found_directory) {
    return 1;
  }
  // Found a matching directory entry, check if subdirectory empty or not
  if (is_subdirectory(entry)) {
    if (!is_subdirectory_immediately_empty(entry)) {
      return 2;
    }

    // Folder is empty and can be deleted
    delete_subdirectory_by_entry(entry, request);
    return 0;
  }

  // Not a folder
  delete_file_by_entry(entry, request);
  return 0;
}

bool is_dir_empty(struct FAT32DirectoryEntry *entry) {
  return entry->user_attribute != UATTR_NOT_EMPTY;
}

bool is_dir_name_same(struct FAT32DirectoryEntry *entry,
                      struct FAT32DriverRequest req) {
  return memcmp(entry->name, req.name, 8) == 0;
};

bool is_dir_ext_same(struct FAT32DirectoryEntry *entry,
                     struct FAT32DriverRequest req) {
  return memcmp(entry->ext, req.ext, 3) == 0;
};

bool is_dir_ext_name_same(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req) {
  return is_dir_name_same(entry, req) && is_dir_ext_same(entry, req);
};

bool is_subdirectory(struct FAT32DirectoryEntry *entry) {
  return entry->attribute == ATTR_SUBDIRECTORY;
};

int ceil(int a, int b) { return (a / b) + ((a % b != 0) ? 1 : 0); }

void create_subdirectory_from_entry(uint32_t cluster_number,
                                    struct FAT32DirectoryEntry *entry,
                                    struct FAT32DriverRequest req) {
  driver_state.fat_table.cluster_map[cluster_number] = FAT32_FAT_END_OF_FILE;

  // Increment the number of entry in its targeted parent's directory table
  increment_n_of_entry(&(driver_state.dir_table_buf));
  memcpy(entry->name, req.name, 8);
  entry->filesize = req.buffer_size;
  entry->cluster_high = (uint16_t)cluster_number >> 16;
  entry->cluster_low = (uint16_t)cluster_number & 0x0000FFFF;
  entry->attribute = (uint8_t)ATTR_SUBDIRECTORY;
  entry->user_attribute = (uint8_t)UATTR_NOT_EMPTY;
  entry->n_of_occupied_cluster = 1;
  struct FAT32DirectoryTable new_directory;
  init_directory_table(&new_directory, req.name, req.parent_cluster_number);

  // Write the new directory into the cluster
  write_clusters(&new_directory, cluster_number, 1);

  // Update the file allocation table in storage
  write_clusters(&driver_state.fat_table, 1, 1);

  // Update directory table of the parent
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
}

void create_file_from_entry(uint32_t cluster_number,
                            struct FAT32DirectoryEntry *entry,
                            struct FAT32DriverRequest req) {
  // Increment the number of entry in its targeted parent's directory table
  increment_n_of_entry(&(driver_state.dir_table_buf));
  int required_clusters = ceil(req.buffer_size, CLUSTER_SIZE);

  for (int i = 0; i < required_clusters; i++) {
    write_clusters(req.buf, cluster_number, 1);
    uint32_t old_cluster_number = cluster_number;
    for (int j = old_cluster_number + 1; j < CLUSTER_MAP_SIZE; j++) {
      // Check if the cluster is empty
      if (driver_state.fat_table.cluster_map[j] == 0) {
        cluster_number = j;
        break;
      }
    }
    driver_state.fat_table.cluster_map[old_cluster_number] = cluster_number;
  }

  memcpy(entry->name, req.name, 8);
  memcpy(entry->ext, req.ext, 3);
  entry->filesize = req.buffer_size;
  entry->cluster_high = cluster_number >> 16;
  entry->cluster_low = cluster_number & 0x0000FFFF;
  entry->user_attribute = UATTR_NOT_EMPTY;
  entry->n_of_occupied_cluster = required_clusters;
  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
};

bool is_subdirectory_immediately_empty(struct FAT32DirectoryEntry *entry) {
  uint16_t now_cluster_number = entry->cluster_low;
  struct FAT32DirectoryTable subdir_table;
  bool found_filled = FALSE;
  bool first_cluster = TRUE;
  do {
    read_clusters(&subdir_table, now_cluster_number, 1);
    uint16_t next_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0xFFFF;
    now_cluster_number =
        driver_state.fat_table.cluster_map[next_cluster_number] & 0xFFFF;
    found_filled = subdir_table.n_of_entry > (first_cluster ? 1 : 0);
    first_cluster = FALSE;
  } while (now_cluster_number != 0xFFFF && !found_filled);
  return !found_filled;
}

bool is_subdirectory_recursively_empty(struct FAT32DirectoryEntry *entry) {
  struct FAT32DirectoryTable subdir_table;
  read_clusters(&subdir_table, entry->cluster_low, 1);
  for (uint8_t j = 1; j < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
       j++) {
    struct FAT32DirectoryEntry *sub_entry = &(subdir_table.table[j]);
    // Check if the entry not empty
    if (!is_dir_empty(sub_entry)) {
      return FALSE;
    }
  }

  return TRUE;
};

void delete_subdirectory_by_entry(struct FAT32DirectoryEntry *entry,
                                  struct FAT32DriverRequest req) {
  uint16_t now_cluster_number = entry->cluster_low;
  do {
    uint16_t next_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0xFFFF;
    driver_state.fat_table.cluster_map[now_cluster_number] = 0;
    now_cluster_number =
        driver_state.fat_table.cluster_map[next_cluster_number] & 0xFFFF;
  } while (now_cluster_number != 0xFFFF);

  memcpy(entry->name, "\0\0\0\0\0\0\0\0", 8);
  entry->user_attribute = 0;
  entry->attribute = 0;
  driver_state.fat_table.cluster_map[entry->cluster_low] = 0;
  entry->cluster_high = 0;
  entry->cluster_low = 0;
  entry->n_of_occupied_cluster = 0;

  // Decrement the number of entry in its targeted parent's directory table
  decrement_n_of_entry(&(driver_state.dir_table_buf));

  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
}

void delete_file_by_entry(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req) {
  uint16_t now_cluster_number = entry->cluster_low;
  do {
    uint16_t next_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0xFFFF;
    driver_state.fat_table.cluster_map[now_cluster_number] = 0;
    now_cluster_number =
        driver_state.fat_table.cluster_map[next_cluster_number] & 0xFFFF;
  } while (now_cluster_number != 0xFFFF);
  memcpy(entry->name, "\0\0\0\0\0\0\0\0", 8);
  memcpy(entry->ext, "\0\0\0", 3);
  entry->cluster_high = 0;
  entry->cluster_low = 0;
  entry->user_attribute = 0;
  entry->attribute = 0;
  entry->n_of_occupied_cluster = 0;

  // Decrement the number of entry in its targeted parent's directory table
  decrement_n_of_entry(&(driver_state.dir_table_buf));

  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
}

void read_directory_by_entry(struct FAT32DirectoryEntry *entry,
                             struct FAT32DriverRequest req) {
  uint16_t next_cluster_number = entry->cluster_low;
  uint8_t nth_cluster = 0;
  do {
    read_clusters(req.buf + CLUSTER_SIZE * nth_cluster, next_cluster_number, 1);
    next_cluster_number =
        driver_state.fat_table.cluster_map[next_cluster_number] & 0x0000FFFF;
    nth_cluster++;
  } while (next_cluster_number != 0xFFFF);
}

bool increment_n_of_entry(struct FAT32DirectoryTable *table) {
  (table->n_of_entry)++;
}

bool decrement_n_of_entry(struct FAT32DirectoryTable *table) {
  (table->n_of_entry)--;
}

bool is_subdirectory_cluster_full(struct FAT32DirectoryTable *subdir) {
  return subdir->n_of_entry >=
         (CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry));
}
