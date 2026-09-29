#include "door_controller.h"

DoorState DoorController::initialStateFor(const ControllerInputs& inputs)
{
    // Contradictory open + locked is pushed through the existing
    // DoorOpenBoltLocked fault path immediately after fsm_.start().
    if (!inputs.doorClosed && inputs.boltLocked) {
        return DoorState::LockedClosed;
    }

    if (inputs.mode == OperatingMode::Disabled) {
        return DoorState::Disabled;
    }

    if (inputs.doorClosed && inputs.boltLocked) {
        return (inputs.mode == OperatingMode::OpenNight)
                   ? DoorState::Unlocking
                   : DoorState::LockedClosed;
    }

    if (inputs.doorClosed && !inputs.boltLocked) {
        return (inputs.mode == OperatingMode::Standard)
                   ? DoorState::Locking
                   : DoorState::UnlockedClosed;
    }

    return DoorState::UnlockedOpen;
}

bool DoorController::electronicControlEnabled(OperatingMode mode)
{
    return mode == OperatingMode::Standard ||
           mode == OperatingMode::OpenNight;
}

DoorController::DoorController(const ControllerInputs& initialInputs,
                               uint32_t nowMs)
    : fsm_(initialStateFor(initialInputs),
           initialInputs.mode,
           initialInputs.doorClosed,
           initialInputs.boltLocked),
      door_(initialInputs.doorClosed, nowMs, DEBOUNCE_DOOR_MS),
      bolt_(initialInputs.boltLocked, nowMs, DEBOUNCE_BOLT_MS),
      rfid_(initialInputs.rfidActive, nowMs, DEBOUNCE_RFID_MS),
      exit_(initialInputs.exitPressed, nowMs, DEBOUNCE_EXIT_MS),
      mode_(initialInputs.mode, nowMs, DEBOUNCE_MODE_MS),
      rfidArmed_(!initialInputs.rfidActive)
{
    fsm_.start();

    // Startup fault priority: contradictory physical sensors before mode fault.
    if (!initialInputs.doorClosed && initialInputs.boltLocked) {
        dispatch(DoorEvent::DoorOpened, nowMs);
    } else if (initialInputs.mode == OperatingMode::Invalid) {
        dispatch(DoorEvent::ModeInvalid, nowMs);
    } else {
        initialiseTimers(nowMs);
    }

    // A held exit button at boot is a valid release request.
    // RFID active at boot is intentionally ignored until it first goes inactive.
    if (initialInputs.exitPressed &&
        electronicControlEnabled(initialInputs.mode) &&
        fsm_.state() != DoorState::Error) {
        dispatch(DoorEvent::ExitButtonRequest, nowMs);
    }
}

void DoorController::tick(uint32_t nowMs, const ControllerInputs& rawInputs)
{
    // Physical changes are handled before timers. If a sensor changes at the
    // same sampled instant that a timer expires, the physical state wins.
    processDoorInput(nowMs, rawInputs.doorClosed);
    processBoltInput(nowMs, rawInputs.boltLocked);
    processModeInput(nowMs, rawInputs.mode);
    processExitInput(nowMs, rawInputs.exitPressed);
    processRfidInput(nowMs, rawInputs.rfidActive);

    processTimers(nowMs);
}

void DoorController::processDoorInput(uint32_t nowMs, bool rawValue)
{
    if (door_.update(rawValue, nowMs)) {
        dispatch(door_.value() ? DoorEvent::DoorClosed
                               : DoorEvent::DoorOpened,
                 nowMs);
    }
}

void DoorController::processBoltInput(uint32_t nowMs, bool rawValue)
{
    if (bolt_.update(rawValue, nowMs)) {
        dispatch(bolt_.value() ? DoorEvent::BoltLocked
                               : DoorEvent::BoltUnlocked,
                 nowMs);
    }
}

void DoorController::processModeInput(uint32_t nowMs, OperatingMode rawValue)
{
    if (!mode_.update(rawValue, nowMs)) {
        return;
    }

    switch (mode_.value()) {
    case OperatingMode::Disabled:
        dispatch(DoorEvent::ModeDisabled, nowMs);
        break;
    case OperatingMode::Standard:
        dispatch(DoorEvent::ModeStandard, nowMs);
        break;
    case OperatingMode::OpenNight:
        dispatch(DoorEvent::ModeOpenNight, nowMs);
        break;
    case OperatingMode::Invalid:
        dispatch(DoorEvent::ModeInvalid, nowMs);
        break;
    }
}

void DoorController::processExitInput(uint32_t nowMs, bool rawValue)
{
    if (exit_.update(rawValue, nowMs) && exit_.value()) {
        dispatch(DoorEvent::ExitButtonRequest, nowMs);
    }
}

void DoorController::processRfidInput(uint32_t nowMs, bool rawValue)
{
    if (!rfid_.update(rawValue, nowMs)) {
        return;
    }

    if (!rfid_.value()) {
        rfidArmed_ = true;
        return;
    }

    if (rfidArmed_) {
        dispatch(DoorEvent::RfidReleaseRequest, nowMs);
    }
}

void DoorController::dispatch(DoorEvent event, uint32_t nowMs)
{
    const DoorState previousState = fsm_.state();
    fsm_.process(event);
    afterFsmEvent(previousState, nowMs);
}

void DoorController::afterFsmEvent(DoorState previousState, uint32_t nowMs)
{
    const DoorState newState = fsm_.state();

    if (newState != previousState) {
        if (newState == DoorState::Locking) {
            operationTimer_.start(nowMs, LOCK_TIME_MS);
        } else if (newState == DoorState::Unlocking) {
            operationTimer_.start(nowMs, UNLOCK_TIME_MS);
        } else {
            operationTimer_.stop();
        }

        if (newState == DoorState::LockRetryWait ||
            newState == DoorState::UnlockRetryWait) {
            retryTimer_.start(nowMs, RETRY_DELAY_MS);
        } else {
            retryTimer_.stop();
        }

        if (newState == DoorState::UnlockedOpen) {
            openWarningTimer_.start(nowMs, OPEN_TIMEOUT_MS);
            maxOpenTimer_.start(nowMs, MAX_OPEN_TIMEOUT_MS);
        } else {
            openWarningTimer_.stop();
            maxOpenTimer_.stop();
        }

        if (newState != DoorState::UnlockedClosed) {
            autoLockTimer_.stop();
        }

        if (newState == DoorState::Error ||
            newState == DoorState::Disabled) {
            operationTimer_.stop();
            retryTimer_.stop();
        }
    }

    if (fsm_.effect() == FsmEffect::RestartAutoLockTimer &&
        newState == DoorState::UnlockedClosed) {
        autoLockTimer_.start(nowMs, autoLockDurationForMode());
    }
}

void DoorController::initialiseTimers(uint32_t nowMs)
{
    const DoorState state = fsm_.state();

    if (state == DoorState::Locking) {
        operationTimer_.start(nowMs, LOCK_TIME_MS);
    } else if (state == DoorState::Unlocking) {
        operationTimer_.start(nowMs, UNLOCK_TIME_MS);
    }

    if (state == DoorState::UnlockedOpen) {
        openWarningTimer_.start(nowMs, OPEN_TIMEOUT_MS);
        maxOpenTimer_.start(nowMs, MAX_OPEN_TIMEOUT_MS);
    }

    if (state == DoorState::UnlockedClosed &&
        electronicControlEnabled(fsm_.mode())) {
        autoLockTimer_.start(nowMs, autoLockDurationForMode());
    }
}

uint32_t DoorController::autoLockDurationForMode() const
{
    return (fsm_.mode() == OperatingMode::OpenNight)
               ? OPEN_NIGHT_TIME_MS
               : AUTO_LOCK_TIME_MS;
}

void DoorController::processTimers(uint32_t nowMs)
{
    if (operationTimer_.expired(nowMs)) {
        operationTimer_.stop();

        if (fsm_.state() == DoorState::Locking) {
            dispatch(DoorEvent::LockTimeout, nowMs);
        } else if (fsm_.state() == DoorState::Unlocking) {
            dispatch(DoorEvent::UnlockTimeout, nowMs);
        }
    }

    if (retryTimer_.expired(nowMs)) {
        retryTimer_.stop();

        if (fsm_.state() == DoorState::LockRetryWait ||
            fsm_.state() == DoorState::UnlockRetryWait) {
            dispatch(DoorEvent::RetryDelayElapsed, nowMs);
        }
    }

    if (autoLockTimer_.expired(nowMs)) {
        autoLockTimer_.stop();

        if (fsm_.state() == DoorState::UnlockedClosed) {
            dispatch(DoorEvent::AutoLockTimeout, nowMs);
        }
    }

    if (openWarningTimer_.expired(nowMs)) {
        openWarningTimer_.stop();

        if (fsm_.state() == DoorState::UnlockedOpen) {
            dispatch(DoorEvent::OpenTimeout, nowMs);
        }
    }

    if (maxOpenTimer_.expired(nowMs)) {
        maxOpenTimer_.stop();

        if (fsm_.state() == DoorState::UnlockedOpen) {
            dispatch(DoorEvent::MaxOpenTimeout, nowMs);
        }
    }
}
