# Skill Cooldown Tracker

This document describes the event-driven cooldown tracker added on top of the stable `CMSG_USE_SKILL (0x0017)` archive-key pipeline.

## Goal

Provide a build-specific but type-driven cooldown model for Guardian/Firebrand skills without hardcoding one-off behavior for each skill.

## Three Skill CD Types

### `STANDARD`

Used for ordinary single-cooldown skills.

Examples:

- `Symbol of Vengeance`
- `Blazing Edge`
- `Cleansing Flame`

State model:

- use event starts one cooldown
- skill becomes ready again when that cooldown reaches zero

### `MULTI_CHARGE`

Used for ammo / mantra style skills where each use consumes one charge and each charge recharges independently.

Examples:

- `Mantra of Potency`
- `Mantra of Flame`
- `Mantra of Liberation`
- `Mantra of Truth`

State model:

- using the skill consumes one charge
- each consumed charge creates a recharge timer
- the skill is castable while `charges > 0`
- the skill is full when all charges are restored and no timers remain

### `CHARGE_PLUS_CD`

Used for skills that both store charges and impose a short post-cast lockout.

Example:

- `Zealot's Flame`

State model:

- using the skill consumes one charge
- a short cast cooldown is applied
- a longer accumulation timer restores the consumed charge
- the skill is castable only when:
  - `charges > 0`
  - and cast cooldown is over

## Event Log

Logs are written to:

`x64/Release/cd-log/YYYYMMDD.log`

Current event types:

- `[USED]`
- `[CD_START]`
- `[CHARGE-]`
- `[CHARGE+]`
- `[READY]`
- `[FULL]`

## Current Limitations

- The tracker currently uses a configuration table, not automatic skill discovery.
- Unknown archive keys are logged but not auto-persisted back into the config.
- CD durations are static config values; dynamic modifiers such as Alacrity are not yet applied.
- This tracker uses `CMSG` action identity plus timers, not verified server-side cooldown completion.

## Why This Matters

The tracker creates a clean separation between:

- action identity from packet events
- generalized cooldown behavior by skill type
- future dynamic modifiers such as:
  - Alacrity
  - hit-driven cooldown reduction
  - charge regeneration acceleration

That makes it a practical staging point for later Willbender / Firebrand specific logic.
