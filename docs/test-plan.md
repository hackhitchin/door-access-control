# Door controller test plan

Status: pre-bench test plan.

## 1. Automated native tests

Run both Unity suites in CI. Required project-code coverage is 100% lines, 100% functions and at least 88% branches. Tests cover FSM transitions, faults, recovery, retries, debounce, timer deadlines, rollover and same-tick ordering.

Specific regression cases that must remain present include:

- entering `Unlocking` with the bolt already unlocked completes immediately;
- release during `Locking` cannot wait forever for a new bolt-unlocked edge;
- Disabled startup never actuates, including contradictory sensor levels;
- a centre-off mode transition shorter than 1000 ms is not published as Disabled;
- lock retries use 1/10/100/1000/10000 s back-off and end in `LockFailed`;
- Open Night permits up to two hours continuously open;
- present physical contradictions update the reported fault while already in `Error`;
- all timers remain correct across `uint32_t`/`millis()` wraparound.

## 2. Target compile

CI must separately compile the complete sketch for the classic ATmega328P Arduino Nano using the pinned Arduino AVR core and Embedded Template Library ETL release. Host compilation is not a substitute for this check.

## 3. Bench test before connecting the lock

With relay contacts unloaded, verify input polarity and stable interpretation for door reed, bolt sensor, RFID, exit button and all key-switch positions. Verify A3/A2 relay direction and confirm neither relay is energised during reset/startup.

Measure the relay contacts or HAI T1/T2 inputs during direct reversals. There must be at least 250 ms with both commands released.

## 4. Bench test with HAI/Utopic connected

Verify one complete successful lock and unlock, then deliberately suppress the expected bolt sensor confirmation to exercise timeout/retry behaviour. Confirm T1/T2 are released during every retry wait and that door opening or an access request cancels a lock retry sequence.

Before deployment, determine the HAI command contract (edge/level behaviour, minimum pulse, JP1 setting, and behaviour if T1+T2 are both asserted) and the Utopic behaviour on command reversal.

## 5. Startup matrix

Exercise every relevant combination of mode, door level, bolt level, held exit and RFID-active-at-boot. At minimum, automate the 4 x 2 x 2 x 2 x 2 matrix and assert state, fault and motor command. RFID-active-at-boot must never count as a fresh release.

## 6. Further robustness tests

Before deployment, add a small simulated plant model in which bolt state follows lock/unlock commands after a delay, plus a deterministic seeded random walk over raw inputs and time. Assert output invariants on every tick.
