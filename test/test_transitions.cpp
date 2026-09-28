#include "unity.h"
#include "door_fsm.h"
#include "test_helpers.h"

void setUp(void) {}
void tearDown(void) {}

// -----------------------------------------------------------------------------
// LOCKED_CLOSED
// -----------------------------------------------------------------------------

void test_locked_closed_rfid_request_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_locked_closed_exit_button_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_locked_closed_manual_unlock_goes_unlocked_closed(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_locked_closed_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_locked_closed_open_night_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

// -----------------------------------------------------------------------------
// UNLOCKING
// -----------------------------------------------------------------------------

void test_unlocking_bolt_unlocked_completes_unlock(void)
{
    DoorFsm fsm(
        DoorState::Unlocking,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_unlocking_door_open_aborts_unlock(void)
{
    DoorFsm fsm(
        DoorState::Unlocking,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

void test_unlock_timeout_enters_retry_wait(void)
{
    DoorFsm fsm(
        DoorState::Unlocking,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::UnlockTimeout);

    EXPECT_STATE(fsm, DoorState::UnlockRetryWait);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(1, fsm.unlockRetries());
}

void test_unlocking_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::Unlocking,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

// -----------------------------------------------------------------------------
// UNLOCK_RETRY_WAIT
// -----------------------------------------------------------------------------

void test_unlock_retry_wait_elapsed_restarts_unlock(void)
{
    DoorFsm fsm(
        DoorState::UnlockRetryWait,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::RetryDelayElapsed);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_unlock_retry_wait_bolt_unlocked_completes_unlock(void)
{
    DoorFsm fsm(
        DoorState::UnlockRetryWait,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_unlock_retry_wait_door_open_goes_unlocked_open(void)
{
    DoorFsm fsm(
        DoorState::UnlockRetryWait,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

void test_unlock_retry_wait_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::UnlockRetryWait,
        OperatingMode::Standard,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.unlockRetries());
}

// -----------------------------------------------------------------------------
// UNLOCKED_CLOSED
// -----------------------------------------------------------------------------

void test_unlocked_closed_door_open_goes_unlocked_open(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_closed_manual_lock_goes_locked_closed(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::BoltLocked);

    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_closed_auto_lock_timeout_starts_locking(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::AutoLockTimeout);

    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_unlocked_closed_rfid_request_restarts_auto_lock_timer(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_unlocked_closed_exit_button_restarts_auto_lock_timer(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_unlocked_closed_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_closed_mode_open_night_restarts_timer(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_unlocked_closed_mode_standard_restarts_timer(void)
{
    DoorFsm fsm(
        DoorState::UnlockedClosed,
        OperatingMode::OpenNight,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeStandard);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

// -----------------------------------------------------------------------------
// LOCKING
// -----------------------------------------------------------------------------

void test_locking_bolt_locked_completes_lock(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::BoltLocked);

    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_locking_door_open_aborts_lock(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LockCommand::Lock),
        static_cast<uint8_t>(fsm.lockCommand())
    );

    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_locking_rfid_request_reverses_to_unlock(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_locking_exit_button_reverses_to_unlock(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_locking_open_night_reverses_to_unlock(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_timeout_enters_retry_wait(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::LockTimeout);

    EXPECT_STATE(fsm, DoorState::LockRetryWait);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(1, fsm.lockRetries());
}

void test_locking_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::Locking,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

// -----------------------------------------------------------------------------
// LOCK_RETRY_WAIT
// -----------------------------------------------------------------------------

void test_lock_retry_wait_elapsed_restarts_lock(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::RetryDelayElapsed);

    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_lock_retry_wait_bolt_locked_completes_lock(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::BoltLocked);

    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_retry_wait_door_open_goes_unlocked_open(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_retry_wait_rfid_request_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::RfidReleaseRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_retry_wait_exit_button_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ExitButtonRequest);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_retry_wait_open_night_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

void test_lock_retry_wait_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::LockRetryWait,
        OperatingMode::Standard,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_EQUAL_UINT8(0, fsm.lockRetries());
}

// -----------------------------------------------------------------------------
// UNLOCKED_OPEN
// -----------------------------------------------------------------------------

void test_unlocked_open_door_closed_standard_goes_unlocked_closed(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
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
}

void test_unlocked_open_open_timeout_warns_only(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::OpenTimeout);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::OpenTimeoutWarning);
}

void test_unlocked_open_mode_disabled_goes_disabled(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeDisabled);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_open_mode_standard_stays_unlocked_open(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::OpenNight,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeStandard);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_unlocked_open_mode_open_night_stays_unlocked_open(void)
{
    DoorFsm fsm(
        DoorState::UnlockedOpen,
        OperatingMode::Standard,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::UnlockedOpen);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

// -----------------------------------------------------------------------------
// DISABLED
// -----------------------------------------------------------------------------

void test_disabled_tracks_door_open(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorOpened);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_FALSE(fsm.doorClosed());
}

void test_disabled_tracks_door_closed(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        false,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::DoorClosed);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_TRUE(fsm.doorClosed());
}

void test_disabled_tracks_bolt_locked(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::BoltLocked);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_TRUE(fsm.boltLocked());
}

void test_disabled_tracks_bolt_unlocked(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::BoltUnlocked);

    EXPECT_STATE(fsm, DoorState::Disabled);
    EXPECT_COMMAND(fsm, LockCommand::None);
    TEST_ASSERT_FALSE(fsm.boltLocked());
}

void test_disabled_to_standard_locked_closed_reconstructs_locked_closed(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeStandard);

    EXPECT_STATE(fsm, DoorState::LockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
}

void test_disabled_to_standard_unlocked_closed_starts_locking(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeStandard);

    EXPECT_STATE(fsm, DoorState::Locking);
    EXPECT_COMMAND(fsm, LockCommand::Lock);
}

void test_disabled_to_standard_open_goes_unlocked_open(void)
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

void test_disabled_to_open_night_locked_closed_starts_unlocking(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        true
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::Unlocking);
    EXPECT_COMMAND(fsm, LockCommand::Unlock);
}

void test_disabled_to_open_night_unlocked_closed_reconstructs_unlocked_closed(void)
{
    DoorFsm fsm(
        DoorState::Disabled,
        OperatingMode::Disabled,
        true,
        false
    );

    fsm.start();
    fsm.process(DoorEvent::ModeOpenNight);

    EXPECT_STATE(fsm, DoorState::UnlockedClosed);
    EXPECT_COMMAND(fsm, LockCommand::None);
    EXPECT_EFFECT(fsm, FsmEffect::RestartAutoLockTimer);
}

void test_disabled_to_open_night_open_goes_unlocked_open(void)
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

// -----------------------------------------------------------------------------
// Test runner
// -----------------------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_locked_closed_rfid_request_starts_unlocking);
    RUN_TEST(test_locked_closed_exit_button_starts_unlocking);
    RUN_TEST(test_locked_closed_manual_unlock_goes_unlocked_closed);
    RUN_TEST(test_locked_closed_mode_disabled_goes_disabled);
    RUN_TEST(test_locked_closed_open_night_starts_unlocking);

    RUN_TEST(test_unlocking_bolt_unlocked_completes_unlock);
    RUN_TEST(test_unlocking_door_open_aborts_unlock);
    RUN_TEST(test_unlock_timeout_enters_retry_wait);
    RUN_TEST(test_unlocking_mode_disabled_goes_disabled);

    RUN_TEST(test_unlock_retry_wait_elapsed_restarts_unlock);
    RUN_TEST(test_unlock_retry_wait_bolt_unlocked_completes_unlock);
    RUN_TEST(test_unlock_retry_wait_door_open_goes_unlocked_open);
    RUN_TEST(test_unlock_retry_wait_mode_disabled_goes_disabled);

    RUN_TEST(test_unlocked_closed_door_open_goes_unlocked_open);
    RUN_TEST(test_unlocked_closed_manual_lock_goes_locked_closed);
    RUN_TEST(test_unlocked_closed_auto_lock_timeout_starts_locking);
    RUN_TEST(test_unlocked_closed_rfid_request_restarts_auto_lock_timer);
    RUN_TEST(test_unlocked_closed_exit_button_restarts_auto_lock_timer);
    RUN_TEST(test_unlocked_closed_mode_disabled_goes_disabled);
    RUN_TEST(test_unlocked_closed_mode_open_night_restarts_timer);
    RUN_TEST(test_unlocked_closed_mode_standard_restarts_timer);

    RUN_TEST(test_locking_bolt_locked_completes_lock);
    RUN_TEST(test_locking_door_open_aborts_lock);
    RUN_TEST(test_locking_rfid_request_reverses_to_unlock);
    RUN_TEST(test_locking_exit_button_reverses_to_unlock);
    RUN_TEST(test_locking_open_night_reverses_to_unlock);
    RUN_TEST(test_lock_timeout_enters_retry_wait);
    RUN_TEST(test_locking_mode_disabled_goes_disabled);

    RUN_TEST(test_lock_retry_wait_elapsed_restarts_lock);
    RUN_TEST(test_lock_retry_wait_bolt_locked_completes_lock);
    RUN_TEST(test_lock_retry_wait_door_open_goes_unlocked_open);
    RUN_TEST(test_lock_retry_wait_rfid_request_starts_unlocking);
    RUN_TEST(test_lock_retry_wait_exit_button_starts_unlocking);
    RUN_TEST(test_lock_retry_wait_open_night_starts_unlocking);
    RUN_TEST(test_lock_retry_wait_mode_disabled_goes_disabled);

    RUN_TEST(test_unlocked_open_door_closed_standard_goes_unlocked_closed);
    RUN_TEST(test_unlocked_open_door_closed_disabled_goes_disabled);
    RUN_TEST(test_unlocked_open_open_timeout_warns_only);
    RUN_TEST(test_unlocked_open_mode_disabled_goes_disabled);
    RUN_TEST(test_unlocked_open_mode_standard_stays_unlocked_open);
    RUN_TEST(test_unlocked_open_mode_open_night_stays_unlocked_open);

    RUN_TEST(test_disabled_tracks_door_open);
    RUN_TEST(test_disabled_tracks_door_closed);
    RUN_TEST(test_disabled_tracks_bolt_locked);
    RUN_TEST(test_disabled_tracks_bolt_unlocked);
    RUN_TEST(test_disabled_to_standard_locked_closed_reconstructs_locked_closed);
    RUN_TEST(test_disabled_to_standard_unlocked_closed_starts_locking);
    RUN_TEST(test_disabled_to_standard_open_goes_unlocked_open);
    RUN_TEST(test_disabled_to_open_night_locked_closed_starts_unlocking);
    RUN_TEST(test_disabled_to_open_night_unlocked_closed_reconstructs_unlocked_closed);
    RUN_TEST(test_disabled_to_open_night_open_goes_unlocked_open);

    return UNITY_END();
}
