#include <atlaspacker/path.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void AppendString(char* buffer, size_t buffer_size, size_t* offset, const char* text)
{
    if (!text || *offset >= buffer_size - 1)
        return;

    size_t len = strlen(text);
    if (len > buffer_size - 1 - *offset)
        len = buffer_size - 1 - *offset;

    memcpy(buffer + *offset, text, len);
    *offset += len;
    buffer[*offset] = 0;
}

static void AppendChar(char* buffer, size_t buffer_size, size_t* offset, char value)
{
    if (*offset >= buffer_size - 1)
        return;

    buffer[*offset] = value;
    *offset += 1;
    buffer[*offset] = 0;
}

static int GetCountString(const char* ptr, const char* index_str, char* out_buffer)
{
    if (!ptr || !out_buffer || *ptr != '{' || *(ptr + 1) != 'N')
        return 0;

    const char* cursor = ptr + 1;
    int count = 0;
    while (*cursor && *cursor != '}')
    {
        if (*cursor != 'N')
        {
            fprintf(stderr, "Invalid character in N pattern near '%s'\n", ptr);
            return 0;
        }
        ++count;
        ++cursor;
    }

    if (*cursor != '}')
    {
        fprintf(stderr, "Missing '}' for N pattern near '%s'\n", ptr);
        return 0;
    }

    int index_number = index_str ? atoi(index_str) : 0;
    snprintf(out_buffer, 32, "%0*d", count, index_number);
    return (int)(cursor - ptr + 1);
}

void apPathResolveStringPatterns(char* buffer, size_t buffer_size, const char* pattern,
                                 const char** key_value_pairs, int num_pairs)
{
    size_t offset = 0;
    buffer[0] = 0;

    if (!pattern)
        return;

    const char* index_str = 0;
    if (key_value_pairs)
    {
        for (int i = 0; i < num_pairs; ++i)
        {
            const char* key = key_value_pairs[i * 2];
            if (key && strcmp(key, "index") == 0)
            {
                index_str = key_value_pairs[i * 2 + 1];
                break;
            }
        }
    }

    for (const char* ptr = pattern; *ptr; )
    {
        const char* value = 0;
        int advance = 0;
        char temp[32];

        if (*ptr == '{' && *(ptr + 1) == 'N')
        {
            advance = GetCountString(ptr, index_str, temp);
            if (advance == 0)
                return;
            value = temp;
        }
        else if (key_value_pairs)
        {
            for (int i = 0; i < num_pairs; ++i)
            {
                const char* key = key_value_pairs[i * 2];
                const char* pair_value = key_value_pairs[i * 2 + 1];
                if (!key || !*key)
                    continue;

                size_t key_len = strlen(key);
                if (strncmp(ptr, key, key_len) == 0)
                {
                    value = pair_value ? pair_value : "";
                    advance = (int)key_len;
                    break;
                }
            }
        }

        if (value)
        {
            AppendString(buffer, buffer_size, &offset, value);
            ptr += advance;
            continue;
        }

        AppendChar(buffer, buffer_size, &offset, *ptr);
        ptr++;
    }
}

void apPathSplitProjectPath(const char* project_path, char* out_dir, size_t out_dir_size,
                            char* out_name, size_t out_name_size)
{
    out_dir[0] = 0;
    out_name[0] = 0;

    if (!project_path || !project_path[0])
    {
        strncpy(out_name, "project", out_name_size - 1);
        out_name[out_name_size - 1] = 0;
        return;
    }

    const char* slash = strrchr(project_path, '/');
    const char* backslash = strrchr(project_path, '\\');
    const char* sep = slash;
    if (backslash && (!sep || backslash > sep))
        sep = backslash;

    const char* base = project_path;
    if (sep)
    {
        size_t dir_len = (size_t)(sep - project_path);
        if (dir_len >= out_dir_size)
            dir_len = out_dir_size - 1;
        memcpy(out_dir, project_path, dir_len);
        out_dir[dir_len] = 0;
        while (dir_len > 0 && (out_dir[dir_len - 1] == '/' || out_dir[dir_len - 1] == '\\'))
        {
            out_dir[--dir_len] = 0;
        }
        base = sep + 1;
    }

    if (!base || !base[0])
    {
        strncpy(out_name, "project", out_name_size - 1);
        out_name[out_name_size - 1] = 0;
        return;
    }

    const char* dot = strrchr(base, '.');
    size_t name_len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    if (name_len >= out_name_size)
        name_len = out_name_size - 1;
    memcpy(out_name, base, name_len);
    out_name[name_len] = 0;
}

void apPathNormalize(char* path)
{
    if (!path)
        return;

    char* dst = path;
    char* src = path;
    int prev_slash = 0;

    while (*src)
    {
        char c = *src++;
        if (c == '\\')
            c = '/';

        if (c == '/')
        {
            if (prev_slash)
                continue;
            prev_slash = 1;
        }
        else
        {
            prev_slash = 0;
        }

        *dst++ = c;
    }
    *dst = 0;
}
