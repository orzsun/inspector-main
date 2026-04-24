# Skill Cooldown Investigation Notes

This document records the current state of the skill cooldown and skill identity investigation in `kx-packet-inspector`.

It is intentionally written as a working note rather than a finalized reference. The goal is to preserve what has already been confirmed, what turned out to be misleading, and which directions currently look most promising.

## Goal

The original goal was to answer two related questions:

1. Can we identify the memory fields that represent a skill's cooldown value or cooldown-end state?
2. Can we reliably identify which in-game skill or skill slot the observed memory changes belong to?

## Current Instrumentation

The project was extended with several temporary diagnostics to support this investigation:

- `SkillMemoryReader` now resolves `ContextCollection` indirectly through cross-thread TLS scanning instead of calling the located function directly on the current thread.
- `SkillMemoryReader::read()` currently resolves and snapshots:
  - `CtxColl`
  - `ChCliCtx`
  - `LocalChar`
  - player `Position`
  - `ChCliSkillbar`
  - `AsContext`
  - `CharSkillbar`
- The UI exposes:
  - re-init and probe diagnostics
  - top-level `CharSkillbar` candidate fields
  - pointer-like child objects from `CharSkillbar + 0x40`, `+0x48`, and `+0x60`
  - per-slot candidate rows based on the current guessed slot stride
  - correlation state for the last pressed hotkey and the most recent outgoing send metadata
- A multi-sample capture mode writes logs into `x64/Release/skill-log`.

## Confirmed Findings

### 1. The context lookup path is now working

The earlier failures were not caused by a bad idea about the object chain, but by implementation mistakes in the TLS scan path. After fixing the `THREAD_BASIC_INFORMATION` handling and scanning thread TLS instead of calling `GetContextCollection` directly on the wrong thread, the following chain became stable:

`ContextCollection -> ChCliCtx -> LocalChar -> ChCliSkillbar -> CharSkillbar`

The tool can now also resolve valid player position data from the same chain, which strongly suggests the base object path is correct.

### 2. The top-level `CharSkillbar` fields do change during skill activity

Repeated captures showed consistent changes in these areas:

- `u32 @ 0x10`
- `u32 @ 0x18`
- `u32 @ 0x24`
- `u64 @ 0x08`
- `u64 @ 0x40`
- `u64 @ 0x48`
- `u64 @ 0x60`

This confirms that `CharSkillbar` is relevant to skill execution state. The question is no longer whether this object matters, but what layer of state it actually represents.

### 3. The top-level fields look more like global skill system state than per-skill cooldown storage

A critical observation from later captures was that unrelated skills still caused the same candidate fields to change.

That means these fields are probably not "the cooldown value for one specific skill". They are more likely to represent:

- shared skillbar state
- shared action/combat state
- pointers to currently active skill-related sub-objects
- timing or phase values used by a broader skill system context

### 4. `u32 @ 0x10` is the best state-code candidate

Across multiple logs, `u32 @ 0x10` changed in short discrete jumps that looked much more like phase changes than like a timer.

Current interpretation:

- `u32 @ 0x10` is likely a state or phase code
- it is unlikely to be a direct skill ID
- it is unlikely to be a direct "remaining cooldown" counter

### 5. `u32 @ 0x18` and `u32 @ 0x24` are the best time-related candidates

These two fields changed in ways that looked time-related, but not like a directly readable countdown.

Current interpretation:

- `u32 @ 0x24` is the strongest candidate for a time target, end tick, or phase boundary
- `u32 @ 0x18` is likely a second time-related or synchronization field
- neither field currently looks like a simple "seconds remaining" value

### 6. `0x40`, `0x48`, and `0x60` behave like child-object pointers

The values at these offsets look like heap pointers and often switch when skill state changes.

This is one of the most important findings so far, because it suggests that the real per-skill identity or per-skill state may live in these child objects rather than in the top-level `CharSkillbar` header.

### 7. `CMSG_USE_SKILL (0x0017)` now provides a reliable action-slot signal

Later instrumentation moved the investigation forward significantly by capturing and classifying the raw `CMSG_USE_SKILL` payload instead of only recording the most recent outgoing packet header.

The current packet breakdown is:

- bytes `0x00..0x01`: opcode `0x0017`
- bytes `0x02..0x03`: fixed field `0x000B`
- bytes `0x04..0x07`: tick/sequence-like value
- bytes `0x08..0x0C`: 5-byte varint candidate, likely agent/entity-related
- byte `0x0D`: tail prefix, currently observed as `0x02`
- byte `0x0E`: tail tag
- byte `0x0F`: tail value

Two tail-tag classes have now been observed repeatedly:

- `0x29`: primary mapping class
- `0x25`: secondary mapping class

Other tags can appear transiently, such as `0x2A`, but these are currently treated as non-primary auxiliary events rather than the core action mapping signal.

### 8. Fresh-packet filtering fixed the stale-packet contamination problem

Earlier packet-to-hotkey mapping attempts were polluted by a simple but important issue: the most recently captured `UseSkillPacket` could persist into the next sampling run.

The current build now clears the cached packet when sampling starts and marks a packet as fresh only if:

`LastUseSkillTickMs >= LastSkillHotkeyTickMs`

This change is critical. It means the action mapping conclusions below are based only on fresh samples and should be treated as the current ground truth.

### 9. Current action-slot mapping from fresh `0x0017` packets

Using only fresh samples, the following mappings are now the best confirmed action signals:

Primary class (`tail_tag == 0x29`):

- `0x29 00` -> `Healing(6)`
- `0x29 01` -> `Utility1(Z)`
- `0x29 04` -> `Elite(Q)`
- `0x29 0C` -> `Profession1(F1)`

Secondary class (`tail_tag == 0x25`):

- `0x25 83` -> `Utility2(X)`

Older observations such as `0x25 EF` should now be considered stale or contaminated and should not be used as mapping evidence.

This mapping does **not** yet prove that `tail_val` is a universal skill ID. It is better interpreted as a stable action-slot or action-branch discriminator within the `UseSkill` packet family.

## Findings That Did Not Hold Up

### 1. The current slot parsing is not trustworthy

The tool currently guesses skill slots using:

`slotAddr = charSkillbar + i * 0x28`

This was only a heuristic. In practice, the resulting `SlotSummary` rows barely changed, even when different skills were used.

Current conclusion:

- the current slot stride is probably wrong
- or the slot array does not begin where we assumed
- or the sampled per-slot structure is not where skill identity is stored

This means the current `Skill ID Candidates` table should be treated as exploratory only, not as a reliable skill ID view.

### 2. Packet metadata alone has not identified the skill

The tool now records:

- last hotkey pressed
- last outgoing packet header
- last outgoing packet size
- last outgoing buffer state

This helped with correlation, but it did not solve skill identity by itself.

In particular, the most recent outgoing packet often remained a small generic-looking event:

- header often appeared as `0x11`
- size was small
- the pattern repeated across different hotkeys

Current conclusion:

- last hotkey is useful context
- recent outgoing packet metadata is not yet specific enough to stand in for skill ID

### 3. `0x40` is not the best primary per-skill target

An earlier working idea was that `CharSkillbar + 0x40` pointed directly to the most useful per-skill state object. Later captures no longer supported that interpretation.

Current conclusion:

- `0x40` often behaves like a shared owner or manager pointer
- `0x48` and `0x60` are more informative as active state targets
- `0x40` should remain visible as a reference, but not as the main reverse-engineering target

## Most Likely Current Interpretation

The best working model right now is:

- `CharSkillbar` contains global or semi-global skill execution state
- top-level `u32 @ 0x10` is a phase/state code
- top-level `u32 @ 0x18` and `u32 @ 0x24` are timing-related values
- top-level `u64 @ 0x40`, `0x48`, and `0x60` point to more specific state objects
- the current slot-table hypothesis is weak
- the actual skill identity is more likely to be stored in one of the child objects than in the top-level header
- the packet layer is now strong enough to identify action slots even when the true in-memory skill ID is still unknown

## Why "Cooldown Finished" Is Easier Than "Exact Cooldown Value"

During testing it became clear that finding a boolean-like state transition is much easier than deriving a clean cooldown timer.

Reasons:

- a direct countdown field should decrease in a simple way, but none of the current candidates behave that cleanly
- a state transition can be recognized from a stable before/after pattern, even if the internal timing representation is complex
- the top-level fields already look phase-oriented, which makes them good candidates for "cooldown active vs cooldown finished" analysis

In other words, the investigation is currently closer to answering:

- "Is this skill still on cooldown?"

than:

- "How many milliseconds remain?"

This is now more than a general intuition. With the packet mappings above, the tool can already tell which action slot triggered. That makes a slot-level readiness model far more realistic than a universal exact cooldown timer model.

## Practical Problems Encountered

### 1. Wrong-thread assumptions caused early false negatives

Calling or reasoning about `GetContextCollection` without respecting the game's thread/TLS model led to zero values and misleading early results.

### 2. Fixed-delay sampling was too narrow

Early two-point captures with a single fixed delay produced noisy interpretations. They were useful for proving that fields changed, but not for distinguishing:

- one-time transitions
- continuously changing values
- delayed phase updates

### 3. Different skills were mixed into the same analysis bucket

Some logs initially compared captures from different skills without a strong enough identity signal. That made it easy to mistake general skill-system transitions for one skill's cooldown state.

### 4. Global state changes can be mistaken for skill-specific state

This became the main analytical trap. A field changing after a skill press does not automatically mean it belongs to that specific skill's cooldown data.

### 5. Stale packets can masquerade as valid action mappings

Before fresh-packet filtering was added, old `UseSkill` packets could be incorrectly associated with new hotkey presses. This created false mappings, especially when different skills were tested back-to-back.

This is now mitigated, but any historical logs created before the fresh filter should be treated cautiously.

## What the Tool Can Reliably Do Right Now

The current build can reliably:

- locate and display `ContextCollection`, `ChCliCtx`, `LocalChar`, `CharSkillbar`, and position
- capture repeated timed snapshots of `CharSkillbar`
- capture the first `0x80` bytes of the pointer targets at `0x40`, `0x48`, and `0x60`
- correlate snapshots with the last pressed hotkey across `1-6`, `Z`, `X`, `C`, `Q`, `E`, and `F1-F7`
- classify fresh `CMSG_USE_SKILL` packets into primary and secondary mapping classes
- identify several action slots from packet tail values
- persist experiments to `skill-log` for later comparison

The current build cannot yet reliably:

- identify the exact skill ID from the current slot-table view
- prove that any single field is the direct remaining cooldown value
- map the current slot candidates to the real in-game skill bar layout
- derive a universal action mapping from `tail_val` alone without considering the tail-tag class

## Recommended Next Steps

### 1. Prioritize child-object comparison over top-level slot guesses

The next promising layer is the memory behind:

- `CharSkillbar + 0x40`
- `CharSkillbar + 0x48`
- `CharSkillbar + 0x60`

These objects should be compared across:

- different hotkeys
- the same hotkey across repeated casts
- different cooldown phases

However, this recommendation should now be interpreted with updated priority:

- `0x48` and `0x60` are the primary child-object targets
- `0x40` is secondary and mainly useful as reference context

### 2. Look for stable identity fields inside child objects

The strongest candidates for future field tables are fixed offsets inside the child objects, especially small `u32` values that remain stable per skill but differ across skills.

### 3. Separate "skill identity" from "cooldown state"

The investigation should treat these as separate problems:

- first identify which object belongs to the triggered skill
- then identify which field inside that object or its owner graph reflects cooldown state

The packet mappings are now good enough to support this separation directly:

- packet tail values identify the action slot class
- `ptr48` / `ptr60` can then be studied only within fresh samples for that action slot

### 4. Use repeated same-skill captures before comparing different skills

The best validation pattern is:

1. pick one hotkey
2. cast the same skill multiple times
3. compare the same object layout across those repeated casts
4. only then compare that pattern against another skill

This reduces the chance of confusing generic combat state with skill-specific state.

## Current Working Hypotheses

These are the hypotheses that best match the evidence so far:

1. `u32 @ 0x10` is a phase/state field, not a skill ID.
2. `u32 @ 0x24` is more likely than `u32 @ 0x18` to represent a cooldown-related target time.
3. The true skill identity is probably not in the current guessed slot table.
4. One of the child objects behind `0x40`, `0x48`, or `0x60` is a better entry point for real skill identity.
5. Detecting "cooldown ended" is more achievable in the short term than deriving an exact countdown value.
6. `tail_tag == 0x29` is the main action-slot mapping class for several common skills.
7. `tail_tag == 0x25` is a second mapping class and must not be mixed with `0x29`.
8. A slot-level readiness model is now a more realistic near-term feature than exact cooldown extraction.

## Current Action Mapping Table

The following table reflects only fresh samples and should be preferred over older notes:

| Packet Class | Tail Value | Action Slot |
| :-- | :-- | :-- |
| `0x29` | `0x00` | `Healing(6)` |
| `0x29` | `0x01` | `Utility1(Z)` |
| `0x29` | `0x04` | `Elite(Q)` |
| `0x29` | `0x0C` | `Profession1(F1)` |
| `0x25` | `0x83` | `Utility2(X)` |

This table should be treated as a build-local action map, not yet as a universal skill-ID table.

## API Cross-Reference Notes

The Guild Wars 2 API can help with build-level interpretation even though it does not currently replace the packet mapping work.

- `/v2/characters/:id/skills` can provide the equipped healing, utility, and elite skills for a specific character.
- `/v2/skills` can resolve those skill IDs into names, descriptions, and metadata.
- Profession-mechanic slots such as `F1` are not directly solved by the current packet notes alone and may require profession-specific handling or additional API/build context.

Relevant references:

- [API:2/characters/:id/skills](https://wiki.guildwars2.com/wiki/API%3A2/characters/%3Aid/skills)
- [API:2/skills](https://wiki.guildwars2.com/wiki/API%3A2/skills)

## Status Summary

The investigation has successfully moved from "cannot read the right object at all" to "can repeatedly read the correct skill-system object and correlate it with input". That is meaningful progress.

The project is now past the stage of "blindly guessing whether a change belongs to a skill at all". The packet layer can already identify several action slots reliably, and the remaining work is mostly about state semantics:

- which child-object fields mean "cooling down"
- which child-object fields mean "ready again"
- whether any field returns to `0` when a slot becomes available

At this stage, the top-level `CharSkillbar` view should be treated as shared control state, the packet layer should be treated as the authoritative action-slot trigger signal, and `ptr48` / `ptr60` should be treated as the main objects for readiness-state discovery.
