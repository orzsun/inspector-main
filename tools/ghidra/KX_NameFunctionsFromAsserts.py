# Recovers original function names from embedded "D:\Perforce\...\Code\..." assertion strings.
# Part of the kx-packet-inspector project.
#
# This script works on Ghidra 10.x/11.x, handles ASCII, UTF-16, and Pascal strings,
# and creates proper namespaces.
#
# Licensed under the same terms as the kx-packet-inspector project:
# https://github.com/kxtools/kx-packet-inspector
#
# MIT License
#
# @category KX

import re
from ghidra.program.model.symbol import SourceType
from ghidra.util.exception import DuplicateNameException, InvalidInputException

# ------------------------------------------------------------------
# SETTINGS
# ------------------------------------------------------------------
DEBUG            = False
DEFAULT_PREFIXES = ("FUN_", "sub_")    # Rename only these
SOURCE_EXTS      = (".cpp", ".c", ".hpp", ".h")
PASCAL_OFFSETS   = (0, 4)              # 0 = C string, 4 = Pascal

PATH_RE = re.compile(
    r"(?:0)?D:\\Perforce\\(?:[^\\]+\\)+?Code\\(.+?)(?:\:\d+)?$",
    re.IGNORECASE,
)

# ------------------------------------------------------------------
# HELPERS
# ------------------------------------------------------------------
def log(msg):
    if DEBUG:
        print(msg)

def split_tokens(path_str):
    """Return list of path tokens inside Code\\, else None."""
    m = PATH_RE.search(path_str)
    if not m:
        return None
    rel = m.group(1)
    for ext in SOURCE_EXTS:
        if rel.endswith(ext):
            rel = rel[:-len(ext)]
            break
    return [p for p in rel.split("\\") if p]

def get_or_create_namespace(tokens):
    """Create (or reuse) nested namespaces and return the deepest one."""
    tab = currentProgram.getSymbolTable()
    ns  = currentProgram.getGlobalNamespace()
    for tok in tokens:
        try:
            ns = tab.getNamespace(tok, ns) or tab.createNameSpace(ns, tok, SourceType.ANALYSIS)
        except InvalidInputException:
            clean = re.sub(r"[^A-Za-z0-9_]", "_", tok)
            ns = tab.getNamespace(clean, ns) or tab.createNameSpace(ns, clean, SourceType.ANALYSIS)
    return ns

def rename_unique(func, base_name, namespace):
    """Rename func to namespace::base_name, adding _1, _2 if needed."""
    idx = 0
    while True:
        cand = base_name if idx == 0 else "{}_{}".format(base_name, idx)
        try:
            func.setName(cand, SourceType.ANALYSIS)
            func.getSymbol().setNamespace(namespace)
            return True
        except DuplicateNameException:
            idx += 1
        except (InvalidInputException, Exception) as exc:
            log("    [skip] {}".format(exc))
            return False

# ------------------------------------------------------------------
# MAIN
# ------------------------------------------------------------------
listing   = currentProgram.getListing()
data_iter = listing.getDefinedData(True)

try:
    monitor.initialize(listing.getNumDefinedData())
except AttributeError:
    monitor.initialize(0)

renamed, scanned = 0, 0
seen_funcs = set()

while data_iter.hasNext() and not monitor.isCancelled():
    data = data_iter.next()
    scanned += 1
    monitor.incrementProgress(1)

    # Convert any Java object to a Python str
    try:
        sval = str(data.getValue())
    except Exception:
        continue

    if "Perforce" not in sval:
        continue

    toks = split_tokens(sval)
    if not toks or len(toks) < 2:
        continue

    ns_tokens, func_token = toks[:-1], toks[-1]
    ns_obj = get_or_create_namespace(ns_tokens)

    for off in PASCAL_OFFSETS:
        addr = data.getAddress().add(off) if off else data.getAddress()
        refs = list(getReferencesTo(addr))
        if not refs:
            continue

        log("\n[STR ] {}  refs={}  {}::{}".format(
            addr, len(refs), "::".join(ns_tokens), func_token))

        for r in refs:
            f = getFunctionContaining(r.getFromAddress())
            if f is None or f in seen_funcs:
                continue
            seen_funcs.add(f)

            if not f.getName().startswith(DEFAULT_PREFIXES):
                log("    [skip] already named {}".format(f.getName()))
                continue

            if rename_unique(f, func_token, ns_obj):
                renamed += 1
                log("    [done] {} -> {}::{}".format(
                    f.getName(), "::".join(ns_tokens), func_token))

print("\n[KX] Name Functions from Asserts: Completed.")
print("    {} strings scanned, {} functions renamed.".format(scanned, renamed))
