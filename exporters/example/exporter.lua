print("DEFOLD EXAMPLE EXPORTER MODULE IMPORTED!")

function printtable(tbl, indent)
  if not indent then indent = 0 end
  for k, v in pairs(tbl) do
    formatting = string.rep("  ", indent) .. k .. ": "
    if type(v) == "table" then
      print(formatting .. '{')
      printtable(v, indent+1)
      print(string.rep("  ", indent) .. '}')
    else
      print(formatting .. tostring(v))
    end
  end
end

local DEFAULTS = {
    test_bool = {
        desc    = "Description Bool",
        type    = "bool",
        value   = true
    },
    test_number = {
        desc    = "Description Number",
        type    = "number",
        value   = 1.0
    },
    test_string = {
        desc    = "Description String",
        type    = "string",
        value   = "hello"
    },
    test_file = {
        display = "A File",
        desc    = "Description of file",
        edit    = "file",
        value   = "default.json"
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
        get_default('test_bool'),
        get_default('test_number'),
        get_default('test_string'),
        get_default('test_file')
    }
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
    printtable(data, 0)
    return true
end
