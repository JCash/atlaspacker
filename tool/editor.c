
#include <stdint.h>

// Editor related
#include "state.h"
#include "worker.h"

#include <atlaspacker/exporter.h>

#define SOKOL_APP_IMPL
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#define SOKOL_NO_ENTRY
#include <sokol_app.h>
#include <sokol_log.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <sokol_imgui.h>

#define THREAD_IMPLEMENTATION
#include <thread.h>

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

#include <nfd.h>

#include <atlaspacker/project.h>

#include <unistd.h> // getcwd

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
} state;

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
    thread_mutex_lock(&state->mutex);
    state->open_project_dialog = true;
    thread_mutex_unlock(&state->mutex);
}

static void SaveFile(AppState* state)
{
    thread_mutex_lock(&state->mutex);
    if (state->path == 0)
    {
        state->save_project_dialog = true;
    }
    else {
        apSaveProject(state->path, state->project);
        UpdateWindowTitle(state, false);
    }

    thread_mutex_unlock(&state->mutex);
}

static void ExportFile(AppState* state)
{
    thread_mutex_lock(&state->mutex);

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

    thread_mutex_unlock(&state->mutex);
}


static void DestroyTextures(AppState* state)
{
    for (int i = 0; i < state->num_page_textures; ++i)
    {
        AppTexture* texture = &state->page_textures[i];
        simgui_destroy_image(texture->imgui_image);
        sg_destroy_image(texture->image);
    }

    free((void*)state->page_textures);
}

static void Quit(AppState* state)
{
    thread_mutex_lock(&state->mutex);

    printf("TODO: Check if the project is dirty!\n");
    DestroyTextures(state);

    if (state->project)
        apDestroyProject(state->project);

    thread_mutex_unlock(&state->mutex);
}
static void CreateTexture(AppState* state, AppTexture* texture, uint8_t* image, int width, int height, int channels)
{
    uint8_t* tmp = 0;
    if (channels == 3)
    {
        // TODO: Move to a apRGBToRGBA() helper function
        tmp = (uint8_t*)malloc(width * height * channels);
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
    texture->imgui_image = simgui_make_image(&(simgui_image_desc_t){
            .image = texture->image,
            .sampler = { 0 }, // TODO: Create a NEAREST sampler
        });
    texture->texture_id = simgui_imtextureid(texture->imgui_image);

    if (tmp)
    {
        free((void*)tmp);
    }
}

static void AllocTextures(AppState* state, int count)
{
    uint32_t size = sizeof(AppTexture) * count;
    state->num_page_textures = count;
    state->page_textures = (AppTexture*)malloc(size);
    memset(state->page_textures, 0, size);
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
    state->zoom = 1.0f / state->num_page_textures;
}

static void CreateAtlasTextures(AppState* state)
{
    DestroyTextures(state);
    AllocTextures(state, state->num_pages);

    int old_num_pages = state->num_page_textures;
    state->num_page_textures = state->num_pages;
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
    AppState* app_state = (AppState*)user_data;

    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    simgui_setup(&(simgui_desc_t){ 0 });

    igGetIO()->ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // initial clear color
    state.pass_action = (sg_pass_action) {
        .colors[0] = { .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.0f, 0.5f, 1.0f, 1.0 } }
    };

    // Create dummy texture for atlas pages
    app_state->zoom = 1.0f;
    app_state->num_page_textures = 0;
    app_state->page_textures = 0;
    CreateDefaultTexture(app_state);
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
    if (igIsMouseClicked_ID(ImGuiMouseButton_Left, 0, 0) && igIsItemHovered(ImGuiHoveredFlags_None))
    {
        if (igIsKeyDown_Nil(ImGuiKey_LeftCtrl) || igIsKeyDown_Nil(ImGuiKey_RightCtrl))
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

static void DrawImageListTree(TreeNode* root, TreeNode* node)
{
    int is_folder = node->type == 0;
    Image* image = is_folder ? 0 : (Image*)node->data;
    const char* name = is_folder ?
                            ((const char*)node->data) :
                            image->path;
    if (!is_folder)
    {
        name = strrchr(name, '/');
        while ((*name) == '/')
            name++;
    }

    igTableNextRow(0, 0.0f);
    igTableNextColumn();

    int selected = node->selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None;
    if (is_folder)
    {
        bool open = igTreeNodeEx_Str(name, selected | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow);
        igTableNextColumn();
        igTextDisabled("--");
        if (open)
        {
            TreeNode* child = node->child;
            while (child)
            {
                DrawImageListTree(root, child);
                child = child->sibling;
            }

            CheckSelectNode(root, node);

            igTreePop();
        }
    }
    else
    {
        if (igTreeNodeEx_Str(name, selected | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick))
        {
            CheckSelectNode(root, node);

            if (igIsMouseDoubleClicked_ID(ImGuiMouseButton_Left, 0) && igIsItemHovered(ImGuiHoveredFlags_None))
            {
                printf("TODO: Open image in sprite editor: %s\n", name);
            }
        }

        igTableNextColumn();
        igTextDisabled("--");
        //igText("%d x %d x %d", image->width, image->width, image->channels);
    }
}

static void DrawImageList(AppState* state)
{
    igSeparator();

    ImVec2 tsz;
    const char* text = "A";
    igCalcTextSize(&tsz, text, text+1, false, -1.0f);

    ImVec2 wsize;
    igGetWindowSize(&wsize);

    ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody;
    if (igBeginTable("Images", 2, table_flags, (ImVec2){wsize.x - igGetStyle()->ScrollbarSize, wsize.y -100}, 0.0f))
    {
        igTableSetupColumn("Name", ImGuiTableColumnFlags_NoHide, 0.0f, 0);
        igTableSetupColumn("Info", ImGuiTableColumnFlags_WidthFixed, tsz.x * 18.0f, 0);
        igTableHeadersRow();

        if (!state->project || !state->project->num_sources)
        {
            igTableNextRow(0, 0.0f);
            igTableNextColumn(); igText("Drop");
            igTableNextColumn(); igTextDisabled("--");
            igTableNextRow(0, 0.0f);
            igTableNextColumn(); igText("files");
            igTableNextColumn(); igTextDisabled("--");
            igTableNextRow(0, 0.0f);
            igTableNextColumn(); igText("here!");
            igTableNextColumn(); igTextDisabled("--");
        }
        else
        {
            // TODO: Add a trylock to the thread api
            thread_mutex_lock(&state->mutex);

            TreeNode* node = state->images_root.child;
            while (node)
            {
                DrawImageListTree(&state->images_root, node);
                node = node->sibling;
            }

            thread_mutex_unlock(&state->mutex);
        }

        igEndTable();
    }

    igSeparator();
    igIndent(16);

    igBeginDisabled(state->open_file_dialog != 0 ||
                    state->open_folder_dialog != 0 ||
                    state->loading_images);

        if (igButton("Add Image(s)", (ImVec2){0,0}))
        {
            // See comment below
        }
        if (igIsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: Since the file dialog mustn't be opened in the
            // scope of a sokol frame, we need to delay it.
            // And since the ImGui::Button() reacts on mouse UP, and the Sokol
            // on_event callback happends before this, we need to start the process on
            // a left click
            state->open_file_dialog = true;
        }

        igSameLine(0, igGetStyle()->ItemSpacing.x);
        if (igButton("Add Folder", (ImVec2){0,0}))
        {
            // See comment above
        }
        if (igIsItemClicked(ImGuiMouseButton_Left))
        {
            // macOS: see comment above
            state->open_folder_dialog = true;
        }
        igSameLine(0, igGetStyle()->ItemSpacing.x);
    igEndDisabled();

    igNewLine();
}


static void ThreadRecreateAtlas(void* _ctx)
{
    AppState* state = (AppState*)_ctx;

    uint64_t tend;
    uint64_t tstart;

    thread_mutex_lock(&state->mutex);
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
        printf("Adding images\n");
        tstart = GetTime();

        // TODO: Make sure we only create apImages for the unique images that we want to pack
        // Any many-to-one mappings needs to happe before this point.
        for (int i = 0; i < state->num_images; ++i)
        {
            Image* image = state->images[i];
            apAddImage(project->context, image->path, image->width, image->height, image->channels, image->data);
        }


        tend = GetTime();

        printf("Adding images took %.2f ms\n", (tend-tstart)/1000.0f);


        tstart = GetTime();

        apPackImages(project->context);

        tend = GetTime();
        printf("Packing atlas images took %.2f ms\n", (tend-tstart)/1000.0f);
    }

    //printf("Created packer contexts\n");

    state->num_pages = 0;
    state->pages = apRenderPages(state->project->context, &state->num_pages, 0);

    state->creating_atlas = 0;

    thread_mutex_unlock(&state->mutex);
}

static void RecreateAtlas(AppState* state)
{
    worker_push_job(state->thread, ThreadRecreateAtlas, (void*)state);
}

static void DrawPackerOptions(AppState* state)
{
    apProject* project = state->project;
    int dirty = 0;

    igBeginGroup();

    igText("General Packer Options");

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

    if (igSliderInt("Page Size (texels)", &page_size_index, 0, num_page_sizes-1, page_size_str, 0))
    {
        int old_size = project->options.page_size;
        int new_size = page_sizes[page_size_index];
        if (old_size != new_size)
        {
            project->options.page_size = new_size;
            dirty = 1;
        }
    }

    igEndGroup();

    igSeparator();

    PackerType packer_types[] = {PT_TILEPACKER, PT_BINPACKER};
    const char* packertype_items[] = {"TilePacker", "BinPacker"};
    int num_packertype_items = sizeof(packertype_items)/sizeof(packertype_items[0]);

    int packer_type_index = project->packer_type;
    if (igCombo_Str_arr("Packer Type", &packer_type_index, packertype_items, num_packertype_items, 0))
    {
        project->packer_type = packer_types[packer_type_index];

        dirty = 1;
    }

    igSeparator();

    igText("Packer Type Specific options");

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

        if (igSliderInt("Tile Size (texels)", &tile_size_index, 0, num_tile_sizes-1, tile_size_str, 0))
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
        if (igCheckbox("No Rotate", &no_rotate))
        {
            options->no_rotate = (int)no_rotate;
            dirty = 1;
        }

        if (igSliderInt("Padding (texels)", &options->padding, 0, 16, "%d", ImGuiSliderFlags_AlwaysClamp))
        {
            dirty = 1;
        }

        if (igSliderInt("Alpha threshold", &options->alpha_threshold, 1, 255, "%d", ImGuiSliderFlags_AlwaysClamp))
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
        if (igCombo_Str_arr("Bin Packer Mode", &binpacker_type_index, binpackertype_items, num_binpackertype_items, 0))
        {
            options->mode = (apBinPackMode)binpacker_type_index;
            dirty = 1;
        }

        bool no_rotate = (bool)options->no_rotate;
        if (igCheckbox("No Rotate", &no_rotate))
        {
            options->no_rotate = (int)no_rotate;
            dirty = 1;
        }
    }

    igSeparator();

    igText("Debug options");

    if (igCheckbox("Draw Triangles", &state->debug_draw_triangles))
    {
    }


    if (dirty)
    {
        RecreateAtlas(state);
    }
}

static void DrawAtlasPages(AppState* state)
{
    thread_mutex_lock(&state->mutex);

    if (state->project->context)
    {
        igText("Atlas: %d pages, %d x %d", state->num_page_textures, state->page_size.width, state->page_size.height);
    }
    else
    {
        igText("");
    }

    igBeginChild_Str("#atlas_texture", (ImVec2){0,0}, 0, 0);

    ImVec2 size;
    igGetWindowSize(&size);

    //static float zoom = 1.0f;

    if (igIsKeyDown_Nil(ImGuiKey_MouseWheelY) && igIsKeyDown_Nil(ImGuiMod_Ctrl))
    {
        const float zoom_speed = 0.01f;
        state->zoom += igGetIO()->MouseWheel * zoom_speed;
        if (state->zoom < 0.02f)
            state->zoom = 0.02f;
        if (state->zoom > 3.0f)
            state->zoom = 3.0f;
    }

    size.x *= state->zoom;
    size.y *= state->zoom;


    for (int i = 0; i < state->num_page_textures; ++i)
    {
        ImVec2 uv0 = {0,0};
        ImVec2 uv1 = {1,1};
        igSameLine(0, 0);

        ImVec2 start_pos;
        igGetCursorScreenPos(&start_pos);

        igImage(state->page_textures[i].texture_id, size, uv0, uv1, (ImVec4){1,1,1,1}, (ImVec4){0,0,0,0});

        if (state->debug_draw_triangles)
        {
            ImDrawList* draw_list = igGetWindowDrawList();

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
                        ImDrawList_AddLine(draw_list, a, b, 0xFF00F0FF, 1.0f);
                    }

                    image = image->next;
                }
            }
        }
    }

    igEndChild();

    thread_mutex_unlock(&state->mutex);
}


static int IsImageSuffix(const char* suffix)
{
    return strcmp(suffix, ".png") == 0 || strcmp(suffix, ".PNG") == 0;
}


typedef struct ImageLoaderContext
{
    TreeNode* root;
    TreeNode* parent; // Current node to attach to
    Image*    list;
    const char* source_dir; // set if there is a source directory
} ImageLoaderContext;

static void AddTreeNodeInternal(TreeNode* parent, TreeNode* node)
{
    // find last child
    if (!parent->child)
        parent->child = node;
    else
    {
        TreeNode* last = parent->child;
        while (last->sibling)
        {
            last = last->sibling;
        }
        last->sibling = node;
    }
}

static TreeNode* AddTreeNode(TreeNode* parent, Image* image)
{
    assert(parent);

    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    memset(node, 0, sizeof(*node));

    node->type = 1; // 0: folder, 1; image
    node->data = (void*)image;
    node->readonly = parent->type == 0; // if parent is a folder, we cannot remove the item

    AddTreeNodeInternal(parent, node);
    return node;
}

static TreeNode* AddFolderTreeNode(TreeNode* parent, const char* name)
{
    assert(parent);

    TreeNode* node = (TreeNode*)malloc(sizeof(TreeNode));
    memset(node, 0, sizeof(*node));

    node->type = 0; // 0: folder, 1; image
    node->data = (void*)strdup(name);

    AddTreeNodeInternal(parent, node);
    return node;
}

static Image* ImageLoad(ImageLoaderContext* ctx, const char* path)
{
    const char* suffix = strrchr(path, '.');
    if (!suffix)
        return 0;

    //printf("Path: %s %s %d\n", path, suffix?suffix:"", IsImageSuffix(suffix));

    if (!IsImageSuffix(suffix))
        return 0;

    // The list becomes in reverse order, but we will sort it anyways
    Image* image = LoadImage(path);

    return image;
}

static int ImageLoadIterator(void* _ctx, const char* path)
{
    ImageLoaderContext* ctx = (ImageLoaderContext*)_ctx;

    const char* suffix = strrchr(path, '.');
    if (!suffix)
        return 0;
    if (!IsImageSuffix(suffix))
        return 0; // continue

    Image* image = ImageLoad(ctx, path);

    // Add it to the list
    image->next = ctx->list->next;
    ctx->list->next = image;

    // Hook it into the tree
    AddTreeNode(ctx->parent, image);

    return image ? 0 : 1; // The iterator wants 1 to quit, 0 to continue
}

static void ThreadLoadImages(void* ctx)
{
    AppState* state = (AppState*)ctx;

    uint64_t tstart = GetTime();

    apProject*      project = 0;
    int             num_sources = 0;
    const char**    sources = 0;

    thread_mutex_lock(&state->mutex);
        state->loading_images = 1;
        project = state->project;
        num_sources = project->num_sources;
        sources = project->sources;
    thread_mutex_unlock(&state->mutex);

    /////////////////////////////////////////////////////////////////////
    // TODO: Don't reload all images. Instead check if they're already loaded, or if they're not referenced anymore
    for (int i = 0; i < state->num_images; ++i)
    {
        DestroyImage(state->images[i]);
    }
    free((void*)state->images);
    state->num_images = 0;
    /////////////////////////////////////////////////////////////////////

    // Build a tree from this list of images
    ImageLoaderContext loader_context;
    memset(&loader_context, 0, sizeof(ImageLoaderContext));
    loader_context.root = &state->images_root;

    Image image_list = { .next = 0 };
    loader_context.list = &image_list;

    loader_context.root = &state->images_root;

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

    //printf("Project directory: '%s'\n", project_dir);

    for (int i = 0; i < num_sources; ++i)
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

    //printf("MAWE Loading images from %d: %s %s  is dir: %d  is file: %d\n", i, path, full_path, IsDir(full_path), IsFile(full_path));
        if (IsFile(full_path))
        {
            image_list.source = 0;
            loader_context.parent = loader_context.root;
            Image* image = ImageLoad(&loader_context, full_path);

            // Add it to the list
            image->next = loader_context.list->next;
            loader_context.list->next = image;

            // Hook it into the tree
            AddTreeNode(loader_context.parent, image);
        }
        else if (IsDir(full_path))
        {
            const char* folder = strrchr(full_path, '/');
            while ((*folder) == '/')
                folder++;

            image_list.source = full_path;
            loader_context.parent = AddFolderTreeNode(loader_context.root, folder);
            IterateFiles(full_path, true, ImageLoadIterator, &loader_context);
        }
    }

    int count = 0;
    Image* first = image_list.next;
    while (first)
    {
        ++count;
        first = first->next;
    }

    state->images = (Image**)malloc(sizeof(Image*)*count);

    state->max_image_size = 0;
    count = 0;
    first = image_list.next;
    while (first)
    {
        state->images[count++] = first;

        if (first->width > state->max_image_size)
            state->max_image_size = first->width;
        if (first->height > state->max_image_size)
            state->max_image_size = first->height;

        first = first->next;
    }
    state->num_images = count;

    SortImages(state->images, state->num_images);

    thread_mutex_lock(&state->mutex);
        state->loading_images = 0;
    thread_mutex_unlock(&state->mutex);

    RecreateAtlas(state);

    uint64_t tend = GetTime();
    printf("ThreadLoadImages: Loaded %d images in %.3f s!\n", state->num_images, (tend - tstart) / 1000000.0f);
}

static void OnSokolFrame(void* user_data)
{
    AppState* app_state = (AppState*)user_data;

    int do_files_load = 0;
    thread_mutex_lock(&app_state->mutex);
    do_files_load = app_state->dirty_fileset;
    app_state->dirty_fileset = 0;

    if (app_state->pages)
        CreateAtlasTextures(app_state);
    thread_mutex_unlock(&app_state->mutex);

    if (do_files_load)
    {
        worker_push_job(app_state->thread, ThreadLoadImages, (void*)app_state);
    }

    int width = sapp_width();
    int height = sapp_height();

    simgui_new_frame(&(simgui_frame_desc_t){
        .width = sapp_width(),
        .height = sapp_height(),
        .delta_time = sapp_frame_duration(),
        .dpi_scale = sapp_dpi_scale(),
    });

    igSetNextWindowPos((ImVec2){0,0}, ImGuiCond_Always, (ImVec2){0,0});
    igSetNextWindowSize((ImVec2){width, height}, ImGuiCond_Always);

    igBegin("#main", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

    ImGuiID dockspace_id = igGetID_Str("MyDockSpace");
    static bool dock_init = true;
    if (dock_init)
    {
        dock_init = false;

        igDockBuilderRemoveNode(dockspace_id);
        igDockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);

        igDockBuilderSetNodeSize(dockspace_id, (ImVec2){width, height});

        ImGuiID dockLeft;
        ImGuiID dockRight;
        igDockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.3f, &dockLeft, &dockRight);

        int left_size = width/3;
        if (left_size < 300)
            left_size = 300;
        igDockBuilderSetNodeSize(dockLeft, (ImVec2){left_size, height});

        igDockBuilderDockWindow("#settings", dockLeft);
        igDockBuilderDockWindow("#textures", dockRight);

        igDockBuilderFinish(dockspace_id);

    }

    igDockSpace(dockspace_id, (ImVec2){0,0}, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar, 0);
    igEnd(); // # main

    // === UI CODE STARTS HERE ===

    igBegin("#settings", 0, ImGuiWindowFlags_MenuBar);

    if (igBeginMenuBar())
    {
        if (igBeginMenu("File", true))
        {
            if (igMenuItem_Bool("Open...", "CTRL+O", false, true))
            {
            }
            if (igIsItemClicked(ImGuiMouseButton_Left))
            {
                // macOS: Since the file dialog mustn't be opened in the
                // scope of a sokol frame, we need to delay it.
                // And since the ImGui::Button() reacts on mouse UP, and the Sokol
                // on_event callback happends before this, we need to start the process on
                // a left click
                OpenFileDialog(app_state);
            }

            if (igMenuItem_Bool("Save", "CTRL+S", false, true))
            {
                SaveFile(app_state);
            }
            if (igIsItemClicked(ImGuiMouseButton_Left))
            {
                // macOS: Since the file dialog mustn't be opened in the
                // scope of a sokol frame, we need to delay it.
                // And since the ImGui::Button() reacts on mouse UP, and the Sokol
                // on_event callback happends before this, we need to start the process on
                // a left click
                app_state->save_project_dialog = true;
            }

            igEndMenu();
        }
        igEndMenuBar();
    }

    if (igBeginTabBar("#tabs", 0))
    {
        if (igBeginTabItem("Images", 0, ImGuiTabItemFlags_NoCloseButton))
        {
            DrawImageList(app_state);
            igEndTabItem();
        }

        if (igBeginTabItem("Packer", 0, ImGuiTabItemFlags_NoCloseButton))
        {
            DrawPackerOptions(app_state);
            igEndTabItem();
        }

        if (igBeginTabItem("Exporter", 0, ImGuiTabItemFlags_NoCloseButton))
        {
            //DrawExporterOptions();
            igEndTabItem();
        }

        igEndTabBar();
    }
    igEnd();

    igBegin("#textures", 0, 0);
        if (igBeginTabBar("#textures_tabs", 0))
        {
            if (igBeginTabItem("#pages", 0, ImGuiTabItemFlags_NoCloseButton))
            {
                DrawAtlasPages(app_state);
                igEndTabItem();
            }

            igEndTabBar();
        }
    igEnd();

    igShowDemoWindow(0);

    /*=== UI CODE ENDS HERE ===*/

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = sglue_swapchain() });
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
    thread_mutex_lock(&state->mutex);

    apProject* p = state->project;
    apProjectAddSources(p, paths, num_paths);

    state->dirty_fileset = 1;

    thread_mutex_unlock(&state->mutex);
}

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
            nfdresult_t result = NFD_OpenDialog("png,jpg", 0, &outpath);
            if (NFD_OKAY == result)
            {
                ProjectAddSource(state, (const char**)&outpath, 1);
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

                    thread_mutex_lock(&state->mutex);
                        state->dirty_fileset = 1;

                        apDestroyProject(state->project);
                        state->project = project;
                        state->path    = strdup(outpath);

                    thread_mutex_unlock(&state->mutex);
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
    AppState app_state;
    memset(&app_state, 0, sizeof(app_state));

    thread_mutex_init(&app_state.mutex);
    app_state.thread = worker_start(app_state.mutex);

    if (argc > 1)
    {
        app_state.path = argv[argc-1];
        app_state.project = apLoadProjectFromPath(app_state.path);
        app_state.dirty_fileset = 1;
    }
    else
    {
        app_state.path      = 0;
        app_state.project   = apLoadProjectFromMemory("untitled", 0);
    }

    char title[1024];
    GetWindowTitle(app_state.path, false, title, sizeof(title));

    sapp_desc desc = {
        .width = 1280,
        .height = 1024,
        .user_data = (void*)&app_state,
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

    if (app_state.thread)
    {
        thread_join(app_state.thread);
        thread_destroy(app_state.thread);
    }

    worker_stop(app_state.thread);

    thread_mutex_term(&app_state.mutex);

    return 0;
}
