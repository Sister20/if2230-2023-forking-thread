#include "lib-header/stdtype.h"
#include "lib-header/fat32.h"
#include "lib-header/stdmem.h"
#include "lib-header/framebuffer.h"

#define SHELL_BUFFER_SIZE 256
#define COMMAND_MAX_SIZE 32
#define COMMAND_COUNT 12
#define DIRECTORY_NAME_LENGTH 8
#define INDEXES_MAX_COUNT SHELL_BUFFER_SIZE
#define PATH_MAX_COUNT 256

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

struct IndexInfo
{
    int index;
    int length;
} __attribute__((packed));

struct IndexInfo defaultIndexInfo = {
    .index = -1,
    .length = 0,
};

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx"
                     : /* <Empty> */
                     : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx"
                     : /* <Empty> */
                     : "r"(ecx));
    __asm__ volatile("mov %0, %%edx"
                     : /* <Empty> */
                     : "r"(edx));
    __asm__ volatile("mov %0, %%eax"
                     : /* <Empty> */
                     : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

void print_newline()
{
    syscall(5, "\n", 1, 0xF);
}

void reset_indexes(struct IndexInfo *indexes)
{
    for (int i = 0; i < INDEXES_MAX_COUNT; i++)
    {
        indexes[i] = defaultIndexInfo;
    }
}

void reset_buffer(char* buf, int length)
{   
    for (int i = 0; i < length; i++) buf[i] = '\0';
}


void get_buffer_indexes(char *buf, struct IndexInfo *indexes, char delimiter, int starting_index, int buffer_length)
{

    // contoh penggunaan:
    // buf = " 12 45 79    \0"
    // diperoleh indexes = [[1, 2], [4, 2], [7, 2]] untuk array di dalam dalam format [indeks, length]

    int i = starting_index;
    int limit = i + buffer_length;
    int count = 0;

    reset_indexes(indexes);

    while (i < limit && buf[i] != '\0')
    {
        while (i < limit && buf[i] == delimiter && buf[i] != '\0')
            i++;

        if (i >= limit || buf[i] == '\0')
            break;

        indexes[count].index = i;

        while (i < limit && buf[i] != delimiter && buf[i] != '\0')
            i++;

        indexes[count].length = (i - indexes[count].index);

        count++;
    }
}

int get_command_number(char *buf, int starting_index, int command_length)
{
    // return -1 if command not found
    // return command_list number if found

    if (command_length > COMMAND_MAX_SIZE)
        return -1;

    for (int i = 0; i < COMMAND_COUNT; i++)
    {
        if (memcmp(buf + starting_index, command_list[i], command_length) == 0)
            return i;
    }

    return -1;
}

bool is_default_index(struct IndexInfo index_info)
{
    return index_info.index == -1 && index_info.length == 0;
}

int get_words_count(struct IndexInfo *indexes)
{
    int count = 0;
    while (count < INDEXES_MAX_COUNT && !is_default_index(indexes[count]))
        count++;

    return count;
}

void copy_directory_info(struct CurrentDirectoryInfo *dest, struct CurrentDirectoryInfo *source)
{
    dest->current_cluster_number = source->current_cluster_number;
    dest->current_path_count = source->current_path_count,
    memcpy(dest->paths, source->paths, PATH_MAX_COUNT * DIRECTORY_NAME_LENGTH);
}


void cd_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo *info)
{

    struct CurrentDirectoryInfo temp_info = {};
    copy_directory_info(&temp_info, info);

    struct IndexInfo param_indexes[INDEXES_MAX_COUNT];
    reset_indexes(param_indexes);

    get_buffer_indexes(buf, param_indexes, '/', indexes->index, indexes->length);

    int i = 0;

    if (buf[indexes->index] == '/')
    {
        info->current_cluster_number = ROOT_CLUSTER_NUMBER;
        info->current_path_count = 0;
    }

    while ((uint32_t) indexes != 0 && i < INDEXES_MAX_COUNT && !is_default_index(param_indexes[i]))
    {

        if (param_indexes[i].length == 1 && buf[param_indexes[i].index] == '.')
        {
            i++;
            continue;
        }

        else if (param_indexes[i].length == 2 && memcmp(buf + param_indexes[i].index, "..", 2) == 0)
        {
            // TODO :go to parent dir

            if (temp_info.current_cluster_number != ROOT_CLUSTER_NUMBER)
            {
                struct ClusterBuffer cl = {0};
                struct FAT32DriverRequest request = {
                    .buf = &cl,
                    .name = "\0\0\0\0\0\0\0\0",
                    .ext = "\0\0\0",
                    .parent_cluster_number = temp_info.current_cluster_number,
                    .buffer_size = CLUSTER_SIZE,
                };

                syscall(6, (uint32_t)&request, 0, 0);

                struct FAT32DirectoryTable *dir_table = request.buf;

                temp_info.current_cluster_number = dir_table->table->cluster_low;
                temp_info.current_path_count--;
            }
        }

        else
        {
            if (param_indexes[i].length > DIRECTORY_NAME_LENGTH) 
            {
                syscall(5, "Directory name is too long: ", 29, 0xF);
                syscall(5, buf + param_indexes[i].index, param_indexes[i].length, 0xF);
                print_newline();

                return;
            }
            struct ClusterBuffer cl[5];

            struct FAT32DriverRequest request = {
                .buf = &cl,
                .name = "\0\0\0\0\0\0\0\0",
                .ext = "\0\0\0",
                .parent_cluster_number = temp_info.current_cluster_number,
                .buffer_size = CLUSTER_SIZE * 5,
            };

            syscall(6, (uint32_t)&request, 0, 0);

            struct FAT32DirectoryTable *dir_table = request.buf;

            memcpy(request.name, temp_info.paths[temp_info.current_path_count - 1], DIRECTORY_NAME_LENGTH);
            request.parent_cluster_number = dir_table->table->cluster_low;

            int32_t retcode;

            syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

            if (retcode != 0)
            {
                syscall(5, "Failed to read directory: ", 27, 0xF);
                syscall(5, request.name, DIRECTORY_NAME_LENGTH, 0xF);
                print_newline();

                if (retcode == 1) syscall(5, "Error: not a folder\n", 21, 0xF);
                else if (retcode == 2) syscall(5, "Error: directory not found\n", 21, 0xF); 
                return;
            }

            dir_table = request.buf;
            uint32_t j = 0;
            bool found = FALSE;

            while (j < dir_table->table->filesize / CLUSTER_SIZE && !found)
            {
                int k = 0;

                while (k < dir_table[j].table->n_of_entries && !found)
                {
                    struct FAT32DirectoryEntry *entry = &dir_table[j].table[k];

                    if (memcmp(entry->name, buf + param_indexes[i].index, param_indexes[i].length) == 0 && entry->attribute == ATTR_SUBDIRECTORY)

                    {
                        temp_info.current_cluster_number = entry->cluster_low;
                        memcpy(temp_info.paths[temp_info.current_path_count], request.name, DIRECTORY_NAME_LENGTH);
                        temp_info.current_path_count++;
                        found = TRUE;
                    }

                    k++;
                }

                j++;
            }
        }

        i++;
    }

    copy_directory_info(info, &temp_info);
}

void ls_command(struct CurrentDirectoryInfo info)
{
    struct ClusterBuffer cl[5];
    struct FAT32DriverRequest request = {
        .buf = &cl,
        .ext = "\0\0\0",
        .parent_cluster_number = info.current_cluster_number,
        .buffer_size = CLUSTER_SIZE * 5,
    };

    memcpy(request.name, info.paths[info.current_path_count - 1], DIRECTORY_NAME_LENGTH);

    int32_t retcode;

    syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        struct FAT32DirectoryTable *dirTable = request.buf;

        int i = 0;
        int dir_table_count = dirTable->table->filesize / CLUSTER_SIZE;

        while (i < dir_table_count)
        {
            int j = 1;
            while (j < dirTable[i].table->n_of_entries)
            {
                syscall(5, (uint32_t)dirTable[i].table[j].name, DIRECTORY_NAME_LENGTH, 0xF);
                j++;
            }

            i++;
        }
    }
}

void mkdir_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo *info)
{
    if (get_words_count(indexes) != 2)
    {
        // tulis parameter cd tidak valid
        return;
    }

    // convert "mkdir" to "cd   "
    buf[0] = 'c'; // m
    buf[1] = 'd'; // k
    for (int i = indexes[0].index + 2; i < indexes[0].length; i++)
    {
        buf[i] = ' '; // dir
    }

    struct IndexInfo new_indexes[INDEXES_MAX_COUNT];
    reset_indexes(new_indexes);
    // [cd] [intial_path]
    get_buffer_indexes(buf, new_indexes, ' ', indexes[0].index, indexes[0].length + indexes[1].length);

    struct IndexInfo new_path_indexes[INDEXES_MAX_COUNT];
    reset_indexes(new_path_indexes);
    // [path_segment_1] [path_segment_2] [path_segment_3] ...
    get_buffer_indexes(buf, new_path_indexes, '/', new_indexes[1].index, new_indexes[1].length);

    int i = 0;
    int target_buf_length = 6; // "cd    "
    while (!is_default_deinx(new_path_indexes[i + 1]))
    {
        target_buf_length += new_path_indexes[i].length;
    }

    char *new_dir_name;
    for (int i = target_buf_length; i < new_path_indexes[i].length; i++)
    {
        if (i == 8)
        {
            // return invalid new_dir_name
        }
        new_dir_name[i - target_buf_length] = buf[i];
    }

    // temporary CurrentDirectoryInfo for creating the new directory
    struct CurrentDirectoryInfo target_directory = {};

    copy_directory_info(&target_directory, info);

    // check if path_segment count > 1
    if (!is_default_index(new_path_indexes[1]))
    {
        char *target_buff;
        for (int i = 0; i < target_buf_length; i++)
        {
            target_buff[i] = buf[i];
        }
        // call cd command to move the directory
        cd_command(target_buff, new_indexes+1, &target_directory); 
    }

    // create new directory in the target_directory
    struct ClusterBuffer cl[5];
    struct FAT32DriverRequest write_request = {
        .buf = &cl,
        .ext = "\0\0\0",
        .name = new_dir_name,
        .parent_cluster_number = target_directory.current_cluster_number,
        .buffer_size = CLUSTER_SIZE * 5,
    };

    int32_t retcode;

    syscall(2, (uint32_t)&write_request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        // mkdir command success
    }
    else
    {
        // directory with the corresponding name already exist.
    }
}

void recursive_rm_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo *info)
{
    
}

int main(void)
{
    const int DIRECTORY_DISPLAY_OFFSET = 23;
    char buf[SHELL_BUFFER_SIZE];
    struct IndexInfo word_indexes[INDEXES_MAX_COUNT];

    char too_many_args_msg[] = "Too many arguments\n"; 

    struct CurrentDirectoryInfo current_directory_info =
        {
            .current_cluster_number = ROOT_CLUSTER_NUMBER,
            .current_path_count = 0,
        };

    
    // fungsi atas-atas belum fix :D

    while (TRUE)
    {
        reset_buffer(buf, SHELL_BUFFER_SIZE);
        reset_indexes(word_indexes);

        const int DIRECTORY_DISPLAY_LENGTH = DIRECTORY_DISPLAY_OFFSET + current_directory_info.current_path_count * DIRECTORY_NAME_LENGTH + 1;

        char directoryDisplay [DIRECTORY_DISPLAY_LENGTH];
        
        memcpy(directoryDisplay, "forking-thread-IF2230:", DIRECTORY_DISPLAY_OFFSET);

        for (int i = 0; i < current_directory_info.current_path_count; i++)
        {
            memcpy(directoryDisplay + (i * DIRECTORY_NAME_LENGTH) + DIRECTORY_DISPLAY_OFFSET, current_directory_info.paths[i], DIRECTORY_NAME_LENGTH);
        }

        memcpy(directoryDisplay + DIRECTORY_DISPLAY_LENGTH-1, "$", 1);

        syscall(5, (uint32_t)directoryDisplay, DIRECTORY_DISPLAY_LENGTH, 0xF);

        syscall(4, (uint32_t)buf, SHELL_BUFFER_SIZE, 0);
        syscall(5, (uint32_t)buf, SHELL_BUFFER_SIZE, 0xF);

        get_buffer_indexes(buf, word_indexes, ' ', 0, SHELL_BUFFER_SIZE);

        int commandNumber = get_command_number(buf, word_indexes[0].index, word_indexes[0].length);

        if (commandNumber == -1)
        {
            syscall(5, "Command not found!\n", 19, 0xF);
        }

        else
        {
            int argsCount = get_words_count(word_indexes);

            if (commandNumber == 0)
            {
                if (argsCount == 1) cd_command(buf, (uint32_t) 0, &current_directory_info);
                else if (argsCount == 2) cd_command(buf, word_indexes + 1, &current_directory_info);
                else syscall(5, too_many_args_msg, 20, 0xF);
            }

            if (commandNumber == 1)
            {
                ls_command(current_directory_info);
            }

            if (commandNumber == 2)
            {
                
            }

            if (commandNumber == 3)
            {

            }

            if (commandNumber == 4)
            {
                
            }

            if (commandNumber == 5)
            {
                
            }

            if (commandNumber == 6)
            {

            }

            if (commandNumber == 7)
            {
                
            }

            if (commandNumber == 8)
            {
                
            }
        }

        if (current_directory_info.current_cluster_number)
        {

        }
    }

    return 0;
}
