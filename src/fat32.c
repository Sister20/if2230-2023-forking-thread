#include "lib-header/fat32.h"
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
static char empty_cluster_value[CLUSTER_SIZE];
struct NodeFileSystem *BPlusTree;

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

  // Initialize root directory
  struct FAT32DirectoryTable root;
  init_directory_table(&root, "root\0\0\0", 2);
  write_clusters(&root, 2, 1);
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

  // Initialize B+ Tree
  BPlusTree = make_tree("root\0\0\0\0", "\0\0\0", 2);
  initialize_b_tree(BPlusTree, "root\0\0\0\0", 2, 2);

  // Initialize static array for empty clusters
  for (int i = 0; i < CLUSTER_SIZE; i++)
  {
    empty_cluster_value[i] = 0;
  }
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
  dir_table->table[0].filesize = CLUSTER_SIZE;
  dir_table->table[0].n_of_entries = 1;
  dir_table->table[0].attribute = ATTR_SUBDIRECTORY;
}

void init_directory_table_child(struct FAT32DirectoryTable *dir_table,
                                char *name, uint32_t parent_dir_cluster)
{
  init_directory_table(dir_table, name, parent_dir_cluster);
  dir_table->table[0].attribute = ATTR_SUBDIRECTORY_CHILD;
}

int8_t read_directory(struct FAT32DriverRequest request)
{

  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  if (request.parent_cluster_number == ROOT_CLUSTER_NUMBER && memcmp("root\0\0\0\0", request.name, 8) == 0)
  {
    // Return error when the buffer size is insufficient
    if (request.buffer_size < driver_state.dir_table_buf.table->filesize)
    {
      return -1;
    }

    // Enough size, read the cluster to the buffer
    read_directory_by_cluster_number(ROOT_CLUSTER_NUMBER, request);
    return 0;
  }

  // If given parent cluster number isn't the head of a directory, return error
  if (is_dirtable_child(&driver_state.dir_table_buf))
  {
    return 3;
  }

  // Iterate through the directory entries, including traversal through all of
  // directory's cluster and find the matching one
  struct FAT32DirectoryEntry *entry;
  bool found_matching_directory = FALSE;
  bool found_matching_file = FALSE;
  bool end_of_directory = FALSE;

  uint16_t now_cluster_number = request.parent_cluster_number;
  while (!end_of_directory && !found_matching_directory)
  {

    // Traverse the table in examined cluster. Starts from 1 because the first
    // entry of the table is the directory itself
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                        !found_matching_directory;
         i++)
    {
      entry = &(driver_state.dir_table_buf.table[i]);

      found_matching_file =
          !is_entry_empty(entry) && is_dir_name_same(entry, request) && memcmp(entry->ext, "\0\0\0", 3) == 0;
      found_matching_directory = found_matching_file && is_subdirectory(entry);
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // If directory is found, get out of the loop
    if (found_matching_directory)
      continue;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }

  // Return error when entry is not a folder
  if (!found_matching_directory && found_matching_file)
  {
    return 1;
  }

  if (!found_matching_directory)
  {
    return 2;
  }

  // Return error when the buffer size is insufficient
  if (request.buffer_size < entry->filesize)
  {
    return -1;
  }

  // Enough size, read the cluster to the buffer
  read_directory_by_entry(entry, request);
  return 0;
}

int8_t read(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
  read_clusters(&driver_state.fat_table, 1, 1);

  // If given parent cluster number isn't the head of a directory, return error
  if (!is_parent_cluster_valid(request))
  {
    return 4;
  }

  // Iterate through the directory entries, including traversal through all of
  // directory's cluster and find the matching one
  struct FAT32DirectoryEntry *entry;
  bool found_matching_file = FALSE;
  bool end_of_directory = FALSE;

  uint16_t now_cluster_number = request.parent_cluster_number;
  while (!end_of_directory && !found_matching_file)
  {

    // Traverse the table in examined cluster. Starts from 1 because the first
    // entry of the table is the directory itself
    for (uint8_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                        !found_matching_file;
         i++)
    {
      entry = &(driver_state.dir_table_buf.table[i]);

      found_matching_file =
          !is_entry_empty(entry) && is_dir_ext_name_same(entry, request);
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // If file is found, get out of the loop
    if (found_matching_file)
      continue;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }

  // Check if the entry isn't empty and matches the requested name and
  // attributes. If it's not satisfied, skip.
  if (!found_matching_file)
  {
    return 3;
  }

  // Return error when entry is a folder
  if (is_subdirectory(entry))
  {
    return 1;
  }

  // Return error when not enough buffer size
  if (request.buffer_size < entry->filesize)
  {
    return 2;
  }

  // Buffer size sufficient, reading the content
  read_directory_by_entry(entry, request);

  return 0;
}

int8_t write(struct FAT32DriverRequest request)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);
  read_clusters(&driver_state.fat_table, 1, 1);

  // If the given parent cluster number isn't the head of a directory, return
  // error

  if (!is_parent_cluster_valid(request))
    return 2;

  // Determine whether we're creating a file or a folder
  bool is_creating_directory = request.buffer_size == 0;

  if (is_requested_directory_already_exist(request))
  {
    return 1;
  }

  // Determine the amount of clusters needed
  int required_clusters = ceil(request.buffer_size, CLUSTER_SIZE);

  if (required_clusters == 0)
    required_clusters++;

  bool check_empty = FALSE;
  uint32_t new_cluster_number = 0;
  int count = 0;
  uint32_t last_occupied_cluster_number;

  // Iterate through the directory entries and find empty cluster
  for (int i = 3; i < CLUSTER_MAP_SIZE && !check_empty; i++)
  {
    // Check if the cluster empty, if yes target the cluster
    check_empty = driver_state.fat_table.cluster_map[i] == (uint32_t)0;

    if (check_empty)
    {
      if (count == 0)
        new_cluster_number = i;
      count++;
      last_occupied_cluster_number = i;
      check_empty = (count == required_clusters);
    }
  }

  // If not enough clusters to create the requested directory, return erro
  if (!check_empty)
  {
    return -1;
  }

  // Iterate through the directory entries and find empty entry
  bool found_empty_entry = FALSE;
  bool cluster_full = FALSE;
  uint16_t now_cluster_number = request.parent_cluster_number;
  uint16_t prev_cluster_number;
  bool end_of_directory = FALSE;
  struct FAT32DirectoryEntry *entry;

  while (!end_of_directory && !found_empty_entry)
  {

    // Skip checking the cluster if it's known that it's full
    cluster_full = is_subdirectory_cluster_full(&(driver_state.dir_table_buf));

    for (uint8_t i = 1;
         (i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)) &&
         !found_empty_entry && !cluster_full;
         i++)
    {

      entry = &(driver_state.dir_table_buf.table[i]);

      // Skip attempting to write if it's not empty
      found_empty_entry = is_entry_empty(entry);
    }

    // Update the prev_cluster_number for the purpose of possibly adding more
    // cluster to the directory
    prev_cluster_number = now_cluster_number;

    if (found_empty_entry)
      continue;

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0xFFFF) == 0xFFFF;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }

  // If there are no empty directories, create new cluster from the requested
  // parent cluster
  if (!found_empty_entry)
  {
    // Create the child cluster of the target directory. dir_table_buf will be
    // set into the table of the child cluster
    bool succesfully_created_child_cluster = create_child_cluster_of_subdir(
        last_occupied_cluster_number, prev_cluster_number, &request);

    if (!succesfully_created_child_cluster)
    {
      return -1;
    }

    // Set the entry to be inserted into as the first element of table of the
    // newly created cluster
    entry = &(driver_state.dir_table_buf.table[1]);
  }

  else
  {
    request.parent_cluster_number = now_cluster_number;
  }

  // set_create_datetime(entry);

  // Create a directory
  if (is_creating_directory)
  {

    if (memcmp("root\0\0\0\0", request.name, 8) == 0)
      return 3;

    create_subdirectory_from_entry(new_cluster_number, entry, request);
    BPlusTree = insert(BPlusTree, request.name, request.ext, request.parent_cluster_number);
    return 0;
  }

  // Create a file
  create_file_from_entry(new_cluster_number, entry, request);
  BPlusTree = insert(BPlusTree, request.name, request.ext, request.parent_cluster_number);
  return 0;
}

int8_t delete(struct FAT32DriverRequest request, bool is_recursive, bool check_recursion)
{
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // If given parent cluster number isn't the head of a directory, return error
  if (!is_parent_cluster_valid(request))
  {
    return 4;
  }

  // Iterate through the directory entries and find the matching one
  bool found_directory = FALSE;
  bool end_of_directory = FALSE;
  struct FAT32DirectoryEntry *entry;

  uint16_t now_cluster_number = request.parent_cluster_number;
  uint16_t prev_cluster_number;
  uint16_t nth_entry;

  while (!end_of_directory && !found_directory)
  {
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) &&
                        !found_directory;
         i++)
    {
      entry = &(driver_state.dir_table_buf.table[i]);
      if (!is_entry_empty(entry))
      {
        found_directory = is_dir_ext_name_same(entry, request);
        if (found_directory)
        {
          nth_entry = i;
        }
      }

      else
      {
        found_directory = FALSE;
      }
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // Take notes of the latest_cluster_number for the proper copying of
    // directory table
    prev_cluster_number = now_cluster_number;

    // If file is found, get out of the loop
    if (found_directory)
    {
      continue;
    }

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }

  // Failed to find any matching directory
  if (!found_directory)
  {
    return 1;
  }

  request.parent_cluster_number = prev_cluster_number;

  if (!is_subdirectory(entry))
  {
    // Not a folder, delete as a file
    delete_file_by_entry(entry, request);
    create_b_tree();
    return 0;
  }

  // Exit if the deletion is not recursive but the directory is not empty
  if (!is_subdirectory_immediately_empty(entry) && !is_recursive)
  {
    return 2;
  }

  // Folder is empty and can be deleted
  if (is_subdirectory_immediately_empty(entry) && !is_recursive)
  {
    delete_subdirectory_by_entry(entry, request);
    create_b_tree();
    return 0;
  }

  uint16_t entry_cluster_position = entry->cluster_low;

  // If check recursion is false, no checking will be done
  if (check_recursion && !is_below_max_recursion_depth(entry_cluster_position, 0))
  {
    return 5;
  }

  // Delete the directory's content
  delete_subdirectory_content(entry_cluster_position);

  // Reset the read clusters to the cluster where the entry of the directory to be deleted is located in the directory table
  read_clusters(&driver_state.dir_table_buf, request.parent_cluster_number, 1);

  // Delete the directory itself
  delete_subdirectory_by_entry(&driver_state.dir_table_buf.table[nth_entry], request);

  create_b_tree();

  return 0;
}

void delete_subdirectory_by_entry(struct FAT32DirectoryEntry *entry,
                                  struct FAT32DriverRequest req)
{

  uint16_t now_cluster_number = entry->cluster_low;
  uint16_t next_cluster_number;
  do
  {
    next_cluster_number =
        (uint16_t)(driver_state.fat_table.cluster_map[now_cluster_number] &
                   0xFFFF);
    driver_state.fat_table.cluster_map[now_cluster_number] = (uint32_t)0;
    reset_cluster(now_cluster_number);
    now_cluster_number = next_cluster_number;
  } while (now_cluster_number != 0xFFFF);

  memcpy(entry->name, "\0\0\0\0\0\0\0\0", 8);
  entry->user_attribute = (uint8_t)0;
  entry->attribute = (uint8_t)0;
  driver_state.fat_table.cluster_map[entry->cluster_low] = (uint32_t)0;
  entry->cluster_high = (uint16_t)0;
  entry->cluster_low = (uint16_t)0;

  // Decrement the number of entry in its targeted parent's directory table
  decrement_subdir_n_of_entry(&(driver_state.dir_table_buf));

  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
}

void delete_file_by_entry(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req)
{
  uint16_t now_cluster_number = entry->cluster_low;
  uint16_t next_cluster_number;
  do
  {
    next_cluster_number =
        (uint16_t)(driver_state.fat_table.cluster_map[now_cluster_number] &
                   0xFFFF);
    driver_state.fat_table.cluster_map[now_cluster_number] = (uint32_t)0;
    reset_cluster(now_cluster_number);
    now_cluster_number = next_cluster_number;
  } while (now_cluster_number != 0xFFFF);
  memcpy(entry->name, "\0\0\0\0\0\0\0\0", 8);
  memcpy(entry->ext, "\0\0\0", 3);
  entry->cluster_high = 0;
  entry->cluster_low = 0;
  entry->user_attribute = 0;
  entry->attribute = 0;

  // Decrement the number of entry in its targeted parent's directory table
  decrement_subdir_n_of_entry(&(driver_state.dir_table_buf));

  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
}

void delete_subdirectory_content(uint16_t target_cluster_number)
{

  read_clusters(&driver_state.dir_table_buf, target_cluster_number, 1);

  struct FAT32DriverRequest req =
      {
          .parent_cluster_number = target_cluster_number};

  // Iterate through the directory entries and delete all
  bool end_of_directory = FALSE;
  struct FAT32DirectoryEntry *entry;

  uint16_t now_cluster_number = target_cluster_number;

  while (!end_of_directory)
  {
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
         i++)
    {
      entry = &(driver_state.dir_table_buf.table[i]);
      // Skip entry if it's empty
      if (is_entry_empty(entry))
      {
        continue;
      }
      if (is_subdirectory(entry))
      {
        memcpy(&(req.name), &(entry->name), 8);
        delete (req, TRUE, FALSE);

        // Reset the content of the driver state to before deletion
        read_clusters(&driver_state.dir_table_buf, target_cluster_number, 1);
      }
      else
      {
        delete_file_by_entry(entry, req);
      }
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      req.parent_cluster_number = now_cluster_number;
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }
}

bool is_entry_empty(struct FAT32DirectoryEntry *entry)
{
  return entry->user_attribute != UATTR_NOT_EMPTY;
}

bool is_dir_name_same(struct FAT32DirectoryEntry *entry,
                      struct FAT32DriverRequest req)
{
  return memcmp(entry->name, req.name, 8) == 0;
};

bool is_dir_ext_same(struct FAT32DirectoryEntry *entry,
                     struct FAT32DriverRequest req)
{
  return memcmp(entry->ext, req.ext, 3) == 0;
};

bool is_dir_ext_name_same(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req)
{
  return is_dir_name_same(entry, req) && is_dir_ext_same(entry, req);
};

bool is_subdirectory(struct FAT32DirectoryEntry *entry)
{
  return entry->attribute == ATTR_SUBDIRECTORY;
};

int ceil(int a, int b) { return (a / b) + ((a % b != 0) ? 1 : 0); }

void create_subdirectory_from_entry(uint32_t cluster_number,
                                    struct FAT32DirectoryEntry *entry,
                                    struct FAT32DriverRequest req)
{
  driver_state.fat_table.cluster_map[cluster_number] = FAT32_FAT_END_OF_FILE;

  // Increment the number of entry in its targeted parent's directory table
  increment_subdir_n_of_entry(&(driver_state.dir_table_buf));
  memcpy(entry->name, req.name, 8);
  memcpy(entry->ext, "\0\0\0", 3);
  entry->filesize = req.buffer_size;
  entry->cluster_high = (uint16_t)cluster_number >> 16;
  entry->cluster_low = (uint16_t)cluster_number & 0x0000FFFF;
  entry->attribute = (uint8_t)ATTR_SUBDIRECTORY;
  entry->user_attribute = (uint8_t)UATTR_NOT_EMPTY;
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
                            struct FAT32DriverRequest req)
{
  // Increment the number of entry in its targeted parent's directory table
  increment_subdir_n_of_entry(&(driver_state.dir_table_buf));
  entry->cluster_high = cluster_number >> 16;
  entry->cluster_low = cluster_number & 0x0000FFFF;

  int required_clusters = ceil(req.buffer_size, CLUSTER_SIZE);

  uint32_t old_cluster_number;
  for (int i = 0; i < required_clusters; i++)
  {
    write_clusters(req.buf + CLUSTER_SIZE * i, cluster_number, 1);
    old_cluster_number = cluster_number;
    for (int j = old_cluster_number + 1; j < CLUSTER_MAP_SIZE; j++)
    {
      // Check if the cluster is empty
      if (driver_state.fat_table.cluster_map[j] == 0)
      {
        cluster_number = j;
        break;
      }
    }
    driver_state.fat_table.cluster_map[old_cluster_number] = cluster_number;
  }
  driver_state.fat_table.cluster_map[old_cluster_number] =
      FAT32_FAT_END_OF_FILE;

  memcpy(entry->name, req.name, 8);
  memcpy(entry->ext, req.ext, 3);
  entry->filesize = req.buffer_size;
  entry->attribute = (uint8_t)0;
  entry->user_attribute = UATTR_NOT_EMPTY;

  write_clusters(&driver_state.fat_table, 1, 1);
  write_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
};

bool is_subdirectory_immediately_empty(struct FAT32DirectoryEntry *entry)
{
  uint16_t now_cluster_number = entry->cluster_low;
  struct FAT32DirectoryTable subdir_table;
  bool found_filled = FALSE;
  do
  {
    read_clusters(&subdir_table, now_cluster_number, 1);
    now_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0xFFFF;

    found_filled = !is_subdirectory_cluster_empty(&subdir_table);
  } while (now_cluster_number != 0xFFFF && !found_filled);
  return !found_filled;
}

// bool is_subdirectory_recursively_empty(struct FAT32DirectoryEntry *entry) {
//   struct FAT32DirectoryTable subdir_table;
//   read_clusters(&subdir_table, entry->cluster_low, 1);
//   for (uint8_t j = 1; j < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
//        j++) {
//     struct FAT32DirectoryEntry *sub_entry = &(subdir_table.table[j]);
//     // Check if the entry not empty
//     if (!is_entry_empty(sub_entry)) {
//       return FALSE;
//     }
//   }

//   return TRUE;
// };

void reset_cluster(uint32_t cluster_number)
{
  write_clusters(empty_cluster_value, cluster_number, 1);
}

void read_directory_by_entry(struct FAT32DirectoryEntry *entry,
                             struct FAT32DriverRequest req)
{
  uint16_t now_cluster_number = entry->cluster_low;
  uint8_t nth_cluster = 0;
  do
  {
    read_clusters(req.buf + CLUSTER_SIZE * nth_cluster, now_cluster_number, 1);
    now_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0x0000FFFF;
    nth_cluster++;
  } while (now_cluster_number != 0xFFFF);
  // set_access_datetime(entry);
}

void read_directory_by_cluster_number(uint16_t cluster_number,
                                      struct FAT32DriverRequest req)
{
  uint16_t now_cluster_number = cluster_number;
  uint8_t nth_cluster = 0;
  do
  {
    read_clusters(req.buf + CLUSTER_SIZE * nth_cluster, now_cluster_number, 1);
    now_cluster_number =
        driver_state.fat_table.cluster_map[now_cluster_number] & 0x0000FFFF;
    nth_cluster++;
  } while (now_cluster_number != 0xFFFF);
  // set_access_datetime(entry);
}

void increment_subdir_n_of_entry(struct FAT32DirectoryTable *table)
{
  table->table[0].n_of_entries++;
}

uint32_t get_subdir_n_of_entry(struct FAT32DirectoryTable *table)
{
  return table->table[0].n_of_entries;
};

void decrement_subdir_n_of_entry(struct FAT32DirectoryTable *table)
{
  table->table[0].n_of_entries--;
}

bool is_subdirectory_cluster_full(struct FAT32DirectoryTable *subdir)
{
  return subdir->table[0].n_of_entries >=
         (CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry));
}

bool is_parent_cluster_valid(struct FAT32DriverRequest request)
{

  struct FAT32DirectoryTable current_parent_table;
  read_clusters(&current_parent_table, request.parent_cluster_number, 1);

  if (current_parent_table.table[0].cluster_low == ROOT_CLUSTER_NUMBER)
  {
    return TRUE;
  }

  if (is_dirtable_child(&driver_state.dir_table_buf) || current_parent_table.table[0].attribute != ATTR_SUBDIRECTORY)
  {
    return FALSE;
  }

  uint32_t target_cluster_number = request.parent_cluster_number;
  int8_t visited_parent[CLUSTER_MAP_SIZE];
  for (uint32_t i = 0; i < CLUSTER_MAP_SIZE; i++)
  {
    visited_parent[i] = 0;
  }

  current_parent_table = driver_state.dir_table_buf;

  while (target_cluster_number < CLUSTER_MAP_SIZE &&
         target_cluster_number > ROOT_CLUSTER_NUMBER &&
         visited_parent[target_cluster_number] == 0)
  {
    visited_parent[target_cluster_number] = 1;
    read_clusters(&current_parent_table, target_cluster_number, 1);

    target_cluster_number = (current_parent_table.table[0].cluster_high << 16) +
                            current_parent_table.table[0].cluster_low;
  }

  return target_cluster_number == ROOT_CLUSTER_NUMBER;
}

bool is_subdirectory_cluster_empty(struct FAT32DirectoryTable *subdir)
{
  return subdir->table[0].n_of_entries == 1;
};
bool is_dirtable_child(struct FAT32DirectoryTable *subdir)
{
  return subdir->table[0].attribute == ATTR_SUBDIRECTORY_CHILD;
};

uint32_t get_n_of_cluster_subdir(struct FAT32DirectoryEntry *entry)
{
  return entry->filesize / CLUSTER_SIZE;
};

bool is_requested_directory_already_exist(struct FAT32DriverRequest req)
{

  read_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);
  // Determine whether we're creating a file or a folder
  bool is_creating_directory = req.buffer_size == 0;

  // Iterate through the directory entries and find the same folder/file. Return
  // early if file with the same name already exist.
  bool same_entry = FALSE;
  uint16_t now_cluster_number = req.parent_cluster_number;
  bool end_of_directory = FALSE;
  while (!end_of_directory && !same_entry)
  {

    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
         i++)
    {

      struct FAT32DirectoryEntry *entry =
          &(driver_state.dir_table_buf.table[i]);

      if (is_entry_empty(entry))
        continue;

      // Check if it's similar

      if (is_creating_directory)
        same_entry = is_dir_name_same(entry, req) && memcmp(entry->ext, "\0\0\0", 3) == 0;
      else
        same_entry = is_dir_ext_name_same(entry, req);

      if (same_entry)
      {
        return TRUE;
      }
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }

  // Reset the dir_table in driver state to the original parent
  read_clusters(&driver_state.dir_table_buf, req.parent_cluster_number, 1);

  return FALSE;
}

bool create_child_cluster_of_subdir(uint32_t last_occupied_cluster_number,
                                    uint16_t prev_cluster_number,
                                    struct FAT32DriverRequest *req)
{
  // Iterate through the file allocation table and find empty cluster for the
  // directory expansion
  uint32_t new_cluster_number_directory;
  bool empty_cluster_found = FALSE;

  // Find clusters after the allocated cluster of the requested directory itself
  for (int i = last_occupied_cluster_number + 1;
       i < CLUSTER_MAP_SIZE && !empty_cluster_found; i++)
  {
    // Check if the cluster is empty, if yes target the cluster
    empty_cluster_found = driver_state.fat_table.cluster_map[i] == (uint32_t)0;

    if (empty_cluster_found)
      new_cluster_number_directory = i;
  }

  // If not enough cluster for expanding directory, return error
  if (!empty_cluster_found)
  {
    return FALSE;
  }

  // Point the last cluster of the directory to the new to-be-allocated-cluster
  driver_state.fat_table.cluster_map[prev_cluster_number] =
      new_cluster_number_directory;

  // Point the to-be-allocated-cluster to EOF
  driver_state.fat_table.cluster_map[new_cluster_number_directory] =
      FAT32_FAT_END_OF_FILE;

  uint16_t cluster_low_original = driver_state.dir_table_buf.table->cluster_low;
  uint16_t cluster_high_original =
      driver_state.dir_table_buf.table->cluster_high;
  uint32_t parent_dir_cluster =
      (cluster_high_original << 16) | cluster_low_original;

  // Create and allocate the table
  struct FAT32DirectoryTable new_cluster_for_directory;
  init_directory_table_child(&new_cluster_for_directory,
                             driver_state.dir_table_buf.table->name,
                             parent_dir_cluster);

  // Set the new cluster to dir_table_buf to be written in the create_from_entry
  // function
  driver_state.dir_table_buf = new_cluster_for_directory;
  req->parent_cluster_number = new_cluster_number_directory;
  return TRUE;
};

void set_create_datetime(struct FAT32DirectoryEntry *entry)
{
  uint32_t FTTimestamp = get_FTTimestamp_time();
  entry->create_date = ((FTTimestamp & 0xFFFF0000) >> 16);
  entry->create_time = (FTTimestamp & 0x0000FFFF);
}

void set_modified_date(struct FAT32DirectoryEntry *entry)
{
  uint32_t FTTimestamp = get_FTTimestamp_time();
  entry->modified_date = ((FTTimestamp & 0xFFFF0000) >> 16);
}

void set_access_datetime(struct FAT32DirectoryEntry *entry)
{
  uint32_t FTTimestamp = get_FTTimestamp_time();
  entry->access_date = ((FTTimestamp & 0xFFFF0000) >> 16);
  entry->access_time = (FTTimestamp & 0x0000FFFF);
}

bool is_below_max_recursion_depth(uint16_t target_cluster_number, uint8_t recursion_count)
{
  // Basis
  if (recursion_count >= MAX_RECURSIVE_OP_DEPTH)
  {
    return FALSE;
  }

  // Get the directory table of the directory to check
  read_clusters(&driver_state.dir_table_buf, target_cluster_number, 1);

  bool end_of_directory = FALSE;
  struct FAT32DirectoryEntry *entry;

  uint16_t now_cluster_number = target_cluster_number;

  while (!end_of_directory)
  {
    for (uint8_t i = 1; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry);
         i++)
    {
      entry = &(driver_state.dir_table_buf.table[i]);
      if (!is_entry_empty(entry) && is_subdirectory(entry))
      {
        // Return false if recursive checking finds the subdirectory in the directory too deep
        if (!is_below_max_recursion_depth(entry->cluster_low, recursion_count + 1))
        {
          return FALSE;
        }
        read_clusters(&driver_state.dir_table_buf, target_cluster_number, 1);
      }
    }

    // If the cluster_number is EOF, then we've finished examining the last
    // cluster of the directory
    end_of_directory = (driver_state.fat_table.cluster_map[now_cluster_number] &
                        0x0000FFFF) == 0xFFFF;

    // Move onto the next cluster if it's not the end yet
    if (!end_of_directory)
    {
      now_cluster_number =
          driver_state.fat_table.cluster_map[now_cluster_number];
      read_clusters(&driver_state.dir_table_buf, (uint32_t)now_cluster_number,
                    1);
    }
  }
  return TRUE;
}