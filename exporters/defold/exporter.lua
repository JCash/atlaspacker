print("DEFOLD DEFAULT EXPORTER MODULE IMPORTED!")

local DEFAULTS = {
    data_file = {
        display = "Atlas File",
        desc    = "The exported .tpinfo file",
        edit    = "file",
        value   = "{project_path}/{project_name}.tpinfo"
    }
}

-- Returns an array, the order is used as the display order in the editor
-- Each option has these values:
--   name:      (required) The name used in IO, i.e. it's the string saved in the file
--   value:     (required) A default value
--   display:   (optional) A nicer display name. If omitted, defaults to the `name`
--   desc:      (optional) A description of the option
--   edit:      (optional) The edit type used by the editor to display the property
function get_defaults()
    local function get_default(name) -- make sure the names are in sync
        local t = DEFAULTS[name]
        t['name'] = name
        return t
    end

    local defaults = {
        get_default('data_file')
    }

    -- print("Testing")
    -- pprint("DEFAULTS", defaults)
    return defaults
end

-- options arrive from the loaded file
-- the function returns an up-to-date version of the options
function update_options(options)
    return options
end

local function ensure_int(value)
    local num = tonumber(value)
    if not num then
        return 0
    end
    if num < 0 then
        return math.ceil(num)
    end
    return math.floor(num)
end

local function indent(count)
    return string.rep("  ", count or 0)
end

local function quote(str)
    return "\"" .. tostring(str) .. "\""
end

local function field(field_name, value, indent_level, is_quoted)
    local v = is_quoted and quote(value) or tostring(value)
    return indent(indent_level or 0) .. field_name .. ": " .. v
end

local function push_lines(output, ...)
    for i = 1, select("#", ...) do
        output[#output + 1] = select(i, ...)
    end
end

local function normalize_path(path)
    local value = tostring(path or "")
    return value:gsub("\\", "/")
end

local function basename(path)
    local normalized = normalize_path(path)
    return normalized:match("([^/]+)$") or normalized
end

local function strip_extension(name)
    return name:gsub("%.[^%.]+$", "")
end

local function sprite_name_from_path(path)
    local name = strip_extension(basename(path))
    if name == "" then
        return "sprite"
    end
    return name
end

local function atlas_page_name(output_path, page_index, total_pages)
    local base = strip_extension(basename(output_path))
    if base == "" then
        base = "atlas"
    end
    if total_pages and total_pages > 1 then
        return string.format("%s_%d.png", base, page_index)
    end
    return base .. ".png"
end

local function export_rect(output, indent_level, field_name, rect)
    push_lines(
        output,
        indent(indent_level) .. field_name .. " {",
        field("x", rect.x, indent_level + 1),
        field("y", rect.y, indent_level + 1),
        field("width", rect.width, indent_level + 1),
        field("height", rect.height, indent_level + 1),
        indent(indent_level) .. "}"
    )
end

local function export_size(output, indent_level, field_name, size)
    push_lines(
        output,
        indent(indent_level) .. field_name .. " {",
        field("width", size.width, indent_level + 1),
        field("height", size.height, indent_level + 1),
        indent(indent_level) .. "}"
    )
end

local function export_point(output, indent_level, field_name, vertex)
    push_lines(
        output,
        indent(indent_level) .. field_name .. " {",
        field("x", vertex.x, indent_level + 1),
        field("y", vertex.y, indent_level + 1),
        indent(indent_level) .. "}"
    )
end

local function export_int_array(output, indent_level, field_name, arr)
    local values = arr or {}
    local s = indent(indent_level) .. field_name .. ": ["
    for i = 1, #values do
        s = s .. tostring(values[i])
        if i < #values then
            s = s .. ", "
        end
    end
    output[#output + 1] = s .. "]"
end

local function export_sprite(output, indent_level, image)
    local source = image and image.source or {}
    local placement = image and image.placement or {}
    local source_width = ensure_int(source.width)
    local source_height = ensure_int(source.height)
    local source_rect = {
        x = 0,
        y = 0,
        width = source_width,
        height = source_height
    }
    local frame_rect = {
        x = ensure_int(placement.x),
        y = ensure_int(placement.y),
        width = ensure_int(placement.width or source_width),
        height = ensure_int(placement.height or source_height)
    }
    local rotation = ensure_int(image and image.rotation or 0)
    local channels = ensure_int(source.channels)
    local is_rotated = rotation ~= 0
    local is_solid = channels > 0 and channels < 4

    push_lines(
        output,
        indent(indent_level) .. "sprites {",
        field("name", sprite_name_from_path(source.path), indent_level + 1, true),
        field("trimmed", false, indent_level + 1),
        field("rotated", is_rotated, indent_level + 1),
        field("is_solid", is_solid, indent_level + 1)
    )

    export_point(output, indent_level + 1, "corner_offset", { x = 0, y = 0 })
    export_rect(output, indent_level + 1, "source_rect", source_rect)
    export_point(output, indent_level + 1, "pivot", { x = 0.5, y = 0.5 })
    export_rect(output, indent_level + 1, "frame_rect", frame_rect)
    export_size(output, indent_level + 1, "untrimmed_size", { width = source_width, height = source_height })

    local vertices = image and image.vertices or {}
    if #vertices > 0 then
        for i = 1, #vertices do
            export_point(output, indent_level + 1, "vertices", vertices[i])
        end
        export_int_array(output, indent_level + 1, "indices", image.triangleIndices or image.indices)
    else
        export_int_array(output, indent_level + 1, "indices", { 1, 2, 3, 0, 1, 3 })

        local x0 = source_rect.x
        local y0 = source_rect.y
        local x1 = x0 + source_rect.width
        local y1 = y0 + source_rect.height

        export_point(output, indent_level + 1, "vertices", { x = x1, y = y0 })
        export_point(output, indent_level + 1, "vertices", { x = x0, y = y0 })
        export_point(output, indent_level + 1, "vertices", { x = x0, y = y1 })
        export_point(output, indent_level + 1, "vertices", { x = x1, y = y1 })
    end

    output[#output + 1] = indent(indent_level) .. "}"
end

local function export_page(output, indent_level, page, page_name)
    push_lines(
        output,
        indent(indent_level) .. "pages {",
        field("name", page_name, indent_level + 1, true)
    )

    local dimensions = page and page.dimensions or {}
    export_size(output, indent_level + 1, "size", {
        width = ensure_int(dimensions.width),
        height = ensure_int(dimensions.height)
    })

    local images = page and page.images or {}
    for i = 1, #images do
        export_sprite(output, indent_level + 1, images[i])
    end

    output[#output + 1] = indent(indent_level) .. "}"
end

local function export_atlas(root, output_path)
    local output = {
        "# Exported by Atlas Packer / Defold Exporter",
        "# For documentation of the fields: https://github.com/JCash/atlaspacker/tree/main/doc",
        "",
        "version: \"2.0\"",
        "description: \"Exported using AtlasPacker\""
    }

    local pages = root and root.pages or {}
    local total_pages = #pages
    for i = 1, total_pages do
        local page = pages[i]
        local page_index = ensure_int(page and page.index or (i - 1))
        local page_name = page and page.name or atlas_page_name(output_path, page_index, total_pages)
        export_page(output, 0, page, page_name)
    end

    return table.concat(output, "\n")
end

local function export_data(root, output_path)
    return export_atlas(root, output_path)
end

-- options: arrive from the loaded file
-- data:    the available exporter data
-- path:    the project path
function export(options, data, path)
    local text = export_atlas(data, path)
    local file, err = io.open(path, "wb")
    if not file then
        print("Failed to write Defold atlas: " .. tostring(err))
        return false
    end
    file:write(text)
    file:close()
    return true
end
