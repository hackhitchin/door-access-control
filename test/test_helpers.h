#pragma once
#include "unity.h"
#include "door_fsm.h"

#define EXPECT_STATE(fsm, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((fsm).state()))

#define EXPECT_COMMAND(fsm, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((fsm).lockCommand()))

#define EXPECT_EFFECT(fsm, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((fsm).effect()))

#define EXPECT_FAULT(fsm, expected) \
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), \
                            static_cast<uint8_t>((fsm).fault()))
