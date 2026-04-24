--[[
  KX CMSG Schema Dumper
  Part of the kx-packet-inspector project.

  This script automatically finds the CMSG schema table by following the pointer
  chain from the static base of the MsgConn object. It then dumps the
  Opcode -> Schema Address mapping, providing both live and relative addresses.

  Licensed under the MIT License, consistent with the kx-packet-inspector project.
  https://github.com/Krixx1337/kx-packet-inspector
]]

-- Configuration
local game_module = "Gw2-64.exe"
local msgconn_static_offset = 0x2628290
local schema_info_offset = 0x18
local table_base_offset = 0x50
local table_size_offset = 0x5c
local entry_size = 16
local schema_offset = 8

-- Helper to check if an address is valid to read from
function isAddressValid(addr)
  -- CE's readBytes function returns nil on an invalid read
  return readBytes(addr, 1, false) ~= nil
end

-- Helper to safely read pointers and check for errors
function getPointer(base, offset)
  if not base or base == 0 then return nil end
  local addr = base + (offset or 0)
  if not isAddressValid(addr) then return nil end
  return readPointer(addr)
end

-- --- Main Script Logic ---
print("--- KX CMSG Schema Dumper ---")

-- Get the module base address for calculating relative offsets
local module_base = getAddress(game_module)
if not module_base or module_base == 0 then
  print(string.format("ERROR: Could not find module '%s'. Is the game running?", game_module))
  return
end
print(string.format("Found module '%s' at: 0x%X", game_module, module_base))

-- 1. Find the live MsgConn object address
local msgconn_ptr_addr = module_base + msgconn_static_offset
local msgconn_addr = getPointer(msgconn_ptr_addr)
if not msgconn_addr then
  print(string.format("ERROR: Could not read MsgConn pointer at 0x%X", msgconn_ptr_addr))
  return
end
print(string.format("Found live MsgConn object at: 0x%X", msgconn_addr))

-- 2. Find the SchemaTableInfo object address
local schema_info_addr = getPointer(msgconn_addr, schema_info_offset)
if not schema_info_addr then
  print("ERROR: Could not read SchemaTableInfo pointer from MsgConn.")
  return
end
print(string.format("Found SchemaTableInfo object at: 0x%X", schema_info_addr))

-- 3. Find the Table Base and Size
local table_base = getPointer(schema_info_addr, table_base_offset)
local table_size_addr = schema_info_addr + table_size_offset
local table_size = 0
if isAddressValid(table_size_addr) then
  table_size = readInteger(table_size_addr)
end

if not table_base or table_size == 0 then
  print("ERROR: Could not read Table Base or Size from SchemaTableInfo.")
  return
end
print(string.format("Found Schema Table at: 0x%X (Size: %d entries)", table_base, table_size))
print("------------------------------------------")

-- 4. Dump the table
local found_count = 0
for i = 0, table_size - 1 do
  local entry_addr = table_base + (i * entry_size)
  local schema_addr = getPointer(entry_addr, schema_offset)
  
  if schema_addr and schema_addr ~= 0 then
    local relative_addr = schema_addr - module_base
    print(string.format("Opcode: 0x%04X -> Schema: 0x%X (Live) | \"%s\"+0x%X (Relative)",
                        i, schema_addr, game_module, relative_addr))
    found_count = found_count + 1
  end
end

print("------------------------------------------")
print(string.format("--- Dump Complete: Found %d schema entries. ---", found_count))