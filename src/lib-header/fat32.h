#ifndef _FAT32_H
#define _FAT32_H

#include "disk.h"
#include "stdtype.h"
#include "cmosrtc.h"

/**
 * FAT32 - IF2230 edition - 2023
 * Check "IF2230 - Guidebook - Milestone 2" for more details
 * https://docs.google.com/document/d/1IFyxHSYYpKgecHcS0T64oDc4bVElaq8tBcm1_mjjGGM/edit#
 */

/* -- IF2230 File System constants -- */
#define BOOT_SECTOR 0
#define CLUSTER_BLOCK_COUNT 4
#define CLUSTER_SIZE (BLOCK_SIZE * CLUSTER_BLOCK_COUNT)
#define CLUSTER_MAP_SIZE 512

/* -- FAT32 FileAllocationTable constants -- */
// FAT reserved value for cluster 0 and 1 in FileAllocationTable
#define CLUSTER_0_VALUE 0x0FFFFFF0
#define CLUSTER_1_VALUE 0x0FFFFFFF

// EOF also double as valid cluster / "this is last valid cluster in the chain"
#define FAT32_FAT_END_OF_FILE 0x0FFFFFFF
#define FAT32_FAT_EMPTY_ENTRY 0x00000000

#define FAT_CLUSTER_NUMBER 1
#define ROOT_CLUSTER_NUMBER 2

/* -- FAT32 DirectoryEntry constants -- */
#define ATTR_SUBDIRECTORY 0b00010000
#define ATTR_SUBDIRECTORY_CHILD 0b00010001
#define UATTR_NOT_EMPTY 0b10101010

/* -- File operation constant -- */
#define MAX_RECURSIVE_OP_DEPTH 64

// Boot sector signature for this file system "FAT32 - IF2230 edition"
extern const uint8_t fs_signature[BLOCK_SIZE];

// Cluster buffer data type - @param buf Byte buffer with size of CLUSTER_SIZE
struct ClusterBuffer
{
  uint8_t buf[CLUSTER_SIZE];
} __attribute__((packed));

/* -- FAT32 Data Structures -- */

/**
 * FAT32 FileAllocationTable, for more information about this, check guidebook
 *
 * @param cluster_map Containing cluster map of FAT32
 */
struct FAT32FileAllocationTable
{
  uint32_t cluster_map[CLUSTER_MAP_SIZE];
} __attribute__((packed));

/**
 * FAT32 standard 8.3 format - 32 bytes DirectoryEntry, Some detail can be found
 * at:
 * https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system#Directory_entry,
 * and click show table.
 *
 * @param name           Entry name
 * @param ext            File extension
 * @param attribute      Will be used exclusively for subdirectory flag /
 * determining this entry is file or folder
 * @param user_attribute If this attribute equal with UATTR_NOT_EMPTY then entry
 * is not empty
 *
 * @param n_of_entries   The number of entries for the directory table
 * containing the entry. Only defined for the first entry in a table (at least
 * 1)
 * @param create_time    The time (minute, hour) when a directory is created, in UTC
 * @param create_date    The date when a directory is created, in UTC
 * @param access_time    The time (minute, hour) when a directory is accessed, in UTC
 * @param access_date    The date when a directory is accessed, in UTCs
 * @param modified_date  Unused / optional
 *
 * @param cluster_high   Upper 16-bit of cluster number
 * @param cluster_low    Lower 16-bit of cluster number
 * @param filesize       Filesize of this file, if this is directory / folder,
 * filesize is the number of cluster it occupies * cluster size
 */
struct FAT32DirectoryEntry
{
  char name[8];
  char ext[3];
  uint8_t attribute;
  uint8_t user_attribute;

  uint8_t n_of_entries;
  uint16_t create_time;
  uint16_t create_date;
  uint16_t access_date;
  uint16_t access_time;
  uint16_t modified_date;

  uint16_t cluster_high;
  uint16_t cluster_low;

  uint32_t filesize;

} __attribute__((packed));

/**
 * @brief FAT32 DirectoryTable, containing directory entry table
 * @param table Table of DirectoryEntry that span within 1 cluster
 * @param n_of_entry The number of entry in the table
 */
struct FAT32DirectoryTable
{
  struct FAT32DirectoryEntry
      table[CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)];
} __attribute__((packed));

/* -- FAT32 Driver -- */

/**
 * FAT32DriverState - Contain all driver states
 *
 * @param fat_table     FAT of the system, will be loaded during
 * initialize_filesystem_fat32()
 * @param dir_table_buf Buffer for directory table
 * @param cluster_buf   Buffer for cluster
 */
struct FAT32DriverState
{
  struct FAT32FileAllocationTable fat_table;
  struct FAT32DirectoryTable dir_table_buf;
  struct ClusterBuffer cluster_buf;
} __attribute__((packed));

/**
 * FAT32DriverRequest - Request for Driver CRUD operation
 *
 * @param buf                   Pointer pointing to buffer
 * @param name                  Name for directory entry
 * @param ext                   Extension for file
 * @param parent_cluster_number Parent directory cluster number, for updating
 * metadata
 * @param buffer_size           Buffer size, CRUD operation will have different
 * behaviour with this attribute
 */
struct FAT32DriverRequest
{
  void *buf;
  char name[8];
  char ext[3];
  uint32_t parent_cluster_number;
  uint32_t buffer_size;
} __attribute__((packed));

/* -- Driver Interfaces -- */

/**
 * Convert cluster number to logical block address
 *
 * @param cluster Cluster number to convert
 * @return uint32_t Logical Block Address
 */
uint32_t cluster_to_lba(uint32_t cluster);

/**
 * Initialize DirectoryTable value with parent DirectoryEntry and directory name
 *
 * @param dir_table          Pointer to directory table
 * @param name               8-byte char for directory name
 * @param parent_dir_cluster Parent directory cluster number
 */
void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name,
                          uint32_t parent_dir_cluster);

/**
 * @brief Initialize DirectoryTable value for a child cluster of a Directory.
 * Sets the first element and its attributes to show that they are a child
 *
 * @param dir_table
 * @param name
 * @param parent_dir_cluster
 */
void init_directory_table_child(struct FAT32DirectoryTable *dir_table,
                                char *name, uint32_t parent_dir_cluster);

/**
 * Checking whether filesystem signature is missing or not in boot sector
 *
 * @return True if memcmp(boot_sector, fs_signature) returning inequality
 */
bool is_empty_storage(void);

/**
 * Create new FAT32 file system. Will write fs_signature into boot sector and
 * proper FileAllocationTable (contain CLUSTER_0_VALUE, CLUSTER_1_VALUE,
 * and initialized root directory) into cluster number 1
 */
void create_fat32(void);

/**
 * Initialize file system driver state, if is_empty_storage() then
 * create_fat32() Else, read and cache entire FileAllocationTable (located at
 * cluster number 1) into driver state
 */
void initialize_filesystem_fat32(void);

/**
 * Write cluster operation, wrapper for write_blocks().
 * Recommended to use struct ClusterBuffer
 *
 * @param ptr            Pointer to source data
 * @param cluster_number Cluster number to write
 * @param cluster_count  Cluster count to write, due limitation of write_blocks
 * block_count 255 => max cluster_count = 63
 */
void write_clusters(const void *ptr, uint32_t cluster_number,
                    uint8_t cluster_count);

/**
 * Read cluster operation, wrapper for read_blocks().
 * Recommended to use struct ClusterBuffer
 *
 * @param ptr            Pointer to buffer for reading
 * @param cluster_number Cluster number to read
 * @param cluster_count  Cluster count to read, due limitation of read_blocks
 * block_count 255 => max cluster_count = 63
 */
void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count);

/* -- CRUD Operation -- */

/**
 *  FAT32 Folder / Directory read
 *
 * @param request buf point to struct FAT32DirectoryTable,
 *                name is directory name,
 *                ext is unused,
 *                parent_cluster_number is target directory table to read,
 *                buffer_size must be exactly sizeof(struct FAT32DirectoryTable)
 * @return Error code: 0 success - 1 not a folder - 2 not found - -1 unknown - 3
 * invalid parent cluster
 */
int8_t read_directory(struct FAT32DriverRequest request);

/**
 * FAT32 read, read a file from file system.
 *
 * @param request All attribute will be used for read, buffer_size will limit
 * reading count
 * @return Error code: 0 success - 1 not a file - 2 not enough buffer - 3 not
 * found - -1 unknown - 4 invalid parent cluster
 */

int8_t read(struct FAT32DriverRequest request);

/**
 * FAT32 write, write a file or folder to file system.
 *
 * @param request All attribute will be used for write, buffer_size == 0 then
 * create a folder / directory
 * @return Error code: 0 success - 1 file/folder already exist - 2 invalid
 * parent cluster - 3 forbidden name - -1 unknown
 */
int8_t write(struct FAT32DriverRequest request);

/**
 * FAT32 delete, delete a file or empty directory (only 1 DirectoryEntry) in
 * file system.
 *
 * @param request buf and buffer_size is unused
 * @param is_recursive whether deletion should be recursive. Value only affects subdirectory deletion and doesn't affect file
 * @param check_recursion whether the given directory have been checked to be shallow enough for recursion. Should be TRUE during initial call
 * @return Error code: 0 success - 1 not found - 2 folder is not empty - -1
 * unknown - 4 invalid parent cluster
 * - 5 folder structure too deep for recursion
 */
int8_t delete(struct FAT32DriverRequest request, bool is_recursive, bool check_recursion);

/* -- Getter/Setter  Auxiliary Function -- */

/**
 * @brief Whether an entry is already occupied by a directory
 *
 * @param entry
 * @return true entry is not occupied
 * @return false entry is occupied
 */
bool is_entry_empty(struct FAT32DirectoryEntry *entry);

/**
 * @brief Whether name of directory in request is the same as name of directory in entry
 *
 * @param entry entry to be compared against req
 * @param req request to be compared against entry
 * @return true name is same
 * @return false name is different
 */
bool is_dir_name_same(struct FAT32DirectoryEntry *entry,
                      struct FAT32DriverRequest req);

/**
 * @brief Whether extension of directory in request is the same as name of directory in entry
 *
 * @param entry entry to be compared against req
 * @param req request to be compared against entry
 * @return true extension is same
 * @return false extension is different
 */
bool is_dir_ext_same(struct FAT32DirectoryEntry *entry,
                     struct FAT32DriverRequest req);

/**
 * @brief Whether extension and name of directory in request is the same as name of directory in entry
 *
 * @param entry entry to be compared against req
 * @param req request to be compared against entry
 * @return true extension and name is same
 * @return false extension and name is different
 */
bool is_dir_ext_name_same(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req);

/**
 * @brief Check user_attribute of an entry
 *
 * @param entry
 * @return true
 * @return false
 */
bool is_subdirectory(struct FAT32DirectoryEntry *entry);

/**
 * @brief Ceiling of 2 a and b
 *
 * @param a
 * @param b
 * @return int ceiling of a/b
 */
int ceil(int a, int b);

bool is_subdirectory_recursively_empty(struct FAT32DirectoryEntry *entry);

bool is_subdirectory_immediately_empty(struct FAT32DirectoryEntry *entry);

/**
 * @brief Whether a subdirectory table in a cluster is full
 *
 * @param subdir the directory table to be checked
 * @return true table is full
 * @return false table is not full
 */
bool is_subdirectory_cluster_full(struct FAT32DirectoryTable *subdir);

/**
 * @brief Whether a subdirectory table in a cluster is empty
 *
 * @param subdir the directory table to be checked
 * @return true table is empty
 * @return false table is not empty
 */
bool is_subdirectory_cluster_empty(struct FAT32DirectoryTable *subdir);

/**
 * @brief Whether the parent cluster to operate on is a valid cluster number or
 * not
 *
 * @return true
 * @return false
 */
bool is_parent_cluster_valid(struct FAT32DriverRequest);

bool is_dirtable_child(struct FAT32DirectoryTable *subdir);

/**
 * @brief Get the number of cluster occupied by a subdirectory
 *
 * @param entry
 * @return uint32_t
 */
uint32_t get_n_of_cluster_subdir(struct FAT32DirectoryEntry *entry);

bool is_requested_directory_already_exist(struct FAT32DriverRequest req);

void increment_subdir_n_of_entry(struct FAT32DirectoryTable *table);

void decrement_subdir_n_of_entry(struct FAT32DirectoryTable *table);

uint32_t get_subdir_n_of_entry(struct FAT32DirectoryTable *table);

/* -- CRUD Auxiliary Function -- */

/**
 * @brief Create a folder into a directory entry
 *
 * @param cluster_number The cluster to which the folder is to be put
 * @param entry The directory entry to be used by the folder
 * @param req The request that contains information about folder creation
 */
void create_subdirectory_from_entry(uint32_t cluster_number,
                                    struct FAT32DirectoryEntry *entry,
                                    struct FAT32DriverRequest req);

/**
 * @brief Create a file into a directory entry
 *
 * @param cluster_number The cluster to which the file is to be put
 * @param entry The directory entry to be used by the file
 * @param req The request that contains information about file creation
 */
void create_file_from_entry(uint32_t cluster_number,
                            struct FAT32DirectoryEntry *entry,
                            struct FAT32DriverRequest req);

/**
 * @brief Reset cluster_number values when deleted
 *
 * @param cluster_number The cluster number to delete
 */
void reset_cluster(uint32_t cluster_number);

/**
 * @brief Delete a directory entry that is a folder
 *
 * @param entry The entry to delete
 * @param req The request that contains information about deletion
 */
void delete_subdirectory_by_entry(struct FAT32DirectoryEntry *entry,
                                  struct FAT32DriverRequest req);

/**
 * @brief Delete a directory entry that is a file
 *
 * @param entry The entry to delete
 * @param req The request that contains information about deletion
 */
void delete_file_by_entry(struct FAT32DirectoryEntry *entry,
                          struct FAT32DriverRequest req);

/**
 * @brief Read a directory entry
 *
 * @param entry The entry to read
 * @param req The request to which read result is to be transferred
 */
void read_directory_by_entry(struct FAT32DirectoryEntry *entry,
                             struct FAT32DriverRequest req);

/**
 * @brief Read a directory
 *
 * @param cluster_number The intial cluster_number of directory
 * @param req The request to which read result is to be transferred
 */
void read_directory_by_cluster_number(uint16_t cluster_number,
                                      struct FAT32DriverRequest req);

/**
 * @brief Create a child cluster of a subdirectory
 *
 * @param last_occupied_cluster_number The last cluster number in the FAT table that is to-be occupied
 * @param prev_cluster_number The cluster number where the last cluster used by the directory is
 * @param req Request that is used by the caller
 * @return true Creating child cluster succesful
 * @return false Creating child cluster not succesful
 */
bool create_child_cluster_of_subdir(uint32_t last_occupied_cluster_number, uint16_t prev_cluster_number, struct FAT32DriverRequest *req);

/* -- Timestamp Management -- */

/**
 * @brief Set the directory entry's create_date and create_time to current date and time
 * @param entry
 */
void set_create_datetime(struct FAT32DirectoryEntry *entry);

/**
 * @brief Set the directory entry's modified_date to current date
 * @param entry
 */
void set_modified_date(struct FAT32DirectoryEntry *entry);

/**
 * @brief Set the directory entry's access_date and access_time to current date and time
 * @param entry
 */
void set_access_datetime(struct FAT32DirectoryEntry *entry);

/**
 * @brief Check whether the depth of the directory is deeper than the MAX_RECURSIVE_OP_DEPTH
 *
 * @param lower_cluster_number the cluster number of the directory to be checked
 * @param recursion_count the number of recursion so far
 * @return true
 * @return false
 */
bool is_below_max_recursion_depth(uint16_t target_cluster_number, uint8_t recursion_count);

#endif
