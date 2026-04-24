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
from ghidra.program.model.mem import MemoryAccessException
from ghidra.program.model.symbol import SourceType
from ghidra.util.exception import DuplicateNameException, InvalidInputException
import sys, os, time, re
from java.lang import System as JavaSystem

# -----------------------------
# KX logging
# -----------------------------

def kx_print(msg="", level="info", prefix="[KX]", ui_yield=False):
    """
    Reliable console logging for Ghidra scripts.

    - level: "info" or "error"
    - prefix: text to prepend, default "[KX]". Duplicate leading prefixes are collapsed.
    - ui_yield: True to hint the UI to update during long runs
    """
    s = "" if msg is None else str(msg)

    # Normalize and collapse duplicate leading prefixes like "[KX] [KX] message"
    if prefix:
        stripped = s.lstrip()
        while stripped.startswith(prefix):
            stripped = stripped[len(prefix):].lstrip()
        s = "{} {}".format(prefix, stripped)
    else:
        s = s.lstrip()

    # Prefer Ghidra Script Console
    try:
        if level == "error":
            printerr(s)
        else:
            println(s)
        if ui_yield:
            try:
                from java.lang import Thread
                Thread.yield()
            except:
                pass
        return
    except:
        pass

    # Fallback to Python stdout
    try:
        import sys
        sys.stdout.write(s + "\n")
        sys.stdout.flush()
    except:
        pass

    # Optional UI yield to help console refresh
    if ui_yield:
        try:
            from java.lang import Thread
            Thread.yield()
        except:
            pass

def kx_print_important(msg=""):
    s = str(msg)
    try:
        printerr(s)  # shows in red in Script Console
    except:
        kx_print(s)

# -----------------------------
# Config
# -----------------------------
DEBUG = False                     # set True if you want verbose logs
MAX_LOG_HITS = 10                 # preview at most this many raw hits when DEBUG
MAX_CLASSES = 20000               # hard cap on number of classes to dump
MAX_MEMBERS_PER_CLASS = 4096      # hard cap per class to avoid bad counts
CHUNK_SIZE = 1024 * 1024          # 1 MB read chunks for manual scan
PROGRESS_EVERY = 200              # console progress cadence in classes

# Function renaming controls
RENAME_FUNCTIONS = True           # turn off if you do not want any renaming
DEFAULT_PREFIXES = ("FUN_", "sub_")  # only rename when function name starts with one of these
AUTO_CREATE_FUNCTIONS = False     # if a hit is not inside a function, optionally create one at the hit
RENAME_NAMESPACE_TOKENS = ("KX", "Register")  # functions will be placed under this namespace chain
FUNC_NAME_PREFIX = "Register_"    # base name prefix, final name like Register_ClassName

# -----------------------------
# Basic helpers
# -----------------------------

def is_valid_read_ptr(p):
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
    if not is_valid_read_ptr(p):
        return None
    s = []
    a = toAddr(p)
    try:
        for _ in range(limit):
            b = getByte(a) & 0xFF
            if b == 0:
                break
            if 32 <= b <= 126:
                s.append(chr(b))
            else:
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
    if not s or s == "EMPTY" or s == "READ_ERROR":
        return False
    try:
        return all(32 <= ord(c) <= 126 for c in s) and len(s) <= 128
    except:
        return False

def read_u32(buf, i):
    # buf is a Java byte[] or list of ints, values may be signed
    return ((buf[i] & 0xFF)
            | ((buf[i+1] & 0xFF) << 8)
            | ((buf[i+2] & 0xFF) << 16)
            | ((buf[i+3] & 0xFF) << 24))

def to_signed32(x):
    return x - 0x100000000 if x & 0x80000000 else x

# -----------------------------
# Identifier and type sanitizers
# -----------------------------

IDENT_OK_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
TYPE_SAFE_CHARS_RE = re.compile(r"^[A-Za-z0-9_:\*\&\[\]<> ]+$")
ARTIFACT_TOKEN_RE = re.compile(r"^[a-z]{1,3}[0-9a-f]{1,4}B$", re.IGNORECASE)

PRIMITIVE_TYPES = set((
    "bool", "char", "signed char", "unsigned char",
    "short", "unsigned short",
    "int", "unsigned int",
    "long", "unsigned long",
    "long long", "unsigned long long",
    "float", "double", "long double",
    "void", "void*"
))
QUALIFIERS = set(("const", "volatile"))
ALLOWED_PREFIXES = (
    "hk", "hkp", "hkx", "hka", "hkcd", "hkai",
    "hkRef", "hkArray", "hkString", "std"
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
    Reject one letter junk and common artifact blobs like pm4B, xb4B, xc4B, t4B.
    """
    if tok in PRIMITIVE_TYPES or tok in QUALIFIERS:
        return True
    if ARTIFACT_TOKEN_RE.match(tok):
        return False
    if not IDENT_OK_RE.match(tok):
        return False
    if len(tok) < 2:
        return False
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
    tokens = re.split(r"(\s+|::|<|>|,|\[|\]|\*+|&+)", s)
    out = []
    for t in tokens:
        if not t:
            continue
        if t.isspace() or t in ("::", "<", ">", ",", "[", "]"):
            out.append(t)
            continue
        if re.match(r"^(?:\*+|&+)$", t):
            out.append(t)
            continue
        if is_valid_type_ident_token(t):
            out.append(t)
            continue
        return fallback
    result = "".join(out).strip()
    if not result or ARTIFACT_TOKEN_RE.match(result):
        return fallback
    return result

# -----------------------------
# Type decoding logic
# -----------------------------

# Educated guess for common Tag IDs to C++ type mapping.
# These are based on the sizes observed in Reflect_GenericCopyDispatcher and Reflect_MemmoveDispatcher.
TAG_ID_TO_CPP_TYPE_MAP = {
    0x1: "bool",
    0x2: "short",
    0x3: "int",
    0x4: "float",
    0x5: "long long",
    0x6: "double",
    0x7: "void*",
    # Extend as needed from the VM switch.
}

def decode_type_data(type_data_val):
    offset = type_data_val & 0xFFFF
    flags = (type_data_val >> 16) & 0xFFFF
    return offset, flags

def describe_type(signature_or_type_val, type_data_or_offset=None):
    """
    Returns a human readable, sanitized type name.
    The second parameter is accepted for future extension.
    """
    if signature_or_type_val == 0:
        return "unknown_t"
    # Try as C string then sanitize
    type_str = read_c_string(signature_or_type_val)
    if type_str and type_str not in ("READ_ERROR", "EMPTY"):
        safe = sanitize_cpp_type_name(type_str, None)
        if safe:
            return safe
    # Try numeric tag map
    if signature_or_type_val in TAG_ID_TO_CPP_TYPE_MAP:
        return TAG_ID_TO_CPP_TYPE_MAP[signature_or_type_val]
    # Opaque
    return "TypeTag_0x{:X}".format(signature_or_type_val)

# -----------------------------
# Heuristics for initializer structs
# -----------------------------

def looks_plausible_initializer(a):
    """
    Validate that address a points to a plausible initializer struct.
    Returns (ok, class_name, parent_name_ptr, member_list_ptr, num_members)
    """
    try:
        class_name_ptr = u64(a.add(0x00))
        parent_name_ptr = u64(a.add(0x08))
        member_list_ptr = u64(a.add(0x18))
        num_members = u32(a.add(0x20))
    except:
        return False, None, None, None, None

    if not is_valid_read_ptr(class_name_ptr):
        return False, None, None, None, None

    name = read_c_string(class_name_ptr)
    if not is_ascii_like(name):
        return False, None, None, None, None

    if num_members > MAX_MEMBERS_PER_CLASS:
        if DEBUG:
            kx_print("    [skip] {} num_members {} too large at {}".format(name, num_members, a))
        num_members = MAX_MEMBERS_PER_CLASS

    if num_members > 0 and not is_valid_read_ptr(member_list_ptr):
        return False, None, None, None, None

    return True, name, parent_name_ptr, member_list_ptr, num_members

# -----------------------------
# Manual byte pattern scanner
# -----------------------------

def _exec_blocks():
    mem = currentProgram.getMemory()
    for blk in mem.getBlocks():
        if blk.isInitialized() and blk.isLoaded() and blk.isExecute():
            yield blk

def _scan_block_for_pattern(blk, pat, mask):
    hits = []
    pat_len = len(pat)
    if pat_len == 0:
        return hits
    start = blk.getStart()
    size = blk.getSize()
    overlap = pat_len - 1
    pos = 0
    while pos < size:
        read_len = min(CHUNK_SIZE, size - pos)
        buf = getBytes(start.add(pos), read_len)
        if buf is None:
            break
        b = [(x & 0xFF) for x in buf]
        limit = read_len - pat_len + 1
        if limit > 0:
            for i in range(0, limit):
                ok = True
                for j in range(pat_len):
                    if mask[j] and b[i + j] != pat[j]:
                        ok = False
                        break
                if ok:
                    hits.append(start.add(pos + i))
        if read_len < CHUNK_SIZE:
            break
        pos += read_len - overlap
    return hits

def _build_sig(pattern_bytes, wildcard_mask):
    pat = [(int(x) & 0xFF) for x in pattern_bytes]
    mask = [1 if (int(m) & 0xFF) else 0 for m in wildcard_mask]
    return pat, mask

def find_initializer_addresses_from_pattern(pattern_bytes, wildcard_mask):
    """
    Scan executable blocks for the exact UI pattern.
    For each hit, decode the LEA rip relative target at +3.
    Validate targets with looks_plausible_initializer.
    Returns (initializer_addresses, rename_map)
    """
    pat, mask = _build_sig(pattern_bytes, wildcard_mask)
    pretty = " ".join("{:02X}".format(b) if m else "??" for b, m in zip(pat, mask))
    kx_print("[KX] Manual scan for UI pattern: {}".format(pretty))

    all_hits = []
    blocks = list(_exec_blocks())
    if DEBUG:
        names = [b.getName() for b in blocks]
        kx_print("[KX] manual scan over {} blocks: {}".format(len(blocks), names))
    for blk in blocks:
        hits = _scan_block_for_pattern(blk, pat, mask)
        kx_print('    [scan] block "{}" {}..{} size {} hits {}'.format(
            blk.getName(), blk.getStart(), blk.getEnd(), blk.getSize(), len(hits)))
        all_hits.extend(hits)

    if DEBUG:
        kx_print("[KX] manual scan total raw hits: {}".format(len(all_hits)))
        for a in all_hits[:MAX_LOG_HITS]:
            kx_print("        hit @ {}".format(a))
        if len(all_hits) > MAX_LOG_HITS:
            kx_print("        ... {} more".format(len(all_hits) - MAX_LOG_HITS))

    inits = []
    seen = set()
    rename_map = {}
    for h in all_hits:
        try:
            window = getBytes(h, 7)  # 48 8D 15 imm32  total 7 bytes
            if window is None or len(window) < 7:
                continue
            if (window[0] & 0xFF, window[1] & 0xFF, window[2] & 0xFF) != (0x48, 0x8D, 0x15):
                continue
            disp_u = read_u32(window, 3)
            disp = to_signed32(disp_u)
            next_ip = h.add(7).getOffset()
            target_off = next_ip + disp
            target = toAddr(target_off)
            if target in seen:
                continue
            ok, name, parent_ptr, member_list_ptr, num_members = looks_plausible_initializer(target)
            if not ok:
                continue
            seen.add(target)
            inits.append(target)

            f = getFunctionContaining(h)
            if f is None and AUTO_CREATE_FUNCTIONS:
                try:
                    disassemble(h)
                    f = createFunction(h, None)
                except Exception:
                    f = None
            if f is not None and f not in rename_map:
                rename_map[f] = name

            if DEBUG:
                kx_print('    [+] initializer "{}" @ {} members {}'.format(name, target, num_members))
            if len(inits) >= MAX_CLASSES:
                break
        except Exception as e:
            if DEBUG:
                kx_print("    [skip] decode error at {}: {}".format(h, e))
            continue

    kx_print("[KX] total plausible initializers: {}".format(len(inits)))
    return inits, rename_map

# -----------------------------
# Renaming support
# -----------------------------

def sanitize_identifier(s):
    s2 = re.sub(r"[^A-Za-z0-9_]", "_", s)
    if s2 and s2[0].isdigit():
        s2 = "_" + s2
    return s2[:200] if len(s2) > 200 else s2

def get_or_create_namespace(tokens):
    tab = currentProgram.getSymbolTable()
    ns = currentProgram.getGlobalNamespace()
    for tok in tokens:
        try:
            ns = tab.getNamespace(tok, ns) or tab.createNameSpace(ns, tok, SourceType.ANALYSIS)
        except InvalidInputException:
            clean = sanitize_identifier(tok)
            ns = tab.getNamespace(clean, ns) or tab.createNameSpace(ns, clean, SourceType.ANALYSIS)
    return ns

def rename_unique(func, base_name, namespace):
    idx = 0
    while True:
        cand = base_name if idx == 0 else "{}_{}".format(base_name, idx)
        try:
            func.setName(cand, SourceType.ANALYSIS)
            func.getSymbol().setNamespace(namespace)
            return True
        except DuplicateNameException:
            idx += 1
        except InvalidInputException:
            if idx == 0:
                cand = sanitize_identifier(cand)
                idx = 0
                continue
            return False
        except Exception:
            return False

def rename_registration_functions(rename_map):
    if not RENAME_FUNCTIONS or not rename_map:
        return 0
    ns = get_or_create_namespace(RENAME_NAMESPACE_TOKENS)
    renamed = 0
    for f, class_name in rename_map.items():
        try:
            old = f.getName()
            if DEFAULT_PREFIXES and not old.startswith(DEFAULT_PREFIXES):
                continue
            base = FUNC_NAME_PREFIX + sanitize_identifier(class_name)
            if rename_unique(f, base, ns):
                renamed += 1
                if DEBUG:
                    kx_print("[KX] renamed {} -> {}::{}".format(old, "::".join(RENAME_NAMESPACE_TOKENS), base))
        except Exception as e:
            if DEBUG:
                kx_print("[KX] rename failed for {}: {}".format(f, e))
    kx_print("[KX] renamed {} functions from registration sites".format(renamed))
    return renamed

# -----------------------------
# Dumping
# -----------------------------

def dump_class_layouts(addresses, output_file):
    member_stride = 0x18
    class_count = 0
    member_count = 0

    output_file.write("//----------------------------------------------------\n")
    output_file.write("// Dumped by the KX Reflection Dumper\n")
    output_file.write("// Part of the kx-packet-inspector project\n")
    output_file.write("//----------------------------------------------------\n\n")
    output_file.write("// --- ArenaNet Custom hkReflect Dump ---\n\n")
    output_file.write("// Generated by KX_ReflectionDumper.py\n")
    output_file.write("// Analysis based on reversing the engine's Type Decoder VM.\n\n")

    total_targets = len(addresses)
    kx_print("[KX] dumping {} candidate classes...".format(min(total_targets, MAX_CLASSES)))

    for idx, addr in enumerate(addresses[:MAX_CLASSES]):
        try:
            ok, class_name_raw, parent_name_ptr, member_list_ptr, num_members = looks_plausible_initializer(addr)
            if not ok:
                if DEBUG:
                    kx_print("    [skip] not plausible at {}".format(addr))
                continue

            # Sanitize class and parent names
            class_name = sanitize_cpp_identifier(class_name_raw, "Class_0x{:X}".format(addr.getOffset()))
            parent_name_raw = read_c_string(parent_name_ptr)
            if not is_ascii_like(parent_name_raw) or parent_name_raw == class_name_raw:
                parent_name_raw = None
            parent_name = sanitize_cpp_identifier(parent_name_raw, "Base") if parent_name_raw else None

            output_file.write("\n//----------------------------------------------------\n")
            output_file.write("// Class: {} (Initializer @ {})\n".format(class_name, addr))

            if parent_name:
                output_file.write("struct {} : public {} {{\n".format(class_name, parent_name))
            else:
                output_file.write("struct {} {{\n".format(class_name))

            if num_members > 0 and is_valid_read_ptr(member_list_ptr):
                mbase = toAddr(member_list_ptr)
                safe_n = min(num_members, MAX_MEMBERS_PER_CLASS)
                for i in range(safe_n):
                    try:
                        entry_addr = mbase.add(i * member_stride)

                        signature_or_type_ptr = u64(entry_addr.add(0x00))
                        name_ptr              = u64(entry_addr.add(0x08))
                        type_data_or_offset   = u64(entry_addr.add(0x10))

                        # Decode offset and flags first
                        field_offset, type_flags = decode_type_data(type_data_or_offset)

                        # Field name sanitize with artifact filter
                        field_name_raw = read_c_string(name_ptr)
                        if not is_ascii_like(field_name_raw):
                            field_name_raw = None
                        if field_name_raw and ARTIFACT_TOKEN_RE.match(field_name_raw):
                            field_name_raw = None
                        fallback_field = "member_0x{:X}".format(name_ptr)
                        safe_field_name = sanitize_cpp_identifier(field_name_raw, fallback_field)

                        # Type name sanitize with artifact filter
                        type_name_raw = describe_type(signature_or_type_ptr, type_data_or_offset)
                        safe_type_name = sanitize_cpp_type_name(type_name_raw, "unknown_t")
                        if ARTIFACT_TOKEN_RE.match(safe_type_name):
                            safe_type_name = "unknown_t"

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

            output_file.write("};\n")
            class_count += 1

            if class_count % PROGRESS_EVERY == 0:
                kx_print("[KX] progress, {} classes written so far".format(class_count))
                try:
                    monitor.setMessage("KX dump progress, {} classes".format(class_count))
                except:
                    pass

        except Exception as e:
            if DEBUG:
                kx_print("    [skip] exception at {}: {}".format(addr, e))
            continue

    return class_count, member_count

# -----------------------------
# Entry point
# -----------------------------

if __name__ == "__main__":
    try:
        kx_print("[KX] KX Reflection Dumper starting")
        output_file_path = askFile("Select Output File", "Save Dump").getAbsolutePath()
        if output_file_path:
            with open(output_file_path, "w") as f:
                # Pattern: 48 8D 15 ?? ?? ?? ?? 48 8B CB FF 50 08
                # Mask:    FF FF FF 00 00 00 00 FF FF FF FF FF FF
                pattern_bytes = [0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xCB, 0xFF, 0x50, 0x08]
                wildcard_mask = [0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF]

                all_initializer_addresses, rename_map = find_initializer_addresses_from_pattern(pattern_bytes, wildcard_mask)

                if not all_initializer_addresses:
                    kx_print_important("[KX] Could not find any ClassInitializers. Aborting.")
                    exit()

                total_classes, total_members = dump_class_layouts(all_initializer_addresses, f)

                if RENAME_FUNCTIONS:
                    try:
                        renamed = rename_registration_functions(rename_map)
                        if DEBUG:
                            kx_print("[KX] rename pass complete, {} functions".format(renamed))
                    except Exception as e:
                        kx_print("[KX] rename pass failed: {}".format(e), level="error")

                summary_message = "--- Dump Finished ---\n"
                summary_message += "Successfully dumped {} classes and {} members.\n".format(total_classes, total_members)
                summary_message += "Output saved to: {}\n".format(output_file_path)

                f.write("\n// --- Summary ---\n")
                f.write("// " + summary_message.replace("\n", "\n// "))

                kx_print_important(summary_message)
                try:
                    monitor.setMessage("KX dump done")
                except:
                    pass
    except Exception as e:
        import traceback
        kx_print_important("An error occurred:")
        traceback.print_exc()
