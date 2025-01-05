
#include "sys.h"

#include <limits.h> // PATH_MAX
#include <string.h>
#include <stdlib.h> // getenv
#include <stdio.h> // printf
#include <unistd.h> // getcwd

extern "C" {
    #include <atlaspacker/file.h>
}

const char* GetWorkingDir(char* buffer, uint32_t buffer_size)
{
    return getcwd(buffer, (size_t)buffer_size);
}


#if defined(__APPLE__)
#include <mach-o/dyld.h>

const char* GetApplicationPath(char* buffer, uint32_t buffer_size)
{
    if(!_NSGetExecutablePath(buffer, &buffer_size))
    {
        char* last = strrchr(buffer, '/');
        if (!last)
            last = strrchr(buffer, '\\');
        if (last)
            *last = 0;
        if (IsDir(buffer))
            return buffer;
        return 0;
    }
    return 0;

    // NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // // print out raw args
    // NSMutableArray* arguments = [NSMutableArray array];
    // for (NSUInteger i = 0; i < argc; i++) {
    //         NSString *argument = [NSString stringWithUTF8String:argv[i]];
    //         if (argument) [arguments addObject:argument];
    // }

    // const char *executablePath = [[[[NSProcessInfo processInfo] arguments] objectAtIndex:0]
    //                                         fileSystemRepresentation];
    // const char *executableDir = [[[[[NSProcessInfo processInfo] arguments] objectAtIndex:0]
    //                                         stringByDeletingLastPathComponent] fileSystemRepresentation];

    // [pool release];
}
#else
    #error "Unsupported platform"
#endif


void GetEnvVarDirs(const char* name, void (*fn)(void* ctx, const char* path), void *ctx)
{
    const char* var = getenv(name);
    if (!var)
        return;

    char buffer[PATH_MAX] = "";
    uint32_t buffer_size = sizeof(buffer);

    const char* cursor = var;
    while (*cursor)
    {
        const char* delim = strchr(cursor, ',');
        size_t len;
        if (delim)
            len = (size_t)(delim - cursor);
        else
            len = strlen(cursor); // Last string in the list

        memcpy(buffer, cursor, len);
        buffer[len] = 0;

        fn(ctx, buffer);

        cursor += len + (delim ? 1 : 0);
    }
}

