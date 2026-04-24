--[[
  KX SMSG Interactive Schema Decoder (GUI Version 2 - Corrected)
  Fixes the crash by using .Left and .Top instead of .Position.X/Y.
]]

-- --- Configuration ---
local GAME_MODULE = "Gw2-64.exe"
local SCHEMA_FIELD_DEF_SIZE = 40
local MAX_RECURSION_DEPTH = 10

local SMSG_TYPECODE_MAP = {
    [0x01] = "short", [0x02] = "byte", [0x03] = "int", [0x04] = "compressed_int",
    [0x05] = "long long", [0x06] = "float", [0x07] = "float[2]", [0x08] = "long long",
    [0x09] = "float[4]", [0x0A] = "special_vec3_and_compressed_int", [0x0B] = "float[4]",
    [0x0C] = "guid", [0x0D] = "string", [0x0E] = "string_utf8", [0x0F] = "Optional Block",
    [0x10] = "Fixed Array", [0x11] = "Variable Array (byte count)",
    [0x12] = "Variable Array (short count)", [0x13] = "Fixed Buffer",
    [0x14] = "Variable Buffer (byte count)", [0x15] = "Variable Buffer (short count)",
    [0x18] = "terminator",
}

-- --- Helper & Parser Functions (Unchanged) ---
local function isAddressValid(addr) return readBytes(addr, 1, false) ~= nil end
local function getPointer(base, offset)
  if not base or base == 0 then return nil end
  local addr = base + (offset or 0)
  if not isAddressValid(addr) then return nil end
  return readPointer(addr)
end

local function parse_schema(stream, schema_address, field_prefix, parsed_schemas, depth)
    if not schema_address or schema_address == 0 then return end
    depth = depth or 0
    if depth > MAX_RECURSION_DEPTH then return end
    if parsed_schemas[schema_address] then return end
    parsed_schemas[schema_address] = true
    local field_index = 0
    while true do
        local field_def_addr = schema_address + (field_index * SCHEMA_FIELD_DEF_SIZE)
        if not isAddressValid(field_def_addr) then break end
        local typecode = readInteger(field_def_addr)
        if not typecode or typecode == 0 or typecode == 0x18 then break end
        local type_info = SMSG_TYPECODE_MAP[typecode] or string.format("unknown (0x%02X)", typecode)
        local current_field_id = (field_prefix ~= "") and (field_prefix .. tostring(field_index)) or tostring(field_index)
        stream:write(string.format("| %-8s | `0x%02X` | `%s`\n", current_field_id, typecode, type_info))
        if typecode == 0x0F or typecode == 0x10 or typecode == 0x11 or typecode == 0x12 then
            local sub_schema_ptr = getPointer(field_def_addr, 24)
            if sub_schema_ptr and sub_schema_ptr ~= 0 then
                parse_schema(stream, sub_schema_ptr, current_field_id .. ".", parsed_schemas, depth + 1)
            end
        end
        field_index = field_index + 1
    end
end

-- --- GUI Creation and Main Logic ---
local function create_decoder_form()
    local form = createForm(true)
    form.Caption = "KX SMSG Decoder"
    form.Width = 500
    form.Height = 600
    form.centerScreen()

    local lbl = createLabel(form)
    lbl.Caption = "SMSG Schema Address (Live):"
    lbl.Left = 10   -- CORRECTED
    lbl.Top = 15    -- CORRECTED

    local editAddr = createEdit(form)
    editAddr.Left = 180  -- CORRECTED
    editAddr.Top = 12    -- CORRECTED
    editAddr.Width = 150
    editAddr.Text = "0x"

    local btnDecode = createButton(form)
    btnDecode.Caption = "Decode Schema"
    btnDecode.Left = 340 -- CORRECTED
    btnDecode.Top = 10   -- CORRECTED
    btnDecode.Width = 140

    local memoResult = createMemo(form)
    memoResult.Left = 10      -- CORRECTED
    memoResult.Top = 45       -- CORRECTED
    memoResult.Width = 475
    memoResult.Height = 540
    memoResult.ReadOnly = true
    memoResult.Scrollbars = ssVertical

    btnDecode.setOnClick(function(sender)
        memoResult.Lines.Text = ''
        local module_base = getAddress(GAME_MODULE)
        if not module_base or module_base == 0 then
            memoResult.Lines.Text = string.format("ERROR: Could not find module '%s'.", GAME_MODULE)
            return
        end
        local schema_addr = tonumber(editAddr.Text, 16)
        if not schema_addr or not isAddressValid(schema_addr) then
            memoResult.Lines.Text = "ERROR: Invalid or unreadable address: " .. editAddr.Text
            return
        end
        local relative_addr = schema_addr - module_base
        local stream = createStringStream('')
        stream:write(string.format("# SMSG Schema: `\"%s\"+0x%X`\n\n", GAME_MODULE, relative_addr))
        stream:write("| Field #  | Typecode | Type Name        |\n")
        stream:write("|:---------|:---------|:-----------------|\n")
        local parsed_schemas = {}
        parse_schema(stream, schema_addr, "", parsed_schemas, 0)
        memoResult.Lines.Text = stream.DataString
    end)
end

-- --- Run the script ---
create_decoder_form()