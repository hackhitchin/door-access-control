#pragma once

#include <stdint.h>
#include "controller_config.h"
#include "controller_types.h"
#include "debounce.h"
#include "door_fsm.h"

struct ControllerInputs
{
    bool doorClosed;
    bool boltLocked;
    bool rfidActive;
    bool exitPressed;
    OperatingMode mode;
};

class DoorController
{
public:
    DoorController(const ControllerInputs& initialInputs, uint32_t nowMs);

    void tick(uint32_t nowMs, const ControllerInputs& rawInputs);

    DoorState state() const { return fsm_.state(); }
    OperatingMode mode() const { return fsm_.mode(); }
    LockCommand lockCommand() const { return fsm_.lockCommand(); }
    FaultCode fault() const { return fsm_.fault(); }
    FsmEffect effect() const { return fsm_.effect(); }

    bool doorClosed() const { return door_.value(); }
    bool boltLocked() const { return bolt_.value(); }
    bool rfidActive() const { return rfid_.value(); }
    bool exitPressed() const { return exit_.value(); }
    bool rfidArmed() const { return rfidArmed_; }

private:
    struct Timer
    {
        bool active = false;
        uint32_t startMs = 0;
        uint32_t durationMs = 0;

        void start(uint32_t nowMs, uint32_t duration)
        {
            active = true;
            startMs = nowMs;
            durationMs = duration;
        }

        void stop() { active = false; }

        bool expired(uint32_t nowMs) const
        {
            return active &&
                   static_cast<uint32_t>(nowMs - startMs) >= durationMs;
        }
    };

    static DoorState initialStateFor(const ControllerInputs& inputs);
    static bool electronicControlEnabled(OperatingMode mode);

    void dispatch(DoorEvent event, uint32_t nowMs);
    void afterFsmEvent(DoorState previousState, uint32_t nowMs);
    void initialiseTimers(uint32_t nowMs);

    void processDoorInput(uint32_t nowMs, bool rawValue);
    void processBoltInput(uint32_t nowMs, bool rawValue);
    void processModeInput(uint32_t nowMs, OperatingMode rawValue);
    void processExitInput(uint32_t nowMs, bool rawValue);
    void processRfidInput(uint32_t nowMs, bool rawValue);
    void processTimers(uint32_t nowMs);

    uint32_t autoLockDurationForMode() const;

    DoorFsm fsm_;

    DebouncedBool door_;
    DebouncedBool bolt_;
    DebouncedBool rfid_;
    DebouncedBool exit_;
    DebouncedValue<OperatingMode> mode_;

    bool rfidArmed_;

    Timer autoLockTimer_;
    Timer openWarningTimer_;
    Timer maxOpenTimer_;
    Timer operationTimer_;
    Timer retryTimer_;
};
