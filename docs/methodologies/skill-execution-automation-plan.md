# Skill Execution Automation Plan

This document describes a proposed end-to-end automation flow for skill execution analysis in `kx-packet-inspector`.

The goal is to connect four layers into one closed loop:

1. user input (`LastSkillHotkeyTickMs`)
2. outgoing `CMSG_USE_SKILL (0x0017)` packet confirmation
3. synchronized memory capture around `CharSkillbar`
4. automated readiness-state observation and archival

This is a design document only. It intentionally focuses on orchestration, timing, task lifecycle, and data association rather than implementation details.

## Objective

Design a system that can automatically:

- detect a relevant skill key press
- confirm the matching `UseSkill` packet
- capture a memory baseline at the correct moment
- perform staged follow-up captures
- poll for readiness-state reversion
- dump the final ready-state snapshot
- archive the full trace under a unique skill/action identity

## Known Inputs

The current project already provides these inputs:

- `LastSkillHotkeyTickMs`
- classified fresh `CMSG_USE_SKILL (0x0017)` packets
- fresh-packet rule:
  - `LastUseSkillTickMs >= LastSkillHotkeyTickMs`
- memory access to:
  - `CharSkillbar`
  - `CharSkillbar + 0x48`
  - `CharSkillbar + 0x60`

These are sufficient to build an event-driven automation layer.

## High-Level Model

The cleanest design is a stateful trace coordinator that owns short-lived skill-trace tasks.

Recommended state model:

`Idle -> ArmedByHotkey -> ConfirmedByPacket -> BaselineCaptured -> CoolingSampled -> PollingReady -> Completed`

Possible terminal states:

- `Completed`
- `TimedOut`
- `Cancelled`
- `RejectedAsStale`

Each skill activation attempt becomes one task instance.

## 1. The Trigger

### Trigger philosophy

The trigger should not fire on a key press alone.

A key press only means:

- the user attempted a skill

It does not guarantee:

- the game accepted the input
- the action belongs to the intended build slot
- the packet is new rather than residual state

Because of this, the trigger should be a two-stage confirmation:

1. `HotkeyEvent`
2. matching fresh `UseSkillPacketEvent`

### Recommended trigger sequence

1. When a skill hotkey is pressed, create a short-lived pending task.
2. Store:
   - `hotkey_name`
   - `hotkey_tick_ms`
3. Start a small confirmation window, for example `200-400ms`.
4. Wait for a fresh `CMSG_USE_SKILL (0x0017)` packet inside that window.
5. Only promote the pending task if:
   - the packet is fresh
   - the packet arrives after the hotkey
   - no newer hotkey has superseded the task

At the moment of promotion, the task becomes the authoritative trace task for that skill activation.

### Why this is the right trigger

This avoids:

- stale packet reuse
- false activations from isolated hotkeys
- mixed traces from overlapping skills

## 2. Baseline Capture Strategy

The system should preserve two different baseline concepts.

### `baseline_pre`

This is the most recent stable memory snapshot before the action is confirmed.

Purpose:

- represent the ready or idle state before skill execution
- provide a true comparison origin for "cooldown ended" analysis

### `baseline_post`

This is captured immediately after packet confirmation.

Purpose:

- capture the first accepted in-game state after the skill actually fires
- serve as the start of the active skill task timeline

### Recommendation

Store both baselines.

This is useful because some fields may change:

- after the hotkey
- before packet logging
- before the next scheduled stage sample

Having both improves later diff analysis.

## 3. Multi-Stage Timing

### Short answer

Do not use only a fixed two-point capture.

The most useful timing model is:

- immediate post-confirmation capture
- short delayed capture
- then polling until ready reversion

### Recommended schedule

`T0`

- capture `baseline_post` immediately after packet confirmation

`T1 = T0 + 0.5s`

- capture the first stable cooldown sample

`Polling phase`

- begin repeated low-cost readiness checks after `T1`

### Why `0.5s`

This delay is long enough for many skills to move from:

- instantaneous activation noise

to:

- a more stable cooldown-state object layout

It is also short enough that long-CD skills such as `Healing(6)` remain easy to track from the start of the cooldown.

### Handling long cooldowns

Long-CD skills should not be fully sampled at fixed intervals forever.

Instead:

- one stable early cooldown sample is captured at `+0.5s`
- later work switches to polling

This keeps the tracing lightweight.

## 4. Non-Blocking Scheduling

The trace system should never block:

- the hook thread
- the UI thread
- any game-facing hot path

### Recommended execution model

Use a dedicated background trace executor.

Good options:

- a single worker thread with a task queue
- a delayed-task scheduler plus background worker

Avoid using ad hoc sleeps on the hook path.

### Use of `std::async`

`std::async` is acceptable for isolated delayed work such as:

- `capture T1 after 0.5s`

But for the full lifecycle, a task queue is a better fit because it makes it easier to manage:

- cancellation
- timeouts
- trace ownership
- logging consistency

### Practical design

The hook layer emits events only.

The background executor performs:

- memory reads
- delayed captures
- readiness polling
- archival writes

## 5. Active Task Structure

Each accepted skill execution should create one `ActiveSkillTraceTask`.

Suggested fields:

- `task_id`
- `hotkey_name`
- `hotkey_tick_ms`
- `packet_tick_ms`
- `tail_tag`
- `tail_val`
- `packet_class`
- `state`
- `started_at`
- `baseline_pre`
- `baseline_post`
- `cooldown_sample`
- `ready_returned_sample`
- `poll_history`
- `ptr48_anchor`
- `ptr60_anchor`
- `ready_candidates`
- `timeout_at`

This structure should be owned by the trace coordinator and visible only through controlled transitions.

## 6. Ready-State Polling

### Core idea

The system should not continuously dump full `0x80` blocks during cooldown.

Instead, after `T1`, it should poll only:

- pointer identity
- a small set of candidate fields

### Candidate sources

The candidate fields should be derived automatically from differences between:

- `baseline_pre`
- `baseline_post`
- `cooldown_sample`

The most useful initial candidates are:

- `ptr48` address
- `ptr60` address
- small `u32` values under `0x10000`
- fields that changed from pre-state to cooldown-state

### Poll interval

Recommended default:

- every `250ms` or `500ms`

This is frequent enough to detect state return without producing unnecessary memory overhead.

## 7. Ready-State Decision Rule

The system should not trust a single field by default.

Instead, the ready condition should be based on reversion toward the original pre-state.

### Suggested rule

Define readiness as:

`Ready = (pointer reversion OR candidate-field reversion) AND stable confirmation`

Where:

- pointer reversion means `ptr48` or `ptr60` returns to a pre-state identity
- candidate-field reversion means a tracked field matches its `baseline_pre` value
- stable confirmation means the result is observed in at least 2 consecutive polls

### Why this is safer

This reduces false positives caused by:

- transitional object swaps
- temporary intermediate values
- animation-stage jitter

## 8. Final Dump Trigger

When the readiness rule passes:

1. immediately capture a full final snapshot
2. optionally capture one more confirmation snapshot after a short delay, such as `+200ms`
3. mark the task `Completed`
4. persist the trace bundle

### Recommended stored phases

- `baseline_pre`
- `baseline_post`
- `cooldown_stable`
- `ready_returned`
- optional `ready_confirmed`

This gives enough material for later structural diffing.

## 9. Timeout Strategy

Every active trace must have a timeout.

Suggested defaults:

- `90s` for likely healing traces
- `120s` generic default for utility, elite, and profession traces

If the timeout expires:

- mark the task `TimedOut`
- persist all data collected so far

Timed-out traces remain useful for later analysis.

## 10. File Association and Archival

Each trace should be archived using a unique action identity derived from packet and input metadata.

### Suggested identity key

`task_id = packet_tick_ms + hotkey_name + tail_tag + tail_val`

### Suggested folder layout

```text
skill-trace/
  <build-profile>/
    <hotkey-name>/
      tag_<tail-tag>_val_<tail-val>/
        <timestamp>/
```

Example:

```text
skill-trace/
  build_necro_reaper_a/
    Healing(6)/
      tag_29_val_00/
        20260416_005846_629/
```

### Suggested files per task

- `meta.json`
- `baseline_pre.txt`
- `baseline_post.txt`
- `cooldown_stable.txt`
- `ready_returned.txt`
- `ready_confirmed.txt` if used
- `diff_pre_vs_cooldown.txt`
- `diff_cooldown_vs_ready.txt`
- `candidates.txt`

### `meta.json` should include

- `task_id`
- `hotkey_name`
- `hotkey_tick_ms`
- `packet_tick_ms`
- `tail_tag`
- `tail_val`
- `packet_class`
- `fresh`
- `state`
- `timeout`

## 11. Build-Level Association

Over time, the project should accumulate a build-local mapping file that records which slots are currently known for the build under analysis.

Suggested file:

`build_profile.json`

Example contents:

- `Healing(6)` -> `tag 0x29 / val 0x00`
- `Utility1(Z)` -> `tag 0x29 / val 0x01`
- `Utility2(X)` -> `tag 0x25 / val 0x83`
- `Elite(Q)` -> `tag 0x29 / val 0x04`
- `Profession1(F1)` -> `tag 0x29 / val 0x0C`

This lets the automation layer:

- recognize expected slots immediately
- group traces under a known build context
- reuse known readiness candidates for the same slot

## 12. Recommended Rollout Order

The safest implementation order is:

1. task creation from hotkey plus fresh packet
2. immediate `baseline_post`
3. delayed `cooldown_sample` at `+0.5s`
4. low-frequency polling for ready reversion
5. final dump and archival
6. build profile reuse and candidate reuse

## 13. Why This Plan Fits the Current Reverse-Engineering State

This system is realistic because it does not require the project to already know:

- the exact in-memory skill ID
- the exact remaining cooldown value
- one perfect universal readiness flag

Instead, it uses the parts that are already strong:

- input timing
- fresh packet confirmation
- action-slot mapping
- stable access to `CharSkillbar`, `ptr48`, and `ptr60`

That makes it well suited for the current stage of the investigation.

## 14. Expected Outcome

If this design is followed, the tool should be able to automatically produce a structured trace for each skill activation:

- what key was pressed
- which fresh `UseSkill` packet confirmed it
- what the skill-state memory looked like before and after execution
- when the memory returned to a ready-like state
- how that trace should be classified for the current build

This creates a practical foundation for the next higher-level feature:

- per-slot readiness tracking

That goal is much more achievable in the short term than exact cooldown extraction, and this automation plan is the most direct path toward it.
