**Status: Unverified**

**This packet was documented by analyzing a schema from the client's data files. It has not been confirmed to match live network traffic and may be outdated or used in specific, niche circumstances.**

---

# CMSG 0x002B — Unknown (Descriptor-driven payload)

Status: Confirmed opcode. Payload originates from a descriptor; exact field layout not finalized.

## Summary

A Client-to-Server packet with opcode `0x002B` is emitted conditionally from a call site that constructs or fetches a small descriptor object. When the descriptor is available, the function sends opcode `0x002B`; when not, it falls back to opcode `0x0050`. This strongly suggests 0x002B is a more specific variant with a richer payload determined by the descriptor.

## Evidence

FUN_1402450C0 @ 0x1402450C0

Decompiled highlights:
```
uVar1 = FUN_1409a35f0();
local_d6 = (ushort)uVar1;
local_c0 = 0x100;
local_c8 = 0;
local_d0 = 0;
local_b8[0] = 0;
local_108 = local_b8;
local_d8 = 1;
local_28 = _DAT_142628808;
uStack_20 = _DAT_142628810;

puVar2 = (uint*)FUN_14024ab10(param_3, &local_d8, local_f0);
if (puVar2 == 0) {
    // Fallback
    local_e8 = *(uint*)(param_1 + 8);
    puVar2 = &local_e8;
    local_f8   = 0x000B0004;
    uStack_f4  = 0x03D60004;
    local_e4   = 0x3D60004000B0004;
    local_108  = (undefined2*)0xC;
    uVar1 = 0x50;                  // opcode 0x0050 in fallback
} else {
    // Descriptor path
    uVar1 = 0x2B;                  // opcode 0x002B when descriptor is present
    local_108 = (undefined2*)(uint64_t)local_f0[0];
}

MsgConn::QueuePacket(DAT_142628800, 0, uVar1, puVar2);
```

Notes:
- The immediate opcode is `0x2B` when `FUN_14024ab10` returns a descriptor pointer. In the fallback branch, the opcode is `0x50`.
- The pointer `puVar2` passed to `QueuePacket` is either the descriptor result or a simple local u32 (for the 0x50 variant).
- The local constants like `0x000B0004` and `0x03D60004` appear to be internal scratch/config values and are not directly written to the network buffer.

## Payload (current understanding)

- Source: The payload pointer (`puVar2`) comes from `FUN_14024ab10(param_3, &local_d8, local_f0)` when the descriptor exists. This indicates the on-wire content is built from or directly points to a descriptor structure resolved at runtime.
- Exact layout: Not yet finalized. No direct `Msg::MsgPack` construction observed in this function; it forwards a pointer to `QueuePacket`, implying prebuilt or previously packed data.

Current, conservative view:
```
struct CMSG_0x002B_Payload {
    // Descriptor-defined content (pointer returned by FUN_14024ab10).
    // Field sequence and sizes TBD.
};
```

Open questions:
- Descriptor structure returned by `FUN_14024ab10`: field order, sizes, and semantics.
- Whether other emitters for `0x002B` supplement or modify the payload.

## Related pipeline context

- The generic send pipeline (BuildAndSendGamePacket @ 0x140FD2927) uses `FUN_140FD5B50` to resolve opcode→schema entries. The `0x002B` entry likely points at a schema or format compatible with the descriptor built/fetched by `FUN_14024ab10`.
- Further analysis should decompile/disassemble the resolver’s table entry for opcode `0x002B` and xref the descriptor constructor to extract the field layout.

## Confidence

- Opcode: High (direct `QueuePacket(..., 0x2B, ...)` in a clearly gated branch).
- Payload: Confirmed descriptor-driven pointer; exact layout not claimed until corroborated by schema/pack sites.
- Name: Unknown (no suitable RTTI/string evidence yet).
