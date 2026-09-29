# Door Access Controller

Arduino Nano based door-control firmware for the Hack Hitchin access-control
system.

The Nano does **not** authenticate RFID cards. Credential verification is
handled separately by the ESP-RFID Blue Board. The Nano is responsible for the
door-control state machine, sensor handling, timing and the Utopic lock
interface.

This project is being developed with an emphasis on:

- predictable behaviour;
- explicit failure handling;
- simple hardware interfaces;
- host-side automated testing;
- code that remains approachable to future hackspace maintainers.

> **Not yet ready for deployment on the real door.**
>
> The FSM/controller are under automated test and CI compiles the complete classic
> Nano target. Hardware bench test and installation validation are still to be
> completed.

## Architecture

The firmware is split into three layers:

```text
physical inputs / millis()
          |
          v
DoorController
- debounce
- edge qualification
- startup handling
- timeout generation
          |
          v
DoorFsm
- states
- transitions
- faults
- retry policy
          |
          v
LockCommand
          |
          v
Arduino GPIO / relays
```

The FSM contains door-control policy. The surrounding controller converts raw,
debounced inputs and elapsed time into FSM events. The `.ino` file is intended
to remain a thin Arduino hardware adapter.

## FSM

The state machine uses ETL `state_chart`.

States:

- `LockedClosed`
- `Unlocking`
- `UnlockRetryWait`
- `UnlockedClosed`
- `Locking`
- `LockRetryWait`
- `UnlockedOpen`
- `Disabled`
- `Error`

There is deliberately no normal `LockedOpen` state. The bolt sensor is only
considered reliable when the door is closed; door-open plus bolt-locked is
treated as a contradictory physical/sensor condition.

### Motor commands

Motor output is derived from FSM state:

```text
Locking    -> Lock
Unlocking  -> Unlock
everything else -> None
```

The lock and unlock outputs are never intentionally asserted together.

During a retry wait the command is `None`. Unlock retries wait 1 s; lock
retries use the configured 1/10/100/1000/10000 s back-off.

A release request received during an in-flight lock operation is queued. The
controller waits for that lock command to confirm or time out, then unlocks
instead of accepting the locked state or starting another lock retry.

## Faults

Current fault codes are:

- `DoorOpenBoltLocked`
- `LockFailed`
- `UnlockFailed`
- `DoorOpenTooLong`
- `InvalidMode`

Entering `Error` removes the lock/unlock motor command.

Faults are not persisted across reset. A fault that is still physically present
after reboot should be detected again from the current inputs.

Where the underlying fault clears and a valid physical state can be
reconstructed, the FSM can return to normal operation without retaining the old
fault.

## Operating modes

The planned three-position maintained key switch has two signal inputs:

```text
D8  Standard
D9  Open Night
```

with pull-ups and contacts that pull the selected input LOW.

Logical encoding:

```text
D8  D9
HIGH HIGH  Disabled
LOW  HIGH  Standard
HIGH LOW   Open Night
LOW  LOW   Invalid / wiring fault
```

`Disabled` currently means no electronic lock or unlock commands. In
particular, the exit button is currently ignored in Disabled mode; this remains
a behaviour item to revisit if required.

## Inputs

Current Nano signal map:

```text
D2  lock-state sensor       active LOW
D3  exit button             active LOW
D4  door-closed reed        active LOW
D7  BlueBoard RFID release  active LOW
D8  Standard-mode contact   active LOW
D9  Open-Night contact      active LOW
```

Debounce times:

```text
door          30 ms
bolt          20 ms
RFID          10 ms
exit button   30 ms
mode switch   1000 ms
```

RFID and exit-button requests intentionally have different boot behaviour:

- RFID already active at boot is ignored until it first becomes inactive;
- an exit button already held at boot is honoured when electronic control is
  enabled.

At power-up the complete raw input set must remain unchanged for 100 ms before
controller construction, but this qualification wait is capped at 2 s so a
chattering input cannot prevent boot indefinitely.

## Outputs

Current relay mapping:

```text
A3 -> RL4 -> Utopic puck T1 -> lock
A2 -> RL3 -> Utopic puck T2 -> unlock
A1 -> RL2 -> FAULT connector
A0 -> RL1 -> BlueBoard "Button" contacts
```

Driving A3 or A2 HIGH energises the corresponding relay and pulls the Utopic
puck input LOW.

The Arduino hardware adapter uses break-before-make when changing lock command:
after either motor relay is released, the opposite relay cannot energise for
250 ms. This dead time is retained across intermediate `None` commands.

The FAULT relay is active for any FSM fault and also after the second failed
lock attempt while the longer retry back-off continues. A later successful lock
clears this degraded indication.

## Timing

Current parameters:

```text
Standard auto-lock        5 s
Open Night period         2 h
door-open warning         30 s
maximum door-open time    5 min Standard / 2 h Open Night
lock attempt timeout      5 s
unlock attempt timeout    5 s
lock retry delays         1 s, 10 s, 100 s, 1000 s, 10000 s
maximum lock retries      5
unlock retry delay        1 s
maximum unlock retries    2
```

Retry counts are retries **after** the initial attempt. Locking now uses five
back-off retries; unlocking retains two 1 s retries.

Timing code uses unsigned `uint32_t` subtraction so it remains correct across
Arduino `millis()` rollover.

## Testing

Native host tests use Unity and run in GitHub Actions on Ubuntu.

The FSM test suite covers:

- normal transitions;
- explicit retry-wait states;
- manual lock/unlock;
- mode transitions;
- fault entry;
- retry exhaustion;
- fault recovery;
- representative end-to-end sequences;
- state/event safety checks.

The established FSM coverage is:

```text
lines       100%
functions   100%
branches    88.9% (combined FSM + controller measurement before review fixes)
```

The remaining uncovered branches are compiler-generated short-circuit paths
that correspond to unreachable/redundant state combinations. CI therefore
requires:

```text
lines       100%
functions   100%
branches    >= 88%
```

A separate controller/wrapper test suite exercises debounce, startup handling,
RFID boot qualification, timeout generation, stale-timer cancellation,
same-tick ordering and `millis()` rollover.

CI builds and runs the FSM and controller test executables before collecting
combined coverage, and separately compiles the complete sketch for a classic
Arduino Nano with pinned AVR-core and ETL versions.

Third-party Unity/ETL code and test sources are excluded from project coverage.

For controlled bench commissioning, defining `DOOR_SERIAL_DIAGNOSTICS=1` in the
sketch enables a compact 115200-baud status line once per second. It reports
time, numeric state/mode/fault/command values, fault indication and debounced
input levels. It is disabled by default; opening USB serial may reset the Nano
via DTR.

## Repository layout

The intended source layout is:

```text
door-access-control/
├── door-access-control.ino
├── src/
│   ├── controller_config.h
│   ├── controller_types.h
│   ├── debounce.h
│   ├── door_controller.cpp
│   ├── door_controller.h
│   ├── door_fsm.cpp
│   ├── door_fsm.h
│   └── pin_defs.h
├── test/
│   ├── test_all.cpp
│   ├── test_controller.cpp
│   └── test_helpers.h
├── docs/
├── reference/
├── hardware/
├── mechanical/
└── .github/
    └── workflows/
        └── tests.yml
```

There should be one canonical copy of each source file. The Arduino sketch
includes the files in `src/`; source files are not duplicated in a separate
Arduino directory.

## Dependencies

- Arduino Nano / ATmega328P
- Embedded Template Library (ETL), using `etl::state_chart`
- Unity test framework for native tests
- LCOV/GCOV for CI coverage

ETL is downloaded by the GitHub Actions test workflow. Unity is kept in the
repository under `third_party/unity`.

## Hardware still outside the firmware

The following are deliberately not part of the current door-control firmware:

- optional backup maglock;
- independent watchdog / monitoring system;
- email or remote fault notification;
- persistent fault history.

These should remain separate from the safety-critical FSM unless their
interfaces are explicitly defined and tested.
