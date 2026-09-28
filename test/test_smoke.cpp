#include "unity.h"
#include "door_fsm.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_locked_closed_has_no_motor_output(void)
{
    DoorFsm fsm(
        DoorState::LockedClosed,
        OperatingMode::Standard,
        true,   // door closed
        true    // bolt locked
    );

    fsm.start();

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(DoorState::LockedClosed),
        static_cast<uint8_t>(fsm.state())
    );

    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(LockCommand::None),
        static_cast<uint8_t>(fsm.lockCommand())
    );
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_locked_closed_has_no_motor_output);

    return UNITY_END();
}