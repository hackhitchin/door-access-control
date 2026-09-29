# Door Controller Transition Specification

Status: Draft v1  
Project: Hitchin Hackspace door access system

This document defines the finite state machine behaviour of the Arduino Nano
door controller.

It is derived from the existing FSM diagram, with the following agreed updates:

- RFID/access-controller unlock requests and internal exit-button requests are
  treated as separate inputs.
- The controller has three operating modes:
  - Disabled
  - Standard
  - Open Night
- Selecting Open Night while locked immediately starts unlocking.
- T1 is held active for the whole `LOCKING` state.
- T2 is held active for the whole `UNLOCKING` state.
- Manual lock/unlock operation is permitted.
- `LOCKED_OPEN` is removed as a normal state because the bolt sensor is not
  considered trustworthy while the door is open.
- The historical struck-through automatic-unlock recovery paths for a
  manually-locked-open door are not implemented.
- An RFID release signal already active at startup is ignored until it clears.
- An exit button held during startup is honoured.
- If startup finds the door closed and unlocked in Standard mode, the controller
  locks immediately rather than waiting for `auto_lock_time`.

---

# 1. States

## 1.1 Stable states

### `LOCKED_CLOSED`

Door is closed and bolt sensor confirms locked.

Normal lock output:

    NONE

### `UNLOCKED_CLOSED`

Door is closed and bolt sensor does not indicate locked.

Normal lock output:

    NONE

### `UNLOCKED_OPEN`

Door is open.

The bolt sensor is not treated as a reliable indication of meaningful lock
state while the door is open.

Normal lock output:

    NONE

### `DISABLED`

Automatic/electronic lock control is disabled by the mode key switch.

Normal lock output:

    NONE

### `ERROR`

The controller has detected a fault requiring fault handling.

Normal lock output:

    NONE

Exact fault-latching and recovery behaviour is defined separately.

---

## 1.2 Transient states

### `LOCKING`

The controller is actively commanding the motorised lock to lock.

Output:

    hold T1 low

The command remains active until:

- the bolt sensor confirms locked;
- the door opens;
- a release request overrides the lock operation;
- the mode changes to Disabled;
- the operation times out and fault/retry logic acts.

### `UNLOCKING`

The controller is actively commanding the motorised lock to unlock.

Output:

    hold T2 low

The command remains active until:

- the bolt sensor confirms unlocked;
- the door opens;
- the mode changes to Disabled;
- the operation times out and fault/retry logic acts.

---

# 2. Events

External / physical events:

    RFID_RELEASE_REQUEST
    EXIT_BUTTON_REQUEST

    DOOR_OPENED
    DOOR_CLOSED

    BOLT_LOCKED
    BOLT_UNLOCKED

    MODE_DISABLED
    MODE_STANDARD
    MODE_OPEN_NIGHT
    MODE_INVALID

Timer / internal events:

    AUTO_LOCK_TIMEOUT
    OPEN_TIMEOUT
    MAX_OPEN_TIMEOUT

    LOCK_TIMEOUT
    UNLOCK_TIMEOUT

---

# 3. Output rule

The lock command is determined primarily by controller state:

| State | Output |
|---|---|
| `LOCKING` | hold T1 low |
| `UNLOCKING` | hold T2 low |
| `LOCK_RETRY_WAIT` | no lock command |
| `UNLOCK_RETRY_WAIT` | no lock command |
| all other states | no lock command |

LOCK and UNLOCK must never be asserted simultaneously.

---

# 4. Runtime transition table

## 4.1 `LOCKED_CLOSED`

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `RFID_RELEASE_REQUEST` | mode != Disabled | begin unlock | `UNLOCKING` | Access-controller request |
| `EXIT_BUTTON_REQUEST` | mode != Disabled | begin unlock | `UNLOCKING` | Internal exit request |
| `BOLT_UNLOCKED` | — | none | `UNLOCKED_CLOSED` | Manual unlock |
| `DOOR_OPENED` | bolt still reports locked | record sensor/physical fault | `ERROR` | Door-open + bolt-locked is not a trusted physical state |
| `MODE_DISABLED` | — | none | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | begin unlock | `UNLOCKING` | Open Night selection immediately unlocks |
| `MODE_STANDARD` | already Standard | none | `LOCKED_CLOSED` | no-op |
| `MODE_INVALID` | — | record mode fault | `ERROR` | proposed fault handling |

---

## 4.2 `UNLOCKING`

Output while in state:

    hold T2 low

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `BOLT_UNLOCKED` | — | release T2; clear unlock retry count | `UNLOCKED_CLOSED` | Successful unlock |
| `UNLOCK_TIMEOUT` | retries remain | release T2; increment retry count; start retry-delay timer | `UNLOCK_RETRY_WAIT` | Maximum 2 retries after initial attempt |
| `UNLOCK_TIMEOUT` | retries exhausted | release T2; record unlock fault | `ERROR` | |
| `DOOR_OPENED` | — | release T2 immediately | `UNLOCKED_OPEN` | Door opening aborts the active unlock command |
| `RFID_RELEASE_REQUEST` | — | none | `UNLOCKING` | Already unlocking |
| `EXIT_BUTTON_REQUEST` | — | none | `UNLOCKING` | Already unlocking |
| `MODE_DISABLED` | — | release T2 | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | continue unlock | `UNLOCKING` | |
| `MODE_STANDARD` | — | continue unlock | `UNLOCKING` | |
| `MODE_INVALID` | — | release T2; record mode fault | `ERROR` | |
| `BOLT_LOCKED` | — | none | `UNLOCKING` | Still waiting for unlock |

## 4.2a `UNLOCK_RETRY_WAIT`

Output while in state:

    none

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `RETRY_DELAY_ELAPSED` | — | restart unlock attempt | `UNLOCKING` | T2 is reasserted on entry to `UNLOCKING` |
| `BOLT_UNLOCKED` | — | clear unlock retry count | `UNLOCKED_CLOSED` | Mechanism may finish moving after T2 is released |
| `DOOR_OPENED` | — | clear unlock retry count | `UNLOCKED_OPEN` | Abort retry sequence |
| `MODE_DISABLED` | — | clear unlock retry count | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | none | `UNLOCK_RETRY_WAIT` | Continue retry sequence |
| `MODE_STANDARD` | — | none | `UNLOCK_RETRY_WAIT` | Continue retry sequence |

---

## 4.3 `UNLOCKED_CLOSED`

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `DOOR_OPENED` | — | start / continue open timing | `UNLOCKED_OPEN` | |
| `BOLT_LOCKED` | — | none | `LOCKED_CLOSED` | Manual lock |
| `AUTO_LOCK_TIMEOUT` | mode == Standard | begin lock | `LOCKING` | |
| `RFID_RELEASE_REQUEST` | mode == Standard | restart `auto_lock_time` | `UNLOCKED_CLOSED` | Already unlocked; grant a fresh unlocked interval |
| `EXIT_BUTTON_REQUEST` | mode == Standard | restart `auto_lock_time` | `UNLOCKED_CLOSED` | Already unlocked; grant a fresh unlocked interval |
| `MODE_DISABLED` | — | none | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | cancel / extend Standard auto-lock behaviour | `UNLOCKED_CLOSED` | Open Night keeps door released |
| `MODE_STANDARD` | from Open Night | start fresh `auto_lock_time` | `UNLOCKED_CLOSED` | Lock when the Standard timer expires |
| `MODE_INVALID` | — | record mode fault | `ERROR` | |

---

## 4.4 `LOCKING`

Output while in state:

    hold T1 low

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `BOLT_LOCKED` | door still closed | release T1; clear lock retry count | `LOCKED_CLOSED` | Successful lock |
| `LOCK_TIMEOUT` | retries remain | release T1; increment retry count; start back-off timer | `LOCK_RETRY_WAIT` | Up to 5 retries after initial attempt |
| `LOCK_TIMEOUT` | retries exhausted | release T1; record lock fault | `ERROR` | |
| `DOOR_OPENED` | — | release T1 immediately | `UNLOCKED_OPEN` | Abort active lock command |
| `RFID_RELEASE_REQUEST` | — | release T1; begin unlock | `UNLOCKING` | Release overrides locking |
| `EXIT_BUTTON_REQUEST` | — | release T1; begin unlock | `UNLOCKING` | Exit request overrides locking |
| `MODE_DISABLED` | — | release T1 | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | release T1; begin unlock | `UNLOCKING` | Open Night immediately unlocks |
| `MODE_STANDARD` | — | continue lock | `LOCKING` | |
| `MODE_INVALID` | — | release T1; record mode fault | `ERROR` | |
| `BOLT_UNLOCKED` | — | none | `LOCKING` | Still waiting for lock |

## 4.4a `LOCK_RETRY_WAIT`

Output while in state:

    none

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `RETRY_DELAY_ELAPSED` | — | restart lock attempt | `LOCKING` | T1 is reasserted on entry to `LOCKING` |
| `BOLT_LOCKED` | — | clear lock retry count | `LOCKED_CLOSED` | Mechanism may finish moving after T1 is released |
| `DOOR_OPENED` | — | clear lock retry count | `UNLOCKED_OPEN` | Abort retry sequence |
| `RFID_RELEASE_REQUEST` | mode != Disabled | clear lock retry count; begin unlock | `UNLOCKING` | Release overrides retry wait |
| `EXIT_BUTTON_REQUEST` | mode != Disabled | clear lock retry count; begin unlock | `UNLOCKING` | Exit overrides retry wait |
| `MODE_DISABLED` | — | clear lock retry count | `DISABLED` | |
| `MODE_OPEN_NIGHT` | — | clear lock retry count; begin unlock | `UNLOCKING` | |

---

## 4.5 `UNLOCKED_OPEN`

The bolt sensor is not used to establish a normal `LOCKED_OPEN` state while the
door is open.

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `DOOR_CLOSED` | mode == Standard | begin / schedule Standard relock behaviour | `UNLOCKED_CLOSED` | |
| `DOOR_CLOSED` | mode == OpenNight | remain released under Open Night policy | `UNLOCKED_CLOSED` | |
| `DOOR_CLOSED` | mode == Disabled | none | `DISABLED` | |
| `BOLT_LOCKED` | door still open | record sensor/physical fault | `ERROR` | Door-open + bolt-locked is treated as fault |
| `MAX_OPEN_TIMEOUT` | — | record open-too-long fault | `ERROR` | |
| `OPEN_TIMEOUT` | — | no automatic lock actuation | `UNLOCKED_OPEN` | Historical automatic-unlock recovery was rejected |
| `RFID_RELEASE_REQUEST` | — | none | `UNLOCKED_OPEN` | Already released |
| `EXIT_BUTTON_REQUEST` | — | none | `UNLOCKED_OPEN` | Already released |
| `MODE_DISABLED` | — | none | `DISABLED` | |
| `MODE_STANDARD` | — | apply Standard open-time limits | `UNLOCKED_OPEN` | |
| `MODE_OPEN_NIGHT` | — | apply Open Night open-time limits | `UNLOCKED_OPEN` | |
| `MODE_INVALID` | — | record mode fault | `ERROR` | |

---

## 4.6 `DISABLED`

Disabled mode performs no electronic lock/unlock action.

RFID and exit-button requests are ignored in this mode.

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| `RFID_RELEASE_REQUEST` | — | ignore | `DISABLED` | |
| `EXIT_BUTTON_REQUEST` | — | ignore | `DISABLED` | TODO: revisit/confirm before release |
| `MODE_STANDARD` | door closed, bolt locked | none | `LOCKED_CLOSED` | Reconstruct physical state |
| `MODE_STANDARD` | door closed, bolt unlocked | begin lock immediately | `LOCKING` | |
| `MODE_STANDARD` | door open | none | `UNLOCKED_OPEN` | Bolt state not trusted while open |
| `MODE_OPEN_NIGHT` | door closed, bolt locked | begin unlock | `UNLOCKING` | Open Night selection immediately releases door |
| `MODE_OPEN_NIGHT` | door closed, bolt unlocked | none | `UNLOCKED_CLOSED` | |
| `MODE_OPEN_NIGHT` | door open | none | `UNLOCKED_OPEN` | |
| `MODE_INVALID` | — | record mode fault | `ERROR` | |
| `BOLT_LOCKED` / `BOLT_UNLOCKED` | — | update observed physical condition only | `DISABLED` | No electronic actuation |
| `DOOR_OPENED` / `DOOR_CLOSED` | — | update observed physical condition only | `DISABLED` | No electronic actuation |

---

## 4.7 `ERROR`

The exact fault model is defined separately.

General behaviour:

- stop T1/T2 actuation;
- retain/report the fault cause;
- do not blindly attempt corrective motor movement;
- allow startup/recovery logic to re-evaluate physical state after reset.

| Event | Guard | Action | Next state | Notes |
|---|---|---|---|---|
| normal request | fault remains present | none | `ERROR` | |
| `MODE_DISABLED` | — | stop actuation | `DISABLED` | proposed operator recovery path |
| fault condition clears | auto-clear allowed for that fault | TBD | reconstructed stable state | fault policy TBD |
| controller reset | — | run startup reconstruction | startup path | old transient state is not restored |

---

# 5. Removed `LOCKED_OPEN` state

The previous FSM diagram contained a `lock: locked / door: open` state.

This state is removed from the revised FSM.

Reason:

- the lock-state sensor is only considered meaningful when the door is closed
  and aligned with the frame;
- therefore `door open + bolt locked` cannot be treated as a trustworthy normal
  physical state;
- the same sensor combination could result from a manually extended bolt,
  sensor error, wiring error, or another abnormal condition;
- automatically operating the lock to recover from that ambiguous condition
  was judged too risky.

Accordingly:

    door open + bolt reports locked

is treated as a sensor / physical fault rather than a normal FSM state.

---

# 6. Removed historical automatic-unlock recovery

The previous FSM drawing contained struck-through transitions equivalent to:

    electronic unlock after open_timeout

and:

    electronic unlock after locked_open_timeout

These were intended to recover from a possible condition where the door had
been left open with the lock manually extended.

They are deliberately not implemented.

Reason:

- the controller cannot reliably determine that this specific condition exists;
- the same observed inputs can represent other abnormal/fault states;
- automatically operating the lock in an ambiguous physical condition could
  make the situation worse.

Therefore no automatic unlock is performed solely because an open-door timeout
expires.

`MAX_OPEN_TIMEOUT` may still produce an error/fault indication: 5 minutes in Standard mode and 2 hours in Open Night.

---

# 7. Startup / recovery behaviour

Startup does not restore transient controller states such as `LOCKING` or
`UNLOCKING`.

Instead, the controller reads the current physical inputs and reconstructs an
appropriate stable state.

RFID and exit-button startup semantics differ:

- an RFID release signal already active at boot is ignored until it returns
  inactive;
- an exit button already held at boot is honoured.

---

## 7.1 Startup reconstruction table

| Mode | Door | Bolt | Exit button held | RFID active at boot | Startup behaviour |
|---|---|---|---|---|---|
| Disabled | any | any | any | any | enter `DISABLED`; no lock actuation |
| Standard | closed | locked | no | ignored | enter `LOCKED_CLOSED` |
| Standard | closed | locked | yes | ignored | begin `UNLOCKING` |
| Standard | closed | unlocked | no | ignored | begin `LOCKING` immediately |
| Standard | closed | unlocked | yes | ignored | enter `UNLOCKED_CLOSED` |
| Standard | open | any | any | ignored | enter `UNLOCKED_OPEN`; if bolt reports locked, raise fault |
| Open Night | closed | locked | any | ignored | begin `UNLOCKING` |
| Open Night | closed | unlocked | any | ignored | enter `UNLOCKED_CLOSED` |
| Open Night | open | any | any | ignored | enter `UNLOCKED_OPEN`; if bolt reports locked, raise fault |
| Invalid | any | any | any | any | enter fault handling |

---

## 7.2 RFID input arming after boot

At startup:

    if RFID input is inactive:
        arm immediately

    if RFID input is active:
        ignore it
        remain disarmed until it becomes inactive

After the input has been observed inactive:

    the next inactive -> active transition
    generates RFID_RELEASE_REQUEST

This prevents the tail end of an ESP-RFID timed output pulse from being treated
as a new access request after a controller reboot.

---

# 8. Retry behaviour

## 8.1 Locking

On entry to `LOCKING`:

    assert T1
    start lock timeout

If `BOLT_LOCKED` occurs before timeout:

    release T1
    clear lock retry count
    enter LOCKED_CLOSED

If `LOCK_TIMEOUT` occurs and retries remain:

    release/restart command according to retry policy
    increment lock retry count
    remain LOCKING

If retries are exhausted:

    release T1
    record lock failure
    enter ERROR

Use a 1 s retry delay. Retry counters count retries after the initial attempt.

---

## 8.2 Unlocking

On entry to `UNLOCKING`:

    assert T2
    start unlock timeout

If `BOLT_UNLOCKED` occurs before timeout:

    release T2
    clear unlock retry count
    enter UNLOCKED_CLOSED

If `UNLOCK_TIMEOUT` occurs and retries remain:

    release/restart command according to retry policy
    increment unlock retry count
    remain UNLOCKING

If retries are exhausted:

    release T2
    record unlock failure
    enter ERROR

Use a 1 s retry delay. Retry counters count retries after the initial attempt.

---

# 9. Operating-mode behaviour

## 9.1 Disabled

Disabled means:

- no electronic lock command;
- no electronic unlock command;
- RFID release requests ignored;
- exit-button requests ignored;
- physical inputs may still be monitored for status/diagnostics.

TODO:

- revisit/confirm whether exit-button requests should remain ignored in Disabled
  mode before final release.

---

## 9.2 Standard

Standard is normal access-controlled operation.

Typical behaviour:

- valid RFID request -> unlock;
- exit button -> unlock;
- after closure / unlocked dwell -> automatic relock;
- normal open-time limits apply.

Exact timing values are defined separately.

---

## 9.3 Open Night

Selecting Open Night while the door is locked immediately starts unlocking.

While in Open Night:

- the door remains released for the longer Open Night policy;
- Standard automatic-relock timing is not used;
- Open Night-specific timing limits may still apply.

Exact timing values are defined separately.

---

# 10. Timing and retry policy

The initial controller parameter values are defined in `parameters.md`.

The transition rules assume the following policy:

- `auto_lock_time` = 5 s
- `open_timeout` = 30 s
- `max_open_timeout` = 5 min
- Open Night duration = 2 h
- `lock_time` = 5 s
- `unlock_time` = 5 s
- maximum lock retries = 5, with 1/10/100/1000/10000 s back-off
- maximum unlock retries = 2
- retry delay = 1 s

Retry counters count retries after the initial attempt.

Therefore, with a maximum retry count of 2:

    initial attempt
    retry 1
    retry 2
    ERROR

A retry counter value of zero means no retries have yet occurred.

---

# 11. Fault clearing and reset policy

Faults auto-clear when their underlying fault condition no longer exists and
the controller can reconstruct a valid stable state.

No FSM fault history is persisted across controller reset.

On reset:

1. historical transient fault state is discarded;
2. current inputs are sampled;
3. startup state reconstruction is performed;
4. any currently-present fault is raised again from the observed inputs.

Persistent fault/event logging, if required, belongs in the external
watchdog/monitoring system rather than in the Nano FSM.

---

# 12. Open Night -> Standard transition

If the controller is unlocked when operating mode changes from Open Night to
Standard:

    start a fresh Standard auto_lock_time

The controller does not lock immediately solely because the mode changed.

If no release request or other event intervenes before the timer expires:

    begin LOCKING

---

# 13. Release request while already unlocked

A new RFID release request while in `UNLOCKED_CLOSED` restarts the Standard
`auto_lock_time`.

A new exit-button request while in `UNLOCKED_CLOSED` also restarts the Standard
`auto_lock_time`.

This ensures that each new legitimate release request receives a complete
unlocked interval rather than inheriting a nearly-expired timer.

In Open Night mode, these requests do not shorten the Open Night release period.

---

# 14. Remaining unresolved decision

The following item remains deliberately unresolved:

- Final confirmation of exit-button behaviour while `Disabled`.

Current first-pass behaviour is:

    Disabled -> ignore EXIT_BUTTON_REQUEST

This must be revisited before final release.

---

# 15. Future optional behaviour

The following are intentionally outside this first-pass FSM:

- backup maglock control;
- watchdog/email monitoring;
- local fault LED strategy;
- remote diagnostics;
- persistent fault logging.

These may consume controller state/fault information later without changing the
core physical state model unless explicitly required.
