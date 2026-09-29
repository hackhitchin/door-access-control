# Door Controller System Invariants

Status: Draft v1  
Project: Hitchin Hackspace door access system

This document defines properties that must remain true regardless of the
sequence of valid inputs, timing events, mode changes, retries, resets, or
manual operation.

These invariants are intended to become permanent requirements and direct
targets for automated tests.

The transition specification defines what the controller does in each state.
This document defines what the controller must never violate.

---

# 1. Output safety invariants

## INV-001 — Lock and unlock outputs are mutually exclusive

The controller must never command both lock and unlock simultaneously.

Formally:

    !(LOCK_COMMAND && UNLOCK_COMMAND)

With the current state model:

    LOCKING    -> LOCK only
    UNLOCKING  -> UNLOCK only
    all others -> neither

This should preferably be made impossible by the software representation, for
example with a single `LockCommand` enum rather than two independent booleans.

---

## INV-002 — Lock output is only active in `LOCKING`

T1 may only be asserted while the FSM is in `LOCKING`.

Formally:

    lock_output_active <=> state == LOCKING

No stable state, Disabled state, or Error state may leave T1 asserted.

---

## INV-003 — Unlock output is only active in `UNLOCKING`

T2 may only be asserted while the FSM is in `UNLOCKING`.

Formally:

    unlock_output_active <=> state == UNLOCKING

No stable state, Disabled state, or Error state may leave T2 asserted.

---

## INV-004 — No motor command in `DISABLED`

While operating mode is Disabled, the controller must not intentionally issue
either electronic lock or unlock commands.

Formally:

    mode == Disabled -> LockCommand::None

This applies to RFID requests and, under the current first-pass policy, to the
internal exit button as well.

The exit-button behaviour in Disabled mode remains a TODO for final review.

---

## INV-005 — No motor command in `ERROR`

While the controller is in `ERROR`, neither T1 nor T2 may remain asserted.

Formally:

    state == ERROR -> LockCommand::None

Fault handling must not blindly operate the lock unless a future explicitly
specified recovery mechanism is added.

---

# 2. Door-position safety invariants

## INV-006 — Never intentionally begin locking while the door is known open

The controller must not enter `LOCKING` while `door_closed == false`.

Formally:

    transition_to(LOCKING) -> door_closed == true

If the door opens while already locking, the lock command must be aborted
immediately and the controller must leave `LOCKING`.

---

## INV-007 — Door opening aborts active motor commands

If `DOOR_OPENED` occurs while in either transient state:

    LOCKING
    UNLOCKING

the active T1/T2 command must be released and the controller must transition to:

    UNLOCKED_OPEN

This prevents continued motor actuation while the door is physically open.

---

## INV-008 — `LOCKED_OPEN` is not a valid normal state

The FSM must never treat:

    door_closed == false
    bolt_locked == true

as a normal stable state.

This combination must be treated as a sensor/physical fault.

Reason:

The bolt-state sensor is only considered meaningful when the door is closed and
aligned with the frame. A bolt indication while the door is open is therefore
ambiguous and must not be used to infer a trustworthy `LOCKED_OPEN` condition.

---

## INV-009 — No automatic recovery actuation from an ambiguous open-door fault

The controller must not automatically issue an unlock command solely because:

    door is open
    bolt sensor reports locked
    an open-door timeout expires

The historical automatic-unlock recovery paths are deliberately excluded from
the design because the physical condition cannot be identified reliably enough.

---

# 3. Release-request invariants

## INV-010 — Valid release request takes priority over an in-flight lock operation

In Standard mode, either of the following valid requests:

    RFID_RELEASE_REQUEST
    EXIT_BUTTON_REQUEST

must guarantee that the controller releases the door after any already-issued
lock command reaches a known conclusion.

If received while in `LOCKING`, the request is queued. The controller keeps the
current lock attempt active until `BOLT_LOCKED` or `LOCK_TIMEOUT`, then enters
`UNLOCKING` instead of accepting the locked state or starting another lock
retry. A request received in `LOCK_RETRY_WAIT` is similarly queued and prevents
the next lock retry.

This avoids depending on whether the Utopic/HAI aborts or completes a previously
issued one-shot lock command when T1 is released. Open Night selection uses the
same pending-release behaviour during a lock operation.

---

## INV-011 — RFID input active at boot is not a new access event

An RFID release input already active during startup must not generate an
`RFID_RELEASE_REQUEST`.

The input remains disarmed until it has first been observed inactive.

Only a subsequent inactive-to-active transition may generate a valid RFID
release event.

This prevents the tail end of an access-controller pulse from causing a new
unlock after a Nano restart.

---

## INV-012 — Exit button held at boot remains a valid request

Unlike the RFID signal, an internal exit button already active during startup
must be honoured when electronic operation is enabled.

This distinction is intentional and must remain explicit in the implementation
and tests.

---

## INV-013 — A new release request while already unlocked restarts the Standard auto-lock timer

In Standard mode, while in `UNLOCKED_CLOSED`:

    RFID_RELEASE_REQUEST
    EXIT_BUTTON_REQUEST

must restart the full `auto_lock_time`.

A new legitimate release request must not inherit a nearly-expired auto-lock
timer.

---

# 4. Mode invariants

## INV-014 — Exactly one logical operating mode exists at a time

The controller must expose exactly one logical operating mode:

    Disabled
    Standard
    OpenNight
    Invalid

The physical key-switch input combination must be decoded before being passed
to the FSM.

The FSM must not operate on two independent mode booleans.

---

## INV-015 — Invalid key-switch combination is never silently interpreted as a valid mode

Any electrical input combination not explicitly assigned to:

    Disabled
    Standard
    OpenNight

must decode as:

    Invalid

It must not silently fall back to Standard or Open Night.

---

## INV-016 — Selecting Open Night while locked causes release

If:

    state == LOCKED_CLOSED
    mode changes to OpenNight

the controller must transition toward:

    UNLOCKING

Open Night selection itself is therefore an intentional release request.

---

## INV-017 — Open Night to Standard gets a fresh Standard auto-lock interval

If the controller is already unlocked and operating mode changes:

    OpenNight -> Standard

a fresh Standard `auto_lock_time` must begin.

The controller must not lock immediately solely because of the mode change.

---

# 5. Retry and timeout invariants

## INV-018 — Every lock/unlock attempt is time bounded

An individual lock or unlock attempt may not hold its command indefinitely.

Maximum single-attempt duration:

    LOCK_TIME_MS
    UNLOCK_TIME_MS

A timeout must either:

- begin the defined retry sequence; or
- enter `ERROR` when retry allowance is exhausted.

---

## INV-019 — Retry count means retries after the initial attempt

Retry counters must not include the initial attempt.

For locking, `MAX_LOCK_RETRIES = 5`, so the maximum sequence is:

    initial attempt
    retry 1
    retry 2
    retry 3
    retry 4
    retry 5
    ERROR

For unlocking, `MAX_UNLOCK_RETRIES = 2`, so the maximum sequence is:

    initial attempt
    retry 1
    retry 2
    ERROR

This semantic must be consistent in implementation, diagnostics, and tests.

---

## INV-020 — Retry limit is finite

The controller must never retry lock or unlock indefinitely.

After the configured maximum number of retries, failure must lead to `ERROR`.

---

## INV-021 — Successful actuation clears the corresponding retry counter

Successful transition to:

    LOCKED_CLOSED

must clear the lock retry counter.

Successful transition from `UNLOCKING` after `BOLT_UNLOCKED` must clear the
unlock retry counter.

A later operation therefore starts with zero retries consumed.

---

## INV-022 — Retry commands are separated by explicit output-off wait states

After a failed attempt the controller must enter:

    LOCK_RETRY_WAIT

or:

    UNLOCK_RETRY_WAIT

for the configured delay for that attempt. Lock retry waits use the defined
1/10/100/1000/10000 s back-off; unlock retry waits use `RETRY_DELAY_MS` (1 s).

During either retry-wait state:

    LockCommand == None

The controller must not implement retries as one uninterrupted assertion of the
same command.

---

# 6. Startup and reset invariants

## INV-023 — Startup begins with no lock command active

Before the controller has reconstructed the physical state:

    LockCommand == None

No lock or unlock output may be asserted merely because the microcontroller has
booted.

---

## INV-024 — Startup does not restore transient FSM states

A reset must not attempt to restore:

    LOCKING
    UNLOCKING

from historical controller state.

Startup reconstructs behaviour from current physical inputs and current
operating mode.

---

## INV-025 — Historical faults are not persisted by the FSM

A controller reset discards historical transient fault state.

After reboot:

- current inputs are re-evaluated;
- a currently-existing fault is raised again;
- a fault that no longer exists is not recreated merely because it happened
  before reset.

Persistent event/fault logging, if desired, belongs to the external watchdog or
monitoring system.

---

## INV-026 — Standard startup with closed/unlocked door locks immediately

If startup finds:

    mode == Standard
    door_closed == true
    bolt_locked == false
    exit button not requesting release

the controller must begin `LOCKING` immediately.

It must not wait for the normal Standard `auto_lock_time`.

An RFID input already active at boot remains subject to INV-011 and does not
prevent this.

---

## INV-027 — Startup with an open door does not command locking

If startup finds:

    door_closed == false

the controller must not command `LOCKING`.

If the bolt sensor also reports locked, the condition is handled as a fault
rather than as a reason to operate the motor.

---

# 7. State-consistency invariants

## INV-028 — `LOCKED_CLOSED` requires closed door and locked indication

Whenever the controller is in `LOCKED_CLOSED`:

    door_closed == true
    bolt_locked == true

If either condition ceases to be true, the controller must process the
corresponding event and leave that state.

---

## INV-029 — `UNLOCKED_CLOSED` requires closed door and no locked indication

Whenever the controller is in `UNLOCKED_CLOSED`:

    door_closed == true
    bolt_locked == false

If the door opens, transition to `UNLOCKED_OPEN`.

If the bolt becomes locked, treat it as manual locking and transition to
`LOCKED_CLOSED`.

---

## INV-030 — `UNLOCKED_OPEN` requires the door to be open

Whenever the controller is in `UNLOCKED_OPEN`:

    door_closed == false

When the door closes, the controller must leave `UNLOCKED_OPEN` and apply the
policy appropriate to the current operating mode.

---

# 8. Fault invariants

## INV-031 — Faults auto-clear only when the underlying condition is gone

The default fault policy is auto-clear.

A fault may only clear when:

- the condition that caused it is no longer present; and
- the controller can reconstruct a valid stable state.

Auto-clear must not merely occur because a timeout elapsed or another unrelated
event occurred.

---

## INV-032 — Fault cause remains distinguishable while in `ERROR`

While the controller is in `ERROR`, the cause of the current fault must remain
available to diagnostics as a distinct fault code or equivalent status value.

Different causes must not be collapsed into an indistinguishable generic error
inside the controller if doing so would prevent useful diagnosis.

The final fault catalogue is defined separately.

---

# 9. Timing invariants

## INV-033 — Time comparisons must remain correct across `millis()` wraparound

All timer-expiry calculations must use unsigned wraparound-safe arithmetic.

For example:

```cpp
if ((uint32_t)(now - started_at) >= timeout) {
    ...
}
```

The implementation must not rely on:

```cpp
now >= started_at + timeout
```

where wraparound could break the comparison.

This invariant must have an explicit automated test.

---

## INV-034 — One timer expiry may only affect the operation it belongs to

Stale timer events from a previous state must not cause transitions in a new
state.

For example:

- an old `LOCK_TIMEOUT` must not affect `UNLOCKED_CLOSED`;
- an old `AUTO_LOCK_TIMEOUT` must not unexpectedly trigger after entering Open
  Night;
- an old `OPEN_TIMEOUT` must not affect a newly closed door.

Timers must therefore be cancelled, invalidated, or associated with the state /
operation instance that created them.

---

# 10. Implementation/test requirements derived from the invariants

The automated test suite should include:

- an assertion of INV-001 after every tested event;
- exhaustive state/event tests for all legal FSM states;
- explicit checks that stable-state sensor requirements remain true;
- tests of every mode transition;
- tests for RFID-at-boot and exit-button-at-boot;
- tests for door opening during both `LOCKING` and `UNLOCKING`;
- tests that retries stop at the configured maximum;
- tests that retry counters have the agreed semantics;
- tests that retry output is deasserted during `RETRY_DELAY_MS`;
- tests that stale timer events cannot affect later states;
- tests across `millis()` wraparound;
- startup tests for every combination of:
  - operating mode;
  - door sensor;
  - bolt sensor;
  - exit-button state;
  - RFID input initial state.

Where practical, invariants should also be expressed as runtime assertions in
the native test build so that long event-sequence and fuzz/property tests fail
immediately when an invariant is violated.

---

# 11. Deliberately not defined here

The following belong in other documents:

- exact transition-by-transition behaviour: `transitions.md`;
- timing/retry values: `parameters.md`;
- exact fault catalogue: `faults.md`;
- hardware pin allocation: `pin_defs.h`;
- electrical polarity and input conditioning;
- backup maglock policy;
- watchdog/email monitoring architecture;
- final decision on exit-button behaviour while Disabled.
