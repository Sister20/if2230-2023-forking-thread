#include "lib-header/stdtype.h"
#include "lib-header/fat32.h"
#include "lib-header/stdmem.h"
#include "lib-header/framebuffer.h"

#define SHELL_BUFFER_SIZE 256
#define COMMAND_MAX_SIZE 32
#define COMMAND_COUNT 12
#define DIRECTORY_NAME_LENGTH 8
#define EXTENSION_NAME_LENGTH 3
#define INDEXES_MAX_COUNT SHELL_BUFFER_SIZE
#define PATH_MAX_COUNT 64
#define MAX_FILE_BUFFER_CLUSTER_SIZE 512 // take arbitrary size of 512 cluster = 512 * 4 * 512 B = 1MB
#define MAX_FOLDER_CLUSTER_SIZE 5
#define EMPTY_EXTENSION "\0\0\0"
#define EMPTY_NAME "\0\0\0\0\0\0\0\0"

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
};

struct IndexInfo
{
    int index;
    int length;
};

struct IndexInfo defaultIndexInfo = {
    .index = -1,
    .length = 0};

struct ParseString
{
    char word[SHELL_BUFFER_SIZE];
    int length;
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
    char newl[1] = "\n";
    syscall(5, (uint32_t)newl, 1, 0xF);
}

void print_space()
{
    char newl[1] = " ";
    syscall(5, (uint32_t)newl, 1, 0xF);
}

void reset_indexes(struct IndexInfo *indexes, uint32_t length)
{

    uint32_t i = 0;
    while (i < length)
    {
        indexes[i].index = defaultIndexInfo.index;
        indexes[i].length = defaultIndexInfo.length;

        i++;
    }
}

void reset_buffer(char *buf, int length)
{
    for (int i = 0; i < length; i++)
        buf[i] = '\0';
}

void get_buffer_indexes(char *buf, struct IndexInfo *indexes, char delimiter, int starting_index, int buffer_length)
{

    // contoh penggunaan:
    // buf = " 12 45 79    \0"
    // diperoleh indexes = [[1, 2], [4, 2], [7, 2]] untuk array di dalam dalam format [indeks, length]

    int i = starting_index;
    int limit = i + buffer_length;
    int count = 0;

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
        char command[COMMAND_MAX_SIZE];

        reset_buffer(command, COMMAND_MAX_SIZE);
        memcpy(command, buf + starting_index, command_length);
        if (memcmp(command, command_list[i], COMMAND_MAX_SIZE) == 0)
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

/**
 * Set char buffer of ParseString data struct with input string with certain size
 * @param parse_string  ParseString data to be filled with input string
 * @param str           the string to be copied into parse_string
 * @param size          the size of string to be copied into parse_string
 */
void set_ParseString(struct ParseString *parse_string, char *str, int size)
{
    memcpy(parse_string->word, str, size);
    parse_string->length = size;
}

/**
 * Split filename into name and extension
 * @param filename original filename before split
 * @param name final filename length after removed of extension
 * @param extension extension of the file
 * @return
 * 0 - if successfully split with filename not empty and extension not empty,
 * 1 - if file does not have extension,
 * 2 - if file has no name
 * 3 - if file name or extension is too long
 */
int split_filename_extension(struct ParseString *filename,
                             struct ParseString *name,
                             struct ParseString *extension)
{
    name->length = 0;
    extension->length = 0;

    // parse filename to name and extension
    struct IndexInfo temp_index[INDEXES_MAX_COUNT];
    reset_indexes(temp_index, INDEXES_MAX_COUNT);
    get_buffer_indexes(filename->word, temp_index, '.', 0, filename->length);

    int words_count = get_words_count(temp_index);

    if (words_count == 1)
    {
        // filename has no extension
        if (temp_index[0].index == 0)
        {
            if (temp_index[0].length > DIRECTORY_NAME_LENGTH)
                return 3;
            // copy name
            memcpy(name->word, filename->word, temp_index[0].length);
            name->length = temp_index[0].length;
            return 1;
        }
        // file has no name (why?)
        return 2;
    }

    int last_word_starting_index = temp_index[words_count - 1].index;
    int last_word_length = temp_index[words_count - 1].length;
    // starting from 0 to (last_word_starting_index - 2) is file name
    int name_length = last_word_starting_index - 1; // therefore (last_word_starting_index - 2) + 1 is length of name

    if (name_length > DIRECTORY_NAME_LENGTH || last_word_length > EXTENSION_NAME_LENGTH)
        return 3;
    // copy name
    memcpy(name->word, filename->word, name_length);
    name->length = name_length;
    // copy extension
    memcpy(extension->word, &(filename->word[last_word_starting_index]), last_word_length);
    extension->length = last_word_length;
    return 0;
}

uint8_t cd_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo *info)
{
    // return 0 if fail, 1 if succeed

    struct CurrentDirectoryInfo temp_info = {};
    copy_directory_info(&temp_info, info);

    struct IndexInfo param_indexes[INDEXES_MAX_COUNT];
    reset_indexes(param_indexes, INDEXES_MAX_COUNT);

    bool is_empty_param = (uint32_t)indexes == 0;

    if ((!is_empty_param && buf[indexes->index] == '/') || is_empty_param)
    {
        temp_info.current_cluster_number = ROOT_CLUSTER_NUMBER;
        temp_info.current_path_count = 0;
    }

    if (!is_empty_param)
    {
        get_buffer_indexes(buf, param_indexes, '/', indexes->index, indexes->length);

        int i = 0;

        while (i < get_words_count(param_indexes))
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

            else if (!(temp_info.current_cluster_number == ROOT_CLUSTER_NUMBER && memcmp("root\0\0\0\0", buf + param_indexes[i].index, param_indexes[i].length) == 0))
            {
                if (param_indexes[i].length > DIRECTORY_NAME_LENGTH)
                {
                    char msg[] = "Directory name is too long: ";
                    syscall(5, (uint32_t)msg, 29, 0xF);
                    syscall(5, (uint32_t)buf + param_indexes[i].index, param_indexes[i].length, 0xF);
                    print_newline();

                    return 0;
                }
                struct ClusterBuffer cl[MAX_FOLDER_CLUSTER_SIZE];

                struct FAT32DriverRequest request = {
                    .buf = &cl,
                    .name = "root\0\0\0\0",
                    .ext = "\0\0\0",
                    .parent_cluster_number = temp_info.current_cluster_number,
                    .buffer_size = CLUSTER_SIZE * MAX_FOLDER_CLUSTER_SIZE,
                };

                struct FAT32DirectoryTable *dir_table;

                if (temp_info.current_path_count > 0)
                {
                    syscall(6, (uint32_t)&request, 0, 0);

                    dir_table = request.buf;

                    memcpy(request.name, temp_info.paths[temp_info.current_path_count - 1], DIRECTORY_NAME_LENGTH);

                    request.parent_cluster_number = dir_table->table->cluster_low;
                }

                int32_t retcode;

                syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

                dir_table = request.buf;
                uint32_t j = 0;
                bool found = FALSE;

                while (j < dir_table->table->filesize / CLUSTER_SIZE && !found)
                {
                    int k = 1;

                    while (k < dir_table[j].table->n_of_entries && !found)
                    {
                        struct FAT32DirectoryEntry *entry = &dir_table[j].table[k];

                        char name[DIRECTORY_NAME_LENGTH] = "\0\0\0\0\0\0\0\0";

                        memcpy(name, buf + param_indexes[i].index, param_indexes[i].length);

                        if (memcmp(entry->name, name, DIRECTORY_NAME_LENGTH) == 0 && entry->attribute == ATTR_SUBDIRECTORY)

                        {
                            if (temp_info.current_path_count == PATH_MAX_COUNT - 1)
                            {
                                char msg[] = "cd command reaches maximum depth\n";
                                syscall(5, (uint32_t)msg, 34, 0xF);
                                return 0;
                            }

                            temp_info.current_cluster_number = entry->cluster_low;
                            memcpy(temp_info.paths[temp_info.current_path_count], name, DIRECTORY_NAME_LENGTH);
                            temp_info.current_path_count++;
                            found = TRUE;
                        }

                        k++;
                    }

                    j++;
                }

                if (!found)
                {
                    char msg[] = "Failed to read directory ";
                    syscall(5, (uint32_t)msg, 26, 0xF);
                    syscall(5, (uint32_t)buf + param_indexes[i].index, param_indexes[i].length, 0xF);
                    print_newline();

                    char errorMsg[] = "Error: directory not found\n";
                    syscall(5, (uint32_t)errorMsg, 28, 0xF);

                    return 0;
                }
            }

            i++;
        }
    }

    copy_directory_info(info, &temp_info);
    return 1;
}

void ls_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo info)
{
    struct CurrentDirectoryInfo temp_info = {};
    copy_directory_info(&temp_info, &info);

    if ((uint32_t)indexes != 0)
    {
        int status = cd_command(buf, indexes, &temp_info);
        if (!status)
            return;
    }

    struct ClusterBuffer cl[MAX_FOLDER_CLUSTER_SIZE];
    struct FAT32DriverRequest request = {
        .buf = &cl,
        .name = "root\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = temp_info.current_cluster_number,
        .buffer_size = CLUSTER_SIZE * MAX_FOLDER_CLUSTER_SIZE,
    };

    if (temp_info.current_path_count > 0)
    {
        syscall(6, (uint32_t)&request, 0, 0);

        struct FAT32DirectoryTable *dir_table = request.buf;

        request.parent_cluster_number = dir_table->table->cluster_low;
        memcpy(request.name, temp_info.paths[temp_info.current_path_count - 1], DIRECTORY_NAME_LENGTH);
    }

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
                uint32_t color;

                if (dirTable[i].table[j].attribute == ATTR_SUBDIRECTORY)
                    color = 0xa;
                else
                    color = 0xf;
                syscall(5, (uint32_t)dirTable[i].table[j].name, DIRECTORY_NAME_LENGTH, color);

                if (dirTable[i].table[j].attribute != ATTR_SUBDIRECTORY && memcmp(dirTable[i].table[j].ext, "\0\0\0", 3) != 0)
                {
                    char point_str[] = ".";
                    syscall(5, (uint32_t)point_str, 1, color);
                    syscall(5, (uint32_t)dirTable[i].table[j].ext, 3, color);
                }

                if (j < dirTable[i].table->n_of_entries - 1 || i < dir_table_count - 1)
                    print_space();
                j++;
            }

            i++;
        }

        print_newline();
    }

    else
    {
        char msg[] = "Failed to read directory ";
        syscall(5, (uint32_t)msg, 26, 0xF);
        syscall(5, (uint32_t)request.name, DIRECTORY_NAME_LENGTH, 0xF);
        print_newline();
    }
}

/**
 * Parse raw path to become useable for cd command
 *
 * @param buf              input command buffer
 * @param indexes          initial IndexInfo struct that contains the raw path at index 1
 * @param new_path_indexes new path after parsing
 *
 * @return -
 */
void parse_path_for_cd(char *buf, struct IndexInfo *indexes, struct IndexInfo *new_path_indexes)
{
    reset_indexes(new_path_indexes, INDEXES_MAX_COUNT);
    // [path_segment_1] [path_segment_2] [path_segment_3] ...
    get_buffer_indexes(buf, new_path_indexes, '/', indexes[0].index, indexes[0].length);
}

/**
 * Invoking cd command from another command
 *
 * @param buf                   input command buffer
 * @param indexes               indexes after split on ' '
 * @param target_directory      new directory info after invoking cd command
 * @param target_name           parsed target name from buffer
 *
 * @return 0: fail;
 *         1: success;
 */
uint8_t invoke_cd(char *buf,
                  struct IndexInfo *indexes,
                  struct CurrentDirectoryInfo *target_directory,
                  struct ParseString *target_name)
{
    struct IndexInfo new_path_indexes[INDEXES_MAX_COUNT];
    parse_path_for_cd(buf, indexes, new_path_indexes);

    int last_word_index = get_words_count(new_path_indexes) - 1;
    int new_buf_length = new_path_indexes[last_word_index - 1].index + new_path_indexes[last_word_index - 1].length;
    target_name->length = new_path_indexes[last_word_index].length;

    memcpy(target_name->word, buf + new_path_indexes[last_word_index].index, target_name->length);

    if (get_words_count(new_path_indexes) > 1)
    {
        char new_buf[new_buf_length];
        for (int i = 0; i < new_buf_length; i++)
        {
            new_buf[i] = buf[i];
        }

        struct IndexInfo new_buf_indexes[INDEXES_MAX_COUNT];
        reset_indexes(new_buf_indexes, INDEXES_MAX_COUNT);
        // [path_segment_1] [path_segment_2] [path_segment_3] ...
        get_buffer_indexes(new_buf, new_buf_indexes, ' ', 0, new_buf_length);
        // call cd command to move the directory
        return cd_command(new_buf, new_buf_indexes + 1, target_directory);
    }

    return 1;
}

/**
 * Handling mkdir command in shell
 *
 * @param buf     buffer of char from user input
 * @param indexes list of splitted command and path
 * @param info    current directory info
 *
 * @return -
 */
void mkdir_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo info)
{
    struct IndexInfo new_path_indexes[INDEXES_MAX_COUNT];
    parse_path_for_cd(buf, indexes, new_path_indexes);

    int target_name_length = new_path_indexes[get_words_count(new_path_indexes) - 1].length;
    if (target_name_length > 8)
    {
        char msg[] = "Invalid new directory name! Maximum 8 characters!";

        syscall(5, (uint32_t)msg, 49, 0xF);
        print_newline();
        return;
    }

    // temporary CurrentDirectoryInfo for creating the new directory
    struct CurrentDirectoryInfo target_directory = {};
    copy_directory_info(&target_directory, &info);
    struct ParseString target_name = {};

    uint8_t cd_res = invoke_cd(buf, indexes, &target_directory, &target_name);

    if (cd_res == 0)
        return;

    // create new directory in the target_directory
    struct FAT32DriverRequest write_request = {
        .buf = 0,
        .ext = "\0\0\0",
        .parent_cluster_number = target_directory.current_cluster_number,
        .buffer_size = 0,
    };

    memcpy(write_request.name, target_name.word, target_name.length);

    int32_t retcode;
    syscall(2, (uint32_t)&write_request, (uint32_t)&retcode, 0);
}

/**
 * Handling cat command in shell
 *
 * @param buf     buffer of char from user input
 * @param indexes list of splitted command and path
 * @param info    current directory info
 *
 * @return -
 */
void cat_command(char *buf, struct IndexInfo *indexes, struct CurrentDirectoryInfo info)
{
    // temporary CurrentDirectoryInfo for creating the new directory
    struct CurrentDirectoryInfo target_directory = {};
    copy_directory_info(&target_directory, &info);
    struct ParseString target_name = {};

    uint8_t cd_res = invoke_cd(buf, indexes, &target_directory, &target_name);

    if (cd_res == 0)
        return;

    // read the file from FATtable
    struct ClusterBuffer cl[MAX_FILE_BUFFER_CLUSTER_SIZE];
    struct FAT32DriverRequest read_request = {
        .buf = &cl,
        .parent_cluster_number = target_directory.current_cluster_number,
        .buffer_size = CLUSTER_SIZE * MAX_FILE_BUFFER_CLUSTER_SIZE,
    };

    struct ParseString target_filename = {};
    set_ParseString(&target_filename, target_name.word, target_name.length);
    struct ParseString target_file_name_parsed = {};
    struct ParseString target_file_name_extension = {};
    int split_result = split_filename_extension(&target_filename, &target_file_name_parsed, &target_file_name_extension);

    if (split_result != 0 && split_result != 1)
    {
        syscall(5, (uint32_t) "Invalid command!", 16, 0xF);
        print_newline();
        return;
    }

    memcpy(read_request.name, target_file_name_parsed.word, target_file_name_parsed.length);
    memcpy(read_request.ext, target_file_name_extension.word, target_file_name_extension.length);

    int32_t retcode;

    syscall(0, (uint32_t)&read_request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        syscall(5, (uint32_t)read_request.buf, read_request.buffer_size, 0xF);
        print_newline();
    }
    else
    {
        syscall(5, (uint32_t) "File not found!", 15, 0xF);
        print_newline();
    }
}
/**
 * Traverse all directories from ROOT to find a folder or file with name equals to target_name
 *
 * @param target_name           file or folder name
 * @param parent_cluster_number parent cluster number for the corresponding folder
 *
 * @return 0                     : target_name not found;
 *         parent_cluster_number : target_name found at file or folder with parent_cluster_number;
 */
uint32_t traverse_directories(char *target_name, uint32_t parent_cluster_number)
{
    bool is_found = 0;
    struct ClusterBuffer cl[10];
    struct FAT32DriverRequest read_folder_request = {
        .buf = &cl,
        .ext = EMPTY_EXTENSION,
        .parent_cluster_number = parent_cluster_number,
        .buffer_size = CLUSTER_SIZE * 10,
    };

    memcpy(read_folder_request.name, target_name, sizeof(target_name));

    int32_t retcode;
    syscall(1, (uint32_t)&read_folder_request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        struct FAT32DirectoryTable *dir_table = read_folder_request.buf;

        uint32_t counter_entry = 0;
        for (uint32_t i = 0; i < 10 && !is_found && counter_entry < dir_table[0].table[0].n_of_entries; i++)
        {
            for (uint32_t j = 1; j < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry) && !is_found && counter_entry < dir_table[0].table[0].n_of_entries; i++)
            {
                counter_entry++;
                if (memcmp(dir_table[i].table[j].name, target_name, sizeof(target_name)) == 0)
                {
                    is_found = parent_cluster_number;
                }
                else if (dir_table[i].table[j].attribute == ATTR_SUBDIRECTORY)
                {
                    is_found = traverse_directories(target_name, (dir_table[i].table[j].cluster_high << 16) +
                                                                     dir_table[i].table[j].cluster_low);
                }
            }
        }
    }

    return is_found;
}

/**
 * Calling syscall type-5 for printing the path after whereis command
 *
 * @param parent_cluster_number folder or file parent_cluster_number
 *
 * @return -
 */
void print_path(uint32_t cluster_number)
{
    // root found - base condition
    if (cluster_number == ROOT_CLUSTER_NUMBER)
    {
        syscall(5, (uint32_t) "/", 1, 0xF);
        return;
    }

    struct ClusterBuffer cl[5];
    struct FAT32DriverRequest read_folder_request = {
        .buf = &cl,
        .ext = EMPTY_EXTENSION,
        .parent_cluster_number = cluster_number,
        .buffer_size = CLUSTER_SIZE * 5,
    };

    int32_t retcode;
    syscall(6, (uint32_t)&read_folder_request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        struct FAT32DirectoryTable *dir_table = read_folder_request.buf;

        print_path((dir_table->table->cluster_high << 16) + dir_table->table->cluster_low);
        syscall(5, (uint32_t) "/", 1, 0xF);
        syscall(5, (uint32_t)dir_table->table->name, 8, 0xF);
        print_newline();
    }
}

/**
 * cp command in shell, copy the file in specified source directory to the specified destination directory with new name
 * @param source_dir    directory of file to be copied
 * @param source_name   name of file to be copied in the specified directory
 * @param dest_dir      write destination directory of file to be copied
 * @param dest_name     name of file to be written in the destination directory
 * @return  0: copy succeeded
 *          1: unable to read file
 *          2: unable to write file
 */
uint8_t cp_command(struct CurrentDirectoryInfo *source_dir,
                   struct ParseString *source_name,
                   struct CurrentDirectoryInfo *dest_dir,
                   struct ParseString *dest_name)
{
    // prepare buffer in memory for copying
    struct ClusterBuffer cl[MAX_FILE_BUFFER_CLUSTER_SIZE];

    /* READING STAGE */

    struct ParseString name;
    struct ParseString ext;
    int splitcode;

    // split source filename to name and extension
    splitcode = split_filename_extension(source_name, &name, &ext);
    if (splitcode == 2 || splitcode == 3)
    {
        char msg[] = "Source file not found!\n";
        syscall(5, (uint32_t)msg, 24, 0xF);

        // return;

    }

    // prepare read file request
    struct FAT32DriverRequest read_request = {
        .buf = cl,
        // .name = EMPTY_NAME,
        .ext = EMPTY_EXTENSION,
        .parent_cluster_number = source_dir->current_cluster_number,
        .buffer_size = CLUSTER_SIZE * MAX_FILE_BUFFER_CLUSTER_SIZE,
    };
    memcpy(read_request.name, name.word, name.length);
    memcpy(read_request.ext, ext.word, ext.length);

    // copy file to buffer memory
    int32_t retcode;
    syscall(0, (uint32_t)&read_request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        // read file to buffer success
        /* WRITING STAGE */

        // split source filename to name and extension
        splitcode = split_filename_extension(dest_name, &name, &ext);
        if (splitcode == 2 || splitcode == 3)
        {
            char msg[] = "Source file not found!\n";
            syscall(5, (uint32_t)msg, 24, 0xF);
            // return;
        }

        // prepare write file request
        struct FAT32DriverRequest write_request = {
            .buf = cl,
            // .name = EMPTY_NAME,
            .ext = EMPTY_EXTENSION,
            .parent_cluster_number = dest_dir->current_cluster_number,
            .buffer_size = CLUSTER_SIZE * MAX_FILE_BUFFER_CLUSTER_SIZE,
        };
        memcpy(write_request.name, name.word, name.length);
        memcpy(write_request.ext, ext.word, ext.length);

        // copy file from memory to disk
        syscall(2, (uint32_t)&write_request, (uint32_t)&retcode, 0);
        if (retcode)
        {
        }
    }
    else
    {
        // try read folder
    }
    return 0;
}

/**
 * rm command in shell, removes the specified file in the specified directory
 * @param file_dir  directory of file to be removed
 * @param file_name name of file in the specified directory to be removed
 *
 * @return  -
 */
int8_t rm_command(struct CurrentDirectoryInfo *file_dir, struct ParseString *file_name)
{
    struct ParseString name;
    struct ParseString ext;
    int splitcode;

    // split source filename to name and extension
    splitcode = split_filename_extension(file_name, &name, &ext);
    if (splitcode == 2 || splitcode == 3)
    {
        char msg[] = "Source file not found!\n";
        syscall(5, (uint32_t)msg, 24, 0xF);
        return 1;
    }

    // create delete request
    struct FAT32DriverRequest delete_request = {
        // .name = EMPTY_NAME,
        .ext = EMPTY_EXTENSION,
        .parent_cluster_number = file_dir->current_cluster_number,
    };
    memcpy(delete_request.name, name.word, name.length);
    memcpy(delete_request.ext, ext.word, ext.length);

    int8_t retcode;
    syscall(3, (uint32_t)&delete_request, (uint32_t)&retcode, 0);

    return retcode;
}

void mv_command(struct CurrentDirectoryInfo *source_dir,
                struct ParseString *source_name,
                struct CurrentDirectoryInfo *dest_dir,
                struct ParseString *dest_name)
{
    cp_command(source_dir,
               source_name,
               dest_dir,
               dest_name);

    rm_command(source_dir, source_name);
}

int main(void)
{
    const int DIRECTORY_DISPLAY_OFFSET = 24;
    char buf[SHELL_BUFFER_SIZE];
    struct IndexInfo word_indexes[INDEXES_MAX_COUNT];

    char too_many_args_msg[] = "Too many arguments\n";
    char missing_args_msg[] = "Missing argument(s)\n";

    struct CurrentDirectoryInfo current_directory_info =
        {
            .current_cluster_number = ROOT_CLUSTER_NUMBER,
            .current_path_count = 0,
        };

    // fungsi atas-atas belum fix :D

    while (TRUE)
    {
        reset_buffer(buf, SHELL_BUFFER_SIZE);
        reset_indexes(word_indexes, INDEXES_MAX_COUNT);

        int DIRECTORY_DISPLAY_LENGTH = DIRECTORY_DISPLAY_OFFSET + (current_directory_info.current_path_count * (DIRECTORY_NAME_LENGTH + 1));

        if (current_directory_info.current_path_count == 0)
            DIRECTORY_DISPLAY_LENGTH++;

        char directoryDisplay[DIRECTORY_DISPLAY_LENGTH];

        memcpy(directoryDisplay, "forking-thread-IF2230:/", DIRECTORY_DISPLAY_OFFSET);

        char slash[] = "/";

        for (uint32_t i = 0; i < current_directory_info.current_path_count; i++)
        {
            int offset = (i * (DIRECTORY_NAME_LENGTH + 1)) + DIRECTORY_DISPLAY_OFFSET;
            if (i > 0)
                memcpy(directoryDisplay + offset - 1, slash, 1);
            memcpy(directoryDisplay + offset, current_directory_info.paths[i], DIRECTORY_NAME_LENGTH);
        }

        memcpy(directoryDisplay + DIRECTORY_DISPLAY_LENGTH - 1, "$", 1);

        syscall(5, (uint32_t)directoryDisplay, DIRECTORY_DISPLAY_LENGTH, 0xF);

        syscall(4, (uint32_t)buf, SHELL_BUFFER_SIZE, 0);

        get_buffer_indexes(buf, word_indexes, ' ', 0, SHELL_BUFFER_SIZE);

        int argsCount = get_words_count(word_indexes);

        if (argsCount > 0)
        {
            int commandNumber = get_command_number(buf, word_indexes[0].index, word_indexes[0].length);

            if (commandNumber == -1)
            {
                char msg[] = "Command not found!\n";
                syscall(5, (uint32_t)msg, 19, 0xF);
            }

            else
            {

                if (commandNumber == 0)
                {
                    if (argsCount == 1)
                    {
                        cd_command(buf, (uint32_t)0, &current_directory_info);
                    }
                    else if (argsCount == 2)
                    {
                        cd_command(buf, word_indexes + 1, &current_directory_info);
                    }

                    else
                        syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                }

                else if (commandNumber == 1)
                {
                    if (argsCount == 1)
                        ls_command(buf, (uint32_t)0, current_directory_info);

                    else if (argsCount == 2)
                        ls_command(buf, word_indexes + 1, current_directory_info);
                    else
                        syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                }

                else if (commandNumber == 2)
                {
                    if (argsCount == 1)
                    {
                        syscall(5, (uint32_t) "Please give the folder path and name!\n", 39, 0xF);
                    }
                    else if (argsCount == 2)
                    {
                        mkdir_command(buf, word_indexes + 1, current_directory_info);
                    }
                    else
                    {
                        syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                    }
                }

                else if (commandNumber == 3)
                {
                    if (argsCount == 1)
                    {
                        syscall(5, (uint32_t) "Please give the file path and name!\n", 39, 0xF);
                    }
                    else if (argsCount == 2)
                    {
                        cat_command(buf, word_indexes + 1, current_directory_info);
                    }
                    else
                    {
                        syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                    }
                }

                else if (commandNumber == 4)
                {
                    // cp_command

                    if (argsCount < 3)
                    {
                        syscall(5, (uint32_t)missing_args_msg, 21, 0xF);
                    }

                    else if (argsCount == 3)
                    {

                        struct CurrentDirectoryInfo source_dir = {
                            .current_cluster_number = current_directory_info.current_cluster_number};
                        struct CurrentDirectoryInfo dest_dir = {
                            .current_cluster_number = current_directory_info.current_cluster_number};

                        struct ParseString source_name;
                        struct ParseString dest_name;

                        // get source directory info & source file name
                        invoke_cd(buf, word_indexes+1, &source_dir, &source_name);

                        // get destination directory info & source file name
                        invoke_cd(buf, word_indexes+2, &dest_dir, &dest_name);

                        // invoke cp command
                        cp_command(&source_dir, &source_name, &dest_dir, &dest_name);
                    }

                    else
                    {
                        syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                    }
                }

                else if (commandNumber == 5)
                {
                    // rm_command

                    if (argsCount < 2)
                    {
                        syscall(5, (uint32_t)missing_args_msg, 21, 0xF);
                    }

                    else
                    {
                        bool isFlagFound = FALSE;

                        if (argsCount == 2)
                        {
                            if (isFlagFound)
                            {
                                syscall(5, (uint32_t)missing_args_msg, 21, 0xF);
                            }

                            else
                            {
                                struct CurrentDirectoryInfo target_dir;
                                copy_directory_info(&target_dir,&current_directory_info);

                                struct ParseString target_name = {};
                                reset_buffer(target_name.word, SHELL_BUFFER_SIZE);

                                // get source directory info & source file name
                                invoke_cd(buf, word_indexes+1, &target_dir, &target_name);

                                // invoke rm command
                                rm_command(&target_dir, &target_name);
                            }
                        }

                        else if (argsCount == 3)
                        {
                            
                        }

                        else
                        {
                            syscall(5, (uint32_t)too_many_args_msg, 20, 0xF);
                        }
                    }
                }

                else if (commandNumber == 6)
                {
                    // mv_command
                    struct CurrentDirectoryInfo source_dir = {};
                    struct CurrentDirectoryInfo dest_dir = {};

                    copy_directory_info(&source_dir, &current_directory_info);
                    copy_directory_info(&dest_dir, &current_directory_info);

                    struct ParseString source_name = {};
                    struct ParseString dest_name = {};

                    // get source directory info & source file name
                    invoke_cd(buf, word_indexes+1, &source_dir, &source_name);

                    // get destination directory info & source file name
                    invoke_cd(buf, word_indexes+2, &dest_dir, &dest_name);

                    // invoke mv command
                    mv_command(&source_dir, &source_name, &dest_dir, &dest_name);
                }

                else if (commandNumber == 7)
                {
                }
            }

            if (current_directory_info.current_cluster_number)
            {
            }
        }
    }

    return 0;
}
