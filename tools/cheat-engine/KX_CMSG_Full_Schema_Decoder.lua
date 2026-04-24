--[[
  KX CMSG Full Schema Decoder
  Part of the kx-packet-inspector project.

  This script performs the entire CMSG schema discovery and decoding process:
  1. It automatically finds the CMSG schema table in memory.
  2. It prompts the user for a save location.
  3. It iterates through every opcode, recursively decodes its schema, and writes
     the formatted structure to a single Markdown file.

  Licensed under the MIT License, consistent with the kx-packet-inspector project.
  https://github.com/Krixx1337/kx-packet-inspector
]]

-- --- Configuration ---
local GAME_MODULE = "Gw2-64.exe"
local MSGCONN_STATIC_OFFSET = 0x2628290
local SCHEMA_INFO_OFFSET = 0x18
local TABLE_BASE_OFFSET = 0x50
local TABLE_SIZE_OFFSET = 0x5c
local ENTRY_SIZE = 16
local SCHEMA_OFFSET_IN_ENTRY = 8

local SCHEMA_FIELD_DEF_SIZE = 40
local MAX_RECURSION_DEPTH = 10

-- This map translates the raw typecode from the schema definition to a human readable string.
local TYPECODE_MAP = {
    [0x01] = "short",
    [0x02] = "byte",
    [0x03] = "short", -- Fall through to case 1 in assembly
    [0x04] = "compressed_int",
    [0x05] = "long long",
    [0x06] = "float", -- or int
    [0x07] = "float[2]",
    [0x08] = "float[3]",
    [0x09] = "float[4]", -- or quat
    [0x0A] = "special_vec3_and_compressed_int",
    [0x0B] = "float[4]", -- same as 0x09
    [0x0C] = "guid", -- 28 bytes
    [0x0D] = "string", -- wchar_t*
    [0x0E] = "string_utf8", -- char*
    [0x0F] = "Optional Block",
    [0x10] = "Fixed Array",
    [0x11] = "Variable Array (byte count)",
    [0x12] = "Variable Array (short count)",
    [0x13] = "Fixed Buffer",
    [0x14] = "Variable Buffer (byte count)",
    [0x15] = "Variable Buffer (short count)",
    [0x16] = "MP_SRV_ALIGN", -- Server only, error on client
    [0x17] = "float", -- or int
    [0x18] = "terminator", -- Marks the end of a schema
    [0x19] = "float", -- or int
    [0x1A] = "long long",
}

-- --- Helper Functions ---
local function isAddressValid(addr)
  return readBytes(addr, 1, false) ~= nil
end

local function getPointer(base, offset)
  if not base or base == 0 then return nil end
  local addr = base + (offset or 0)
  if not isAddressValid(addr) then return nil end
  return readPointer(addr)
end

-- --- Recursive Parser ---
local function parse_schema(file, schema_address, field_prefix, parsed_schemas, depth)
    if not schema_address or schema_address == 0 then return end

    depth = depth or 0
    if depth > MAX_RECURSION_DEPTH then return end

    -- Prevent cycles when schemas reference each other
    if parsed_schemas[schema_address] then return end
    parsed_schemas[schema_address] = true

    local field_index = 0

    while true do
        local field_def_addr = schema_address + (field_index * SCHEMA_FIELD_DEF_SIZE)
        if not isAddressValid(field_def_addr) then break end

        local typecode = readInteger(field_def_addr)
        if not typecode or typecode == 0 or typecode == 0x18 then
            -- 0 or 0x18 means terminator
            break
        end

        local type_info = TYPECODE_MAP[typecode] or "unknown"

        local current_field_id
        if field_prefix ~= "" then
            current_field_id = field_prefix .. tostring(field_index)
        else
            current_field_id = tostring(field_index)
        end

        file:write(string.format("| %s | `0x%02X` | `%s` |\n", current_field_id, typecode, type_info))

        -- Complex types that carry a sub schema pointer at +24
        if typecode == 0x0F or typecode == 0x10 or typecode == 0x11 or typecode == 0x12 then
            local sub_schema_ptr = getPointer(field_def_addr, 24)
            if sub_schema_ptr and sub_schema_ptr ~= 0 then
                local sub_prefix = current_field_id .. "."
                parse_schema(file, sub_schema_ptr, sub_prefix, parsed_schemas, depth + 1)
            end
        end

        field_index = field_index + 1
    end
end

-- --- Main Script Logic ---
local function main()
    print("--- KX CMSG Full Schema Decoder ---")

    -- Prompt the user for a universal output path
    local default_path_suggestion = [[C:\CMSG_Protocol_Layout.md]]
    local output_path = inputQuery("Save CMSG Protocol Dump", "Enter the full path for the output file:", default_path_suggestion)

    if not output_path or #output_path == 0 then
        print("No output path provided. Aborting script.")
        return
    end
    print("Output will be saved to: " .. output_path)

    local file, err = io.open(output_path, "w")
    if not file then
        print("ERROR: Could not open file for writing: " .. tostring(err))
        return
    end

    local module_base = getAddress(GAME_MODULE)
    if not module_base or module_base == 0 then
        print(string.format("ERROR: Could not find module '%s'. Is the game running?", GAME_MODULE))
        file:close()
        return
    end

    -- 1. Find the live MsgConn object address
    local msgconn_ptr_addr = module_base + MSGCONN_STATIC_OFFSET
    local msgconn_addr = getPointer(msgconn_ptr_addr)
    if not msgconn_addr then
        print(string.format("ERROR: Could not read MsgConn pointer at 0x%X", msgconn_ptr_addr))
        file:close()
        return
    end

    -- 2. Find the SchemaTableInfo object address
    local schema_info_addr = getPointer(msgconn_addr, SCHEMA_INFO_OFFSET)
    if not schema_info_addr then
        print("ERROR: Could not read SchemaTableInfo pointer from MsgConn.")
        file:close()
        return
    end

    -- 3. Find the Table Base and Size
    local table_base = getPointer(schema_info_addr, TABLE_BASE_OFFSET)
    local table_size = readInteger(schema_info_addr + TABLE_SIZE_OFFSET)

    if not table_base or not table_size or table_size == 0 then
        print("ERROR: Could not read Table Base or Size from SchemaTableInfo.")
        file:close()
        return
    end

    print(string.format("Schema table found. Size: %d entries. Writing to %s", table_size, output_path))

    -- 4. Write file header
    file:write("# CMSG Protocol Schema Dump\n\n")
    file:write("This document was automatically generated by `KX_CMSG_Full_Schema_Decoder.lua`.\n\n")

    -- 5. Dump all schemas
    for i = 0, table_size - 1 do
        local entry_addr = table_base + (i * ENTRY_SIZE)
        local schema_addr = getPointer(entry_addr, SCHEMA_OFFSET_IN_ENTRY)

        file:write(string.format("## CMSG 0x%04X\n\n", i))
        if schema_addr and schema_addr ~= 0 then
            local relative_addr = schema_addr - module_base
            file:write('**Schema Address:** `"' .. GAME_MODULE .. '"+0x' .. string.format("%X`\n\n", relative_addr))
            file:write("| Field # | Typecode | Type Name |\n")
            file:write("| :--- | :--- | :--- |\n")

            local parsed_schemas = {}
            parse_schema(file, schema_addr, "", parsed_schemas, 0)
        else
            file:write("_(No schema defined for this opcode.)_\n")
        end
        file:write("\n")
    end

    file:close()
    print("\n--------------------------------------------------")
    print("Schema dump complete!")
    print("Output saved to: " .. output_path)
end

-- Run the main function
main()
