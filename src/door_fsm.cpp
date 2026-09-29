#include "door_fsm.h"

const DoorFsm::Transition DoorFsm::transitions_[] = {
    // LOCKED_CLOSED
    Transition(id(DoorState::LockedClosed), id(DoorEvent::RfidReleaseRequest),
               id(DoorState::Unlocking), nullptr,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::ExitButtonRequest),
               id(DoorState::Unlocking), nullptr,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::DoorOpened),
               id(DoorState::Error), &DoorFsm::faultDoorOpenBoltLocked),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking)),
    Transition(id(DoorState::LockedClosed), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // UNLOCKING
    Transition(id(DoorState::Unlocking), id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearUnlockRetriesAndRestartAutoLock),
    Transition(id(DoorState::Unlocking), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen), &DoorFsm::clearUnlockRetries),
    Transition(id(DoorState::Unlocking), id(DoorEvent::UnlockTimeout),
               id(DoorState::UnlockRetryWait), &DoorFsm::countUnlockRetry,
               &DoorFsm::canRetryUnlock),
    Transition(id(DoorState::Unlocking), id(DoorEvent::UnlockTimeout),
               id(DoorState::Error), &DoorFsm::faultUnlockFailed),
    Transition(id(DoorState::Unlocking), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled), &DoorFsm::clearUnlockRetries),
    Transition(id(DoorState::Unlocking), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // UNLOCK_RETRY_WAIT
    Transition(id(DoorState::UnlockRetryWait), id(DoorEvent::RetryDelayElapsed),
               id(DoorState::Unlocking)),
    Transition(id(DoorState::UnlockRetryWait), id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearUnlockRetriesAndRestartAutoLock),
    Transition(id(DoorState::UnlockRetryWait), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen), &DoorFsm::clearUnlockRetries),
    Transition(id(DoorState::UnlockRetryWait), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled), &DoorFsm::clearUnlockRetries),
    Transition(id(DoorState::UnlockRetryWait), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // UNLOCKED_CLOSED
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen)),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed), &DoorFsm::clearBothRetries),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::AutoLockTimeout),
               id(DoorState::Locking), nullptr, &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::RfidReleaseRequest),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeIsStandard),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::ExitButtonRequest),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeIsStandard),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer),
    Transition(id(DoorState::UnlockedClosed), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // LOCKING
    // Release requests are queued until the current lock command has either
    // completed or timed out. This avoids acknowledging release while a
    // one-shot lock command may still be completing mechanically.
    Transition(id(DoorState::Locking), id(DoorEvent::BoltLocked),
               id(DoorState::Unlocking), &DoorFsm::clearLockRetries,
               &DoorFsm::releasePending),
    Transition(id(DoorState::Locking), id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::Locking), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::Locking), id(DoorEvent::RfidReleaseRequest),
               id(DoorState::Locking), &DoorFsm::setReleasePending,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::Locking), id(DoorEvent::ExitButtonRequest),
               id(DoorState::Locking), &DoorFsm::setReleasePending,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::Locking), id(DoorEvent::ModeOpenNight),
               id(DoorState::Locking), &DoorFsm::setReleasePending),
    Transition(id(DoorState::Locking), id(DoorEvent::LockTimeout),
               id(DoorState::Unlocking), &DoorFsm::clearLockRetries,
               &DoorFsm::releasePending),
    Transition(id(DoorState::Locking), id(DoorEvent::LockTimeout),
               id(DoorState::LockRetryWait), &DoorFsm::countLockRetry,
               &DoorFsm::canRetryLock),
    Transition(id(DoorState::Locking), id(DoorEvent::LockTimeout),
               id(DoorState::Error), &DoorFsm::faultLockFailed),
    Transition(id(DoorState::Locking), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::Locking), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // LOCK_RETRY_WAIT
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::RetryDelayElapsed),
               id(DoorState::Unlocking), &DoorFsm::clearLockRetries,
               &DoorFsm::releasePending),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::RetryDelayElapsed),
               id(DoorState::Locking)),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::BoltLocked),
               id(DoorState::Unlocking), &DoorFsm::clearLockRetries,
               &DoorFsm::releasePending),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::RfidReleaseRequest),
               id(DoorState::LockRetryWait), &DoorFsm::setReleasePending,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::ExitButtonRequest),
               id(DoorState::LockRetryWait), &DoorFsm::setReleasePending,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::ModeOpenNight),
               id(DoorState::LockRetryWait), &DoorFsm::setReleasePending),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled), &DoorFsm::clearLockRetries),
    Transition(id(DoorState::LockRetryWait), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // UNLOCKED_OPEN
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::DoorClosed),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer,
               &DoorFsm::modeAllowsElectronicControl),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::DoorClosed),
               id(DoorState::Disabled), nullptr, &DoorFsm::modeIsDisabled),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::BoltLocked),
               id(DoorState::Error), &DoorFsm::faultDoorOpenBoltLocked),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::OpenTimeout),
               id(DoorState::UnlockedOpen), &DoorFsm::noteOpenTimeout),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::MaxOpenTimeout),
               id(DoorState::Error), &DoorFsm::faultDoorOpenTooLong),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled)),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedOpen)),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedOpen)),
    Transition(id(DoorState::UnlockedOpen), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // DISABLED
    Transition(id(DoorState::Disabled), id(DoorEvent::DoorOpened),
               id(DoorState::Disabled)),
    Transition(id(DoorState::Disabled), id(DoorEvent::DoorClosed),
               id(DoorState::Disabled)),
    Transition(id(DoorState::Disabled), id(DoorEvent::BoltLocked),
               id(DoorState::Disabled)),
    Transition(id(DoorState::Disabled), id(DoorEvent::BoltUnlocked),
               id(DoorState::Disabled)),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeStandard),
               id(DoorState::Error), &DoorFsm::faultDoorOpenBoltLocked,
               &DoorFsm::physicalOpenBoltLocked),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeStandard),
               id(DoorState::LockedClosed), nullptr, &DoorFsm::physicalLockedClosed),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeStandard),
               id(DoorState::Locking), nullptr, &DoorFsm::physicalUnlockedClosed),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedOpen), nullptr, &DoorFsm::physicalUnlockedOpen),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeOpenNight),
               id(DoorState::Error), &DoorFsm::faultDoorOpenBoltLocked,
               &DoorFsm::physicalOpenBoltLocked),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking), nullptr, &DoorFsm::physicalLockedClosed),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedClosed), &DoorFsm::restartAutoLockTimer,
               &DoorFsm::physicalUnlockedClosed),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedOpen), nullptr, &DoorFsm::physicalUnlockedOpen),
    Transition(id(DoorState::Disabled), id(DoorEvent::ModeInvalid),
               id(DoorState::Error), &DoorFsm::faultInvalidMode),

    // ERROR: auto-clear / reconstruction
    Transition(id(DoorState::Error), id(DoorEvent::ModeDisabled),
               id(DoorState::Disabled), &DoorFsm::clearFaultAndRetries),

    Transition(id(DoorState::Error), id(DoorEvent::ModeStandard),
               id(DoorState::LockedClosed), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverStandardLocked),
    Transition(id(DoorState::Error), id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearFaultRetriesAndRestartAutoLock,
               &DoorFsm::recoverEnabledUnlockedClosed),
    Transition(id(DoorState::Error), id(DoorEvent::ModeStandard),
               id(DoorState::UnlockedOpen), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverEnabledUnlockedOpen),

    Transition(id(DoorState::Error), id(DoorEvent::ModeOpenNight),
               id(DoorState::Unlocking), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverOpenNightLocked),
    Transition(id(DoorState::Error), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearFaultRetriesAndRestartAutoLock,
               &DoorFsm::recoverEnabledUnlockedClosed),
    Transition(id(DoorState::Error), id(DoorEvent::ModeOpenNight),
               id(DoorState::UnlockedOpen), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverEnabledUnlockedOpen),

    Transition(id(DoorState::Error), id(DoorEvent::DoorClosed),
               id(DoorState::LockedClosed), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverStandardLocked),
    Transition(id(DoorState::Error), id(DoorEvent::DoorClosed),
               id(DoorState::Unlocking), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverOpenNightLocked),
    Transition(id(DoorState::Error), id(DoorEvent::DoorClosed),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearFaultRetriesAndRestartAutoLock,
               &DoorFsm::recoverEnabledUnlockedClosed),

    Transition(id(DoorState::Error), id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedClosed),
               &DoorFsm::clearFaultRetriesAndRestartAutoLock,
               &DoorFsm::recoverEnabledUnlockedClosed),
    Transition(id(DoorState::Error), id(DoorEvent::BoltUnlocked),
               id(DoorState::UnlockedOpen), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverEnabledUnlockedOpen),

    Transition(id(DoorState::Error), id(DoorEvent::BoltLocked),
               id(DoorState::LockedClosed), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverStandardLocked),
    Transition(id(DoorState::Error), id(DoorEvent::BoltLocked),
               id(DoorState::Unlocking), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverOpenNightLocked),

    Transition(id(DoorState::Error), id(DoorEvent::DoorOpened),
               id(DoorState::UnlockedOpen), &DoorFsm::clearFaultAndRetries,
               &DoorFsm::recoverEnabledUnlockedOpen)
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
      releasePending_(false),
      lockRetries_(0),
      unlockRetries_(0),
      effect_(FsmEffect::None),
      fault_(FaultCode::None)
{
}

void DoorFsm::start()
{
    chart_.start();
}

void DoorFsm::process(DoorEvent event)
{
    effect_ = FsmEffect::None;
    observe(event);

    if (state() == DoorState::Error) {
        refreshObservableFault();
    }

    chart_.process_event(id(event));
}

DoorState DoorFsm::state() const
{
    return static_cast<DoorState>(chart_.get_state_id());
}

LockCommand DoorFsm::lockCommand() const
{
    if (mode_ == OperatingMode::Disabled ||
        mode_ == OperatingMode::Invalid ||
        state() == DoorState::Error) {
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
    case DoorEvent::DoorOpened:   doorClosed_ = false; break;
    case DoorEvent::DoorClosed:   doorClosed_ = true;  break;
    case DoorEvent::BoltLocked:   boltLocked_ = true;  break;
    case DoorEvent::BoltUnlocked: boltLocked_ = false; break;

    case DoorEvent::ModeDisabled:  mode_ = OperatingMode::Disabled;  break;
    case DoorEvent::ModeStandard:  mode_ = OperatingMode::Standard;  break;
    case DoorEvent::ModeOpenNight: mode_ = OperatingMode::OpenNight; break;
    case DoorEvent::ModeInvalid:   mode_ = OperatingMode::Invalid;   break;

    default:
        break;
    }
}

void DoorFsm::refreshObservableFault()
{
    // Observable present-tense faults take priority over historical operation
    // failures while Error is active. Historical LockFailed/UnlockFailed is
    // retained when neither of these present conditions exists.
    if (!doorClosed_ && boltLocked_) {
        fault_ = FaultCode::DoorOpenBoltLocked;
    } else if (mode_ == OperatingMode::Invalid) {
        fault_ = FaultCode::InvalidMode;
    }
}

bool DoorFsm::modeAllowsElectronicControl()
{
    return mode_ == OperatingMode::Standard ||
           mode_ == OperatingMode::OpenNight;
}

bool DoorFsm::modeIsStandard()
{
    return mode_ == OperatingMode::Standard;
}

bool DoorFsm::modeIsDisabled()
{
    return mode_ == OperatingMode::Disabled;
}

bool DoorFsm::canRetryLock()
{
    return lockRetries_ < MAX_LOCK_RETRIES;
}

bool DoorFsm::canRetryUnlock()
{
    return unlockRetries_ < MAX_UNLOCK_RETRIES;
}

bool DoorFsm::releasePending()
{
    return releasePending_;
}

bool DoorFsm::physicalOpenBoltLocked()
{
    return !doorClosed_ && boltLocked_;
}

bool DoorFsm::physicalLockedClosed()
{
    return doorClosed_ && boltLocked_;
}

bool DoorFsm::physicalUnlockedClosed()
{
    return doorClosed_ && !boltLocked_;
}

bool DoorFsm::physicalUnlockedOpen()
{
    return !doorClosed_ && !boltLocked_;
}

bool DoorFsm::recoverStandardLocked()
{
    return mode_ == OperatingMode::Standard && physicalLockedClosed();
}

bool DoorFsm::recoverOpenNightLocked()
{
    return mode_ == OperatingMode::OpenNight && physicalLockedClosed();
}

bool DoorFsm::recoverEnabledUnlockedClosed()
{
    return modeAllowsElectronicControl() && physicalUnlockedClosed();
}

bool DoorFsm::recoverEnabledUnlockedOpen()
{
    return modeAllowsElectronicControl() && physicalUnlockedOpen();
}

void DoorFsm::clearLockRetries()
{
    lockRetries_ = 0;
    releasePending_ = false;
}

void DoorFsm::clearUnlockRetries()
{
    unlockRetries_ = 0;
}

void DoorFsm::clearBothRetries()
{
    lockRetries_ = 0;
    unlockRetries_ = 0;
    releasePending_ = false;
}

void DoorFsm::countLockRetry()
{
    ++lockRetries_;
}

void DoorFsm::countUnlockRetry()
{
    ++unlockRetries_;
}

void DoorFsm::setReleasePending()
{
    releasePending_ = true;
}

void DoorFsm::restartAutoLockTimer()
{
    effect_ = FsmEffect::RestartAutoLockTimer;
}

void DoorFsm::noteOpenTimeout()
{
    effect_ = FsmEffect::OpenTimeoutWarning;
}

void DoorFsm::clearUnlockRetriesAndRestartAutoLock()
{
    clearUnlockRetries();
    restartAutoLockTimer();
}

void DoorFsm::faultDoorOpenBoltLocked()
{
    fault_ = FaultCode::DoorOpenBoltLocked;
}

void DoorFsm::faultLockFailed()
{
    fault_ = FaultCode::LockFailed;
}

void DoorFsm::faultUnlockFailed()
{
    fault_ = FaultCode::UnlockFailed;
}

void DoorFsm::faultDoorOpenTooLong()
{
    fault_ = FaultCode::DoorOpenTooLong;
}

void DoorFsm::faultInvalidMode()
{
    fault_ = FaultCode::InvalidMode;
}

void DoorFsm::clearFaultAndRetries()
{
    fault_ = FaultCode::None;
    clearBothRetries();
}

void DoorFsm::clearFaultRetriesAndRestartAutoLock()
{
    clearFaultAndRetries();
    restartAutoLockTimer();
}
