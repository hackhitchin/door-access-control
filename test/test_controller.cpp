#include <stdint.h>

#include "unity.h"
#include "controller_config.h"
#include "controller_types.h"
#include "debounce.h"
#include "door_controller.h"

void setUp(void) {}
void tearDown(void) {}

#define EXPECT_STATE(controller, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((controller).state()))

#define EXPECT_COMMAND(controller, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((controller).lockCommand()))

#define EXPECT_FAULT(controller, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((controller).fault()))

#define EXPECT_EFFECT(controller, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((controller).effect()))

static ControllerInputs makeInputs(OperatingMode mode,
                                   bool doorClosed,
                                   bool boltLocked,
                                   bool rfidActive = false,
                                   bool exitPressed = false)
{
    ControllerInputs inputs;
    inputs.doorClosed = doorClosed;
    inputs.boltLocked = boltLocked;
    inputs.rfidActive = rfidActive;
    inputs.exitPressed = exitPressed;
    inputs.mode = mode;
    return inputs;
}

static uint32_t plusMs(uint32_t start, uint32_t delta)
{
    return static_cast<uint32_t>(start + delta);
}

static void expectOutputMatchesState(const DoorController& controller)
{
    if (controller.state() == DoorState::Locking) {
        EXPECT_COMMAND(controller, LockCommand::Lock);
    } else if (controller.state() == DoorState::Unlocking) {
        EXPECT_COMMAND(controller, LockCommand::Unlock);
    } else {
        EXPECT_COMMAND(controller, LockCommand::None);
    }
}

// -----------------------------------------------------------------------------
// Startup reconstruction.
// These test the controller's current responsibility for choosing the initial
// stable/transient state. They deliberately do NOT duplicate every FSM
// transition.
// -----------------------------------------------------------------------------

void test_startup_standard_closed_locked(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_startup_standard_closed_unlocked_starts_locking(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Locking);
    EXPECT_COMMAND(controller, LockCommand::Lock);
}

void test_startup_open_night_closed_locked_starts_unlocking(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

void test_startup_open_unlocked_does_not_drive_motor(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, false, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::UnlockedOpen);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_startup_disabled_does_not_drive_motor(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Disabled, true, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Disabled);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_startup_disabled_open_and_bolt_locked_stays_disabled(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Disabled, false, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Disabled);
    EXPECT_COMMAND(controller, LockCommand::None);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_disabled_contradiction_faults_when_standard_is_selected(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Disabled, false, true);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::Disabled);

    inputs.mode = OperatingMode::Standard;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::DoorOpenBoltLocked);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_startup_open_and_bolt_locked_enters_sensor_fault(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, false, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::DoorOpenBoltLocked);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_startup_invalid_mode_enters_mode_fault(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Invalid, true, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::InvalidMode);
    EXPECT_COMMAND(controller, LockCommand::None);
}

// -----------------------------------------------------------------------------
// Boot-specific request behaviour.
// -----------------------------------------------------------------------------

void test_rfid_active_at_boot_is_not_a_release_request(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true, true, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    TEST_ASSERT_FALSE(controller.rfidArmed());
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_exit_held_at_boot_is_honoured(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true, false, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

void test_exit_held_at_boot_with_unlocked_bolt_completes_release_immediately(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false, false, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_release_during_locking_waits_for_lock_completion_then_unlocks(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::Locking);

    inputs.exitPressed = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_EXIT_MS, inputs);

    // The release is queued while the previous lock command may still be in
    // flight. It is not acknowledged merely because the bolt still reads open.
    EXPECT_STATE(controller, DoorState::Locking);
    EXPECT_COMMAND(controller, LockCommand::Lock);

    inputs.boltLocked = true;
    controller.tick(500, inputs);
    controller.tick(500 + DEBOUNCE_BOLT_MS, inputs);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_release_during_locking_timeout_completes_if_bolt_still_unlocked(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    inputs.exitPressed = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_EXIT_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);

    controller.tick(LOCK_TIME_MS, inputs);

    // The lock never confirmed. The pending release takes precedence over a
    // retry, then level reconciliation sees that the bolt is already unlocked.
    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_exit_held_at_boot_is_ignored_in_disabled_mode(void)
{
    const ControllerInputs inputs =
        makeInputs(OperatingMode::Disabled, true, true, false, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Disabled);
    EXPECT_COMMAND(controller, LockCommand::None);
}

// -----------------------------------------------------------------------------
// Debouncer unit tests.
// -----------------------------------------------------------------------------

void test_bool_debounce_ignores_short_pulse(void)
{
    DebouncedBool input(false, 100, 30);

    TEST_ASSERT_FALSE(input.update(true, 110));
    TEST_ASSERT_FALSE(input.value());

    TEST_ASSERT_FALSE(input.update(false, 125));
    TEST_ASSERT_FALSE(input.value());

    TEST_ASSERT_FALSE(input.update(false, 200));
    TEST_ASSERT_FALSE(input.value());
}

void test_bool_debounce_changes_exactly_at_threshold(void)
{
    DebouncedBool input(false, 100, 30);

    TEST_ASSERT_FALSE(input.update(true, 110));
    TEST_ASSERT_FALSE(input.update(true, 139));
    TEST_ASSERT_FALSE(input.value());

    TEST_ASSERT_TRUE(input.update(true, 140));
    TEST_ASSERT_TRUE(input.value());
}

void test_value_debounce_ignores_short_invalid_mode(void)
{
    DebouncedValue<OperatingMode> mode(OperatingMode::Standard, 0,
                                       DEBOUNCE_MODE_MS);

    TEST_ASSERT_FALSE(mode.update(OperatingMode::Invalid, 10));
    TEST_ASSERT_FALSE(mode.update(OperatingMode::Invalid,
                                  10 + DEBOUNCE_MODE_MS - 1));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperatingMode::Standard),
                            static_cast<uint8_t>(mode.value()));

    TEST_ASSERT_FALSE(mode.update(OperatingMode::Standard,
                                  10 + DEBOUNCE_MODE_MS));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(OperatingMode::Standard),
                            static_cast<uint8_t>(mode.value()));
}

void test_bool_debounce_handles_uint32_wraparound(void)
{
    const uint32_t start = UINT32_MAX - 10U;
    DebouncedBool input(false, start, 30);

    TEST_ASSERT_FALSE(input.update(true, start));

    const uint32_t before = plusMs(start, 29);
    TEST_ASSERT_FALSE(input.update(true, before));
    TEST_ASSERT_FALSE(input.value());

    const uint32_t at = plusMs(start, 30);
    TEST_ASSERT_TRUE(input.update(true, at));
    TEST_ASSERT_TRUE(input.value());
}

// -----------------------------------------------------------------------------
// RFID qualification.
// -----------------------------------------------------------------------------

void test_boot_active_rfid_rearms_only_after_stable_inactive(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true, true, false);

    DoorController controller(inputs, 0);
    TEST_ASSERT_FALSE(controller.rfidArmed());

    inputs.rfidActive = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_RFID_MS - 1, inputs);

    TEST_ASSERT_FALSE(controller.rfidArmed());
    EXPECT_STATE(controller, DoorState::LockedClosed);

    controller.tick(100 + DEBOUNCE_RFID_MS, inputs);

    TEST_ASSERT_TRUE(controller.rfidArmed());
    EXPECT_STATE(controller, DoorState::LockedClosed);
}

void test_rfid_after_rearming_generates_release(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true, true, false);

    DoorController controller(inputs, 0);

    inputs.rfidActive = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_RFID_MS, inputs);
    TEST_ASSERT_TRUE(controller.rfidArmed());

    inputs.rfidActive = true;
    controller.tick(200, inputs);
    controller.tick(200 + DEBOUNCE_RFID_MS, inputs);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

void test_rfid_pulse_shorter_than_debounce_is_ignored(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.rfidActive = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_RFID_MS - 1, inputs);

    inputs.rfidActive = false;
    controller.tick(100 + DEBOUNCE_RFID_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
}

// -----------------------------------------------------------------------------
// Exit-button qualification.
// -----------------------------------------------------------------------------

void test_exit_press_shorter_than_debounce_is_ignored(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.exitPressed = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_EXIT_MS - 1, inputs);

    inputs.exitPressed = false;
    controller.tick(100 + DEBOUNCE_EXIT_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
}

void test_stable_exit_press_generates_release(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.exitPressed = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_EXIT_MS, inputs);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

// -----------------------------------------------------------------------------
// Sensor debounce / event forwarding.
// -----------------------------------------------------------------------------

void test_door_open_shorter_than_debounce_is_ignored(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, false);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    inputs.doorClosed = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_DOOR_MS - 1, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    TEST_ASSERT_TRUE(controller.doorClosed());

    inputs.doorClosed = true;
    controller.tick(100 + DEBOUNCE_DOOR_MS, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
}

void test_stable_door_open_is_forwarded_to_fsm(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, false);

    DoorController controller(inputs, 0);

    inputs.doorClosed = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_DOOR_MS, inputs);

    TEST_ASSERT_FALSE(controller.doorClosed());
    EXPECT_STATE(controller, DoorState::UnlockedOpen);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_bolt_change_shorter_than_debounce_is_ignored(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_BOLT_MS - 1, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    TEST_ASSERT_TRUE(controller.boltLocked());

    inputs.boltLocked = true;
    controller.tick(100 + DEBOUNCE_BOLT_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
}

void test_stable_manual_unlock_is_forwarded_to_fsm(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_BOLT_MS, inputs);

    TEST_ASSERT_FALSE(controller.boltLocked());
    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
}

// -----------------------------------------------------------------------------
// Mode debounce.
// -----------------------------------------------------------------------------

void test_short_invalid_mode_condition_is_ignored(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.mode = OperatingMode::Invalid;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS - 1, inputs);

    inputs.mode = OperatingMode::Standard;
    controller.tick(100 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_stable_invalid_mode_is_forwarded_to_fsm(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.mode = OperatingMode::Invalid;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::InvalidMode);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_stable_open_night_mode_change_is_forwarded(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.mode = OperatingMode::OpenNight;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

// -----------------------------------------------------------------------------
// Operation timers and retry delay.
// -----------------------------------------------------------------------------

void test_lock_timeout_fires_at_exact_deadline(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::Locking);

    controller.tick(LOCK_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::Locking);

    controller.tick(LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_unlock_timeout_fires_at_exact_deadline(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, true);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::Unlocking);

    controller.tick(UNLOCK_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::Unlocking);

    controller.tick(UNLOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockRetryWait);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_unlock_retry_wait_fires_at_exact_deadline(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, true);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::Unlocking);

    controller.tick(UNLOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockRetryWait);
    EXPECT_COMMAND(controller, LockCommand::None);

    controller.tick(
        UNLOCK_TIME_MS + RETRY_DELAY_MS - 1,
        inputs
    );

    EXPECT_STATE(controller, DoorState::UnlockRetryWait);

    controller.tick(
        UNLOCK_TIME_MS + RETRY_DELAY_MS,
        inputs
    );

    EXPECT_STATE(controller, DoorState::Unlocking);
    EXPECT_COMMAND(controller, LockCommand::Unlock);
}

void test_retry_wait_fires_at_exact_deadline(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    controller.tick(LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);

    controller.tick(LOCK_TIME_MS + RETRY_DELAY_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);

    controller.tick(LOCK_TIME_MS + RETRY_DELAY_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
    EXPECT_COMMAND(controller, LockCommand::Lock);
}

// -----------------------------------------------------------------------------
// Auto-lock timers.
// -----------------------------------------------------------------------------

void test_standard_auto_lock_fires_at_exact_deadline(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    // Manual unlock is accepted after bolt debounce and starts auto-lock timer.
    inputs.boltLocked = false;
    controller.tick(100, inputs);
    const uint32_t unlockedAt = 100 + DEBOUNCE_BOLT_MS;
    controller.tick(unlockedAt, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(unlockedAt + AUTO_LOCK_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(unlockedAt + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
    EXPECT_COMMAND(controller, LockCommand::Lock);
}

void test_rfid_request_restarts_auto_lock_timer(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(0, inputs);
    const uint32_t unlockedAt = DEBOUNCE_BOLT_MS;
    controller.tick(unlockedAt, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    // Start RFID candidate at t=4000, accepted at 4010.
    inputs.rfidActive = true;
    controller.tick(4000, inputs);
    const uint32_t requestAt = 4000 + DEBOUNCE_RFID_MS;
    controller.tick(requestAt, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    // Original timer would have expired at 5020.
    controller.tick(unlockedAt + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(requestAt + AUTO_LOCK_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(requestAt + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
}

void test_exit_request_restarts_auto_lock_timer(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(0, inputs);
    const uint32_t unlockedAt = DEBOUNCE_BOLT_MS;
    controller.tick(unlockedAt, inputs);

    inputs.exitPressed = true;
    controller.tick(4000, inputs);
    const uint32_t requestAt = 4000 + DEBOUNCE_EXIT_MS;
    controller.tick(requestAt, inputs);

    controller.tick(unlockedAt + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(requestAt + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
}

void test_open_night_timer_uses_existing_two_hour_setting(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(OPEN_NIGHT_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(OPEN_NIGHT_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
    EXPECT_COMMAND(controller, LockCommand::Lock);
}

// -----------------------------------------------------------------------------
// Door-open timers.
// -----------------------------------------------------------------------------

void test_open_warning_fires_without_faulting(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, false, false);

    DoorController controller(inputs, 0);

    controller.tick(OPEN_TIMEOUT_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    controller.tick(OPEN_TIMEOUT_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);
    EXPECT_EFFECT(controller, FsmEffect::OpenTimeoutWarning);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_max_open_timeout_enters_fault(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, false, false);

    DoorController controller(inputs, 0);

    controller.tick(MAX_OPEN_TIMEOUT_MS, inputs);

    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::DoorOpenTooLong);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_open_night_max_open_timeout_is_two_hours(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, false, false);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    controller.tick(MAX_OPEN_TIMEOUT_MS, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);
    EXPECT_FAULT(controller, FaultCode::None);

    controller.tick(OPEN_NIGHT_MAX_OPEN_TIMEOUT_MS - 1UL, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    controller.tick(OPEN_NIGHT_MAX_OPEN_TIMEOUT_MS, inputs);
    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::DoorOpenTooLong);
}

void test_closing_door_cancels_old_open_timers(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, false, false);

    DoorController controller(inputs, 0);

    inputs.doorClosed = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_DOOR_MS, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    // Well beyond the old open timers but well short of the 2-hour Open Night
    // auto-lock timer.
    controller.tick(MAX_OPEN_TIMEOUT_MS + 1000, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
}

// -----------------------------------------------------------------------------
// Stale operation timer cancellation.
// -----------------------------------------------------------------------------

void test_successful_lock_cancels_old_lock_timeout(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    inputs.boltLocked = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_BOLT_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);

    controller.tick(LOCK_TIME_MS + 1000, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_successful_unlock_cancels_old_unlock_timeout(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_BOLT_MS, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(UNLOCK_TIME_MS + 1000, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
}

// -----------------------------------------------------------------------------
// millis() wraparound.
// -----------------------------------------------------------------------------

void test_lock_timeout_handles_millis_wraparound(void)
{
    const uint32_t start = UINT32_MAX - 1000U;
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, start);

    controller.tick(plusMs(start, LOCK_TIME_MS - 1), inputs);
    EXPECT_STATE(controller, DoorState::Locking);

    controller.tick(plusMs(start, LOCK_TIME_MS), inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);
}

void test_auto_lock_handles_millis_wraparound(void)
{
    const uint32_t start = UINT32_MAX - 100U;
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, start);

    // Begin a manual-unlock sensor transition at startup time and let it settle
    // across the uint32 rollover.
    inputs.boltLocked = false;
    controller.tick(start, inputs);

    const uint32_t unlockedAt = plusMs(start, DEBOUNCE_BOLT_MS);
    controller.tick(unlockedAt, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(plusMs(unlockedAt, AUTO_LOCK_TIME_MS - 1), inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(plusMs(unlockedAt, AUTO_LOCK_TIME_MS), inputs);
    EXPECT_STATE(controller, DoorState::Locking);
}

// -----------------------------------------------------------------------------
// Same-tick priority: physical/mode/request inputs are processed before timers.
// -----------------------------------------------------------------------------

void test_bolt_success_wins_if_it_debounces_on_lock_timeout_tick(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    inputs.boltLocked = true;
    controller.tick(LOCK_TIME_MS - DEBOUNCE_BOLT_MS, inputs);
    controller.tick(LOCK_TIME_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_door_open_wins_if_it_debounces_on_auto_lock_tick(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    // Manual unlock at t=20 starts the Standard auto-lock timer.
    inputs.boltLocked = false;
    controller.tick(0, inputs);
    const uint32_t unlockedAt = DEBOUNCE_BOLT_MS;
    controller.tick(unlockedAt, inputs);

    const uint32_t deadline = unlockedAt + AUTO_LOCK_TIME_MS;

    inputs.doorClosed = false;
    controller.tick(deadline - DEBOUNCE_DOOR_MS, inputs);
    controller.tick(deadline, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedOpen);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_disabled_mode_wins_if_it_debounces_on_lock_timeout_tick(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    inputs.mode = OperatingMode::Disabled;
    controller.tick(LOCK_TIME_MS - DEBOUNCE_MODE_MS, inputs);
    controller.tick(LOCK_TIME_MS, inputs);

    EXPECT_STATE(controller, DoorState::Disabled);
    EXPECT_COMMAND(controller, LockCommand::None);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_rfid_request_wins_if_it_debounces_on_auto_lock_tick(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);

    inputs.boltLocked = false;
    controller.tick(0, inputs);
    const uint32_t unlockedAt = DEBOUNCE_BOLT_MS;
    controller.tick(unlockedAt, inputs);

    const uint32_t oldDeadline = unlockedAt + AUTO_LOCK_TIME_MS;

    inputs.rfidActive = true;
    controller.tick(oldDeadline - DEBOUNCE_RFID_MS, inputs);
    controller.tick(oldDeadline, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(oldDeadline + AUTO_LOCK_TIME_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    controller.tick(oldDeadline + AUTO_LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);
}

// -----------------------------------------------------------------------------
// Full wrapper-timed retry sequences.
// -----------------------------------------------------------------------------

void test_lock_attempt_uses_backoff_then_ends_in_lock_failed(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    const uint32_t retryStarts[] = {
        6000UL, 21000UL, 126000UL, 1131000UL, 11136000UL
    };
    const uint32_t retryWaitStarts[] = {
        5000UL, 11000UL, 26000UL, 131000UL, 1136000UL
    };
    const uint32_t retryDelays[] = {
        LOCK_RETRY_1_DELAY_MS, LOCK_RETRY_2_DELAY_MS,
        LOCK_RETRY_3_DELAY_MS, LOCK_RETRY_4_DELAY_MS,
        LOCK_RETRY_5_DELAY_MS
    };

    for (uint8_t i = 0; i < MAX_LOCK_RETRIES; ++i) {
        controller.tick(retryWaitStarts[i], inputs);
        EXPECT_STATE(controller, DoorState::LockRetryWait);
        EXPECT_COMMAND(controller, LockCommand::None);

        controller.tick(retryWaitStarts[i] + retryDelays[i] - 1UL, inputs);
        EXPECT_STATE(controller, DoorState::LockRetryWait);

        controller.tick(retryStarts[i], inputs);
        EXPECT_STATE(controller, DoorState::Locking);
        EXPECT_COMMAND(controller, LockCommand::Lock);
    }

    controller.tick(11141000UL, inputs);
    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::LockFailed);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_success_during_lock_retry_wait_prevents_another_attempt(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);

    controller.tick(LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);

    inputs.boltLocked = true;
    controller.tick(5500, inputs);
    controller.tick(5500 + DEBOUNCE_BOLT_MS, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);

    controller.tick(LOCK_TIME_MS + RETRY_DELAY_MS + 1000, inputs);

    EXPECT_STATE(controller, DoorState::LockedClosed);
    EXPECT_FAULT(controller, FaultCode::None);
}

void test_fault_indicator_activates_after_second_failed_lock_attempt(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, false);

    DoorController controller(inputs, 0);
    TEST_ASSERT_FALSE(controller.faultIndicated());

    // Initial attempt fails: retry count becomes one, but no degraded warning yet.
    controller.tick(LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);
    TEST_ASSERT_FALSE(controller.faultIndicated());

    // Retry 1 starts after 1 s and then also fails. This is the second failed
    // lock attempt overall, so indicate the degraded/unsecured condition while
    // continuing the longer back-off sequence.
    controller.tick(LOCK_TIME_MS + LOCK_RETRY_1_DELAY_MS, inputs);
    EXPECT_STATE(controller, DoorState::Locking);

    controller.tick(LOCK_TIME_MS + LOCK_RETRY_1_DELAY_MS + LOCK_TIME_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockRetryWait);
    TEST_ASSERT_TRUE(controller.faultIndicated());

    // A late physical success during the wait clears retries and indication.
    inputs.boltLocked = true;
    controller.tick(11500UL, inputs);
    controller.tick(11500UL + DEBOUNCE_BOLT_MS, inputs);
    EXPECT_STATE(controller, DoorState::LockedClosed);
    TEST_ASSERT_FALSE(controller.faultIndicated());
}

// -----------------------------------------------------------------------------
// Controller-level output invariant checks.
// -----------------------------------------------------------------------------

void test_representative_sequence_always_has_state_appropriate_output(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, true, true);

    DoorController controller(inputs, 0);
    expectOutputMatchesState(controller);

    inputs.rfidActive = true;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_RFID_MS, inputs);
    expectOutputMatchesState(controller);

    inputs.boltLocked = false;
    controller.tick(200, inputs);
    controller.tick(200 + DEBOUNCE_BOLT_MS, inputs);
    expectOutputMatchesState(controller);

    inputs.doorClosed = false;
    controller.tick(300, inputs);
    controller.tick(300 + DEBOUNCE_DOOR_MS, inputs);
    expectOutputMatchesState(controller);

    inputs.doorClosed = true;
    controller.tick(400, inputs);
    controller.tick(400 + DEBOUNCE_DOOR_MS, inputs);
    expectOutputMatchesState(controller);
}

void test_mode_centre_off_shorter_than_settle_time_is_not_published(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, false);

    DoorController controller(inputs, 0);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    inputs.mode = OperatingMode::Disabled;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS - 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    // Move on to Standard before Disabled has been stable for a full second.
    inputs.mode = OperatingMode::Standard;
    controller.tick(100 + DEBOUNCE_MODE_MS - 1, inputs);
    controller.tick(100 + 2 * DEBOUNCE_MODE_MS - 1, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
}

void test_mode_change_while_open_restarts_max_open_timer_for_new_mode(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::Standard, false, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    // Change to Open Night.
    inputs.mode = OperatingMode::OpenNight;
    controller.tick(1000, inputs);
    controller.tick(1000 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    // Standard's old 5-minute deadline must no longer fault.
    controller.tick(MAX_OPEN_TIMEOUT_MS + 1, inputs);
    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    // But Open Night's two-hour deadline should.
    const uint32_t modeChangeTime = 1000 + DEBOUNCE_MODE_MS;

    controller.tick(
        modeChangeTime + OPEN_NIGHT_MAX_OPEN_TIMEOUT_MS - 1,
        inputs
    );
    EXPECT_STATE(controller, DoorState::UnlockedOpen);

    controller.tick(
        modeChangeTime + OPEN_NIGHT_MAX_OPEN_TIMEOUT_MS,
        inputs
    );
    EXPECT_STATE(controller, DoorState::Error);
    EXPECT_FAULT(controller, FaultCode::DoorOpenTooLong);
}

void test_stable_standard_mode_change_is_forwarded(void)
{
    ControllerInputs inputs =
        makeInputs(OperatingMode::OpenNight, true, false);

    DoorController controller(inputs, 0);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);

    inputs.mode = OperatingMode::Standard;
    controller.tick(100, inputs);
    controller.tick(100 + DEBOUNCE_MODE_MS, inputs);

    EXPECT_STATE(controller, DoorState::UnlockedClosed);
    EXPECT_COMMAND(controller, LockCommand::None);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_startup_standard_closed_locked);
    RUN_TEST(test_startup_standard_closed_unlocked_starts_locking);
    RUN_TEST(test_startup_open_night_closed_locked_starts_unlocking);
    RUN_TEST(test_startup_open_unlocked_does_not_drive_motor);
    RUN_TEST(test_startup_disabled_does_not_drive_motor);
    RUN_TEST(test_startup_disabled_open_and_bolt_locked_stays_disabled);
    RUN_TEST(test_disabled_contradiction_faults_when_standard_is_selected);
    RUN_TEST(test_startup_open_and_bolt_locked_enters_sensor_fault);
    RUN_TEST(test_startup_invalid_mode_enters_mode_fault);

    RUN_TEST(test_rfid_active_at_boot_is_not_a_release_request);
    RUN_TEST(test_exit_held_at_boot_is_honoured);
    RUN_TEST(test_exit_held_at_boot_with_unlocked_bolt_completes_release_immediately);
    RUN_TEST(test_release_during_locking_waits_for_lock_completion_then_unlocks);
    RUN_TEST(test_release_during_locking_timeout_completes_if_bolt_still_unlocked);
    RUN_TEST(test_exit_held_at_boot_is_ignored_in_disabled_mode);

    RUN_TEST(test_bool_debounce_ignores_short_pulse);
    RUN_TEST(test_bool_debounce_changes_exactly_at_threshold);
    RUN_TEST(test_value_debounce_ignores_short_invalid_mode);
    RUN_TEST(test_bool_debounce_handles_uint32_wraparound);

    RUN_TEST(test_boot_active_rfid_rearms_only_after_stable_inactive);
    RUN_TEST(test_rfid_after_rearming_generates_release);
    RUN_TEST(test_rfid_pulse_shorter_than_debounce_is_ignored);

    RUN_TEST(test_exit_press_shorter_than_debounce_is_ignored);
    RUN_TEST(test_stable_exit_press_generates_release);

    RUN_TEST(test_door_open_shorter_than_debounce_is_ignored);
    RUN_TEST(test_stable_door_open_is_forwarded_to_fsm);
    RUN_TEST(test_bolt_change_shorter_than_debounce_is_ignored);
    RUN_TEST(test_stable_manual_unlock_is_forwarded_to_fsm);

    RUN_TEST(test_short_invalid_mode_condition_is_ignored);
    RUN_TEST(test_stable_invalid_mode_is_forwarded_to_fsm);
    RUN_TEST(test_stable_open_night_mode_change_is_forwarded);

    RUN_TEST(test_lock_timeout_fires_at_exact_deadline);
    RUN_TEST(test_unlock_timeout_fires_at_exact_deadline);
    RUN_TEST(test_unlock_retry_wait_fires_at_exact_deadline);
    RUN_TEST(test_retry_wait_fires_at_exact_deadline);

    RUN_TEST(test_standard_auto_lock_fires_at_exact_deadline);
    RUN_TEST(test_rfid_request_restarts_auto_lock_timer);
    RUN_TEST(test_exit_request_restarts_auto_lock_timer);
    RUN_TEST(test_open_night_timer_uses_existing_two_hour_setting);

    RUN_TEST(test_open_warning_fires_without_faulting);
    RUN_TEST(test_max_open_timeout_enters_fault);
    RUN_TEST(test_open_night_max_open_timeout_is_two_hours);
    RUN_TEST(test_closing_door_cancels_old_open_timers);

    RUN_TEST(test_successful_lock_cancels_old_lock_timeout);
    RUN_TEST(test_successful_unlock_cancels_old_unlock_timeout);

    RUN_TEST(test_lock_timeout_handles_millis_wraparound);
    RUN_TEST(test_auto_lock_handles_millis_wraparound);

    RUN_TEST(test_bolt_success_wins_if_it_debounces_on_lock_timeout_tick);
    RUN_TEST(test_door_open_wins_if_it_debounces_on_auto_lock_tick);
    RUN_TEST(test_disabled_mode_wins_if_it_debounces_on_lock_timeout_tick);
    RUN_TEST(test_rfid_request_wins_if_it_debounces_on_auto_lock_tick);

    RUN_TEST(test_lock_attempt_uses_backoff_then_ends_in_lock_failed);
    RUN_TEST(test_success_during_lock_retry_wait_prevents_another_attempt);
    RUN_TEST(test_fault_indicator_activates_after_second_failed_lock_attempt);

    RUN_TEST(test_representative_sequence_always_has_state_appropriate_output);
    RUN_TEST(test_mode_centre_off_shorter_than_settle_time_is_not_published);
    RUN_TEST(test_mode_change_while_open_restarts_max_open_timer_for_new_mode);
    RUN_TEST(test_stable_standard_mode_change_is_forwarded);

    return UNITY_END();
}
