#pragma once
#include <stdint.h>

enum class DoorState : uint8_t {
    LockedClosed = 0,
    Unlocking,
    UnlockRetryWait,
    UnlockedClosed,
    Locking,
    LockRetryWait,
    UnlockedOpen,
    Disabled,

    // Reserved for the later fault-handling pass.
    Error
};

enum class DoorEvent : uint8_t {
    RfidReleaseRequest = 0,
    ExitButtonRequest,

    DoorOpened,
    DoorClosed,

    BoltLocked,
    BoltUnlocked,

    ModeDisabled,
    ModeStandard,
    ModeOpenNight,
    ModeInvalid,

    AutoLockTimeout,
    OpenTimeout,
    MaxOpenTimeout,

    LockTimeout,
    UnlockTimeout,
    RetryDelayElapsed
};

enum class OperatingMode : uint8_t {
    Disabled = 0,
    Standard,
    OpenNight,
    Invalid
};

enum class LockCommand : uint8_t {
    None = 0,
    Lock,
    Unlock
};

enum class FsmEffect : uint8_t {
    None = 0,
    RestartAutoLockTimer,
    OpenTimeoutWarning
};
