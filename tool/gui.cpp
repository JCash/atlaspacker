// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2024 Mathias Westerdahl

#include "gui.h"
#include "commands.h"
#include "state.h"

#include <stdio.h>

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
            if (ImGui::MenuItem("Open...", "CTRL+O"))
            {
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                // macOS: Since the file dialog mustn't be opened in the
                // scope of a sokol frame, we need to delay it.
                // And since the ImGui::Button() reacts on mouse UP, and the Sokol
                // on_event callback happens before this, we need to start the process on
                // a left click
                CommandProjectFileOpen(state->uithread, state);
            }

            if (ImGui::MenuItem("Save", "CTRL+S"))
            {
                CommandProjectFileSave(state->uithread, state);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                // macOS: Since the file dialog mustn't be opened in the
                // scope of a sokol frame, we need to delay it.
                // And since the ImGui::Button() reacts on mouse UP, and the Sokol
                // on_event callback happends before this, we need to start the process on
                // a left click
                CommandProjectFileSave(state->uithread, state);
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
            //DrawExporterOptions();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();

    ImGui::Begin("#textures");
        if (ImGui::BeginTabBar("#textures_tabs"))
        {
            if (ImGui::BeginTabItem("#pages", 0, ImGuiTabItemFlags_None))
            {
                // {
                //     SCOPED_MUTEX(state->mutex);

                //     if (state->pages)
                //         CreateAtlasTextures(state);
                // }

                DrawAtlasPages(state);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    ImGui::End();

    ImGui::ShowDemoWindow();
}
