# CMSG_PING_RESPONSE (0x0006)

**Direction:** Client -> Server
**Status:** Confirmed Builder, Unobserved in Normal Gameplay

## Summary

This packet is sent by the client as a direct response to a server command (`SMSG_SERVER_COMMAND 0x0027`, subtype 4). It functions as a "ping-pong" mechanism to confirm client responsiveness.

Analysis of the builder function reveals that the packet is only sent if a value provided by the server is 5001 or greater. During normal gameplay, the server always sends a smaller value, so this CMSG packet is never actually sent. This suggests it is part of a legacy or rarely used anti-AFK or state-check system.

## Construction

*   **Builder Function:** `CMSG::BuildPingResponse`
*   **Trigger:** Called by `SMSG_Handler_PingResponse_Trigger`
*   **Pathway:** Direct Queue (`MsgConn::QueuePacket`)

## Packet Structure

The builder function sends a null payload.