# Door Controller Fault Catalogue

Status: Draft v1  
Project: Hitchin Hackspace door access system

This document defines the fault conditions recognised by the door-controller
FSM, the conditions that raise them, their required controller response, and
their auto-clear conditions.

Faults are separate from FSM states.

The FSM has a general:

    ERROR

state, while the controller also records the reason for entering that state as
a distinct fault code.

The first implementation should preserve the current fault cause for
diagnostics while the fault is active.

---

# 1. Fault handling principles

Unless explicitly stated otherwise:

1. Entering `ERROR` immediately removes any active T1/T2 command.
2. The controller does not automatically operate the lock while in `ERROR`.
3. Faults auto-clear when the underlying fault condition is gone and the
   controller can reconstruct a valid stable state.
4. Fault history is not persisted across Nano reset.
5. Persistent historical logging is the responsibility of the future external
   watchdog / monitoring system.
6. A reset does not itself "fix" a fault:
   if the same invalid condition still exists after startup, the fault is
   raised again.

---

# 2. Proposed fault-code representation

Suggested first-pass representation:

```cpp
enum class FaultCode : uint8_t {
    None = 0,

    DoorOpenBoltLocked,
    LockFailed,
    UnlockFailed,
    DoorOpenTooLong,
    InvalidMode
};
```

This deliberately starts small.

Additional codes should only be added when the controller can actually
distinguish the condition reliably.

Do not create speculative fault codes for physical causes that cannot be
separated from one another by the available sensors.

---

# 3. Fault catalogue

## FLT-001 — Door open while bolt reports locked

Code:

    FaultCode::DoorOpenBoltLocked

Raised when:

    door_closed == false
    bolt_locked == true

or when either of the following occurs:

- `DOOR_OPENED` while in `LOCKED_CLOSED`;
- `BOLT_LOCKED` while in `UNLOCKED_OPEN`.

Response:

    stop T1/T2
    enter ERROR

No automatic corrective lock/unlock command is issued.

Rationale:

The bolt-state sensor is considered trustworthy only while the door is closed
and aligned with the frame.

Therefore this observed combination does not prove that the lock is genuinely
in a normal "locked open" physical state.

Possible causes include:

- manually extended bolt while the door is open;
- bolt-sensor fault;
- door-sensor fault;
- wiring fault;
- unusual mechanical condition.

The controller cannot safely distinguish those causes.

Auto-clear condition:

    door_closed == true

and the resulting closed-door sensor combination corresponds to a valid stable
state:

    closed + locked   -> LOCKED_CLOSED
    closed + unlocked -> UNLOCKED_CLOSED

If the door remains open while `bolt_locked == true`, the fault remains active.

Historical note:

The previous FSM considered automatically commanding UNLOCK after an
open/locked timeout. That recovery was deliberately rejected because the
specific physical condition cannot be identified reliably enough.

---

## FLT-002 — Lock operation failed

Code:

    FaultCode::LockFailed

Raised when:

- the controller enters `LOCKING`;
- the bolt fails to reach the locked indication within `LOCK_TIME_MS`;
- the configured number of retries is exhausted.

With the current parameters:

    initial attempt
    retry 1
    retry 2
    -> fault

Response:

    release T1
    enter ERROR

Possible physical causes include:

- mechanical obstruction;
- door not aligned correctly;
- Utopic lock failure;
- 433 MHz interface failure;
- lock relay failure;
- bolt sensor failure;
- wiring failure.

The controller does not attempt to identify which physical cause produced the
failed operation.

Auto-clear condition:

If the bolt subsequently reports a valid stable state while the door is closed,
the fault may auto-clear and the physical state is reconstructed.

Examples:

    door closed + bolt locked
        -> LOCKED_CLOSED

    door closed + bolt unlocked
        -> UNLOCKED_CLOSED

A future policy may choose to immediately re-apply normal Standard/Open Night
behaviour after reconstruction.

The exhausted retry counter is cleared when the fault clears.

---

## FLT-003 — Unlock operation failed

Code:

    FaultCode::UnlockFailed

Raised when:

- the controller enters `UNLOCKING`;
- the bolt remains locked beyond `UNLOCK_TIME_MS`;
- the configured number of retries is exhausted.

With the current parameters:

    initial attempt
    retry 1
    retry 2
    -> fault

Response:

    release T2
    enter ERROR

Possible physical causes include:

- mechanical obstruction;
- Utopic lock failure;
- 433 MHz interface failure;
- unlock relay failure;
- bolt sensor failure;
- wiring failure.

The controller does not attempt to identify which physical cause produced the
failed operation.

Auto-clear condition:

If the bolt subsequently reports unlocked while the door is closed:

    door closed + bolt unlocked
        -> UNLOCKED_CLOSED

the fault may clear.

If the door opens and the bolt indication no longer represents a meaningful
physical lock state, normal open-door reconstruction/fault rules apply.

The exhausted retry counter is cleared when the fault clears.

---

## FLT-004 — Door open too long

Code:

    FaultCode::DoorOpenTooLong

Raised when:

    state == UNLOCKED_OPEN

and:

    MAX_OPEN_TIMEOUT_MS

expires.

Current initial threshold:

    5 minutes

Response:

    enter ERROR
    do not command the lock

`OPEN_TIMEOUT_MS` at 30 seconds is not itself a fault.

It is a warning / diagnostic threshold only.

Rationale:

The controller must not attempt the historical automatic-unlock recovery while
the door is open.

The timeout therefore records an abnormal prolonged-open condition without
blindly actuating the lock.

Auto-clear condition:

When the door closes and the controller can reconstruct a valid physical state.

For example:

    door closes + bolt unlocked
        -> UNLOCKED_CLOSED

Normal mode policy then resumes.

---

## FLT-005 — Invalid operating-mode input

Code:

    FaultCode::InvalidMode

Raised when the physical key-switch input combination does not decode to one
of:

    Disabled
    Standard
    OpenNight

Examples include an impossible combination such as both dedicated mode inputs
active simultaneously, depending on the final key-switch wiring.

Response:

    stop T1/T2
    enter ERROR

The invalid combination must never be silently interpreted as Standard or Open
Night.

Auto-clear condition:

When the key-switch inputs again decode to a valid mode.

The controller then reconstructs the appropriate physical state and applies the
selected mode policy.

---

# 4. Conditions that are NOT faults

The following are intentionally not faults.

## 4.1 Manual unlock

If:

    LOCKED_CLOSED
    bolt_locked changes true -> false

without an electronic unlock command, this is treated as:

    manual unlock

and transitions to:

    UNLOCKED_CLOSED

---

## 4.2 Manual lock

If:

    UNLOCKED_CLOSED
    bolt_locked changes false -> true

without an electronic lock command, this is treated as:

    manual lock

and transitions to:

    LOCKED_CLOSED

---

## 4.3 Door opens during `LOCKING`

This is not itself a fault.

Required behaviour:

    release T1
    transition to UNLOCKED_OPEN

If the bolt sensor simultaneously produces an invalid open-door locked
indication, FLT-001 may then be raised.

---

## 4.4 Door opens during `UNLOCKING`

This is not itself a fault.

Required behaviour:

    release T2
    transition to UNLOCKED_OPEN

Again, if the resulting sensor combination is contradictory, FLT-001 applies.

---

## 4.5 `OPEN_TIMEOUT_MS` expires

The initial 30-second open timeout is not an ERROR condition.

It exists for:

- diagnostics;
- future watchdog notification;
- possible local warning indication.

Only `MAX_OPEN_TIMEOUT_MS` currently raises FLT-004.

---

## 4.6 RFID input active at startup

An RFID input already active when the Nano boots is not a fault.

It is ignored until the input first returns inactive.

This is deliberate startup qualification, not error handling.

---

## 4.7 Exit button held at startup

An exit button held during startup is not a fault.

When electronic operation is enabled, it is treated as a valid release request.

---

# 5. Fault priority

The first implementation does not require simultaneous multi-fault reporting.

If more than one fault condition is observable at the same time, the controller
may record one primary `FaultCode`.

Suggested priority:

1. `DoorOpenBoltLocked`
2. `InvalidMode`
3. active operation failure (`LockFailed` / `UnlockFailed`)
4. `DoorOpenTooLong`

Reasoning:

- contradictory physical sensor state should take priority over inferred
  operation outcomes;
- invalid operator-mode wiring prevents a reliable control policy;
- lock/unlock operation failures are then reported;
- prolonged-open timing is lower priority than contradictory hardware state.

A future watchdog may independently monitor raw inputs and report multiple
simultaneous diagnostic conditions even if the Nano FSM keeps only one primary
fault code.

---

# 6. Auto-clear rules

Faults auto-clear by default.

Auto-clear must be based on the underlying condition becoming valid, not merely
on time passing.

| Fault | Auto-clear condition |
|---|---|
| `DoorOpenBoltLocked` | sensor combination becomes physically valid |
| `LockFailed` | valid closed-door lock state becomes observable |
| `UnlockFailed` | valid unlocked state becomes observable |
| `DoorOpenTooLong` | door closes into a valid state |
| `InvalidMode` | key-switch inputs decode to a valid mode |

After auto-clear:

1. clear the current `FaultCode`;
2. clear any associated exhausted retry counter;
3. reconstruct the stable state from current inputs;
4. apply current operating-mode policy.

---

# 7. Reset behaviour

Fault codes are not persisted in EEPROM.

On reset:

    FaultCode = None

until startup evaluation identifies a current fault.

Examples:

A previous `LockFailed` followed by a reset with:

    door closed
    bolt locked

starts normally as:

    LOCKED_CLOSED

A reset with:

    door open
    bolt locked

raises:

    DoorOpenBoltLocked

again because the fault condition is still observable.

---

# 8. Diagnostics interface

The controller should make at least the following available to future local or
remote diagnostics:

```cpp
struct ControllerStatus {
    State state;
    FaultCode fault;
    uint8_t lockRetries;
    uint8_t unlockRetries;
};
```

The diagnostic interface must not be required for correct door operation.

Potential future consumers include:

- local error LED(s);
- existing FAULT relay;
- serial diagnostics;
- independent watchdog controller;
- email / remote alert system.

---

# 9. Future faults not yet justified

Do not add the following as first-pass FSM faults unless additional hardware or
diagnostics make them independently detectable:

- door sensor failed;
- bolt sensor failed;
- Utopic lock failed;
- 433 MHz transmitter failed;
- relay failed;
- cable disconnected;
- motor jammed.

At present these may produce observable symptoms such as:

    DoorOpenBoltLocked
    LockFailed
    UnlockFailed

but the Nano cannot reliably determine the underlying physical cause.

The independent watchdog system may later provide additional diagnostic
coverage.

---

# 10. Test requirements

Each fault requires tests for:

1. exact trigger condition;
2. transition into `ERROR`;
3. immediate removal of T1/T2 actuation;
4. correct `FaultCode`;
5. no unintended corrective lock/unlock command;
6. persistence while the underlying condition remains;
7. correct auto-clear condition;
8. correct state reconstruction after clearing;
9. equivalent behaviour after controller reset where applicable.

The tests should also verify that all conditions listed in Section 4 remain
non-fault behaviours.
