#include "unity.h"
#include "door_fsm.h"
#include "test_helpers.h"

void setUp(void) {}
void tearDown(void) {}

static void expect_safe_output(const DoorFsm& fsm)
{
    if (fsm.state() == DoorState::Locking) {
        EXPECT_COMMAND(fsm, LockCommand::Lock);
    } else if (fsm.state() == DoorState::Unlocking) {
        EXPECT_COMMAND(fsm, LockCommand::Unlock);
    } else {
        EXPECT_COMMAND(fsm, LockCommand::None);
    }
}

// -----------------------------------------------------------------------------
// Normal transitions
// -----------------------------------------------------------------------------

void test_locked_closed_rfid_request_starts_unlocking(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_locked_closed_exit_button_starts_unlocking(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_locked_closed_manual_unlock_goes_unlocked_closed(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_locked_closed_disabled_goes_disabled(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);
    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_open_door_closed_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Disabled,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_locked_closed_open_night_unlocks(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_unlocking_success(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

void test_unlocking_door_open_aborts(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlock_timeout_enters_wait(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::UnlockTimeout);
    EXPECT_STATE(fsm, DoorState::UnlockRetryWait);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(1, fsm.unlockRetries());
}

void test_unlock_retry_wait_restarts_unlock(void)
{
    DoorFsm fsm(DoorState::UnlockRetryWait, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::RetryDelayElapsed);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_unlocked_closed_door_open(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
}

void test_unlocked_closed_manual_lock(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
}

void test_unlocked_closed_auto_lock_timeout(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::AutoLockTimeout);
    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_rfid_while_unlocked_restarts_timer(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_exit_button_while_unlocked_restarts_timer(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_open_night_to_standard_gets_fresh_timer(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::OpenNight, true, false);
    fsm.start();
    fsm.process(DoorEvent::ModeStandard);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_locking_success(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_locking_door_open_aborts(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();
    EXPECT_COMMAND(fsm, LockCommand::Lock);
    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_locking_rfid_reverses_to_unlock(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_lock_timeout_enters_wait(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::LockTimeout);
    EXPECT_STATE(fsm, DoorState::LockRetryWait);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(1, fsm.lockRetries());
}

void test_lock_retry_wait_restarts_lock(void)
{
    DoorFsm fsm(DoorState::LockRetryWait, OperatingMode::Standard, true, false);
    fsm.start();
    fsm.process(DoorEvent::RetryDelayElapsed);
    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_unlocked_open_door_closed_standard(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::DoorClosed);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_open_timeout_is_warning_only(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::OpenTimeout);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_EFFECT(fsm, FsmEffect::OpenTimeoutWarning);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_disabled_ignores_rfid(void)
{
    DoorFsm fsm(DoorState::Disabled, OperatingMode::Disabled, true, true);
    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_disabled_ignores_exit_button(void)
{
    DoorFsm fsm(DoorState::Disabled, OperatingMode::Disabled, true, true);
    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);
    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_disabled_to_standard_locked(void)
{
    DoorFsm fsm(DoorState::Disabled, OperatingMode::Disabled, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeStandard);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
}

void test_disabled_to_standard_unlocked_starts_locking(void)
{
    DoorFsm fsm(DoorState::Disabled, OperatingMode::Disabled, true, false);
    fsm.start();
    fsm.process(DoorEvent::ModeStandard);
    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_disabled_to_open_night_locked_unlocks(void)
{
    DoorFsm fsm(DoorState::Disabled, OperatingMode::Disabled, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

// -----------------------------------------------------------------------------
// Fault entry tests
// -----------------------------------------------------------------------------

void test_locked_closed_door_open_faults(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::DoorOpenBoltLocked);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_open_bolt_locked_faults(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::DoorOpenBoltLocked);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_max_open_timeout_faults(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::DoorOpenTooLong);
}

void test_invalid_mode_faults_from_locked_closed(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_invalid_mode_faults_from_locking_and_removes_output(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();
    EXPECT_COMMAND(fsm, LockCommand::Lock);
    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_lock_failure_after_five_backoff_retries(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();

    for (uint8_t retry = 1; retry <= MAX_LOCK_RETRIES; ++retry) {
        fsm.process(DoorEvent::LockTimeout);
        EXPECT_STATE(fsm, DoorState::LockRetryWait);
        TEST_ASSERT_EQUAL_UINT8(retry, fsm.lockRetries());

        fsm.process(DoorEvent::RetryDelayElapsed);
        EXPECT_STATE(fsm, DoorState::Locking);
    }

    fsm.process(DoorEvent::LockTimeout);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::LockFailed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(MAX_LOCK_RETRIES, fsm.lockRetries());
}

void test_unlock_failure_after_two_retries(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();

    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::UnlockFailed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(2, fsm.unlockRetries());
}

// -----------------------------------------------------------------------------
// Fault auto-clear / recovery
// -----------------------------------------------------------------------------

void test_door_open_bolt_locked_fault_clears_when_door_closes_standard(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::DoorClosed);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_door_open_bolt_locked_fault_clears_when_bolt_retracts(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::BoltUnlocked);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_invalid_mode_clears_to_disabled(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::ModeDisabled);
    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_invalid_mode_clears_to_standard_locked(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeInvalid);
    fsm.process(DoorEvent::ModeStandard);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_invalid_mode_clears_to_open_night_and_unlocks_if_locked(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::ModeInvalid);
    fsm.process(DoorEvent::ModeOpenNight);
    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_FAULT(fsm, FaultCode::None);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_lock_failed_clears_when_bolt_becomes_locked(void)
{
    DoorFsm fsm(DoorState::Locking, OperatingMode::Standard, true, false);
    fsm.start();

    // Exhaust all configured retries.
    for (uint8_t retry = 0; retry < MAX_LOCK_RETRIES; ++retry) {
        fsm.process(DoorEvent::LockTimeout);
        EXPECT_STATE(fsm, DoorState::LockRetryWait);

        fsm.process(DoorEvent::RetryDelayElapsed);
        EXPECT_STATE(fsm, DoorState::Locking);
    }

    // One final failed attempt after all retries are consumed.
    fsm.process(DoorEvent::LockTimeout);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::LockFailed);

    // A subsequently observed successful physical lock clears the fault.
    fsm.process(DoorEvent::BoltLocked);

    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_unlock_failed_clears_when_bolt_becomes_unlocked(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();
    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::BoltUnlocked);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

void test_door_open_too_long_clears_on_valid_close(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::DoorClosed);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

// -----------------------------------------------------------------------------
// Invariant-oriented tests
// -----------------------------------------------------------------------------

void test_all_states_have_safe_output_mapping(void)
{
    struct Case {
        DoorState state;
        OperatingMode mode;
        bool doorClosed;
        bool boltLocked;
        LockCommand expected;
    };

    const Case cases[] = {
        {DoorState::LockedClosed,    OperatingMode::Standard, true,  true,  LockCommand::None},
        {DoorState::Unlocking,       OperatingMode::Standard, true,  true,  LockCommand::Unlock},
        {DoorState::UnlockRetryWait, OperatingMode::Standard, true,  true,  LockCommand::None},
        {DoorState::UnlockedClosed,  OperatingMode::Standard, true,  false, LockCommand::None},
        {DoorState::Locking,         OperatingMode::Standard, true,  false, LockCommand::Lock},
        {DoorState::LockRetryWait,   OperatingMode::Standard, true,  false, LockCommand::None},
        {DoorState::UnlockedOpen,    OperatingMode::Standard, false, false, LockCommand::None},
        {DoorState::Disabled,        OperatingMode::Disabled, true,  true,  LockCommand::None},
        {DoorState::Error,           OperatingMode::Standard, true,  true,  LockCommand::None},
    };

    for (const Case& c : cases) {
        DoorFsm fsm(c.state, c.mode, c.doorClosed, c.boltLocked);
        fsm.start();
        EXPECT_COMMAND(fsm, c.expected);
    }
}

void test_every_single_event_from_each_working_state_keeps_output_safe(void)
{
    const DoorEvent events[] = {
        DoorEvent::RfidReleaseRequest,
        DoorEvent::ExitButtonRequest,
        DoorEvent::DoorOpened,
        DoorEvent::DoorClosed,
        DoorEvent::BoltLocked,
        DoorEvent::BoltUnlocked,
        DoorEvent::ModeDisabled,
        DoorEvent::ModeStandard,
        DoorEvent::ModeOpenNight,
        DoorEvent::ModeInvalid,
        DoorEvent::AutoLockTimeout,
        DoorEvent::OpenTimeout,
        DoorEvent::MaxOpenTimeout,
        DoorEvent::LockTimeout,
        DoorEvent::UnlockTimeout,
        DoorEvent::RetryDelayElapsed
    };

    struct Initial {
        DoorState state;
        OperatingMode mode;
        bool doorClosed;
        bool boltLocked;
    };

    const Initial states[] = {
        {DoorState::LockedClosed,    OperatingMode::Standard, true,  true},
        {DoorState::Unlocking,       OperatingMode::Standard, true,  true},
        {DoorState::UnlockRetryWait, OperatingMode::Standard, true,  true},
        {DoorState::UnlockedClosed,  OperatingMode::Standard, true,  false},
        {DoorState::Locking,         OperatingMode::Standard, true,  false},
        {DoorState::LockRetryWait,   OperatingMode::Standard, true,  false},
        {DoorState::UnlockedOpen,    OperatingMode::Standard, false, false},
        {DoorState::Disabled,        OperatingMode::Disabled, true,  true}
    };

    for (const Initial& initial : states) {
        for (DoorEvent event : events) {
            DoorFsm fsm(initial.state, initial.mode,
                        initial.doorClosed, initial.boltLocked);
            fsm.start();
            fsm.process(event);
            expect_safe_output(fsm);
        }
    }
}

void test_error_state_never_drives_motor(void)
{
    DoorFsm fsm(DoorState::UnlockedOpen, OperatingMode::Standard, false, false);
    fsm.start();
    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_COMMAND(fsm, LockCommand::None);

    // Unrelated event while fault still present must not create motor output.
    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

// -----------------------------------------------------------------------------
// Representative end-to-end sequences
// -----------------------------------------------------------------------------

void test_normal_rfid_entry_sequence(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();

    fsm.process(DoorEvent::RfidReleaseRequest);
    EXPECT_STATE(fsm, DoorState::Unlocking);

    fsm.process(DoorEvent::BoltUnlocked);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);

    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::UnlockedOpen);

    fsm.process(DoorEvent::DoorClosed);
    EXPECT_STATE(fsm, DoorState::UnlockedClosed);

    fsm.process(DoorEvent::AutoLockTimeout);
    EXPECT_STATE(fsm, DoorState::Locking);

    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::LockedClosed);

    EXPECT_FAULT(fsm, FaultCode::None);
}

void test_lock_retry_then_success_sequence(void)
{
    DoorFsm fsm(DoorState::UnlockedClosed, OperatingMode::Standard, true, false);
    fsm.start();

    fsm.process(DoorEvent::AutoLockTimeout);
    EXPECT_STATE(fsm, DoorState::Locking);

    fsm.process(DoorEvent::LockTimeout);
    EXPECT_STATE(fsm, DoorState::LockRetryWait);
    EXPECT_COMMAND(fsm, LockCommand::None);

    fsm.process(DoorEvent::RetryDelayElapsed);
    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);

    fsm.process(DoorEvent::BoltLocked);
    EXPECT_STATE(fsm, DoorState::LockedClosed);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
    EXPECT_FAULT(fsm, FaultCode::None);
}

// -----------------------------------------------------------------------------
// Guard branch coverage
// -----------------------------------------------------------------------------
void test_disabled_to_standard_open_does_not_match_locked_or_closed_unlocked_guards(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeStandard);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}


void test_disabled_to_open_night_open_does_not_match_closed_guards(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}


void test_error_standard_unlocked_closed_skips_locked_guard(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();

    // Enter Error.
    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    // Change the physical state while remaining in Error.
    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
}


void test_error_open_night_unlocked_closed_skips_locked_guard(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::OpenNight,
        false,
        false
    );

    fsm.start();

    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_FAULT(fsm, FaultCode::None);
}


void test_error_standard_open_recovers_when_bolt_unlocked(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();

    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_FAULT(fsm, FaultCode::None);
}


void test_error_open_night_open_recovers_when_bolt_unlocked(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::OpenNight,
        false,
        false
    );

    fsm.start();

    fsm.process(DoorEvent::MaxOpenTimeout);
    EXPECT_STATE(fsm, DoorState::Error);

    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_FAULT(fsm, FaultCode::None);
}
void test_error_fault_updates_when_physical_contradiction_appears(void)
{
    DoorFsm fsm(DoorState::Unlocking, OperatingMode::Standard, true, true);
    fsm.start();

    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);
    fsm.process(DoorEvent::RetryDelayElapsed);
    fsm.process(DoorEvent::UnlockTimeout);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::UnlockFailed);

    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::DoorOpenBoltLocked);
}

void test_invalid_mode_fault_is_replaced_by_physical_contradiction(void)
{
    DoorFsm fsm(DoorState::LockedClosed, OperatingMode::Standard, true, true);
    fsm.start();

    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);

    fsm.process(DoorEvent::DoorOpened);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::DoorOpenBoltLocked);
}

void test_invalid_mode_error_does_not_recover_on_door_closed(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();

    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);

    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_invalid_mode_error_does_not_recover_on_bolt_unlocked(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();

    fsm.process(DoorEvent::ModeInvalid);
    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);

    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::Error);
    EXPECT_FAULT(fsm, FaultCode::InvalidMode);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_locked_closed_rfid_request_starts_unlocking);
    RUN_TEST(test_locked_closed_exit_button_starts_unlocking);
    RUN_TEST(test_locked_closed_manual_unlock_goes_unlocked_closed);
    RUN_TEST(test_locked_closed_disabled_goes_disabled);
    RUN_TEST(test_unlocked_open_door_closed_disabled_goes_disabled);
    RUN_TEST(test_locked_closed_open_night_unlocks);
    RUN_TEST(test_unlocking_success);
    RUN_TEST(test_unlocking_door_open_aborts);
    RUN_TEST(test_unlock_timeout_enters_wait);
    RUN_TEST(test_unlock_retry_wait_restarts_unlock);
    RUN_TEST(test_unlocked_closed_door_open);
    RUN_TEST(test_unlocked_closed_manual_lock);
    RUN_TEST(test_unlocked_closed_auto_lock_timeout);
    RUN_TEST(test_rfid_while_unlocked_restarts_timer);
    RUN_TEST(test_exit_button_while_unlocked_restarts_timer);
    RUN_TEST(test_open_night_to_standard_gets_fresh_timer);
    RUN_TEST(test_locking_success);
    RUN_TEST(test_locking_door_open_aborts);
    RUN_TEST(test_locking_rfid_reverses_to_unlock);
    RUN_TEST(test_lock_timeout_enters_wait);
    RUN_TEST(test_lock_retry_wait_restarts_lock);
    RUN_TEST(test_unlocked_open_door_closed_standard);
    RUN_TEST(test_open_timeout_is_warning_only);
    RUN_TEST(test_disabled_ignores_rfid);
    RUN_TEST(test_disabled_ignores_exit_button);
    RUN_TEST(test_disabled_to_standard_locked);
    RUN_TEST(test_disabled_to_standard_unlocked_starts_locking);
    RUN_TEST(test_disabled_to_open_night_locked_unlocks);

    RUN_TEST(test_locked_closed_door_open_faults);
    RUN_TEST(test_unlocked_open_bolt_locked_faults);
    RUN_TEST(test_max_open_timeout_faults);
    RUN_TEST(test_invalid_mode_faults_from_locked_closed);
    RUN_TEST(test_invalid_mode_faults_from_locking_and_removes_output);
    RUN_TEST(test_lock_failure_after_five_backoff_retries);
    RUN_TEST(test_unlock_failure_after_two_retries);

    RUN_TEST(test_door_open_bolt_locked_fault_clears_when_door_closes_standard);
    RUN_TEST(test_door_open_bolt_locked_fault_clears_when_bolt_retracts);
    RUN_TEST(test_invalid_mode_clears_to_disabled);
    RUN_TEST(test_invalid_mode_clears_to_standard_locked);
    RUN_TEST(test_invalid_mode_clears_to_open_night_and_unlocks_if_locked);
    RUN_TEST(test_lock_failed_clears_when_bolt_becomes_locked);
    RUN_TEST(test_unlock_failed_clears_when_bolt_becomes_unlocked);
    RUN_TEST(test_door_open_too_long_clears_on_valid_close);

    RUN_TEST(test_all_states_have_safe_output_mapping);
    RUN_TEST(test_every_single_event_from_each_working_state_keeps_output_safe);
    RUN_TEST(test_error_state_never_drives_motor);

    RUN_TEST(test_normal_rfid_entry_sequence);
    RUN_TEST(test_lock_retry_then_success_sequence);

    RUN_TEST(test_disabled_to_standard_open_does_not_match_locked_or_closed_unlocked_guards);
    RUN_TEST(test_disabled_to_open_night_open_does_not_match_closed_guards);
    RUN_TEST(test_error_standard_unlocked_closed_skips_locked_guard);
    RUN_TEST(test_error_open_night_unlocked_closed_skips_locked_guard);
    RUN_TEST(test_error_standard_open_recovers_when_bolt_unlocked);
    RUN_TEST(test_error_open_night_open_recovers_when_bolt_unlocked);
    RUN_TEST(test_error_fault_updates_when_physical_contradiction_appears);
    RUN_TEST(test_invalid_mode_fault_is_replaced_by_physical_contradiction);
    RUN_TEST(test_invalid_mode_error_does_not_recover_on_door_closed);
    RUN_TEST(test_invalid_mode_error_does_not_recover_on_bolt_unlocked);
    return UNITY_END();
}
