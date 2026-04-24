**Status: Unverified**

**This packet was documented by analyzing a schema from the client's data files. It has not been confirmed to match live network traffic and may be outdated or used in specific, niche circumstances.**

---

# CMSG 0x005F — Unknown (pointer payload from object)

Status: Confirmed opcode. Payload pointer origin identified; size not explicitly set at this site. Name TBD.

## Summary

A Client-to-Server packet with opcode `0x005F` is emitted with a payload pointer taken from an object field (offset `+0x38`).

## Evidence

Emitter: CMSG_Build_0x005F_FromObj38 @ 0x14023C182

Decompiled highlights:
```
MsgConn::QueuePacket(DAT_1426280F0, 0, 0x5F, unaff_RDI + 0x38);
```

Notes:
- Immediate opcode is `0x5F`.
- The payload pointer is `(obj+0x38)`.
- No explicit size is established at this call site; the size is likely implicit/known to the receiver or schema-driven elsewhere.

## Payload (current understanding)

Conservative view:
```
struct CMSG_0x005F_Payload {
    // Pointer to data at (obj+0x38).
    // Exact size and field composition are not set at this call site.
};
```

Open questions:
- Actual size and structure referred to by the pointer.
- Additional emitters and schema references that could define a fixed length.

## Confidence

- Opcode: High (direct `QueuePacket(..., 0x5F, ...)`).
- Pointer origin: High (object+0x38).
- Size/fields: Unknown at this site.
