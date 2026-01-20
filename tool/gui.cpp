// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "gui.h"
#include "commands.h"
#include "state.h"

extern "C" {
    #include <atlaspacker/exporter.h>
    #include <atlaspacker/file.h>
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <imgui.h>
#include <imgui_internal.h> // Until the dock builder API is stable


static void SetSelections(TreeNode* node, int select)
{
    node->selected = select;
    TreeNode* child = node->child;
    while (child)
    {
        SetSelections(child, select);
        child = child->sibling;
    }
}

static void CheckSelectNode(TreeNode* root, TreeNode* node)
{
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
    {
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
        {
            SetSelections(node, !node->selected);
        }
        else
        {
            SetSelections(root, 0);
            node->selected = 1;
        }
    }
}

static void ShowToolTipImage(AppState* state, TreeNode* node)
{
    Image* image = GetImage(state, node->path_hash);
    if (!image)
        return;

    if (!ImGui::BeginTooltip())
        return;

    AppTexture* texture = (AppTexture*)image->context;
    if (!texture)
    {
        texture = CreateTextureFromImage(image);
        image->context = texture;
    }

    ImGui::Text("Path: %s", image->path);
    ImGui::Text("Size: %u x %u x %u", image->width, image->height, image->channels);

    if (texture && texture->texture_id)
    {
        ImVec2 uv0 = {0,0};
        ImVec2 uv1 = {1,1};
        ImVec2 size(image->width, image->height);
        ImGui::Image(texture->texture_id, size, uv0, uv1);
    }

    ImGui::EndTooltip();
}

static apOptionValue* CloneOptionList(const apOptionValue* src)
{
    apOptionValue* head = 0;
    apOptionValue* tail = 0;
    while (src)
    {
        apOptionValue* option = (apOptionValue*)malloc(sizeof(apOptionValue));
        memset(option, 0, sizeof(*option));

        option->type = src->type;
        option->name = src->name ? strdup(src->name) : 0;
        option->edit = src->edit ? strdup(src->edit) : 0;
        option->desc = src->desc ? strdup(src->desc) : 0;
        option->display = src->display ? strdup(src->display) : 0;
        if (src->type == OVT_STRING)
        {
            option->value.string = src->value.string ? strdup(src->value.string) : 0;
        }
        else
        {
            option->value.number = src->value.number;
        }

        if (!head)
            head = option;
        else
            tail->next = option;
        tail = option;
        src = src->next;
    }
    return head;
}

static void CacheExporterOptions(AppState* state, const char* exporter_name, apOptionValue* options)
{
    if (!exporter_name || !options)
        return;

    hash_t key = Hash(exporter_name);
    apOptionValue** existing = state->exporter_options_cache.Get(key);
    if (existing)
    {
        apDestroyOptions(*existing);
        state->exporter_options_cache.Erase(key);
    }

    if (state->exporter_options_cache.Full())
    {
        uint32_t cap = state->exporter_options_cache.Capacity();
        state->exporter_options_cache.SetCapacity(cap ? (cap + 8) : 8);
    }

    state->exporter_options_cache.Put(key, options);
}

static apOptionValue* TakeCachedExporterOptions(AppState* state, const char* exporter_name)
{
    if (!exporter_name)
        return 0;

    hash_t key = Hash(exporter_name);
    apOptionValue** cached = state->exporter_options_cache.Get(key);
    if (!cached)
        return 0;

    apOptionValue* options = *cached;
    state->exporter_options_cache.Erase(key);
    return options;
}

static void SetActiveExporter(AppState* state, apProject* project, const char* exporter_name)
{
    if (!exporter_name || !*exporter_name)
        return;

    bool same_exporter = project->exporter && strcmp(project->exporter, exporter_name) == 0;
    if (same_exporter && project->exporter_defaults && project->exporter_options)
        return;

    if (!same_exporter && project->exporter_options)
    {
        CacheExporterOptions(state, project->exporter, project->exporter_options);
        project->exporter_options = 0;
    }

    if (!same_exporter)
    {
        free((void*)project->exporter);
        project->exporter = strdup(exporter_name);
    }

    apDestroyOptions(project->exporter_defaults);
    project->exporter_defaults = 0;

    free((void*)state->exporter_path);
    state->exporter_path = 0;

    char path[PATH_MAX];
    const char* exporter_path = FindExporter(state, exporter_name, path, sizeof(path));
    if (exporter_path)
    {
        state->exporter_path = strdup(exporter_path);
        project->exporter_defaults = apExportGetDefaultOptions(project, state->exporter_path);
    }
    else
    {
        fprintf(stderr, "Failed to find exporter '%s/exporter.lua'\n", exporter_name);
    }

    if (!same_exporter)
    {
        project->exporter_options = TakeCachedExporterOptions(state, exporter_name);
    }

    if (!project->exporter_options && project->exporter_defaults)
    {
        project->exporter_options = CloneOptionList(project->exporter_defaults);
    }
}

struct ExporterListContext
{
    jc::Array<const char*>* names;
};

static void FreeExporterNames(jc::Array<const char*>& names)
{
    for (size_t i = 0; i < names.Size(); ++i)
        free((void*)names[i]);
    names.SetSize(0);
}

static bool HasExporterName(const jc::Array<const char*>& names, const char* name)
{
    for (size_t i = 0; i < names.Size(); ++i)
    {
        if (strcmp(names[i], name) == 0)
            return true;
    }
    return false;
}

static void AddExporterName(jc::Array<const char*>& names, char* name)
{
    if (!name || !*name)
    {
        free(name);
        return;
    }

    if (HasExporterName(names, name))
    {
        free(name);
        return;
    }

    if (names.Full())
        names.SetCapacity(names.Capacity() + 1);
    names.Push(name);
}

static int ExporterFileIterator(void* _ctx, const char* path)
{
    ExporterListContext* ctx = (ExporterListContext*)_ctx;
    const char* basename = strrchr(path, '/');
    basename = basename ? (basename + 1) : path;
    if (strcmp(basename, "exporter.lua") != 0)
        return 1;

    const char* dir_end = basename - 1;
    if (dir_end <= path)
        return 1;

    const char* dir_start = dir_end;
    while (dir_start > path && *(dir_start - 1) != '/')
        dir_start--;

    size_t len = (size_t)(dir_end - dir_start);
    if (len == 0)
        return 1;

    char* name = (char*)malloc(len + 1);
    memcpy(name, dir_start, len);
    name[len] = 0;
    AddExporterName(*ctx->names, name);
    return 1;
}

static void CollectExporters(AppState* state, jc::Array<const char*>& names)
{
    ExporterListContext ctx = {};
    ctx.names = &names;
    for (uint32_t i = 0; i < state->exporter_folders.Size(); ++i)
    {
        if (IsDir(state->exporter_folders[i]))
            IterateFiles(state->exporter_folders[i], 1, ExporterFileIterator, &ctx);
    }
    if (state->prefs)
    {
        for (uint32_t i = 0; i < state->prefs->exporter_folders.Size(); ++i)
        {
            if (IsDir(state->prefs->exporter_folders[i]))
                IterateFiles(state->prefs->exporter_folders[i], 1, ExporterFileIterator, &ctx);
        }
    }
}


static void DrawImageListTree(AppState* state, TreeNode* root, TreeNode* node)
{
    int is_folder = node->type == TN_TYPE_FOLDER;
    Image* image = GetImage(state, node->path_hash);

    const char* name = node->path;
    if (!is_folder)
    {
        name = strrchr(name, '/');
        while ((*name) == '/')
            name++;
    }

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    int selected = node->selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    if (is_folder)
    {
        bool open = ImGui::TreeNodeEx(name, selected | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("--");
        if (open)
        {
            TreeNode* child = node->child;
            while (child)
            {
                DrawImageListTree(state, root, child);
                child = child->sibling;
            }

            CheckSelectNode(root, node);

            ImGui::TreePop();
        }
    }
    else
    {
        if (ImGui::TreeNodeEx(name, selected | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick))
        {
            CheckSelectNode(root, node);

            if (state->selected_images.Full())
            {
                uint32_t cap = state->selected_images.Capacity() + 32;
                state->selected_images.SetCapacity(cap);
            }
            state->selected_images.Put(node->path_hash, node->selected || ImGui::IsItemHovered(ImGuiHoveredFlags_None));

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
            {
                printf("TODO: Open image in sprite editor: %s\n", name);
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            {
                ShowToolTipImage(state, node);
            }
        }

        ImGui::TableNextColumn();
        if (image)
            ImGui::TextDisabled("%d x %d x %d", image->width, image->width, image->channels);
        else
            ImGui::TextDisabled("--");
    }
}

static void DrawImageList(AppState* state)
{
    ImGui::Separator();

    const char* text = "A";
    ImVec2 tsz = ImGui::CalcTextSize(text, text+1);

    ImVec2 wsize = ImGui::GetWindowSize();

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (ImGui::BeginTable("Images", 2, table_flags, ImVec2(wsize.x - ImGui::GetStyle().ScrollbarSize, wsize.y -100)))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, tsz.x * 18.0f);
        ImGui::TableHeadersRow();

        if (!state->project || !state->project->num_sources)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("Drop");
            ImGui::TableNextColumn(); ImGui::TextDisabled("--");
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("files");
            ImGui::TableNextColumn(); ImGui::TextDisabled("--");
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("here!");
            ImGui::TableNextColumn(); ImGui::TextDisabled("--");
        }
        else if (state->images_root)
        {
            // TODO: Add a trylock to the thread api
            SCOPED_MUTEX(state->mutex);

            TreeNode* node = state->images_root->child;
            while (node)
            {
                DrawImageListTree(state, state->images_root, node);
                node = node->sibling;
            }
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::Indent(16);

    bool disabled = false;
    {
        SCOPED_MUTEX(state->mutex);
        disabled = state->modal_dialog != 0 ||
                   state->loading_images; // thread
    }

    ImGui::BeginDisabled(disabled);

        if (ImGui::Button("Add Image(s)"))
        {
            // See comment below
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: Since the file dialog mustn't be opened in the
            // scope of a sokol frame, we need to delay it.
            // And since the ImGui::Button() reacts on mouse UP, and the Sokol
            // on_event callback happends before this, we need to start the process on
            // a left click
            CommandAddImageFile(state->uithread, state);
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Add Folder"))
        {
            // See comment above
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: see comment above
            CommandAddImageFolder(state->uithread, state);
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
    ImGui::EndDisabled();

    ImGui::NewLine();
}

static void DrawPackerOptions(AppState* state)
{
    apProject* project = state->project;
    int dirty = 0;

    ImGui::BeginGroup();

    ImGui::Text("General Packer Options");

    int page_sizes[8] = { 0 };
    int num_page_sizes = 1;
    int page_size_index = 0;
    int current_size = apIsPowerOfTwo(state->max_image_size) ? state->max_image_size : apNextPowerOfTwo(state->max_image_size);
    while (current_size <= 16384)
    {
        if (project->options.page_size == current_size)
        {
            page_size_index = num_page_sizes;
        }
        page_sizes[num_page_sizes++] = current_size;
        current_size *= 2;
    }

    // Our input list is dynamic, as it's dependent on the biggest image size
    char page_size_str[64] = "Dynamic";
    if (page_size_index)
        snprintf(page_size_str, sizeof(page_size_str), "%d", page_sizes[page_size_index]);

    if (ImGui::SliderInt("Page Size (texels)", &page_size_index, 0, num_page_sizes-1, page_size_str, 0))
    {
        int old_size = project->options.page_size;
        int new_size = page_sizes[page_size_index];
        if (old_size != new_size)
        {
            project->options.page_size = new_size;
            dirty = 1;
        }
    }

    ImGui::EndGroup();

    ImGui::Separator();

    PackerType packer_types[] = {PT_TILEPACKER, PT_BINPACKER};
    const char* packertype_items[] = {"TilePacker", "BinPacker"};
    int num_packertype_items = sizeof(packertype_items)/sizeof(packertype_items[0]);

    int packer_type_index = project->packer_type;
    if (ImGui::Combo("Packer Type", &packer_type_index, packertype_items, num_packertype_items, 0))
    {
        project->packer_type = packer_types[packer_type_index];

        dirty = 1;
    }

    ImGui::Separator();

    ImGui::Text("Packer Type Specific options");

    if (project->packer_type == PT_TILEPACKER)
    {
        apTilePackerOptions* options = &project->options_tp;

        int tile_sizes[] = {1, 2, 4, 8, 16, 32, 64};
        int num_tile_sizes = sizeof(tile_sizes)/sizeof(tile_sizes[0]);
        int tile_size_index = 0;
        for (int i = 0; i < num_tile_sizes; ++i)
        {
            if (options->tile_size == tile_sizes[i])
            {
                tile_size_index = i;
                break;
            }
        }

        char tile_size_str[64];
        snprintf(tile_size_str, sizeof(tile_size_str), "%d", tile_sizes[tile_size_index]);

        if (ImGui::SliderInt("Tile Size (texels)", &tile_size_index, 0, num_tile_sizes-1, tile_size_str, 0))
        {
            int old_size = options->tile_size;
            int new_size = tile_sizes[tile_size_index];
            if (old_size != new_size)
            {
                options->tile_size = new_size;
                dirty = 1;
            }
        }

        bool no_rotate = (bool)options->no_rotate;
        if (ImGui::Checkbox("No Rotate", &no_rotate))
        {
            options->no_rotate = (int)no_rotate;
            dirty = 1;
        }

        if (ImGui::SliderInt("Padding (texels)", &options->padding, 0, 16, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            dirty = 1;
        }

        if (ImGui::SliderInt("Alpha threshold", &options->alpha_threshold, 1, 255, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            dirty = 1;
        }
    }
    else if (project->packer_type == PT_BINPACKER)
    {
        apBinPackerOptions* options = &project->options_bp;

        const char* binpackertype_items[] = {"Default (Skyline BL)", "Skyline Bottom Left"};
        int num_binpackertype_items = sizeof(binpackertype_items)/sizeof(binpackertype_items[0]);

        int binpacker_type_index = (int)options->mode;
        if (ImGui::Combo("Bin Packer Mode", &binpacker_type_index, binpackertype_items, num_binpackertype_items, 0))
        {
            options->mode = (apBinPackMode)binpacker_type_index;
            dirty = 1;
        }

        bool no_rotate = (bool)options->no_rotate;
        if (ImGui::Checkbox("No Rotate", &no_rotate))
        {
            options->no_rotate = (int)no_rotate;
            dirty = 1;
        }
    }

    ImGui::Separator();

    ImGui::Text("Debug options");

    if (ImGui::Checkbox("Draw Triangles", &state->debug_draw_triangles))
    {
    }

    if (dirty)
    {
        CommandRecreateAtlas(state->thread, state);
    }
}

static void DrawAtlasPages(AppState* state)
{
    SCOPED_MUTEX(state->mutex);

    if (!state->project || !state->project->context)
        return;

    ImGui::Text("Atlas: %zu pages, %d x %d", state->page_textures.Size(), state->page_size.width, state->page_size.height);

    ImGui::BeginChild("#atlas_texture");

    ImVec2 size = ImGui::GetWindowSize();

    if (ImGui::IsKeyDown(ImGuiKey_MouseWheelY) && ImGui::IsKeyDown(ImGuiMod_Ctrl))
    {
        const float zoom_speed = 0.01f;
        state->zoom += ImGui::GetIO().MouseWheel * zoom_speed;
        if (state->zoom < 0.02f)
            state->zoom = 0.02f;
        if (state->zoom > 3.0f)
            state->zoom = 3.0f;
    }

    size.x *= state->zoom;
    size.y *= state->zoom;

    for (int i = 0; i < state->page_textures.Size(); ++i)
    {
        ImVec2 uv0 = {0,0};
        ImVec2 uv1 = {1,1};
        ImGui::SameLine(0, 0);

        ImVec2 start_pos = ImGui::GetCursorScreenPos();

        if (state->page_textures[i]->texture_id == 0)
            continue;

        ImGui::Image(state->page_textures[i]->texture_id, size, uv0, uv1);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 pos = start_pos;
        pos.x += size.x * 0.5f;
        pos.y += size.y * 0.5f;

        ImVec2 b = start_pos;
        b.x += size.x * 0.75f;
        b.y += size.y * 0.75f;

        apPage* page = apGetPage(state->project->context, i);
        if (!page)
            continue;
        apImage* image = apPageGetFirstImage(page);

        float width = (float)page->dimensions.width;
        float height = (float)page->dimensions.height;
        while (image)
        {
            bool do_draw = state->debug_draw_triangles;
            if (!do_draw)
            {
                hash_t path_hash = Hash(image->path);
                bool* selected = state->selected_images.Get(path_hash);
                if (selected)
                    do_draw |= *selected;
            }

            if (do_draw)
            {
                int num_vertices = image->num_vertices;
                apPosf* vertices = image->vertices;
                for (int v0 = 0; v0 < num_vertices; ++v0)
                {
                    int v1 = (v0+1)%num_vertices;
                    apPosf p0 = vertices[v0];
                    apPosf p1 = vertices[v1];

                    // convert to units
                    p0.x /= width;
                    p0.y /= height;
                    p1.x /= width;
                    p1.y /= height;

                    // convert to window size
                    p0.x *= size.x;
                    p0.y *= size.y;
                    p1.x *= size.x;
                    p1.y *= size.y;

                    // // Add window start pos
                    p0.x += start_pos.x;
                    p0.y += start_pos.y;
                    p1.x += start_pos.x;
                    p1.y += start_pos.y;

                    ImVec2 a = { p0.x, p0.y };
                    ImVec2 b = { p1.x, p1.y };
                    draw_list->AddLine(a, b, 0xFF00F0FF, 1.0f);
                }
            }

            image = image->next;
        }
    }

    ImGui::EndChild();
}

static void DrawPreferences(AppState* state)
{
    if (!state->prefs)
        return;

    Preferences* prefs = state->prefs;
    ImGui::Separator();

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (ImGui::BeginListBox("Exporter Folders"))
    {
        for (int i = prefs->exporter_folders.Size()-1; i >= 0; i--)
        {
            const char* folder = prefs->exporter_folders[i];
            bool selected = state->selected_exporter_folder == i;
            if (ImGui::Selectable(folder, &selected))
            {
                state->selected_exporter_folder = i;
            }
        }

        ImGui::BeginDisabled();
        for (int i = 0; i < state->exporter_folders.Size(); ++i)
        {
            const char* folder = state->exporter_folders[i];
            ImGui::Text(folder);
        }
        ImGui::EndDisabled();

        ImGui::EndListBox();
    }

    ImGui::Separator();
    ImGui::Indent(16);

    bool disabled = false;
    {
        SCOPED_MUTEX(state->mutex);
        disabled = state->modal_dialog != 0;
    }

    ImGui::BeginDisabled(disabled);

        if (ImGui::Button("Add Folder"))
        {
            // See comment below
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: Since the file dialog mustn't be opened in the
            // scope of a sokol frame, we need to delay it.
            // And since the ImGui::Button() reacts on mouse UP, and the Sokol
            // on_event callback happends before this, we need to start the process on
            // a left click
            CommandFolderOpen(state->uithread, state, [](void* _ctx, const char* path) {
                AppState* ctx = (AppState*)_ctx;
                if (ctx->prefs->exporter_folders.Full())
                    ctx->prefs->exporter_folders.SetCapacity(ctx->prefs->exporter_folders.Capacity()+1);
                ctx->prefs->exporter_folders.Push(strdup(path));
            }, (void*)state);
        }

    ImGui::EndDisabled();

    ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);

    ImGui::BeginDisabled(disabled || (state->selected_exporter_folder == -1));

        if (ImGui::Button("Remove Folder"))
        {
            if (state->selected_exporter_folder != -1)
            {
                prefs->exporter_folders.Erase(state->selected_exporter_folder);
                state->selected_exporter_folder = -1;
            }
        }

    ImGui::EndDisabled();

    ImGui::NewLine();

    // ImGui::BeginDisabled(true);
    //     ImGui::Text("Path: %s", project_path);
    // ImGui::EndDisabled();
}

static bool DrawOption(apOptionValue* option, apOptionValue* base)
{
    if (!base)
        base = option;

    const char* name = base->display ? base->display : base->name;
    const char* edit = base->edit ? base->edit : (option->edit ? option->edit : "");
    const char* desc = base->desc;
    bool dirty = false;

    switch(base->type)
    {
    case OVT_BOOL:
        {
            bool value = option->value.number != 0;
            if ((dirty = ImGui::Checkbox(name, &value)))
            {
                option->value.number = value;

                if (desc)
                    ImGui::SetItemTooltip("%s", desc);
            }
        } break;

    case OVT_NUMBER:
        {
            if (strcmp(edit, "int") == 0)
            {
                int value = (int)option->value.number;
                if ((dirty = ImGui::InputInt(name, &value)))
                {
                    option->value.number = value;

                    if (desc)
                        ImGui::SetItemTooltip("%s", desc);
                }
            }
            else
            {
                double value = option->value.number;
                if ((dirty = ImGui::InputDouble(name, &value)))
                {
                    option->value.number = value;

                    if (desc)
                        ImGui::SetItemTooltip("%s", desc);
                }
            }
        } break;

    case OVT_STRING:
        {
            const char* value = option->value.string;

            char buffer[1024];
            size_t buffer_size = sizeof(buffer);
            strncpy(buffer, value, buffer_size);

            if (strcmp(edit, "file") == 0)
            {
                if (ImGui::Button("..."))
                {
                    // TODO: Open file selection!

                    // MAKE PATH RELATIVE to the project file!

                    if (desc)
                        ImGui::SetItemTooltip("%s", desc);
                }

                ImGui::SameLine(0);

                if ((dirty = ImGui::InputText(name, buffer, buffer_size)))
                {
                    free((void*)option->value.string);
                    option->value.string = strdup(buffer);

                    if (desc)
                        ImGui::SetItemTooltip("%s", desc);
                }
            }
            else
            {
                if ((dirty = ImGui::InputText(name, buffer, buffer_size)))
                {
                    free((void*)option->value.string);
                    option->value.string = strdup(buffer);

                    if (desc)
                        ImGui::SetItemTooltip("%s", desc);
                }
            }
        } break;
    }

    return dirty;
}

static apOptionValue* FindOptionByName(apOptionValue* options, const char* name)
{
    while (options)
    {
        if (strcmp(name, options->name) == 0)
            return options;
        options = options->next;
    }
    return 0;
}

static bool DrawOptions(apOptionValue* options, apOptionValue* defaults)
{
    // TODO: Check if we're altering a default option.
    // if so, we need to add the altered option to the project options

    bool dirty = false;
    while (defaults)
    {
        apOptionValue* option = FindOptionByName(options, defaults->name);
        if (!option)
            option = defaults;
        dirty |= DrawOption(option, defaults);
        defaults = defaults->next;
    }
    return dirty;
}

static void DrawExporterOptions(AppState* state)
{
    SCOPED_MUTEX(state->mutex);

    apProject* project = state->project;

    if (!project || !project->context)
        return;

    ImGui::BeginGroup();

    ImGui::Separator();

    jc::Array<const char*> exporter_names;
    CollectExporters(state, exporter_names);
    int num_exporter_names = (int)exporter_names.Size();

    const char* current_exporter = project->exporter;
    int exporter_name_index = 0;

    if (current_exporter && num_exporter_names > 0)
    {
        for (exporter_name_index = 0; exporter_name_index < num_exporter_names; ++exporter_name_index)
        {
            if (strcmp(exporter_names[exporter_name_index], current_exporter) == 0)
                break;
        }
    }

    if (num_exporter_names == 0)
    {
        ImGui::TextDisabled("No exporters found");
    }
    else if (ImGui::Combo("Exporter", &exporter_name_index, exporter_names.Begin(), num_exporter_names, 0))
    {
        SetActiveExporter(state, project, exporter_names[exporter_name_index]);
    }

    FreeExporterNames(exporter_names);

    ImGui::Separator();

    ImGui::Text("Exporter Options");

    bool dirty = false;
    if (project->exporter_defaults)
    {
        dirty |= DrawOptions(project->exporter_options, project->exporter_defaults);
    }

    ImGui::EndGroup();

    ImGui::Separator();
}

void DrawEditor(AppState* state, int width, int height)
{
    ImGui::SetNextWindowPos(ImVec2(0,0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);

    ImGui::Begin("#main", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    static bool dock_init = true;
    if (dock_init)
    {
        dock_init = false;

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

        ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(width, height));

        ImGuiID dockLeft;
        ImGuiID dockRight;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.3f, &dockLeft, &dockRight);

        int left_size = width/3;
        if (left_size < 300)
            left_size = 300;
        ImGui::DockBuilderSetNodeSize(dockLeft, ImVec2(left_size, height));

        ImGui::DockBuilderDockWindow("#settings", dockLeft);
        ImGui::DockBuilderDockWindow("#textures", dockRight);

        ImGui::DockBuilderFinish(dockspace_id);

    }

    ImGui::DockSpace(dockspace_id, ImVec2(0,0), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar, 0);
    ImGui::End(); // # main

    // === UI CODE STARTS HERE ===

    ImGui::Begin("#settings", 0, ImGuiWindowFlags_MenuBar);

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            bool open_requested = false;
            bool open_activated = ImGui::MenuItem("Open...", "CTRL+O");
            bool open_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            // macOS: Since the file dialog mustn't be opened in the
            // scope of a sokol frame, we need to delay it.
            // And since the ImGui::Button() reacts on mouse UP, and the Sokol
            // on_event callback happens before this, we need to start the process on
            // a left click
            if (open_clicked)
            {
                state->menu_skip_open_release = 1;
                open_requested = true;
            }
            if (open_activated)
            {
                if (state->menu_skip_open_release)
                {
                    state->menu_skip_open_release = 0;
                }
                else
                {
                    open_requested = true;
                }
            }
            if (open_requested)
            {
                CommandProjectFileOpen(state->uithread, state);
            }

            bool save_requested = false;
            bool save_activated = ImGui::MenuItem("Save", "CTRL+S");
            bool save_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            // macOS: See comment above
            if (save_clicked)
            {
                state->menu_skip_save_release = 1;
                save_requested = true;
            }
            if (save_activated)
            {
                if (state->menu_skip_save_release)
                {
                    state->menu_skip_save_release = 0;
                }
                else
                {
                    save_requested = true;
                }
            }
            if (save_requested)
            {
                CommandProjectFileSave(state->uithread, state);
            }

            bool export_requested = false;
            bool export_activated = ImGui::MenuItem("Export", "CTRL+E");
            bool export_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

            // macOS: See comment above
            if (export_clicked)
            {
                state->menu_skip_export_release = 1;
                export_requested = true;
            }
            if (export_activated)
            {
                if (state->menu_skip_export_release)
                {
                    state->menu_skip_export_release = 0;
                }
                else
                {
                    export_requested = true;
                }
            }
            if (export_requested)
            {
                CommandProjectFileExport(state->uithread, state);
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (ImGui::BeginTabBar("#tabs"))
    {
        if (ImGui::BeginTabItem("Images", 0, ImGuiTabItemFlags_None))
        {
            DrawImageList(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Packer", 0, ImGuiTabItemFlags_None))
        {
            DrawPackerOptions(state);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Exporter", 0, ImGuiTabItemFlags_None))
        {
            DrawExporterOptions(state);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::Begin("#textures");
        if (ImGui::BeginTabBar("#textures_tabs"))
        {
            if (ImGui::BeginTabItem("Pages", 0, ImGuiTabItemFlags_None))
            {
                DrawAtlasPages(state);
                ImGui::EndTabItem();
            }

            if (state->show_preferences)
            {
                if (ImGui::BeginTabItem("Preferences", 0, ImGuiTabItemFlags_None))
                {
                    DrawPreferences(state);
                    ImGui::EndTabItem();
                }
            }

            ImGui::EndTabBar();
        }
    ImGui::End();

    //ImGui::ShowDemoWindow();
}
