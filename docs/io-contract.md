# Door Controller I/O Contract

Status: Draft v1  
Scope: Arduino Nano motorised-door controller  
Project: Hitchin Hackspace door access system

## 1. Purpose

This document defines the logical interface between the door controller firmware
and the surrounding hardware.

The controller does **not** authenticate RFID cards or make membership/access
decisions.

Credential verification is performed by the existing ESP-RFID system. The door
controller receives an unlock/release request from that system and controls the
motorised Utopic lock so that, as far as practical, the door behaves like a
conventional access-controlled maglock door.

The controller also monitors the physical state of:

- the door;
- the lock bolt;
- the internal exit button;
- the operating-mode key switch.

An optional backup maglock may be added later as a failure-safety device. It is
outside the scope of the initial implementation and initial test suite.

---

# 2. Design principles

The controller shall distinguish between:

1. physical inputs;
2. derived software events;
3. controller state;
4. physical outputs.

The finite state machine shall operate on logical signals/events and shall not
depend directly on Arduino GPIO numbers, electrical polarity, relay wiring, or
other hardware implementation details.

Hardware-specific code shall translate between electrical signals and the
logical interface described here.

---

# 3. Physical inputs

## 3.1 Door closed sensor

Logical name:

    door_closed

Type:

    Boolean level

Meaning:

    true  = door is physically in the closed position
    false = door is not in the closed position

Hardware:

    Magnetic reed switch.

The signal shall be debounced or filtered before being presented to the state
machine.

The electrical polarity at the Arduino input is a hardware-layer detail and
does not affect the logical definition above.

---

## 3.2 Bolt locked sensor

Logical name:

    bolt_locked

Type:

    Boolean level

Meaning:

    true  = mechanical lock bolt is detected in the locked position
    false = lock bolt is not detected in the locked position

Hardware:

    PR12-4DN inductive proximity sensor.

The sensor has an NPN output. Electrical conditioning and signal polarity are
hardware-layer details.

The signal shall be filtered/debounced as necessary before being presented to
the state machine.

The sensor indicates the physical bolt position; it does not indicate whether
the position was reached manually or electronically.

---

## 3.3 RFID release request

Logical name:

    rfid_release

Type:

    Boolean level at the hardware interface.

Source:

    ESP-RFID "Blue Board" access-control system.

Meaning:

    active   = the external access-control system is requesting door release
    inactive = there is currently no RFID/access-system release request

The ESP-RFID system is responsible for credential verification.

The Nano shall not interpret card identifiers or perform membership lookup.

### RFID startup behaviour

An RFID release signal which is already active when the Nano starts shall NOT
be treated as a new access request.

At startup:

1. sample the RFID release input;
2. if it is inactive, arm RFID request detection immediately;
3. if it is active, ignore it;
4. remain disarmed until the signal returns inactive;
5. after it has returned inactive, arm the input;
6. the next inactive-to-active transition is a valid RFID release request.

Conceptually:

    boot
      |
      +-- RFID inactive --> armed
      |
      +-- RFID active ----> wait for inactive --> armed

This prevents the remaining portion of an ESP-RFID timed output pulse from
being interpreted as a new access event following a controller restart.

---

## 3.4 Internal exit button

Logical name:

    exit_button

Type:

    Boolean level

Meaning:

    active   = internal exit button is being pressed
    inactive = internal exit button is not being pressed

The exit button is electrically separate from the RFID release request and must
remain logically separate in the firmware.

Unlike the RFID release input, an exit button which is already pressed during
controller startup SHALL be honoured.

Therefore:

    boot + exit_button active

may initiate an unlock operation immediately, subject to normal state-machine
safety rules.

The exit button is intentionally given different startup semantics from the
RFID release input.

---

# 4. Operating mode inputs

The controller shall support three operator-selected modes:

    Disabled
    Standard
    OpenNight

The intended hardware is a three-position key switch.

The current design assumption is that each position will have its own Nano
input:

    mode_disabled
    mode_standard
    mode_open_night

Exactly one mode input shall normally be active.

The logical representation used by the state machine shall be:

    enum class OperatingMode {
        Disabled,
        Standard,
        OpenNight,
        Invalid
    };

The hardware layer shall convert the three input lines into this enumeration.

## 4.1 Valid combinations

    Disabled     Standard     OpenNight       Result
    ------------------------------------------------
    active       inactive     inactive        Disabled
    inactive     active       inactive        Standard
    inactive     inactive     active          OpenNight

Any other combination shall produce:

    OperatingMode::Invalid

Examples include:

    no input active
    two inputs active
    all three inputs active

These conditions may indicate:

- disconnected wiring;
- shorted wiring;
- switch failure;
- incorrect installation.

The final electrical implementation of the key switch is TBD.

---

# 5. Physical outputs

## 5.1 Lock command

Logical command:

    Lock

Hardware action:

    energise the appropriate PCB relay so that T1 on the Utopic 433 MHz
    interface is held LOW.

The command shall remain asserted for the entire LOCKING state.

It shall cease when:

- the bolt sensor confirms that the bolt is locked; or
- the locking attempt times out; or
- the FSM otherwise leaves the LOCKING state.

The Utopic lock controller itself is responsible for stopping its motor at the
end of travel.

The Nano does not directly control motor current or motor end stops.

---

## 5.2 Unlock command

Logical command:

    Unlock

Hardware action:

    energise the appropriate PCB relay so that T2 on the Utopic 433 MHz
    interface is held LOW.

The command shall remain asserted for the entire UNLOCKING state.

It shall cease when:

- the bolt sensor confirms that the bolt is unlocked; or
- the unlocking attempt times out; or
- the FSM otherwise leaves the UNLOCKING state.

The Utopic lock controller itself is responsible for stopping its motor at the
end of travel.

---

## 5.3 No command

Logical command:

    None

Meaning:

    neither the T1 nor T2 relay is commanded.

The controller output shall be represented internally as one mutually-exclusive
value:

    enum class LockCommand {
        None,
        Lock,
        Unlock
    };

It shall therefore be impossible for the FSM to request LOCK and UNLOCK
simultaneously.

---

# 6. Relationship between states and lock outputs

The normal mapping is:

    FSM state             Lock command
    -----------------------------------
    LOCKING               Lock
    UNLOCKING             Unlock
    all other states      None

The command is therefore predominantly a property of the current transient
state rather than a momentary action generated on state entry.

This rule may later be extended if a specific fault-recovery procedure requires
otherwise.

---

# 7. Optional backup maglock

A backup maglock has been considered as a possible safety/failure-response
device.

It is not required for the first implementation.

The initial FSM and test suite shall therefore not require a maglock output.

Provision may be left in the architecture for a future output such as:

    backup_maglock

but its behaviour is explicitly outside the scope of v1.

---

# 8. Derived events

The hardware/input layer may convert physical signal changes into events for
the FSM.

Expected derived events include:

    DOOR_OPENED
    DOOR_CLOSED

    BOLT_LOCKED
    BOLT_UNLOCKED

    RFID_RELEASE_REQUEST
    EXIT_BUTTON_REQUEST

    MODE_DISABLED
    MODE_STANDARD
    MODE_OPEN_NIGHT
    MODE_INVALID

and timer-generated events such as:

    LOCK_TIMEOUT
    UNLOCK_TIMEOUT
    AUTO_LOCK_TIMEOUT
    OPEN_TIMEOUT
    MAX_OPEN_TIMEOUT

The exact event representation will be defined with the FSM transition table.

---

# 9. Manual operation

Manual operation of the physical lock is permitted.

A change in the bolt sensor must therefore not automatically be considered a
fault merely because no electronic lock/unlock command was being issued.

For example:

    LOCKED_CLOSED
        bolt_locked changes true -> false

may represent:

    manual unlock

and can result in transition to:

    UNLOCKED_CLOSED

Likewise:

    UNLOCKED_CLOSED
        bolt_locked changes false -> true

may represent:

    manual lock

and can result in transition to:

    LOCKED_CLOSED

Whether a particular manually-created state requires subsequent corrective
action is defined by the FSM, not by the input layer.

---

# 10. Sensor consistency

The following sensor combination is intrinsically suspicious:

    door_closed = false
    bolt_locked = true

The bolt position is only reliably meaningful when the door is aligned with the
frame.

This combination therefore indicates that:

- one sensor may be faulty;
- wiring may be faulty;
- a mechanically abnormal condition may exist.

The FSM shall treat this combination as a fault condition rather than blindly
attempting corrective lock movement.

Detailed fault behaviour will be defined separately.

---

# 11. Startup behaviour

On startup the controller shall first establish the physical condition of the
door rather than assume that the pre-reset FSM state is known.

Transient states such as:

    LOCKING
    UNLOCKING

cannot be reconstructed following a reset because the controller no longer
knows whether a command had previously been in progress.

The startup sequence shall therefore be:

1. initialise outputs to `LockCommand::None`;
2. initialise GPIO and input conditioning;
3. read the operating-mode inputs;
4. establish stable door and bolt sensor values;
5. initialise RFID request arming state;
6. inspect the exit-button state;
7. reconstruct the appropriate stable physical/FSM state;
8. apply normal FSM policy from that state.

No T1 or T2 command shall be asserted merely because the controller has booted.

---

# 12. Startup state reconstruction

The basic physical states are reconstructed from:

    door_closed
    bolt_locked

as follows:

    door_closed    bolt_locked     reconstructed condition
    ------------------------------------------------------
    true           true            LOCKED_CLOSED
    true           false           UNLOCKED_CLOSED
    false          false           UNLOCKED_OPEN
    false          true            sensor/physical fault

The operating mode is then applied.

---

# 13. Startup policy

The general design principle is:

> After reconstructing its physical state, the controller should behave as
> though it had already been running in that state.

Startup should not normally introduce special state transitions.

Known exceptions are listed below.

## 13.1 RFID release active at boot

An RFID request already active at startup shall be ignored until the signal
first becomes inactive.

It must not initiate an electronic unlock.

This is deliberately different from normal running behaviour.

## 13.2 Exit button active at boot

An exit button which is physically pressed during startup shall be honoured.

If the door is locked and normal safety conditions permit unlocking, the
controller may enter UNLOCKING.

## 13.3 Closed and unlocked at boot

If startup finds:

    door_closed = true
    bolt_locked = false
    mode = Standard
    no valid unlock request

the controller shall attempt to lock immediately.

It shall not wait for the normal `auto_lock_time`.

This is deliberately different from entering UNLOCKED_CLOSED during normal
operation.

## 13.4 Door open at boot

If startup finds:

    door_closed = false
    bolt_locked = false

the controller shall reconstruct:

    UNLOCKED_OPEN

and shall not attempt to lock while the door remains open.

Normal policy resumes after the door closes.

## 13.5 Contradictory sensors at boot

If startup finds:

    door_closed = false
    bolt_locked = true

the controller shall enter the appropriate fault handling.

It shall not blindly command the lock motor.

---

# 14. Disabled mode

`Disabled` is an explicit operator-selected mode.

The exact transition behaviour on:

    entering Disabled
    leaving Disabled
    changing mode while the door is open
    changing mode while locked/unlocked

will be defined in the state-transition specification.

The input contract only guarantees that the FSM receives an unambiguous
`OperatingMode`.

---

# 15. Standard mode

`Standard` is the normal access-controlled operating mode.

Detailed values such as:

    auto_lock_time
    open_timeout
    lock_time
    unlock_time
    max_lock_attempts
    max_unlock_attempts

are controller configuration values and are not part of the physical I/O
contract.

---

# 16. Open Night mode

`OpenNight` permits the door to remain unlocked for substantially longer than
normal Standard operation.

The exact timing and state-transition behaviour will be defined by the FSM
specification.

Selection is via the three-position operator key switch.

---

# 17. Diagnostics

Diagnostics are separate from primary door control.

The controller should make internal status available in a form equivalent to:

    struct ControllerStatus {
        State state;
        FaultCode fault;
        uint8_t lock_attempts;
        uint8_t unlock_attempts;
    };

Possible consumers include:

- local LEDs;
- serial diagnostics;
- an independent watchdog;
- an external monitoring/email system.

A failure in the diagnostic system shall not be required for correct normal
door-control operation.

The exact diagnostic hardware interface is outside the scope of this document.

---

# 18. Hardware abstraction boundary

The state-machine/controller layer shall not directly depend on:

    digitalRead()
    digitalWrite()
    Arduino pin numbers
    active-high/active-low GPIO details
    relay wiring
    sensor electrical interface details

A hardware-specific layer shall convert between those details and logical
values approximately equivalent to:

    struct Inputs {
        bool doorClosed;
        bool boltLocked;

        bool rfidRelease;
        bool exitButton;

        OperatingMode mode;
    };

and:

    struct Outputs {
        LockCommand lockCommand;
    };

This permits the controller logic to be compiled and tested on a host computer
without Arduino hardware.

---

# 19. Items deliberately left for later specifications

The following are not required to complete this I/O contract:

- complete FSM transition table;
- exact timeout values;
- retry counts;
- complete fault catalogue;
- local LED behaviour;
- watchdog/email behaviour;
- backup-maglock behaviour;
- Arduino GPIO-number assignment;
- electrical active-high/active-low definitions;
- key-switch contact wiring;
- test implementation.

These belong in subsequent design documents.

---

# 20. Outstanding hardware details

The following should be recorded when confirmed:

- Nano pin used for each physical input;
- Nano pin used for each relay output;
- electrical polarity of each Nano input;
- required input pull-ups/pull-downs;
- exact relay polarity;
- exact three-position key-switch wiring;
- input debounce/filter values;
- actual ESP-RFID relay pulse duration configured on the installed Blue Board.

None of these alter the logical I/O definitions in this document.