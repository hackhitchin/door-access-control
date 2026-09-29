# Wrapper / Controller Test Plan

This document covers the code around the already-tested `DoorFsm`:

- `src/door_controller.cpp`
- `src/door_controller.h`
- `src/debounce.h`
- `src/controller_config.h`
- the very thin `door-access-control.ino` hardware adapter

The FSM transition tests remain separate.

The wrapper test executable should explicitly compile:

```text
src/door_fsm.cpp
src/door_controller.cpp
test/test_controller.cpp
```

`door-access-control.ino` should remain outside the normal host-side coverage target because it is the hardware-specific adapter using `digitalRead()`, `digitalWrite()`, `pinMode()` and `millis()`.

---

# Test objectives

The wrapper tests should prove that:

1. raw input changes are debounced correctly;
2. only stable input changes generate FSM events;
3. startup reconstructs the correct state;
4. RFID active at boot is suppressed until it clears;
5. exit held at boot is honoured;
6. controller timers generate the correct FSM events;
7. timers remain correct across `uint32_t` / `millis()` wraparound;
8. stale timers do not affect later states;
9. simultaneous sensor/timer conditions are handled in the intended priority order;
10. the controller never creates an invalid lock/unlock output;
11. mode changes are debounced and interpreted correctly;
12. wrapper-level fault startup/recovery paths behave as specified.

---

# Test groups

## WRAP-START — Startup reconstruction

### WRAP-START-001 — Standard, closed, locked
Initial inputs:

```text
mode        = Standard
doorClosed  = true
boltLocked  = true
rfidActive  = false
exitPressed = false
```

Expected:

```text
state       = LockedClosed
command     = None
fault       = None
```

### WRAP-START-002 — Standard, closed, unlocked
Expected:

```text
state   = Locking
command = Lock
```

This verifies the "lock immediately on startup" rule.

### WRAP-START-003 — Standard, open, unlocked
Expected:

```text
state   = UnlockedOpen
command = None
```

No lock command may be generated while the door is open.

### WRAP-START-004 — Open Night, closed, locked
Expected:

```text
state   = Unlocking
command = Unlock
```

This preserves the current FSM behaviour.

### WRAP-START-005 — Open Night, closed, unlocked
Expected:

```text
state   = UnlockedClosed
command = None
```

The current Open Night timing behaviour remains unchanged.

### WRAP-START-006 — Disabled
For physically valid locked/unlocked/open combinations:

```text
state   = Disabled
command = None
```

### WRAP-START-007 — Open + bolt locked contradiction
Initial:

```text
doorClosed = false
boltLocked = true
```

Expected:

```text
state = Error
fault = DoorOpenBoltLocked
command = None
```

### WRAP-START-008 — Invalid mode at startup
Expected:

```text
state = Error
fault = InvalidMode
command = None
```

### WRAP-START-009 — Contradictory sensors have priority over invalid mode
Initial:

```text
mode = Invalid
doorClosed = false
boltLocked = true
```

Expected fault priority:

```text
DoorOpenBoltLocked
```

---

## WRAP-RFID — RFID edge qualification

### WRAP-RFID-001 — RFID inactive at startup is armed
Expected:

```text
rfidArmed() == true
```

### WRAP-RFID-002 — RFID active at startup is disarmed
Expected:

```text
rfidArmed() == false
```

and no release request is generated.

### WRAP-RFID-003 — Active-at-boot remains ignored while held
Keep RFID active for longer than the debounce time.

Expected:

- no unlock transition;
- still disarmed.

### WRAP-RFID-004 — Active-at-boot rearms after stable inactive period
Change RFID inactive and hold for at least `DEBOUNCE_RFID_MS`.

Expected:

```text
rfidArmed() == true
```

but no release event yet.

### WRAP-RFID-005 — Subsequent stable active edge generates release
After WRAP-RFID-004, assert RFID active for the debounce time.

From `LockedClosed`, expected:

```text
state   = Unlocking
command = Unlock
```

### WRAP-RFID-006 — RFID glitch shorter than debounce is ignored
Pulse active for less than `DEBOUNCE_RFID_MS`.

Expected:

- no stable input change;
- no FSM transition.

### WRAP-RFID-007 — Continuous active signal generates one request only
After a valid active edge, leave the signal active.

Expected:

- one release event;
- no repeated event every `tick()`.

---

## WRAP-EXIT — Exit button

### WRAP-EXIT-001 — Exit held at boot is honoured
Start `LockedClosed`, Standard, exit pressed.

Expected:

```text
state   = Unlocking
command = Unlock
```

### WRAP-EXIT-002 — Exit held at boot in Disabled remains inactive
Expected:

```text
state   = Disabled
command = None
```

This reflects the current agreed behaviour; revisit if Disabled-mode exit behaviour changes.

### WRAP-EXIT-003 — Exit glitch shorter than debounce is ignored

### WRAP-EXIT-004 — Stable press creates exactly one request

### WRAP-EXIT-005 — Release alone creates no request

### WRAP-EXIT-006 — Second press after release creates a second request

---

## WRAP-DOOR — Door sensor debounce

### WRAP-DOOR-001 — Short open glitch ignored

### WRAP-DOOR-002 — Stable open edge recognised after 30 ms

From `UnlockedClosed`, expected:

```text
UnlockedOpen
```

### WRAP-DOOR-003 — Stable close edge recognised after 30 ms

From `UnlockedOpen`, expected:

```text
UnlockedClosed
```

and the appropriate auto-lock timer is started by the FSM effect.

### WRAP-DOOR-004 — Door opening during Locking aborts lock
Expected:

```text
state   = UnlockedOpen
command = None
```

### WRAP-DOOR-005 — Door opening during Unlocking aborts unlock
Expected:

```text
state   = UnlockedOpen
command = None
```

---

## WRAP-BOLT — Bolt sensor debounce

### WRAP-BOLT-001 — Short bolt sensor glitch ignored

### WRAP-BOLT-002 — Stable manual unlock recognised

From `LockedClosed`:

```text
UnlockedClosed
```

### WRAP-BOLT-003 — Stable manual lock recognised

From `UnlockedClosed`:

```text
LockedClosed
```

### WRAP-BOLT-004 — Bolt locked while door open faults

Expected:

```text
Error / DoorOpenBoltLocked
```

### WRAP-BOLT-005 — Successful lock sensor edge cancels operation timeout

After reaching `LockedClosed`, advancing time beyond the old lock timeout must not create a later retry/fault.

### WRAP-BOLT-006 — Successful unlock sensor edge cancels operation timeout

Equivalent stale-timer test for unlocking.

---

## WRAP-MODE — Key switch debounce and mode behaviour

### WRAP-MODE-001 — Mode change shorter than 50 ms ignored

### WRAP-MODE-002 — Disabled -> Standard, physically locked
Expected:

```text
LockedClosed
```

### WRAP-MODE-003 — Disabled -> Standard, physically unlocked and closed
Expected:

```text
Locking
```

### WRAP-MODE-004 — Disabled -> Standard, open and unlocked
Expected:

```text
UnlockedOpen
```

### WRAP-MODE-005 — LockedClosed -> Open Night
Expected:

```text
Unlocking
```

### WRAP-MODE-006 — Open Night -> Standard while unlocked
Expected:

- remain `UnlockedClosed`;
- fresh Standard auto-lock timer.

### WRAP-MODE-007 — Invalid combination creates InvalidMode fault

### WRAP-MODE-008 — Transition through an invalid intermediate switch position
Simulate mechanical switching where both mode inputs briefly appear active.

Verify behaviour after the 50 ms debounce:

- if the invalid condition lasts less than 50 ms: ignore it;
- if it persists for 50 ms: generate `InvalidMode`.

---

# Timer tests

All timer tests should check the instant before expiry and the instant at expiry.

For a duration `T`:

```text
T - 1 ms -> must not fire
T        -> must fire
```

## WRAP-TIMER-001 — Standard auto-lock timeout

Start `UnlockedClosed`, Standard.

At:

```text
AUTO_LOCK_TIME_MS - 1
```

expected:

```text
UnlockedClosed
```

At:

```text
AUTO_LOCK_TIME_MS
```

expected:

```text
Locking
```

## WRAP-TIMER-002 — New RFID request restarts Standard auto-lock timer

Example:

1. start timer at t=0;
2. valid RFID request at t=4000 ms;
3. verify no lock at t=5000 ms;
4. verify lock at t=9000 ms.

## WRAP-TIMER-003 — Exit request restarts Standard auto-lock timer

Same principle as RFID.

## WRAP-TIMER-004 — Open Night duration

Verify current configured `OPEN_NIGHT_TIME_MS` behaviour without changing its semantics.

## WRAP-TIMER-005 — Lock operation timeout

From `Locking`:

```text
LOCK_TIME_MS - 1 -> still Locking
LOCK_TIME_MS     -> LockRetryWait
```

## WRAP-TIMER-006 — Unlock operation timeout

Equivalent for `Unlocking`.

## WRAP-TIMER-007 — Retry wait

From `LockRetryWait` / `UnlockRetryWait`:

```text
RETRY_DELAY_MS - 1 -> remain waiting
RETRY_DELAY_MS     -> resume operation
```

## WRAP-TIMER-008 — Door-open warning

From `UnlockedOpen`:

```text
OPEN_TIMEOUT_MS
```

Expected:

- remain `UnlockedOpen`;
- `OpenTimeoutWarning` effect occurs;
- no fault.

## WRAP-TIMER-009 — Maximum-open fault

At:

```text
MAX_OPEN_TIMEOUT_MS
```

Expected:

```text
Error / DoorOpenTooLong
```

## WRAP-TIMER-010 — Leaving open state cancels open timers

Open door, then close it before either timeout.

Advance time beyond both previous deadlines.

Expected:

- no stale `OpenTimeout`;
- no stale `DoorOpenTooLong`.

## WRAP-TIMER-011 — Leaving transient operation cancels operation timer

Complete locking/unlocking before timeout, then advance beyond the old deadline.

Expected no stale timeout event.

---

# `millis()` wraparound tests

These are important because a Nano may run continuously for more than the approximately 49.7-day `millis()` rollover period.

## WRAP-WRAP-001 — Debounce across rollover

Example start:

```text
now = UINT32_MAX - 10
```

Change an input, cross through zero, and verify it becomes stable only after the correct debounce duration.

## WRAP-WRAP-002 — Auto-lock timer across rollover

Start the auto-lock timer shortly before `UINT32_MAX`, advance through rollover, and verify expiry at the correct elapsed duration.

## WRAP-WRAP-003 — Lock timeout across rollover

## WRAP-WRAP-004 — Unlock timeout across rollover

## WRAP-WRAP-005 — Retry delay across rollover

## WRAP-WRAP-006 — Open warning/max-open timers across rollover

---

# Simultaneous-event ordering

`DoorController::tick()` currently processes:

```text
door
bolt
mode
exit
RFID
timers
```

This ordering should be explicitly tested so later refactoring cannot silently change it.

## WRAP-ORDER-001 — Door opens at same tick as auto-lock timeout

Expected:

- door-open event wins;
- controller must not start locking an open door.

## WRAP-ORDER-002 — Bolt reaches locked at same tick as lock timeout

Expected:

- bolt success wins;
- enter `LockedClosed`;
- no retry.

## WRAP-ORDER-003 — Bolt reaches unlocked at same tick as unlock timeout

Expected:

- unlock success wins;
- enter `UnlockedClosed`;
- no retry.

## WRAP-ORDER-004 — Mode Disabled at same tick as operation timeout

Expected:

- Disabled takes effect before timeout;
- motor command becomes `None`;
- no retry should be started.

## WRAP-ORDER-005 — Release request at same tick as auto-lock timeout

Document and test the current processing order.

Because Exit and RFID are processed before timers, a valid release request should restart the timer before the old auto-lock timeout is evaluated.

---

# Retry sequence tests

## WRAP-RETRY-001 — Lock: initial attempt + two retries + failure

Verify actual time sequence:

```text
Locking
-> LOCK_TIME_MS
LockRetryWait
-> RETRY_DELAY_MS
Locking
-> LOCK_TIME_MS
LockRetryWait
-> RETRY_DELAY_MS
Locking
-> LOCK_TIME_MS
Error / LockFailed
```

Check that:

- lock relay command is active only during `Locking`;
- command is `None` during every retry wait;
- retry counter semantics remain "two retries after the initial attempt".

## WRAP-RETRY-002 — Unlock equivalent

## WRAP-RETRY-003 — Success during retry wait

If the sensor reaches the desired state during the wait, transition immediately to the stable state and never start another motor command.

---

# Output invariants at controller level

After every test event/tick, assert:

### WRAP-INV-001
`LockCommand::Lock` only when FSM state is `Locking`.

### WRAP-INV-002
`LockCommand::Unlock` only when FSM state is `Unlocking`.

### WRAP-INV-003
`Disabled` always produces `None`.

### WRAP-INV-004
`Error` always produces `None`.

### WRAP-INV-005
Retry-wait states always produce `None`.

### WRAP-INV-006
The wrapper never attempts to lock when its stable door reading is open.

### WRAP-INV-007
An ignored/debouncing raw input must not alter the FSM state.

---

# Exhaustive / generated testing

After the focused tests above are passing, add a generated safety test over combinations of:

```text
mode
door raw value
bolt raw value
RFID raw value
exit raw value
time advancement
```

This does not replace named tests. Its purpose is to repeatedly assert the controller invariants while applying many legal and contradictory sequences.

A fixed deterministic seed should be used if pseudo-random sequences are added.

---

# Hardware adapter (`door-access-control.ino`) checks

The `.ino` should remain extremely small, so most behaviour does not need host coverage.

The following should nevertheless be checked by inspection and later on a bench Nano:

## HW-001
All four confirmed inputs are configured `INPUT_PULLUP`.

## HW-002
Both mode inputs are configured `INPUT_PULLUP`.

## HW-003
Relay pins are written LOW before `pinMode(..., OUTPUT)` to avoid an energised startup glitch.

## HW-004
`LockCommand::Lock` results in:

```text
PIN_LOCK_RELAY   = HIGH
PIN_UNLOCK_RELAY = LOW
```

## HW-005
`LockCommand::Unlock` results in:

```text
PIN_LOCK_RELAY   = LOW
PIN_UNLOCK_RELAY = HIGH
```

## HW-006
`None` results in both LOW.

## HW-007
Changing directly from Lock to Unlock, or Unlock to Lock, performs break-before-make with at least 250 ms with both relay commands released.

## HW-008
FAULT and BlueBoard Button relays remain inactive until deliberately implemented.

---

# Suggested CI structure

Keep two native test executables so failures are easy to understand:

```text
test_fsm
test_controller
```

`test_fsm`:

```text
src/door_fsm.cpp
test/test_all.cpp
```

`test_controller`:

```text
src/door_fsm.cpp
src/door_controller.cpp
test/test_controller.cpp
```

Coverage should then be captured after both executables have run.

The filtered LCOV result should include:

```text
src/door_fsm.cpp
src/door_controller.cpp
```

and any instantiated/testable logic from:

```text
src/debounce.h
```

Suggested enforcement remains:

```text
lines      = 100%
functions  = 100%
branches   >= agreed reachable threshold
```

Do not include third-party ETL/Unity code or the Arduino core in the project's coverage target.
