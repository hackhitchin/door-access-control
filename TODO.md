# TODO

## Hardware changes / decisions

- [ ] Replace existing key switch with three-position maintained key switch
- [x] Allocate Nano inputs for the three operating modes using two signals:
      D8 = Standard, D9 = Open Night, neither = Disabled, both = Invalid
- [ ] Confirm the exact contact/wiring arrangement of the purchased key switch
      matches the active-low `INPUT_PULLUP` scheme
- [ ] Update DipTrace schematic for the new key-switch wiring
- [ ] Confirm/configure ESP-RFID unlock pulse duration
- [ ] Decide whether to fit backup maglock
- [ ] If fitted, design backup-maglock interface and failure behaviour
- [ ] Design independent watchdog / monitoring system
- [x] Drive existing FAULT relay for FSM faults and degraded extended lock retry

## FSM / behaviour

- [ ] Update the FSM diagram to distinguish RFID release from exit-button release
- [x] Implement Disabled / Standard / Open Night operating modes
- [x] Implement invalid key-switch state handling
- [x] Define and implement startup reconstruction for current physical states
- [x] Ignore RFID release already active at boot until it clears
- [x] Honour exit button if held during boot when electronic control is enabled
- [x] Define and implement manual lock / unlock behaviour
- [x] Define and implement contradictory door/bolt sensor handling
- [x] Define behaviour when a release request remains active
- [x] Define Standard-mode auto-lock timing: 5 s
- [x] Define current Open Night timing: 2 h
- [x] Define lock timeout: 5 s
- [x] Define unlock timeout: 5 s
- [x] Define lock retry policy: 5 retries after the initial attempt with 1/10/100/1000/10000 s back-off
- [x] Define maximum unlock retries: 2 after the initial attempt
- [x] Define open warning / maximum-open behaviour: 30 s warning; 5 min Standard fault; 2 h Open Night fault
- [x] Faults auto-clear when the underlying condition is gone and a valid state
      can be reconstructed
- [x] Fault history does not survive controller reset
- [ ] Revisit whether the exit button should remain ignored in Disabled mode
- [ ] Review whether startup reconstruction belongs inside `DoorController` or
      should move into the FSM / a dedicated recovery entry point

## Outputs / actuation

- [x] T1 remains asserted for the complete `Locking` state
- [x] T2 remains asserted for the complete `Unlocking` state
- [x] Lock/unlock outputs are released during retry wait, Disabled and Error
- [x] LOCK and UNLOCK cannot be intentionally asserted simultaneously
- [x] Arduino output adapter enforces 250 ms opposite-direction dead time across intermediate `None` commands
- [ ] Bench-test relay polarity and break-before-make behaviour on a real Nano /
      controller PCB

## Arduino / controller wrapper

- [x] Add thin `door-access-control.ino` hardware adapter
- [x] Add central `src/pin_defs.h`
- [x] Add controller timing/debounce configuration
- [x] Add input debounce layer
- [x] Add RFID boot disarm / re-arm logic
- [x] Add exit-held-at-boot handling
- [x] Add timeout/event generation around the FSM
- [x] Use wrap-safe `uint32_t` timing arithmetic
- [x] Add controller/wrapper native test suite
- [x] Confirm controller/wrapper tests pass in GitHub Actions
- [x] Bring controller/wrapper code back to required coverage thresholds
- [x] Add Arduino Nano compile check in CI
- [x] Compile the complete firmware with the Arduino toolchain
- [ ] Bench-test all physical inputs, FAULT indication and relay outputs before door installation

## Documentation

- [x] Add/update `docs/io-contract.md`
- [x] Add/update `docs/transitions.md`
- [x] Add/update `docs/parameters.md`
- [x] Add/update `docs/invariants.md`
- [x] Add/update `docs/faults.md`
- [x] Add/update `docs/startup-recovery.md`
- [x] Add/update `docs/test-plan.md`
- [x] Consolidate controller/wrapper coverage into canonical `docs/test-plan.md`
- [ ] Store the current editable FSM source plus exported diagram
- [x] Store lock and 433 MHz interface manuals under `reference/lock/`
- [x] Store current access-control system documentation under `reference/`
- [x] Store DipTrace PCB source and schematic exports under `hardware/pcb/`
- [x] Document wiring routes and wire colours
- [x] Document DIN terminal assignments
- [x] Store mechanical CAD for lock installation under `mechanical/lock/`

## Software / testing

- [x] Select ETL `state_chart` as the FSM implementation
- [x] Set up native host tests using Unity
- [x] Vendor Unity test framework in the repository
- [x] Set up GitHub Actions
- [x] Add FSM host-side transition/fault/sequence tests
- [x] Add exhaustive working-state × event safety tests
- [x] Add coverage reporting with LCOV/GCOV
- [x] Enforce 100% line coverage for instrumented project code
- [x] Enforce 100% function coverage for instrumented project code
- [x] Set branch coverage threshold to >= 88% to allow documented unreachable
      compiler-generated short-circuit branches
- [x] Add startup-state tests for the current controller implementation
- [x] Add failure / retry / timeout sequence tests
- [x] Add `millis()` wraparound tests for the controller layer
- [x] Pin ETL used by native and Arduino CI builds to 20.48.1
- [ ] Decide whether Unity should also be version-pinned/documented explicitly
- [x] Add a CI build that verifies the code fits and compiles for Arduino Nano
- [x] Add optional compile-time serial diagnostics for bench testing
- [ ] Add hardware-in-the-loop / bench test procedure before deployment

## Deployment / validation

- [ ] Verify exact installed wiring against `pin_defs.h`
- [ ] Verify key-switch positions electrically
- [ ] Verify door reed polarity
- [ ] Verify PR12-4DN lock sensor polarity and thresholds
- [ ] Verify BlueBoard release polarity and pulse duration
- [ ] Verify exit-button polarity
- [ ] Verify T1 lock direction
- [ ] Verify T2 unlock direction
- [ ] Test every operating mode on bench hardware
- [ ] Test power-up in each relevant physical door/bolt/mode combination
- [ ] Test lock and unlock timeout/retry behaviour on real hardware
- [ ] Test sensor contradiction fault on bench hardware
- [ ] Test recovery from each observable fault
- [ ] Review final firmware and test results before installing on the real door
