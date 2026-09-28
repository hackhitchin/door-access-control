#include "door_fsm.h"

#include <stddef.h>

namespace
{
constexpr uint8_t MAX_LOCK_RETRIES   = 2;
constexpr uint8_t MAX_UNLOCK_RETRIES = 2;
}

// NOTE:
// Fault-causing transitions are deliberately NOT included in this prototype.
// See README.md. In particular:
//   - contradictory door/bolt sensor states
//   - invalid mode
//   - max-open timeout
//   - exhausted lock retries
//   - exhausted unlock retries
// are left for the fault-handling pass.

const DoorFsm::Transition DoorFsm::transitions_[] = {
    // ---------------------------------------------------------------------
    // LOCKED_CLOSED
    // ---------------------------------------------------------------------
    Transition(id(DoorState::LockedClosed),
               id(DoorEvent::RfidReleaseRequest),
               id(DoorState::Unlocking),
               nullptr,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::LockedClosed),
               id(DoorEvent::ExitButtonRequest),
               id(DoorState::Unlocking),
               nullptr,
               &DoorFsm::modeAllowsElectronicControl),

    // Manual unlock.
    Transition(id(DoorState::LockedClosed),
               id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer),

    Transition(id(DoorState::LockedClosed),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),

    // Selecting Open Night while locked immediately unlocks.
    Transition(id(DoorState::LockedClosed),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking)),

    // ---------------------------------------------------------------------
    // UNLOCKING
    // ---------------------------------------------------------------------
    Transition(id(DoorState::Unlocking),
               id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearUnlockRetriesAndRestartAutoLock),

    // Door opening aborts the active unlock command.
    Transition(id(DoorState::Unlocking),
               id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen),
               &DoorFsm::clearUnlockRetries),

    // Failed attempt -> output-off retry wait.
    Transition(id(DoorState::Unlocking),
               id(DoorEvent::UnlockTimeout),
               id(DoorState::UnlockRetryWait),
               &DoorFsm::countUnlockRetry,
               &DoorFsm::canRetryUnlock),

    Transition(id(DoorState::Unlocking),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled),
               &DoorFsm::clearUnlockRetries),

    // ---------------------------------------------------------------------
    // UNLOCK_RETRY_WAIT
    // ---------------------------------------------------------------------
    Transition(id(DoorState::UnlockRetryWait),
               id(DoorEvent::RetryDelayElapsed),
               id(DoorState::Unlocking)),

    // The mechanism may finish moving after T2 has been released.
    Transition(id(DoorState::UnlockRetryWait),
               id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearUnlockRetriesAndRestartAutoLock),

    Transition(id(DoorState::UnlockRetryWait),
               id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen),
               &DoorFsm::clearUnlockRetries),

    Transition(id(DoorState::UnlockRetryWait),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled),
               &DoorFsm::clearUnlockRetries),

    // ---------------------------------------------------------------------
    // UNLOCKED_CLOSED
    // ---------------------------------------------------------------------
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen)),

    // Manual lock.
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed),
               &DoorFsm::clearBothRetries),

    // Standard and Open Night both use an auto-lock timer; the timer source
    // chooses the duration appropriate to the current mode.
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::AutoLockTimeout),
               id(DoorState::Locking),
               nullptr,
               &DoorFsm::modeAllowsElectronicControl),

    // A new release request while already unlocked grants a fresh interval.
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::RfidReleaseRequest),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeIsStandard),

    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::ExitButtonRequest),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeIsStandard),

    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),

    // Entering Open Night while already unlocked restarts the auto-lock timer;
    // the timer source will use the Open Night duration.
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer),

    // Returning Open Night -> Standard gets a fresh Standard interval.
    Transition(id(DoorState::UnlockedClosed),
               id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer),

    // ---------------------------------------------------------------------
    // LOCKING
    // ---------------------------------------------------------------------
    Transition(id(DoorState::Locking),
               id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed),
               &DoorFsm::clearLockRetries),

    // Door opening aborts the active lock command.
    Transition(id(DoorState::Locking),
               id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen),
               &DoorFsm::clearLockRetries),

    // Release requests override an in-progress lock.
    Transition(id(DoorState::Locking),
               id(DoorEvent::RfidReleaseRequest),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::Locking),
               id(DoorEvent::ExitButtonRequest),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::Locking),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries),

    // Failed attempt -> output-off retry wait.
    Transition(id(DoorState::Locking),
               id(DoorEvent::LockTimeout),
               id(DoorState::LockRetryWait),
               &DoorFsm::countLockRetry,
               &DoorFsm::canRetryLock),

    Transition(id(DoorState::Locking),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled),
               &DoorFsm::clearLockRetries),

    // ---------------------------------------------------------------------
    // LOCK_RETRY_WAIT
    // ---------------------------------------------------------------------
    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::RetryDelayElapsed),
               id(DoorState::Locking)),

    // The mechanism may finish moving after T1 has been released.
    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed),
               &DoorFsm::clearLockRetries),

    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen),
               &DoorFsm::clearLockRetries),

    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::RfidReleaseRequest),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::ExitButtonRequest),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking),
               &DoorFsm::clearLockRetries),

    Transition(id(DoorState::LockRetryWait),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled),
               &DoorFsm::clearLockRetries),

    // ---------------------------------------------------------------------
    // UNLOCKED_OPEN
    // ---------------------------------------------------------------------
    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::DoorClosed),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeAllowsElectronicControl),

    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::DoorClosed),
               id(DoorState::Disabled),
               nullptr,
               &DoorFsm::modeIsDisabled),

    // 30 s warning only: no motor action and no state change.
    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::OpenTimeout),
               id(DoorState::UnlockedOpen),
               &DoorFsm::noteOpenTimeout),

    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),

    // Changing between Standard and Open Night while the door is open does not
    // cause motor actuation. observe() updates the mode before table dispatch.
    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedOpen)),

    Transition(id(DoorState::UnlockedOpen),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedOpen)),

    // ---------------------------------------------------------------------
    // DISABLED
    // ---------------------------------------------------------------------
    // Keep the physical snapshot current while disabled.
    Transition(id(DoorState::Disabled),
               id(DoorEvent::DoorOpened),
               id(DoorState::Disabled)),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::DoorClosed),
               id(DoorState::Disabled)),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::BoltLocked),
               id(DoorState::Disabled)),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::BoltUnlocked),
               id(DoorState::Disabled)),

    // Re-enable Standard: reconstruct from current physical state.
    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeStandard),
               id(DoorState::LockedClosed),
               nullptr,
               &DoorFsm::physicalLockedClosed),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeStandard),
               id(DoorState::Locking),
               nullptr,
               &DoorFsm::physicalUnlockedClosed),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedOpen),
               nullptr,
               &DoorFsm::physicalUnlockedOpen),

    // Re-enable Open Night: locked -> immediately unlock.
    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking),
               nullptr,
               &DoorFsm::physicalLockedClosed),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedClosed),
               &DoorFsm::restartAutoLockTimer,
               &DoorFsm::physicalUnlockedClosed),

    Transition(id(DoorState::Disabled),
               id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedOpen),
               nullptr,
               &DoorFsm::physicalUnlockedOpen)
};

uint8_t DoorFsm::id(DoorState state)
{
    return static_cast<uint8_t>(state);
}

uint8_t DoorFsm::id(DoorEvent event)
{
    return static_cast<uint8_t>(event);
}

DoorFsm::DoorFsm(DoorState initialState,
                 OperatingMode initialMode,
                 bool initialDoorClosed,
                 bool initialBoltLocked)
    : chart_(*this,
             transitions_,
             transitions_ + (sizeof(transitions_) / sizeof(transitions_[0])),
             nullptr,
             nullptr,
             id(initialState)),
      mode_(initialMode),
      doorClosed_(initialDoorClosed),
      boltLocked_(initialBoltLocked),
      lockRetries_(0),
      unlockRetries_(0),
      effect_(FsmEffect::None)
{
}

void DoorFsm::start()
{
    chart_.start();
}

void DoorFsm::process(DoorEvent event)
{
    effect_ = FsmEffect::None;

    // Keep the current physical/mode snapshot independent of whether the event
    // has a transition in the prototype table. This will also be useful when
    // fault transitions are added.
    observe(event);

    chart_.process_event(id(event));
}

DoorState DoorFsm::state() const
{
    return static_cast<DoorState>(chart_.get_state_id());
}

LockCommand DoorFsm::lockCommand() const
{
    // Invalid/Disabled mode never intentionally drives the motor.
    if (mode_ == OperatingMode::Disabled ||
        mode_ == OperatingMode::Invalid) {
        return LockCommand::None;
    }

    switch (state()) {
    case DoorState::Locking:
        return LockCommand::Lock;

    case DoorState::Unlocking:
        return LockCommand::Unlock;

    default:
        return LockCommand::None;
    }
}

void DoorFsm::observe(DoorEvent event)
{
    switch (event) {
    case DoorEvent::DoorOpened:
        doorClosed_ = false;
        break;

    case DoorEvent::DoorClosed:
        doorClosed_ = true;
        break;

    case DoorEvent::BoltLocked:
        boltLocked_ = true;
        break;

    case DoorEvent::BoltUnlocked:
        boltLocked_ = false;
        break;

    case DoorEvent::ModeDisabled:
        mode_ = OperatingMode::Disabled;
        break;

    case DoorEvent::ModeStandard:
        mode_ = OperatingMode::Standard;
        break;

    case DoorEvent::ModeOpenNight:
        mode_ = OperatingMode::OpenNight;
        break;

    case DoorEvent::ModeInvalid:
        mode_ = OperatingMode::Invalid;
        break;

    default:
        break;
    }
}

bool DoorFsm::modeAllowsElectronicControl() const
{
    return mode_ == OperatingMode::Standard ||
           mode_ == OperatingMode::OpenNight;
}

bool DoorFsm::modeIsStandard() const
{
    return mode_ == OperatingMode::Standard;
}

bool DoorFsm::modeIsOpenNight() const
{
    return mode_ == OperatingMode::OpenNight;
}

bool DoorFsm::modeIsDisabled() const
{
    return mode_ == OperatingMode::Disabled;
}

bool DoorFsm::canRetryLock() const
{
    return lockRetries_ < MAX_LOCK_RETRIES;
}

bool DoorFsm::canRetryUnlock() const
{
    return unlockRetries_ < MAX_UNLOCK_RETRIES;
}

bool DoorFsm::physicalLockedClosed() const
{
    return doorClosed_ && boltLocked_;
}

bool DoorFsm::physicalUnlockedClosed() const
{
    return doorClosed_ && !boltLocked_;
}

bool DoorFsm::physicalUnlockedOpen() const
{
    return !doorClosed_ && !boltLocked_;
}

void DoorFsm::clearLockRetries()
{
    lockRetries_ = 0;
}

void DoorFsm::clearUnlockRetries()
{
    unlockRetries_ = 0;
}

void DoorFsm::clearBothRetries()
{
    lockRetries_ = 0;
    unlockRetries_ = 0;
}

void DoorFsm::countLockRetry()
{
    ++lockRetries_;
}

void DoorFsm::countUnlockRetry()
{
    ++unlockRetries_;
}

void DoorFsm::restartAutoLockTimer()
{
    effect_ = FsmEffect::RestartAutoLockTimer;
}

void DoorFsm::noteOpenTimeout()
{
    effect_ = FsmEffect::OpenTimeoutWarning;
}

void DoorFsm::clearLockRetriesAndRestartAutoLock()
{
    clearLockRetries();
    restartAutoLockTimer();
}

void DoorFsm::clearUnlockRetriesAndRestartAutoLock()
{
    clearUnlockRetries();
    restartAutoLockTimer();
}
