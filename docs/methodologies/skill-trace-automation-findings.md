# Skill Trace Automation Findings

This document records the first practical findings from the automated skill trace pipeline introduced after the initial [Skill Execution Automation Plan](./skill-execution-automation-plan.md).

The goal of this phase was to validate the closed loop:

`hotkey -> fresh 0x0017 packet -> baseline capture -> cooldown sample -> readiness polling -> archival`

The logs referenced here come from the automatically generated `x64/Release/skill-trace` directory.

## What Is Working

The automation framework itself is functioning.

Confirmed behaviors:

- A trace task is created when a monitored hotkey is pressed.
- A task is promoted only after a fresh `CMSG_USE_SKILL (0x0017)` packet is observed.
- The system successfully writes structured trace folders grouped by:
  - build profile
  - hotkey name
  - tail-derived archive key
  - timestamp
- The trace task can complete end-to-end and produce:
  - `baseline_pre.txt`
  - `baseline_post.txt`
  - `cooldown_stable.txt`
  - `ready_returned.txt`
  - `diff_pre_vs_cooldown.txt`
  - `diff_cooldown_vs_ready.txt`

This confirms that the automation plan is operational as a data collection system.

## New Findings

### 1. `Completed` Did Not Mean "Truly Ready"

The current `Completed` state means:

- the task observed a candidate readiness transition
- the readiness polling rule fired
- a final dump was captured

It does **not** yet reliably mean:

- the skill returned to the exact pre-cast ready state

In multiple traces, the final `ready_returned` sample shows a meaningful change from the cooldown sample, but still does not fully match `baseline_pre`.

Practical interpretation:

- current `Completed` should be read as:
  - "candidate ready moment detected"
- not:
  - "verified ready state restored"

### 2. Address Reversion Alone Is Too Weak for Ready Detection

The current polling logic is too permissive because it can treat pointer reversion or partial field reversion as enough evidence of readiness.

Observed pattern:

- `ptr48` or `ptr60` may return to a familiar address
- but key fields inside the object still differ from baseline

This means the current readiness detector is biased toward:

- object topology changes

instead of:

- confirmed state equivalence

### 3. `cooldown_stable` Is Sometimes Captured Too Early

The current trace pipeline captures `cooldown_stable` at a fixed `+500ms` after packet confirmation.

New logs show this is too early for some skills.

Example pattern:

- `baseline_pre` and `cooldown_stable` are almost identical
- the only difference may be a single small state value
- the later `ready_returned` sample then looks more like a second transition than a real return from cooldown

This is especially clear in recent `Healing(6)` traces where:

- `diff_pre_vs_cooldown` is extremely small
- `diff_cooldown_vs_ready` contains the larger structural change

That strongly suggests the `+500ms` sample sometimes lands before the trace has entered a stable cooldown state.

### 4. Tail Parsing Is Still Inconsistent Across Packet Variants

The automation layer currently records archive keys as `tail_tag` and `tail_val`, but recent traces show that this logic is still inconsistent, especially on secondary packet variants.

Observed examples:

- `Healing(6)` often lands in `tag_00_val_00`
- `Elite(Q)` often lands in `tag_04_val_00`
- `Utility2(X)` and `Utility3(C)` can still land in values such as:
  - `tag_A1_val_88`

This does not match the earlier manual packet investigation, where:

- primary branch samples were associated with `0x29 XX`
- secondary branch samples were associated with `0x25 XX`

Current conclusion:

- the automated trace layer is not yet extracting the tail fields using the same interpretation as the manual `UseSkill Breakdown`
- archive keys derived from the current tail parser should therefore be treated as provisional

## Concrete Trace Examples

### Healing(6)

Recent `Healing(6)` traces show a useful failure mode:

- `baseline_pre` and `cooldown_stable` are nearly identical
- only a very small field may change during `pre -> cooldown`
- `ready_returned` then introduces a much larger object/state transition

Interpretation:

- the task probably did not lock onto a real cooldown-stable window
- the final state is not sufficient evidence of a full return to baseline

### Utility2(X)

Recent `Utility2(X)` traces still produce archive keys that do not align with the manual packet findings.

Interpretation:

- the automation system is still parsing the packet tail incorrectly for at least some secondary-branch samples
- any downstream correlation keyed off those values remains unreliable until the packet breakdown logic is unified

## Updated Interpretation of the Automation Pipeline

At this stage the system should be understood as:

- a working automated skill-event trace collector

not yet as:

- a reliable automatic readiness detector
- a reliable tail-based slot identity recorder

This is still valuable because it means the project now has:

- deterministic, reproducible event windows
- consistent on-disk traces
- enough structure to iterate on the readiness model and packet-field model quickly

## Immediate Priorities

### Priority 1: Fix Tail Field Extraction

The automatic trace code should use the same packet-tail interpretation as the existing manual packet analysis UI.

Expected direction:

- preserve the full trailing bytes
- separate:
  - tail prefix
  - tail tag
  - tail value
  - optional extra byte if present
- avoid collapsing the trailing bytes into an incorrect `tag/value` pair

### Priority 2: Replace Fixed `+500ms` Cooldown Locking

The system should stop assuming that `+500ms` is the stable cooldown window.

Better approach:

- perform short staged checks after packet confirmation, for example:
  - `+100ms`
  - `+250ms`
  - `+500ms`
  - `+1000ms`
- only commit `cooldown_stable` after a real post-cast structural/state change is observed

### Priority 3: Tighten Ready Detection

Ready detection should require more than address reversion.

Suggested direction:

- require pointer return plus field return
- or require multiple field candidates to move back toward `baseline_pre`
- use at least two consecutive polls that satisfy the stricter rule

## Current Best Summary

The first automation rollout is successful as a trace collection framework.

It already proves that:

- hotkey-triggered automatic tracing is feasible
- fresh packet confirmation can be used to anchor memory capture
- automated archival produces useful multi-stage datasets

It does **not** yet prove that:

- the current `Completed` state equals true skill readiness
- the current tail archive keys are canonical

Those two limitations are now the main blockers to turning the system into a reliable "skill ready / not ready" engine.

## Naming Correction

Later trace review showed that the old `fully recharged` label was too optimistic. The evidence only supported detection of a transition into the complete cooldown path, not a verified return to a fully ready state.

The state should therefore be interpreted as:

- `castable`
  - confirmed: the skill can still be cast
- `cooldown_path_detected`
  - transition-only: the skill has entered a complete cooldown-related state path
- `cooldown_active`
  - inferred: the skill is in an intermediate cooldown state
- `ready_confirmed`
  - reserved placeholder for future work once a verified "ready again" signal is found
