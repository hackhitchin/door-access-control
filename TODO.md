# TODO

## Hardware
## Hardware changes / decisions
- [ ] Replace existing key switch with three-position key switch
- [ ] Select three available Nano inputs for Disabled / Standard / OpenNight
- [ ] Define electrical wiring of the three-position switch
- [ ] Update DipTrace schematic for new key switch wiring
- [ ] Confirm/configure ESP-RFID unlock pulse duration
- [ ] Decide whether to fit backup maglock
- [ ] If fitted, design backup maglock interface and failure behaviour
- [ ] Design independent watchdog/monitoring system
- [ ] Decide whether to use existing FAULT relay/output for diagnostics

## FSM / behaviour
- [ ] Update FSM diagram to distinguish RFID unlock from exit-button unlock
- [ ] Add three operating modes: Disabled / Standard / Open Night
- [ ] Add invalid key-switch state handling
- [ ] Define startup / recovery behaviour for every stable physical state
- [ ] Ignore RFID release already active at boot until it clears
- [ ] Honour exit button if held during boot
- [ ] Define behaviour for manual lock / unlock operation
- [ ] Define behaviour for contradictory door/bolt sensors
- [ ] Define behaviour when release request remains active
- [ ] Define exact Standard-mode auto-lock timing
- [ ] Define exact Open Night timing
- [ ] Define lock timeout
- [ ] Define unlock timeout
- [ ] Define maximum lock retry count
- [ ] Define maximum unlock retry count
- [ ] Define open timeout / maximum-open timeout behaviour
- [ ] Decide whether faults latch or clear automatically
- [ ] Decide which faults should survive a controller reset
Exit button while Disabled: should it still electronically unlock? - currently disabled means do nothing

## Outputs / actuation
- [ ] Confirm T1 remains held low for entire LOCKING state
- [ ] Confirm T2 remains held low for entire UNLOCKING state
- [ ] Confirm relay release behaviour on timeout / fault
- [ ] Ensure LOCK and UNLOCK can never be asserted simultaneously

## Documentation
- [ ] Add `docs/io-contract.md`
- [ ] Create `docs/transitions.md`
- [ ] Create `docs/invariants.md`
- [ ] Create `docs/faults.md`
- [ ] Create `docs/startup-recovery.md`
- [ ] Create `docs/test-plan.md`
- [ ] Store current FSM source + exported diagram
- [ ] Store lock and 433 MHz interface manuals
- [ ] Store current access-control system documentation
- [ ] Store DipTrace PCB source and schematic export
- [ ] Document wiring routes and wire colours
- [ ] Document DIN terminal assignments
- [ ] Store mechanical CAD for lock installation

## Software / testing
- [ ] Decide final FSM implementation library
- [ ] Decide whether to use ETL `state_chart`
- [ ] Set up native host tests
- [ ] Vendor or otherwise pin Unity test framework
- [ ] Set up GitHub Actions
- [ ] Add Arduino Nano compile check in CI
- [ ] Add host-side unit tests
- [ ] Add coverage reporting
- [ ] Decide required coverage threshold
- [ ] Add exhaustive state × event tests
- [ ] Add startup-state tests
- [ ] Add failure / retry / timeout sequence tests
- [ ] Add millis wraparound test