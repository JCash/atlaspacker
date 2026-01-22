print("DEFOLD DEFAULT EXPORTER MODULE IMPORTED!")

local DEFAULTS = {
    data_file = {
        display = "Atlas File",
        desc    = "The exported .tpinfo file",
        edit    = "file",
        value   = "default.tpinfo"
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

-- options: arrive from the loaded file
-- data:    the available exporter data
-- path:    the project path
function export(options, data, path)
    print("EXPORTER FUNCTION")
    print("DATA")
    pprint(data)
    return true
end
