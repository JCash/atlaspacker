#include <stdint.h>

#include "state.h"
#include "thread.h"
#include "worker.h"

extern "C" {
    #include <atlaspacker/file.h>
    #include <atlaspacker/exporter.h>
    #include <atlaspacker/project.h>

    #include <nfd.h>
}

#include <imgui.h>
#include <imgui_internal.h> // Until the dock builder API is stable

#define SOKOL_APP_IMPL
#define SOKOL_IMPL
#define SOKOL_IMGUI_IMPL
#define SOKOL_GLCORE
#define SOKOL_NO_ENTRY
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sokol_imgui.h>

static const char* VERSION = "0.1";

// #define NOC_FILE_DIALOG_IMPLEMENTATION
// #if defined(__APPLE__)
//     #define NOC_FILE_DIALOG_OSX
// #elif defined(_MSC_VER)
//     #define NOC_FILE_DIALOG_WIN
// #else
//     #define NOC_FILE_DIALOG_GTK
// #endif
// #include <noc_file_dialog.h>

#include <unistd.h> // getcwd

static void OnFileDialogAddImages(AppState* state, const char** paths, int numpaths);

#if defined(__APPLE__)
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_QUIT           SAPP_KEYCODE_Q
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_SUPER
#elif defined(_WIN32)
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_CTRL
#else
    #define KEY_CODE_OPEN           SAPP_KEYCODE_O
    #define KEY_CODE_SAVE           SAPP_KEYCODE_S
    #define KEY_CODE_EXPORT         SAPP_KEYCODE_E
    #define KEY_CODE_QUIT           SAPP_KEYCODE_Q
    #define KEY_CODE_MODIFIERS      SAPP_MODIFIER_SUPER
#endif

// TODO: Move this to AppState
static struct {
    sg_pass_action pass_action;
} SokolActionState;

static const char* GetWindowTitle(const char* path, bool dirty, char* buffer, uint32_t buffer_size)
{
    const char* asterisk = dirty ? "*" : "";
    snprintf(buffer, buffer_size, "AtlasPacker %s: %s%s", VERSION, path?path:"untitled", asterisk);
    return buffer;
}

static void UpdateWindowTitle(AppState* state, bool dirty)
{
    char title[1024];
    GetWindowTitle(state->path, dirty, title, sizeof(title));
    sapp_set_window_title(title);
}

static void OpenFileDialog(AppState* state)
{
    SCOPED_MUTEX(state->mutex);
    state->open_project_dialog = true;
}

static void SaveFile(AppState* state)
{
    SCOPED_MUTEX(state->mutex);
    if (state->path == 0)
    {
        state->save_project_dialog = true;
    }
    else {
        apSaveProject(state->path, state->project);
        UpdateWindowTitle(state, false);
    }
}

static void ExportFile(AppState* state)
{
    SCOPED_MUTEX(state->mutex);

    if (!state->project)
    {
        // pass
    }
    else
    {
        // TODO: Each exporter could expose settings.
        // We could store those settings as json, and pass them on to this function when exporting
        const char* exporter_path = "exporters/defold/exporter.lua";
        const char* output_path = "/Users/mathiaswesterdahl/work/projects/users/mawe/extension-texturepacker/examples/ap/spineboy/ap_spineboy.tpinfo";
        apExportProject(state->project, exporter_path, output_path);
    }
}

static void AllocTextures(AppState* state, int count)
{
    state->page_textures.SetCapacity(count);
    state->page_textures.SetSize(count);
}

static void DestroyTextures(AppState* state)
{
    for (int i = 0; i < state->page_textures.Size(); ++i)
    {
        AppTexture* texture = &state->page_textures[i];
        sg_destroy_image(texture->image);
    }
    state->page_textures.SetSize(0);
}

static void DestroyImages(AppState* state)
{
    for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
    {
        Image* image = *it.GetValue();
        AppTexture* texture = (AppTexture*)image->context;
        if (texture)
        {
            sg_destroy_image(texture->image);
            delete texture;
        }
        DestroyImage(image);
    }
}


static void Quit(AppState* state)
{
    SCOPED_MUTEX(state->mutex);

    printf("TODO: Check if the project is dirty!\n");
    DestroyTextures(state);
    DestroyImages(state);

    if (state->project)
        apDestroyProject(state->project);
    state->project = 0;
}
static void CreateTexture(AppState* state, AppTexture* texture, uint8_t* image, int width, int height, int channels)
{
    if (width == 0 || height == 0 || channels == 0)
    {
        texture->texture_id = 0;
        return;
    }

    uint8_t* tmp = 0;
    if (channels == 3)
    {
        // TODO: Move to a apRGBToRGBA() helper function
        tmp = (uint8_t*)malloc(width * height * 4);
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                tmp[y * 4 * width + x * 4 + 0] = image[y * 3 * width + x * 3 + 0];
                tmp[y * 4 * width + x * 4 + 1] = image[y * 3 * width + x * 3 + 1];
                tmp[y * 4 * width + x * 4 + 2] = image[y * 3 * width + x * 3 + 2];
                tmp[y * 4 * width + x * 4 + 3] = 0xFF;
            }
        }

        image = tmp;
    }

    sg_image_desc def_image_desc;
    _simgui_clear(&def_image_desc, sizeof(def_image_desc));
    def_image_desc.width = width;
    def_image_desc.height = height;
    def_image_desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    def_image_desc.data.subimage[0][0].ptr = image;
    def_image_desc.data.subimage[0][0].size = width * height * 4;
    def_image_desc.label = "atlas-image";

    texture->image = sg_make_image(&def_image_desc);
    texture->texture_id = simgui_imtextureid(texture->image);

    if (tmp)
    {
        free((void*)tmp);
    }
}

static AppTexture* MakeTextureFromImage(AppState* state, Image* image)
{
    AppTexture* texture = new AppTexture;
    CreateTexture(state, texture, image->data, image->width, image->height, image->channels);

    if (texture->texture_id == 0)
    {
        delete texture;
        return 0;
    }
    return texture;
}

static void CreateDefaultTexture(AppState* state)
{
    const int width = 64;
    const int height = 64;
    uint32_t def_pixels[width*height];
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if ((x+y)&1) def_pixels[y*width + x] = 0xFF333333;
            else         def_pixels[y*width + x] = 0xFF555555;
        }
    }
    DestroyTextures(state);
    AllocTextures(state, 1);
    CreateTexture(state, &state->page_textures[0], (uint8_t*)def_pixels, width, height, 4);
    state->zoom = 1.0f / state->page_textures.Size();
}

static void CreateAtlasTextures(AppState* state)
{
    int old_num_pages = state->page_textures.Size();

    DestroyTextures(state);
    AllocTextures(state, state->num_pages);

    for (int i = 0; i < state->num_pages; ++i)
    {
        Page* page = &state->pages[i];
        CreateTexture(state, &state->page_textures[i], page->data, page->width, page->height, page->channels);

        state->page_size.width = page->width;
        state->page_size.height = page->height;
        free((void*)page->data);
    }

    if (state->num_pages == 0)
        state->num_pages = 1;
    if (old_num_pages != state->num_pages) // We want to maintain the zoom while the user is updating settings
        state->zoom = 1.0f / state->num_pages;

    free((void*)state->pages);
    state->pages = 0;
    state->num_pages = 0;
}

static void OnSokolInit(void* user_data)
{
    AppState* state = (AppState*)user_data;

    sg_desc desc = {
        .environment = sglue_environment(),
        .logger.func = slog_func,
    };
    sg_setup(&desc);

    simgui_desc_t imdesc = { 0 };
    simgui_setup(&imdesc);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // initial clear color
    SokolActionState.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 1.0f, 1.0 } }
    };

    // Create dummy texture for atlas pages
    state->zoom = 1.0f;

    CreateDefaultTexture(state);
}

static Image* GetImage(AppState* state, hash_t path_hash)
{
    SCOPED_MUTEX(state->mutex);
    Image** pimage = state->images.Get(path_hash);
    return pimage ? *pimage : 0;
}

static void AddImage(AppState* state, Image* image)
{
    SCOPED_MUTEX(state->mutex);

    if (state->images.Full())
    {
        uint32_t cap = state->images.Capacity() + 32;
        state->images.SetCapacity(cap);
    }
    state->images.Put(image->path_hash, image);
}

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
        texture = MakeTextureFromImage(state, image);
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
        disabled = state->open_file_dialog != 0 ||
                   state->open_folder_dialog != 0 ||
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
            state->open_file_dialog = true;
            state->file_dialog_extensions = "png,jpg";
            state->file_dialog_callback = (FileDialogCallbackFn)OnFileDialogAddImages;
        }

        ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::Button("Add Folder"))
        {
            // See comment above
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: see comment above
            state->open_folder_dialog = true;
        }
        ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
    ImGui::EndDisabled();

    ImGui::NewLine();
}


static void ThreadRecreateAtlas(void* _ctx)
{
    AppState* state = (AppState*)_ctx;

    uint64_t tend;
    uint64_t tstart;

    SCOPED_MUTEX(state->mutex);
    state->creating_atlas = 1;

    apProject* project = state->project;
    apPacker* packer = 0;

    printf("Creating packer of type: %d\n", project->packer_type);

    if (project->packer_type == PT_TILEPACKER)
        packer = apTilePackerCreate(&project->options_tp);
    else
        packer = apBinPackerCreate(&project->options_bp);

    if (!packer)
    {
        // Handle any errors
    }

    tstart = GetTime();
    project->context = apCreate(&project->options, packer);
    if (project->context)
    {
        printf("Adding images: %u\n", state->images.Size());
        tstart = GetTime();

        // TODO: Make sure we only create apImages for the unique images that we want to pack
        // Any many-to-one mappings needs to happen before this point.
        for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
        {
            Image* image = *it.GetValue();
            apAddImage(project->context, image->path, image->width, image->height, image->channels, image->data);
        }

        tend = GetTime();

        printf("Adding images took %.2f ms\n", (tend-tstart)/1000.0f);


        tstart = GetTime();

        apPackImages(project->context);

        tend = GetTime();
        printf("Packing atlas images took %.2f ms\n", (tend-tstart)/1000.0f);
    }

    state->num_pages = 0;
    state->pages = apRenderPages(project->context, &state->num_pages, 0);

    state->creating_atlas = 0;
}

static void RecreateAtlas(AppState* state)
{
    WorkerPushJob(state->thread, ThreadRecreateAtlas, (void*)state);
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
        RecreateAtlas(state);
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

        if (state->page_textures[i].texture_id == 0)
            continue;

        ImGui::Image(state->page_textures[i].texture_id, size, uv0, uv1);

        bool do_draw = state->debug_draw_triangles;
        if (!do_draw)
        {

        }

        if (do_draw)
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            ImVec2 pos = start_pos;
            pos.x += size.x * 0.5f;
            pos.y += size.y * 0.5f;

            ImVec2 b = start_pos;
            b.x += size.x * 0.75f;
            b.y += size.y * 0.75f;

            // TODO: check if we can detect "hover" over each image

            if (state->project && state->project->context)
            {
                apPage* page = apGetPage(state->project->context, i);
                apImage* image = apPageGetFirstImage(page);

                float width = (float)page->dimensions.width;
                float height = (float)page->dimensions.height;
                while (image)
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

                    image = image->next;
                }
            }
        }
    }

    ImGui::EndChild();
}


static int IsImageSuffix(const char* suffix)
{
    return suffix != 0 && (strcmp(suffix, ".png") == 0 || strcmp(suffix, ".PNG") == 0);
}


typedef struct ImageLoaderContext
{
    TreeNode* root;
    TreeNode* parent; // Current node to attach to
    Image*    list;
} ImageLoaderContext;


typedef int (*QsortFn)(const void*, const void*);
static int ComparePaths(const char** _a, const char** _b)
{
    const char* a = *_a;
    const char* b = *_b;
    return strcmp(a, b);
}

struct ImageListContext
{
    const char*            root;
    jc::Array<const char*> paths;

    ~ImageListContext()
    {
        for (uint32_t i = 0; i < paths.Size(); ++i)
        {
            free((void*)paths[i]);
        }
    }

    void SortPaths()
    {
        qsort(paths.Begin(), (size_t)paths.Size(), sizeof(const char*), (QsortFn)ComparePaths);
    }

    void AddPath(const char* path)
    {
        // TODO: Make this relative path more robust
        const char* relative = strstr(path, root);

        if (paths.Full())
            paths.SetCapacity(paths.Capacity()+32);
        paths.Push(strdup(relative));
    }
};

static int ImageListIterator(void* _ctx, const char* path)
{
    ImageListContext* ctx = (ImageListContext*)_ctx;

    const char* suffix = strrchr(path, '.');
    if (!IsImageSuffix(suffix))
        return 0; // continue

    ctx->AddPath(path);
    return 0;
}

// Called from the worker thread
static void LoadImageAndAddNode(AppState* state, TreeNode* parent, const char* path)
{
    hash_t path_hash = Hash(path);

    Image* image = GetImage(state, path_hash);
    if (!image)
    {
        Image* image = LoadImage(path);
        AddImage(state, image);
        image->path_hash = path_hash;
    }
    // TODO: increment ref count

    // Hook it into the tree
    TreeNode* n = TreeNodeFindChild(parent, path);
    if (!n)
    {
        n = TreeNodeCreateImage(path);
        TreeNodeAdd(parent, n);
    }
}

static void ThreadLoadImages(void* ctx)
{
    AppState* state = (AppState*)ctx;

    uint64_t tstart = GetTime();

    jc::Array<const char*> sources;
    TreeNode* root = 0;

    {
        SCOPED_MUTEX(state->mutex);

        state->loading_images = 1;

        apProject* project = state->project;
        sources.SetCapacity(project->num_sources);
        for (uint32_t i = 0; i < project->num_sources; ++i)
        {
            sources.Push(strdup(project->sources[i]));
        }

        root = state->images_root ? TreeNodeTreeClone(state->images_root) : TreeNodeCreateFolder("images");
    }

    char project_dir[2048];
    if (state->path)
    {
        strncpy(project_dir, state->path, sizeof(project_dir));
        char* end = strrchr(project_dir, '/');
        if (end)
            *(end+1) = 0;
    }
    else
    {
        getcwd(project_dir, sizeof(project_dir));
    }

    for (int i = 0; i < sources.Size(); ++i)
    {
        const char* path = sources[i];

        char full_path[2048];
        if (path[0] == '.' && path[1] == '/') // relative path
        {
            path += 2;
            strncpy(full_path, project_dir, sizeof(full_path));
            strncat(full_path, path, sizeof(full_path) - strlen(full_path) - 1);
        }
        else
        {
            strncpy(full_path, path, sizeof(full_path));
        }

        if (IsFile(full_path))
        {
            LoadImageAndAddNode(state, root, path);
        }
        else if (IsDir(full_path))
        {
            const char* folder = strrchr(full_path, '/');
            while ((*folder) == '/')
                folder++;

            TreeNode* foldernode = TreeNodeFindChild(root, folder);
            if (!foldernode)
            {
                foldernode = TreeNodeCreateFolder(folder);
                TreeNodeAdd(root, foldernode);
            }

            ImageListContext file_list;
            file_list.root = full_path;
            IterateFiles(full_path, true, ImageListIterator, &file_list);
            file_list.SortPaths();

            for (uint32_t j = 0; j < file_list.paths.Size(); ++j)
            {
                const char* relative = file_list.paths[j];
                LoadImageAndAddNode(state, foldernode, relative);
            }
        }
    }

    {
        SCOPED_MUTEX(state->mutex);

        state->max_image_size = 0;
        for (jc::HashTable<hash_t, Image*>::Iterator it = state->images.Begin(); it != state->images.End(); ++it)
        {
            Image* image = *it.GetValue();
            if (image->width > state->max_image_size)
                state->max_image_size = image->width;
            if (image->height > state->max_image_size)
                state->max_image_size = image->height;
        }

        state->loading_images = 0;

        if (state->images_root)
            TreeNodeTreeDestroy(state->images_root);
        state->images_root = root;

        RecreateAtlas(state);
    }

    uint64_t tend = GetTime();
    printf("ThreadLoadImages: Loaded %u images in %.3f s!\n", state->images.Size(), (tend - tstart) / 1000000.0f);
}

static void OnSokolFrame(void* user_data)
{
    AppState* state = (AppState*)user_data;

    int do_files_load = 0;
    {
        SCOPED_MUTEX(state->mutex);
        do_files_load = state->dirty_fileset;
        state->dirty_fileset = 0;
    }

    if (do_files_load)
    {
        WorkerPushJob(state->thread, ThreadLoadImages, (void*)state);
    }

    int width = sapp_width();
    int height = sapp_height();

    simgui_frame_desc_t frame_desc = {
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    };
    simgui_new_frame(&frame_desc);

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
                OpenFileDialog(state);
            }

            if (ImGui::MenuItem("Save", "CTRL+S"))
            {
                SaveFile(state);
            }
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                // macOS: Since the file dialog mustn't be opened in the
                // scope of a sokol frame, we need to delay it.
                // And since the ImGui::Button() reacts on mouse UP, and the Sokol
                // on_event callback happends before this, we need to start the process on
                // a left click
                state->save_project_dialog = true;
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
                if (state->pages)
                    CreateAtlasTextures(state);

                DrawAtlasPages(state);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    ImGui::End();

    ImGui::ShowDemoWindow();

    /*=== UI CODE ENDS HERE ===*/
    sg_pass pass = { .action = SokolActionState.pass_action, .swapchain = sglue_swapchain() };
    sg_begin_pass(&pass);
    simgui_render();
    sg_end_pass();
    sg_commit();
}

static void OnSokolCleanup(void* user_data) {
    simgui_shutdown();
    sg_shutdown();
}

static void ProjectAddSource(AppState* state, const char** paths, uint32_t num_paths)
{
    SCOPED_MUTEX(state->mutex);

    apProject* p = state->project;
    if (p)
    {
        apProjectAddSources(p, paths, num_paths);

        state->dirty_fileset = 1;
    }
}

// ********************************************************************************************
// File dialog callbacks

static void OnFileDialogAddImages(AppState* state, const char** paths, int numpaths)
{
    printf("Add file: %s %d", paths[0], numpaths);
    ProjectAddSource(state, paths, numpaths);
}

// ********************************************************************************************

static void OnSokolEvent(const sapp_event* ev, void* user_data) {
    AppState* state = (AppState*)user_data;

    if (ev->type == SAPP_EVENTTYPE_KEY_DOWN)
    {
        if (ev->key_code == KEY_CODE_OPEN && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            OpenFileDialog(state);
        }
        else if (ev->key_code == KEY_CODE_SAVE && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            SaveFile(state);
        }
        else if (ev->key_code == KEY_CODE_EXPORT && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            ExportFile(state);
        }
        else if (ev->key_code == KEY_CODE_QUIT && (ev->modifiers & KEY_CODE_MODIFIERS)==KEY_CODE_MODIFIERS)
        {
            sapp_request_quit();
        }
        else
        {
            simgui_handle_event(ev);
        }
    }
    else if (ev->type == SAPP_EVENTTYPE_QUIT_REQUESTED)
    {
        printf("Quit requested\n");
        Quit(state);
    }
    else if (ev->type == SAPP_EVENTTYPE_FILES_DROPPED)
    {

        // the mouse position where the drop happened
        // float x = ev->mouse_x;
        // float y = ev->mouse_y;

        // // get the number of files and their paths like this:
        // const int num_dropped_files = sapp_get_num_dropped_files();
        // for (int i = 0; i < num_dropped_files; i++) {
        //     const char* path = sapp_get_dropped_file_path(i);
        //     ...
        // }
    }
    else if (ev->type == SAPP_EVENTTYPE_MOUSE_UP)
    {
        simgui_handle_event(ev);

        if (state->open_file_dialog)
        {
            nfdchar_t* outpath = 0;
            // nfdresult_t result = NFD_OpenDialog("png,jpg", 0, &outpath);
            // if (NFD_OKAY == result)
            // {
            //     ProjectAddSource(state, (const char**)&outpath, 1);
            //     free(outpath);
            // }
            nfdresult_t result = NFD_OpenDialog(state->file_dialog_extensions, 0, &outpath);
            if (NFD_OKAY == result)
            {
                state->file_dialog_callback(state, (const char**)&outpath, 1);
                free(outpath);
            }

        }
        else if (state->open_folder_dialog)
        {
            nfdchar_t* outpath = 0;
            nfdresult_t result = NFD_PickFolder(0, &outpath);
            if (NFD_OKAY == result)
            {
                ProjectAddSource(state, (const char**)&outpath, 1);
                free(outpath);
            }
        }
        else if (state->save_project_dialog)
        {
            nfdchar_t* outpath = 0;
            nfdresult_t result = NFD_SaveDialog("ap", 0, &outpath);
            if (NFD_OKAY == result)
            {
                if (apSaveProject(outpath, state->project))
                {
                    state->path = outpath; // it'll be deleted with the project
                }
                else
                {
                    free(outpath);
                }
                UpdateWindowTitle(state, false);
            }
        }
        else if (state->open_project_dialog)
        {
            nfdchar_t* outpath = 0;
            nfdresult_t result = NFD_OpenDialog("ap", 0, &outpath);
            if (NFD_OKAY == result)
            {
                apProject* project = apLoadProjectFromPath(outpath);
                if (project)
                {
                    apDebugPrintProject(project);

                    SCOPED_MUTEX(state->mutex);
                    state->dirty_fileset = 1;

                    if (state->project)
                        apDestroyProject(state->project);
                    state->project = project;
                    state->path    = strdup(outpath);
                }
                else
                {
                    free(outpath);
                }
            }
        }

        state->open_file_dialog = false;
        state->open_folder_dialog = false;
        state->save_project_dialog = false;
        state->open_project_dialog = false;
    }
    else
    {
        simgui_handle_event(ev);
    }
}

int main(int argc, char* argv[])
{
    AppState state;
    memset(&state, 0, sizeof(state));

    state.mutex = MutexCreate();

    state.thread = WorkerStart(state.mutex);

    if (argc > 1)
    {
        state.path = argv[argc-1];
        state.project = apLoadProjectFromPath(state.path);
        state.dirty_fileset = 1;

        if (!state.project)
        {
            fprintf(stderr, "Failed to read prooject from %s\n", state.path);
            state.project   = apLoadProjectFromMemory("untitled", 0);
            state.dirty_fileset = 0;
        }
    }
    else
    {
        state.path      = 0;
        state.project   = apLoadProjectFromMemory("untitled", 0);
    }

    char title[1024];
    GetWindowTitle(state.path, false, title, sizeof(title));

    sapp_desc desc = {
        .width = 1280,
        .height = 1024,
        .user_data = (void*)&state,
        .init_userdata_cb = OnSokolInit,
        .frame_userdata_cb = OnSokolFrame,
        .cleanup_userdata_cb = OnSokolCleanup,
        .event_userdata_cb = OnSokolEvent,
        .logger.func = slog_func,
        .enable_dragndrop = true,
        .max_dropped_files = 8 *1024,
        .max_dropped_file_path_length = 8192,
        .window_title = title,
    };

    sapp_run(&desc);

    if (state.thread)
    {
        thread_join(state.thread);
        thread_destroy(state.thread);
    }

    WorkerStop(state.thread);

    MutexDestroy(state.mutex);

    return 0;
}
