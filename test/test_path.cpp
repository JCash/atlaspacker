// Copyright (c) 2021-2026 Mathias Westerdahl
// Licensed under the MIT License. See http://opensource.org/licenses/MIT
// https://github.com/JCash/atlaspacker

#include <string.h>

#define JC_TEST_USE_DEFAULT_MAIN
#include <jc_test.h>

extern "C"
{
#include <atlaspacker/path.h>
}

TEST(Path, SplitUnixPath)
{
    char dir[128];
    char name[128];
    apPathSplitProjectPath("/tmp/atlas/test.ap", dir, sizeof(dir), name, sizeof(name));
    ASSERT_STREQ("/tmp/atlas", dir);
    ASSERT_STREQ("test", name);
}

TEST(Path, SplitWindowsPath)
{
    char dir[128];
    char name[128];
    apPathSplitProjectPath("C:\\work\\atlas.ap", dir, sizeof(dir), name, sizeof(name));
    ASSERT_STREQ("C:\\work", dir);
    ASSERT_STREQ("atlas", name);
}

TEST(Path, SplitNoDir)
{
    char dir[128];
    char name[128];
    apPathSplitProjectPath("atlas.ap", dir, sizeof(dir), name, sizeof(name));
    ASSERT_STREQ("", dir);
    ASSERT_STREQ("atlas", name);
}

TEST(Path, ResolveBasicPattern)
{
    char        buffer[256];
    const char* pairs[] = {
        "{project_path}", "/tmp/", "{project_name}", "atlas", "{image_format}", "png", "index", "3"
    };
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_path}{project_name}_{N}.{image_format}", pairs, 4);
    ASSERT_STREQ("/tmp/atlas_3.png", buffer);
}

TEST(Path, ResolveCountPattern)
{
    char        buffer[256];
    const char* pairs[] = {
        "{project_name}", "atlas", "index", "7"
    };
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{N}", pairs, 2);
    ASSERT_STREQ("atlas_7", buffer);
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{NN}", pairs, 2);
    ASSERT_STREQ("atlas_07", buffer);
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{NNN}", pairs, 2);
    ASSERT_STREQ("atlas_007", buffer);

    const char* pairs2[] = {
        "{project_name}", "atlas", "index", "14"
    };

    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{N}", pairs2, 2);
    ASSERT_STREQ("atlas_14", buffer);
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{NN}", pairs2, 2);
    ASSERT_STREQ("atlas_14", buffer);
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{project_name}_{NNN}", pairs2, 2);
    ASSERT_STREQ("atlas_014", buffer);
}

TEST(Path, ResolveMalformedMissingBrace)
{
    char        buffer[256];
    const char* pairs[] = {
        "index", "5"
    };
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{N", pairs, 1);
    ASSERT_STREQ("", buffer);
}

TEST(Path, ResolveMalformedEmptyBraces)
{
    char buffer[256];
    apPathResolveStringPatterns(buffer, sizeof(buffer), "{}", 0, 0);
    ASSERT_STREQ("{}", buffer);
}

TEST(Path, ResolveMalformedNoOpeningBrace)
{
    char buffer[256];
    apPathResolveStringPatterns(buffer, sizeof(buffer), "nameN}", 0, 0);
    ASSERT_STREQ("nameN}", buffer);
}

TEST(Path, NormalizePathSlashes)
{
    char buffer[256];
    strncpy(buffer, "C:\\\\work\\\\//atlas\\\\file.png", sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;
    apPathNormalize(buffer);
    ASSERT_STREQ("C:/work/atlas/file.png", buffer);
}

TEST(Path, NormalizePathNoChange)
{
    char buffer[256];
    strncpy(buffer, "/tmp/atlas/file.png", sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;
    apPathNormalize(buffer);
    ASSERT_STREQ("/tmp/atlas/file.png", buffer);
}
