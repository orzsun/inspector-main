# Dumps ArenaNet's custom reflection system to a C++ header file.
# Part of the kx-packet-inspector project.
#
# This script is the final version based on a successful forensic analysis
# of the engine's type decoder virtual machine.
#
# Licensed under the same terms as the kx-packet-inspector project:
# https://github.com/kxtools/kx-packet-inspector
#
# MIT License
#
# @category KX

from ghidra.program.model.address import Address
import os, re

# --- Helper Functions ---

def is_valid_read_ptr(p):
    """Checks if a long integer is a plausible pointer in the program's memory space."""
    if p is None or p == 0:
        return False
    try:
        blk = getMemoryBlock(toAddr(p))
        if blk is None or not blk.isInitialized() or not blk.isRead():
            return False
    except:
        return False
    return True

def read_c_string(p, limit=128):
    """Safely reads a C-style string from a given address."""
    if not is_valid_read_ptr(p):
        return None  # Return None for invalid pointers
    s = []
    a = toAddr(p)
    try:
        for _ in range(limit):
            b = getByte(a) & 0xFF
            if b == 0:
                break
            if 32 <= b <= 126:  # Printable ASCII
                s.append(chr(b))
            else:
                # Stop at the first non-printable character
                break
            a = a.add(1)
    except:
        return "READ_ERROR"
    return "".join(s) if s else "EMPTY"

def u64(addr):
    return getLong(addr) & 0xFFFFFFFFFFFFFFFF

def u32(addr):
    return getInt(addr) & 0xFFFFFFFF

def is_ascii_like(s):
    if not s or s in ("EMPTY", "READ_ERROR"):
        return False
    try:
        return all(32 <= ord(c) <= 126 for c in s) and len(s) <= 128
    except:
        return False

# --- Identifier and Type Sanitizers ---

IDENT_OK_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
TYPE_SAFE_CHARS_RE = re.compile(r"^[A-Za-z0-9_:\*\&\[\]<> ]+$")

# Allow these tokens verbatim as part of a type
PRIMITIVE_TYPES = {
    "bool", "char", "signed char", "unsigned char",
    "short", "unsigned short",
    "int", "unsigned int",
    "long", "unsigned long",
    "long long", "unsigned long long",
    "float", "double", "long double",
    "void", "void*"
}
QUALIFIERS = {"const", "volatile"}
ALLOWED_PREFIXES = (
    "hk", "hkp", "hkx", "hka", "hkcd", "hkai", "hkRef", "hkArray", "hkString",
    "std"
)

def sanitize_cpp_identifier(name, fallback):
    """
    Make a safe C++ identifier for fields or class names.
    If the cleaned name is empty or invalid, use fallback.
    """
    if not name:
        return fallback

    clean = re.sub(r"[^A-Za-z0-9_]", "_", name)
    clean = re.sub(r"_+", "_", clean).strip("_")

    if clean and clean[0].isdigit():
        clean = "_" + clean

    if not clean or not IDENT_OK_RE.match(clean):
        return fallback

    return clean[:128]

def is_valid_type_ident_token(tok):
    """
    Accept realistic identifier tokens that could appear in a C++ type.
    Reject one letter junk and odd lowercase blobs like 'pm4B', 'xc4B'.
    """
    if tok in PRIMITIVE_TYPES or tok in QUALIFIERS:
        return True
    if not IDENT_OK_RE.match(tok):
        return False
    # Must be at least 2 chars to avoid 'P', 'x' noise
    if len(tok) < 2:
        return False
    # Accept if it starts with uppercase (e.g., Havok classes), or allowed prefixes
    if tok[0].isupper():
        return True
    for pref in ALLOWED_PREFIXES:
        if tok.startswith(pref):
            return True
    return False

def is_plausible_type_string(s):
    """
    Conservative filter for type names coming from strings.
    """
    if not s or s in ("EMPTY", "READ_ERROR"):
        return False
    s = s.strip()
    if s in PRIMITIVE_TYPES:
        return True
    if not TYPE_SAFE_CHARS_RE.match(s):
        return False
    if not re.search(r"[A-Za-z]", s):
        return False
    return True

def sanitize_cpp_type_name(name, fallback):
    """
    Keep a readable but safe C++ type name, allowing namespaces and pointer/reference syntax.
    If invalid, return fallback.
    """
    if not name:
        return fallback

    s = " ".join(name.split())

    if s in PRIMITIVE_TYPES:
        return s

    if not is_plausible_type_string(s):
        return fallback

    # Tokenize around operators and separators
    tokens = re.split(r"(\s+|::|<|>|,|\[|\]|\*+|&+)", s)
    out = []
    for t in tokens:
        if not t:
            continue
        if t.isspace() or t in ("::", "<", ">", ",", "[", "]"):
            out.append(t)
            continue
        # Jython safe equivalent of re.fullmatch
        if re.match(r"^(?:\*+|&+)$", t):
            out.append(t)
            continue
        if is_valid_type_ident_token(t):
            out.append(t)
            continue
        # Unknown token, bail to fallback
        return fallback

    result = "".join(out).strip()
    return result if result else fallback

# --- Type Decoding Logic ---

# Educated guess for common Tag IDs to C++ type mapping.
TAG_ID_TO_CPP_TYPE_MAP = {
    0x1: "bool",        # 1-byte operations
    0x2: "short",       # 2-byte operations
    0x3: "int",         # 4-byte operations
    0x4: "float",       # 4-byte operations
    0x5: "long long",   # 8-byte operations
    0x6: "double",      # 8-byte operations
    0x7: "void*",       # 8-byte operations (generic pointer)
    # Add more as discovered from the actual Type Decoder VM switch.
}

def decode_type_data(type_data_val):
    """
    Decodes the typeDataOrOffset field from the MemberInitializer.
    Based on forensic analysis, this is a bitfield.
    """
    offset = type_data_val & 0xFFFF
    flags = (type_data_val >> 16) & 0xFFFF
    return offset, flags

def describe_type(signature_or_type_val):
    """
    Creates a human readable description of a member's type with sanitization.
    Attempts to infer C++ types based on common Tag IDs and by reading strings.
    """
    # 0 is commonly used for no tag
    if signature_or_type_val == 0:
        return "unknown_t"

    # Try as C string then sanitize strictly
    type_str = read_c_string(signature_or_type_val)
    if type_str and type_str not in ("READ_ERROR", "EMPTY"):
        safe = sanitize_cpp_type_name(type_str, None)
        if safe:
            return safe

    # Try as known primitive tag id
    if signature_or_type_val in TAG_ID_TO_CPP_TYPE_MAP:
        return TAG_ID_TO_CPP_TYPE_MAP[signature_or_type_val]

    # Fallback to opaque tag
    return "TypeTag_0x{:X}".format(signature_or_type_val)

# --- Main Dumper Logic ---

def dump_class_layouts(start_ptr, end_ptr, output_file):
    cur = toAddr(start_ptr)
    end = toAddr(end_ptr)

    class_stride = 0x28  # 40 bytes (confirmed)
    member_stride = 0x18 # 24 bytes (confirmed)

    class_count = 0
    member_count = 0

    output_file.write("//----------------------------------------------------\n")
    output_file.write("// Dumped by the KX Reflection Dumper\n")
    output_file.write("// Part of the kx-packet-inspector project\n")
    output_file.write("//----------------------------------------------------\n\n")
    output_file.write("// --- ArenaNet Custom hkReflect Dump ---\n")
    output_file.write("// Generated by KX_ReflectionDumper.py (Final Version)\n")
    output_file.write("// Analysis based on reversing the engine's Type Decoder VM.\n\n")

    while cur < end and not monitor.isCancelled():
        # Read the ClassInitializer structure
        class_name_ptr  = u64(cur.add(0x00))
        parent_name_ptr = u64(cur.add(0x08))
        member_list_ptr = u64(cur.add(0x18))
        num_members     = u32(cur.add(0x20))

        class_name_raw = read_c_string(class_name_ptr)
        if not is_ascii_like(class_name_raw):
            class_name_raw = "Class_0x{:X}".format(class_name_ptr)
        class_name = sanitize_cpp_identifier(class_name_raw, "Class_0x{:X}".format(class_name_ptr))

        parent_name_raw = read_c_string(parent_name_ptr)
        if not is_ascii_like(parent_name_raw) or parent_name_raw == class_name_raw:
            parent_name_raw = None
        parent_name = sanitize_cpp_identifier(parent_name_raw, "Base") if parent_name_raw else None

        output_file.write("//----------------------------------------------------\n")
        output_file.write("// Class: {} (Initializer @ {})\n".format(class_name, cur))

        if parent_name:
            output_file.write("struct {} : public {} {{\n".format(class_name, parent_name))
        else:
            output_file.write("struct {} {{\n".format(class_name))

        if num_members > 0 and is_valid_read_ptr(member_list_ptr):
            mbase = toAddr(member_list_ptr)
            for i in range(num_members):
                try:
                    entry_addr = mbase.add(i * member_stride)

                    # Read MemberInitializer fields
                    signature_or_type_ptr = u64(entry_addr.add(0x00))
                    name_ptr              = u64(entry_addr.add(0x08))
                    type_data_or_offset   = u64(entry_addr.add(0x10))

                    # Decode offset and flags first, so field_offset exists before writing
                    field_offset, type_flags = decode_type_data(type_data_or_offset)

                    # Field name sanitize
                    field_name_raw = read_c_string(name_ptr)
                    if not is_ascii_like(field_name_raw):
                        field_name_raw = None
                    # Nuke artifacts like pm4B, xc4B, t4B, xb4B
                    if field_name_raw and re.match(r"^[a-z]{1,3}[0-9a-f]{1,4}B$", field_name_raw):
                        field_name_raw = None
                    fallback_field = "member_0x{:X}".format(name_ptr)
                    safe_field_name = sanitize_cpp_identifier(field_name_raw, fallback_field)

                    # Type name sanitize
                    type_name_raw = describe_type(signature_or_type_ptr)
                    safe_type_name = sanitize_cpp_type_name(type_name_raw, "unknown_t")
                    # Nuke the same artifact pattern on types
                    if re.match(r"^[a-z]{1,3}[0-9a-f]{1,4}B$", safe_type_name):
                        safe_type_name = "unknown_t"

                    # Write the member line
                    output_file.write(
                        "    /* 0x{:04X} */ {:<40} {}; // Flags: 0x{:04X}\n".format(
                            field_offset, safe_type_name, safe_field_name, type_flags
                        )
                    )
                    member_count += 1

                except MemoryAccessException:
                    output_file.write("    // Read error in member list at entry {}\n".format(i))
                    break
                except Exception as e:
                    output_file.write("    // Exception at entry {}: {}\n".format(i, e))
                    break

        output_file.write("};\n\n")

        class_count += 1
        cur = cur.add(class_stride)

    return class_count, member_count

# --- Script Entry Point ---

if __name__ == "__main__":
    try:
        output_file_path = askFile("Select Output File", "Save Dump").getAbsolutePath()

        if output_file_path:
            with open(output_file_path, "w") as f:
                initializer_blocks = [
                    (0x142607840, 0x14260BC60),
                ]

                total_classes = 0
                total_members = 0

                f.write("#pragma once\n\n")
                f.write("// This file was generated by a script. Do not edit.\n\n")

                for start_addr, end_addr in initializer_blocks:
                    f.write("// Dumping block: 0x{:X} - 0x{:X}\n".format(start_addr, end_addr))
                    classes, members = dump_class_layouts(start_addr, end_addr, f)
                    total_classes += classes
                    total_members += members

                summary_message = "--- Dump Finished ---\n"
                summary_message += "Successfully dumped {} classes and {} members.\n".format(total_classes, total_members)
                summary_message += "Output saved to: {}\n".format(output_file_path)

                f.write("\n// --- Summary ---\n")
                f.write("// " + summary_message.replace("\n", "\n// "))

                print(summary_message)

    except Exception as e:
        import traceback
        print("An error occurred:")
        traceback.print_exc()
