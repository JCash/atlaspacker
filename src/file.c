
#include <atlaspacker/file.h>

#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // strcmp

#if defined(_WIN32)
    #include "win32/dirent.h"
#else
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <dirent.h>
#endif


int IsFile(const char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISREG(path_stat.st_mode);
}

int IsDir(const char* path)
{
    struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

int IterateFiles(const char* dirpath, int recursive, int (*callback)(void* ctx, const char*), void* ctx)
{
    struct dirent* entry = 0;
    DIR* dir = 0;
    dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Failed to open dir: '%s'\n", dirpath);
        return 0;
    }

    while( (entry = readdir(dir)) )
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char abs_path[2048];
        snprintf(abs_path, sizeof(abs_path), "%s/%s", dirpath, entry->d_name);

        int isdir = IsDir(abs_path);
        if (!isdir)
            callback(ctx, abs_path);

        if (isdir && recursive) {

            // Make sure the directory still exists (the callback might have removed it!)
            int should_continue = IterateFiles(abs_path, recursive, callback, ctx);
            if (!should_continue) {
                goto cleanup;
            }
        }
    }

cleanup:
    closedir(dir);
    return 1;
}

uint8_t* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    uint32_t length = (uint32_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* data = (uint8_t*)malloc(length+1);
    fread(data, 1, length, file);
    fclose(file);

    data[length] = 0; // in case it's a string
    return data;
}

