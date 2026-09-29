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

bool sameInputs(const ControllerInputs& a, const ControllerInputs& b)
{
    return a.doorClosed == b.doorClosed &&
           a.boltLocked == b.boltLocked &&
           a.rfidActive == b.rfidActive &&
           a.exitPressed == b.exitPressed &&
           a.mode == b.mode;
}

ControllerInputs readStableStartupInputs()
{
    ControllerInputs candidate = readInputs();
    const uint32_t qualificationStartedMs = millis();
    uint32_t stableSinceMs = qualificationStartedMs;

    while (static_cast<uint32_t>(millis() - stableSinceMs) <
               STARTUP_INPUT_STABLE_MS &&
           static_cast<uint32_t>(millis() - qualificationStartedMs) <
               STARTUP_INPUT_TIMEOUT_MS) {
        const ControllerInputs current = readInputs();
        if (!sameInputs(current, candidate)) {
            candidate = current;
            stableSinceMs = millis();
        }
        delay(1);
    }

    // A permanently chattering input must not prevent the controller booting.
    // The normal debouncers and fault handling take over from this sample.
    return candidate;
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
    static const ControllerInputs stableInputs = readStableStartupInputs();
    static DoorController instance(stableInputs, millis());
    return instance;
}

void applyLockCommand(LockCommand command, uint32_t nowMs)
{
    static LockCommand energised = LockCommand::None;
    static LockCommand lastReleased = LockCommand::None;
    static bool releaseTimeValid = false;
    static uint32_t lastReleasedMs = 0;

    if (command == LockCommand::None) {
        digitalWrite(PIN_LOCK_RELAY, LOW);
        digitalWrite(PIN_UNLOCK_RELAY, LOW);

        if (energised != LockCommand::None) {
            lastReleased = energised;
            lastReleasedMs = nowMs;
            releaseTimeValid = true;
            energised = LockCommand::None;
        }
        return;
    }

    if (command == energised) {
        return;
    }

    if (energised != LockCommand::None) {
        // Release the old mechanical relay first. The requested opposite
        // direction is reconsidered on subsequent loop iterations.
        digitalWrite(PIN_LOCK_RELAY, LOW);
        digitalWrite(PIN_UNLOCK_RELAY, LOW);
        lastReleased = energised;
        lastReleasedMs = nowMs;
        releaseTimeValid = true;
        energised = LockCommand::None;
        return;
    }

    // Enforce dead time from the actual release instant, even if one or more
    // explicit None commands occurred between opposite directions. Reasserting
    // the same direction is safe and does not need the delay.
    if (releaseTimeValid &&
        command != lastReleased &&
        static_cast<uint32_t>(nowMs - lastReleasedMs) <
            RELAY_REVERSAL_DEADTIME_MS) {
        digitalWrite(PIN_LOCK_RELAY, LOW);
        digitalWrite(PIN_UNLOCK_RELAY, LOW);
        return;
    }

    if (command == LockCommand::Lock) {
        digitalWrite(PIN_LOCK_RELAY, HIGH);
    } else {
        digitalWrite(PIN_UNLOCK_RELAY, HIGH);
    }
    energised = command;
}

} // namespace

void setup()
{
    configurePins();

    DoorController& c = controller();
    applyLockCommand(c.lockCommand(), millis());
    digitalWrite(PIN_FAULT_RELAY, c.faultIndicated() ? HIGH : LOW);
}

void loop()
{
    const uint32_t nowMs = millis();

    DoorController& c = controller();
    c.tick(nowMs, readInputs());

    applyLockCommand(c.lockCommand(), nowMs);
    digitalWrite(PIN_FAULT_RELAY, c.faultIndicated() ? HIGH : LOW);
}
