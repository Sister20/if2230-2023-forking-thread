#include "lib-header/stdtype.h"
#include "lib-header/fat32.h"
#include "lib-header/stdmem.h"

#define SHELL_BUFFER_SIZE     256
#define COMMAND_MAX_SIZE      32
#define COMMAND_COUNT         12
#define DIRECTORY_NAME_LENGTH 8
#define INDEXES_MAX_COUNT     SHELL_BUFFER_SIZE
#define PATH_MAX_COUNT        256
const char command_list[COMMAND_COUNT][COMMAND_MAX_SIZE] = {
    "cd\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "ls\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "mkdir\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "cat\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "cp\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "rm\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "mv\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "whereis\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0",
};

struct CurrentDirectoryInfo
{
    char paths[PATH_MAX_COUNT][DIRECTORY_NAME_LENGTH];
    uint32_t current_path_count;
    uint16_t current_cluster_number;
    // char current_directory_name[8];
} __attribute__((packed));

struct IndexInfo {
    int index;
    int length;
} __attribute__((packed));

struct IndexInfo defaultIndexInfo = {
    .index = -1,
    .length = 0,
};

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

void get_buffer_indexes(char* buf, struct IndexInfo* indexes, char delimiter, int starting_index, int buffer_length) {

    // contoh penggunaan:
    // buf = " 12 45 79    \0"
    // diperoleh indexes = [[1, 2], [4, 2], [7, 2]] untuk array di dalam dalam format [indeks, length]

    int i = starting_index;
    int limit = i + buffer_length;
    int count = 0;
    
    reset_indexes(indexes);

    while (i < limit && buf[i] != '\0')
    {
        while(i < limit && buf[i] == delimiter && buf[i] != '\0') i++;

        if (i >= limit || buf[i] == '\0') break;

        indexes[count].index = i;

        while(i < limit && buf[i] != delimiter && buf[i] != '\0') i++;

        indexes[count].length = (i - indexes[count].index);

        count++;
    }
}

int get_command_number(char* buf, int starting_index, int command_length) {
    // return -1 if command not found
    // return command_list number if found

    if (command_length > COMMAND_MAX_SIZE) return -1;

    for (int i = 0; i < COMMAND_COUNT; i++)
    {
        if (memcmp(buf + starting_index, command_list[i], command_length) == 0) return i;
    }

    return -1;
}

void reset_indexes(struct IndexInfo* indexes)
{
    for (int i = 0; i < INDEXES_MAX_COUNT; i++) {
        indexes[i] = defaultIndexInfo;
    }
}

bool is_default_index(struct IndexInfo index_info)
{
    return index_info.index == -1 && index_info.length == 0;
}

int get_words_count(struct IndexInfo* indexes)
{
    int count = 0;
    while (count < INDEXES_MAX_COUNT && !is_default_index(indexes[count])) count++;

    return count;
}

void cd_command(char* buf, struct IndexInfo* indexes, struct CurrentDirectoryInfo* info)
{
    if (get_words_count(indexes) != 2)
    {
        // tulis parameter cd tidak valid
        return;
    }
    
    struct IndexInfo param_indexes[INDEXES_MAX_COUNT];
    reset_indexes(param_indexes);

    get_buffer_indexes(buf, param_indexes, '/', indexes[1].index, indexes[1].length);

    int i = 0;

    if (buf[indexes[1].index] == '/') 
    {
        info->current_cluster_number = ROOT_CLUSTER_NUMBER;
        info->current_path_count = 0;
    }

    while(i < INDEXES_MAX_COUNT && !is_default_index(param_indexes[i]))
    {

        if (param_indexes[i].length == 1 && buf[param_indexes[i].index] == '.')
        {
            i++;
            continue;
        }

        else if (param_indexes[i].length == 2 && memcmp(buf+param_indexes[i].index, "..", 2) == 0)
        {
            // TODO :go to parent dir

            if (info->current_cluster_number != ROOT_CLUSTER_NUMBER)
            {
                struct ClusterBuffer cl = {0};
                struct FAT32DriverRequest request = {
                    .buf                   = &cl,
                    .name                  = "\0\0\0\0\0\0\0\0",
                    .ext                   = "\0\0\0",
                    .parent_cluster_number = info->current_cluster_number,
                    .buffer_size           = CLUSTER_SIZE * 5,
                };

                syscall(6, (uint32_t) &request, 0, 0);

                struct FAT32DirectoryTable* dir_table = request.buf; 

                info->current_cluster_number = dir_table->table->cluster_low;
                info->current_path_count--;
            }
        }

        else
        {
            struct ClusterBuffer cl[5];

            struct FAT32DriverRequest request = {
                .buf                   = &cl,
                .name                  = "\0\0\0\0\0\0\0\0",
                .ext                   = "\0\0\0",
                .parent_cluster_number = info->current_cluster_number,
                .buffer_size           = CLUSTER_SIZE * 5,
            };

            syscall(6, (uint32_t) &request, 0, 0);

            struct FAT32DirectoryTable* dir_table = request.buf; 

            memcpy(request.name, info->paths[info->current_path_count-1], DIRECTORY_NAME_LENGTH);
            request.parent_cluster_number = dir_table->table->cluster_low;

            int32_t retcode;

            syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

            dir_table = request.buf; 
            int j = 0;
            bool found = FALSE;

            while (j < dir_table->table->filesize / CLUSTER_SIZE && !found)
            {
                int k = 0;

                while (k < dir_table[j].table->n_of_entries && !found)
                {
                    struct FAT32DirectoryEntry *entry = &dir_table[j].table[k];
                    
                    if (memcmp(entry->name, buf+param_indexes[i].index, param_indexes[i].length) == 0 
                        &&  entry->attribute == ATTR_SUBDIRECTORY)
                    
                    {
                        info->current_cluster_number = entry->cluster_low;
                        memcpy(info->paths[info->current_path_count], request.name, DIRECTORY_NAME_LENGTH); 
                        info->current_path_count++;
                        found = TRUE;
                    }

                    k++;
                }

                j++;
            }
        }

        i++;
    }
}

void ls_command(uint16_t current_cluster_number, struct CurrentDirectoryInfo info)
{
    struct ClusterBuffer cl[5];
    struct FAT32DriverRequest request = {
        .buf                   = &cl,
        .name                  = info.paths[current_cluster_number-1],
        .ext                   = "\0\0\0",
        .parent_cluster_number = info.current_cluster_number,
        .buffer_size           = CLUSTER_SIZE * 5,
    };

    int32_t retcode;

    syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

    if (retcode == 0) {
        struct FAT32DirectoryTable dirTable[5] =  request.buf;

        int i = 0;
        int entry_count = 0;
        int dir_table_count = dirTable->table->filesize / CLUSTER_SIZE;

        while (i < dir_table_count)
        {
            int j = 1;
            while(j < dirTable[i].table->n_of_entries)
            {
                //TODO : output nama file/dir
                j++;
            }

            i++;
        }
    }
}

int main(void) {

    char buf[SHELL_BUFFER_SIZE];
    int word_indexes[INDEXES_MAX_COUNT];
    // char paths[PATH_MAX_COUNT][8];
    // int current_path_count = 0;
    // uint16_t parent_cluster_number = ROOT_CLUSTER_NUMBER;

    struct CurrentDirectoryInfo current_directory_info =
    {
        .current_cluster_number = ROOT_CLUSTER_NUMBER,
        .current_path_count = 0,
    };

    // fungsi atas-atas belum fix :D

    while (TRUE) {

        reset_indexes(word_indexes);

        syscall(4, (uint32_t) buf, SHELL_BUFFER_SIZE, 0);
        syscall(5, (uint32_t) buf, SHELL_BUFFER_SIZE, 0xF); // syscall ini belum implement fungsi put
    }

    return 0;
}

