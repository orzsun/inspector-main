# CMSG_AGENT_LINK (0x0036)

**Direction**: Client -> Server
**Status**: Confirmed Builder (Schema-Driven), Unobserved in Live Traffic

## Summary

This packet is designed to link two agents together (e.g., a pet to a character). Its structure is defined by a schema, and a dedicated builder function exists in the client. However, this packet has not been observed during normal gameplay, suggesting it is used in very specific contexts or is legacy code.

## Construction

*   **Builder Function:** [`CMSG::BuildAgentLink`](../evidence/CMSG_BuildAgentLink.c)
*   **Pathway:** Direct Queue (`MsgConn::QueuePacket`)
*   **Schema Address:** `DAT_142513080`

## Packet Structure (from Schema)

| Offset | Type | Name | Description |
|---|---|---|---|
| 0x00 | `uint16` | AgentId | The ID of the agent being linked (the "child"). |
| 0x02 | `uint16` | ParentId | The ID of the agent to link to (the "parent"). |
| 0x04 | `uint16` | Flags | Purpose unknown. |