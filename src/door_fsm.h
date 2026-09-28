#pragma once

#include <etl/state_chart.h>
#include "controller_types.h"

class DoorFsm
{
public:
    using Chart = etl::state_chart<DoorFsm>;
    using Transition = Chart::transition;

    DoorFsm(DoorState initialState,
            OperatingMode initialMode,
            bool initialDoorClosed,
            bool initialBoltLocked);

    void start();
    void process(DoorEvent event);

    DoorState state() const;
    OperatingMode mode() const { return mode_; }

    bool doorClosed() const { return doorClosed_; }
    bool boltLocked() const { return boltLocked_; }

    uint8_t lockRetries() const { return lockRetries_; }
    uint8_t unlockRetries() const { return unlockRetries_; }

    LockCommand lockCommand() const;
    FsmEffect effect() const { return effect_; }

private:
    static uint8_t id(DoorState state);
    static uint8_t id(DoorEvent event);

    void observe(DoorEvent event);

    // Guards.
    bool modeAllowsElectronicControl();
    bool modeIsStandard();
    bool modeIsOpenNight();
    bool modeIsDisabled() ;

    bool canRetryLock();
    bool canRetryUnlock();

    bool physicalLockedClosed();
    bool physicalUnlockedClosed();
    bool physicalUnlockedOpen();

    // Actions.
    void clearLockRetries();
    void clearUnlockRetries();
    void clearBothRetries();

    void countLockRetry();
    void countUnlockRetry();

    void restartAutoLockTimer();
    void noteOpenTimeout();

    void clearLockRetriesAndRestartAutoLock();
    void clearUnlockRetriesAndRestartAutoLock();

    Chart chart_;

    OperatingMode mode_;
    bool doorClosed_;
    bool boltLocked_;

    uint8_t lockRetries_;
    uint8_t unlockRetries_;

    FsmEffect effect_;

    static const Transition transitions_[];
};
