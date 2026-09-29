#include "src/pin_defs.h"
#include "src/controller_config.h"
#include "src/controller_types.h"
#include "src/door_fsm.h"
#include "src/door_controller.h"

namespace
{

bool activeLow(uint8_t pin)
{
    return digitalRead(pin) == LOW;
}

OperatingMode readOperatingMode()
{
    const bool standardSelected = activeLow(PIN_MODE_STANDARD);
    const bool openNightSelected = activeLow(PIN_MODE_OPEN_NIGHT);

    if (!standardSelected && !openNightSelected) {
        return OperatingMode::Disabled;
    }
    if (standardSelected && !openNightSelected) {
        return OperatingMode::Standard;
    }
    if (!standardSelected && openNightSelected) {
        return OperatingMode::OpenNight;
    }

    return OperatingMode::Invalid;
}

ControllerInputs readInputs()
{
    ControllerInputs inputs;
    inputs.doorClosed = activeLow(PIN_DOOR_CLOSED);
    inputs.boltLocked = activeLow(PIN_LOCK_STATE);
    inputs.rfidActive = activeLow(PIN_CARD_READER_J5);
    inputs.exitPressed = activeLow(PIN_EXIT_BUTTON);
    inputs.mode = readOperatingMode();
    return inputs;
}

void configurePins()
{
    pinMode(PIN_LOCK_STATE, INPUT_PULLUP);
    pinMode(PIN_EXIT_BUTTON, INPUT_PULLUP);
    pinMode(PIN_DOOR_CLOSED, INPUT_PULLUP);
    pinMode(PIN_CARD_READER_J5, INPUT_PULLUP);

    // Key-switch contacts pull these lines to GND when selected.
    pinMode(PIN_MODE_STANDARD, INPUT_PULLUP);
    pinMode(PIN_MODE_OPEN_NIGHT, INPUT_PULLUP);

    // Safe state before switching the relay-drive pins to outputs.
    digitalWrite(PIN_LOCK_RELAY, LOW);
    digitalWrite(PIN_UNLOCK_RELAY, LOW);
    digitalWrite(PIN_FAULT_RELAY, LOW);
    digitalWrite(PIN_BLUEBOARD_BUTTON, LOW);

    pinMode(PIN_LOCK_RELAY, OUTPUT);
    pinMode(PIN_UNLOCK_RELAY, OUTPUT);
    pinMode(PIN_FAULT_RELAY, OUTPUT);
    pinMode(PIN_BLUEBOARD_BUTTON, OUTPUT);
}

DoorController& controller()
{
    // First called from setup(), after pin configuration.
    // Function-local static avoids heap allocation.
    static DoorController instance(readInputs(), millis());
    return instance;
}

void applyLockCommand(LockCommand command)
{
    static LockCommand applied = LockCommand::None;

    if (command == applied) {
        return;
    }

    // Break-before-make prevents any overlap of T1 and T2.
    digitalWrite(PIN_LOCK_RELAY, LOW);
    digitalWrite(PIN_UNLOCK_RELAY, LOW);

    if (command == LockCommand::Lock) {
        digitalWrite(PIN_LOCK_RELAY, HIGH);
    } else if (command == LockCommand::Unlock) {
        digitalWrite(PIN_UNLOCK_RELAY, HIGH);
    }

    applied = command;
}

} // namespace

void setup()
{
    configurePins();

    DoorController& c = controller();
    applyLockCommand(c.lockCommand());
}

void loop()
{
    const uint32_t nowMs = millis();

    DoorController& c = controller();
    c.tick(nowMs, readInputs());

    applyLockCommand(c.lockCommand());
}
