# Alacrity Buff Field Hypothesis

This note records the first-pass working hypothesis for locating Alacrity-related packet fields.

## Priority Packets

### 1. `SMSG_AGENT_ATTRIBUTE_UPDATE (0x0028)`

Current status:

- highest-priority candidate for buff application/removal
- already labeled in the codebase as:
  - agent stats
  - buffs/debuffs

Why it matters:

- likely to contain direct attribute or effect deltas tied to boon state
- best first target for Alacrity on/off comparison

### 2. `SMSG_AGENT_STATE_BULK (0x0016)`

Current status:

- second-priority candidate
- likely a bulk or batched state/effect container

Why it matters:

- may accompany or summarize effect changes
- useful for time-aligning with `0x0028`

### 3. `SMSG_AGENT_UPDATE_BATCH (0x0001)` fast path

Current assessment:

- documented as a high-frequency fast path
- may bypass the normal logger output depending on hook position

Workload assessment for dedicated hook:

- medium
- not a trivial parser-only change
- likely requires either:
  - a deeper receive hook closer to dispatch
  - or a dedicated fast-path tap before packet handling branches away from the normal log path

Conclusion:

- `0x0001` is a credible place where buff transitions may exist
- but `0x0028` and `0x0016` should be exhausted first because they are cheaper to instrument

## Initial Alacrity Field Hypothesis Table

This table is intentionally conservative and should be updated only after live comparison of:

- no-Alacrity baseline
- Alacrity applied
- Alacrity removed

| Field | Current Hypothesis |
| --- | --- |
| packet opcode | `0x0028` (highest-priority candidate, low confidence) |
| buff_id offset | unknown |
| buff_id type | likely `u32` or compact integer field |
| apply/remove discriminator | unknown |
| agent_id offset | `u16 @ 0x00` is current first candidate, low confidence |
| confidence | low |

## Recommended Capture Procedure

1. Stand out of combat and capture a baseline with no Alacrity.
2. Gain Alacrity from a teammate or a self-Alacrity skill.
3. Record the first `0x0028` and `0x0016` packets that appear when Alacrity is gained.
4. Wait for Alacrity to expire.
5. Record the matching `0x0028` and `0x0016` packets when it disappears.
6. Diff the packet field rows and isolate offsets that only move during the Alacrity on/off transition.

## Current Best Guess

At this stage:

- `0x0028` is the strongest working hypothesis for a direct Alacrity-related field change
- `0x0016` is the best secondary correlation packet
- `0x0001` remains a fast-path risk that may need a dedicated hook later if the other two do not explain the buff transition
