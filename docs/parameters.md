# Door Controller Parameters

Status: Draft v1  
Project: Hitchin Hackspace door access system

This document defines the initial configurable timing and retry values used by
the door-controller FSM.

These are starting values for implementation and testing. The lock-motion
timings should be measured on the installed hardware and adjusted if required.

---

# 1. Initial values

| Parameter | Initial value | Purpose |
|---|---:|---|
| `AUTO_LOCK_TIME_MS` | 5,000 ms | Delay before relocking in Standard mode |
| `OPEN_TIMEOUT_MS` | 30,000 ms | Door-open warning/diagnostic threshold |
| `MAX_OPEN_TIMEOUT_MS` | 300,000 ms | Door-open duration that raises an error |
| `OPEN_NIGHT_TIME_MS` | 7,200,000 ms | Open Night release duration |
| `LOCK_TIME_MS` | 5,000 ms | Maximum duration of one lock attempt |
| `UNLOCK_TIME_MS` | 5,000 ms | Maximum duration of one unlock attempt |
| `RETRY_DELAY_MS` | 1,000 ms | Delay between failed actuation attempts |
| `MAX_LOCK_RETRIES` | 2 | Retries after the initial lock attempt |
| `MAX_UNLOCK_RETRIES` | 2 | Retries after the initial unlock attempt |

---

# 2. Suggested firmware constants

```cpp
constexpr uint32_t AUTO_LOCK_TIME_MS      = 5'000;
constexpr uint32_t OPEN_TIMEOUT_MS         = 30'000;
constexpr uint32_t MAX_OPEN_TIMEOUT_MS     = 300'000;

constexpr uint32_t OPEN_NIGHT_TIME_MS      = 7'200'000; // 2 hours

constexpr uint32_t LOCK_TIME_MS            = 5'000;
constexpr uint32_t UNLOCK_TIME_MS          = 5'000;
constexpr uint32_t RETRY_DELAY_MS          = 1'000;

constexpr uint8_t MAX_LOCK_RETRIES         = 2;
constexpr uint8_t MAX_UNLOCK_RETRIES       = 2;
```

---

# 3. Retry semantics

The retry counters count retries, not total attempts.

With:

    MAX_LOCK_RETRIES = 2

the sequence is:

    initial lock attempt
    retry 1
    retry 2
    ERROR

The same rule applies to unlocking.

After a failed attempt:

1. release the active T1/T2 command;
2. wait `RETRY_DELAY_MS`;
3. increment the retry counter;
4. begin another attempt;
5. restart the corresponding operation timeout.

A successful lock/unlock clears the corresponding retry counter.

---

# 4. Standard mode timing

In Standard mode:

- a new valid release request unlocks the door;
- while `UNLOCKED_CLOSED`, each new RFID or exit-button request restarts
  `AUTO_LOCK_TIME_MS`;
- switching from Open Night to Standard while already unlocked starts a fresh
  `AUTO_LOCK_TIME_MS`;
- when `AUTO_LOCK_TIME_MS` expires, locking begins if no other event prevents it.

`OPEN_TIMEOUT_MS` is a warning/diagnostic threshold only.

It does not command the lock.

`MAX_OPEN_TIMEOUT_MS` raises an open-too-long fault.

---

# 5. Open Night timing

Selecting Open Night while locked starts unlocking immediately.

The initial Open Night duration is:

    2 hours

represented by:

    OPEN_NIGHT_TIME_MS = 7,200,000

The detailed Open Night timer lifecycle will be verified during implementation,
but Standard-mode auto-lock timing is not used while Open Night remains active.

---

# 6. Lock and unlock attempt timing

Each lock attempt may hold T1 active for up to:

    5 seconds

Each unlock attempt may hold T2 active for up to:

    5 seconds

These values are deliberately conservative starting values.

Before final deployment, measure normal lock and unlock times on the installed
Utopic hardware under representative conditions and confirm that the timeout
provides adequate margin without unnecessarily delaying fault detection.

---

# 7. Fault/reset policy related to parameters

Faults auto-clear when their underlying condition clears and the FSM can return
to a valid stable state.

Fault history is not persisted across reset.

Persistent historical logging, if required, is delegated to the external
watchdog/monitoring system.
