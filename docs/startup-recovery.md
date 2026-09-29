# Startup and recovery

Status: implemented behaviour, September 2026.

## Startup input qualification

After GPIO configuration, the Arduino adapter keeps all relay outputs off and waits until the complete input set has remained unchanged for 100 ms. Qualification is capped at 2 s so a permanently chattering input cannot prevent boot forever; on timeout the latest sample is used and the normal debounce/fault logic takes over.

Runtime mode changes use a 1000 ms stable period. This is intentional for the centre-off maintained key switch: a normal transition through the centre position should not publish a temporary `Disabled` mode unless the switch is actually left there for at least one second.

## Startup reconstruction

- Disabled is authoritative: startup enters `Disabled` and does not actuate, regardless of door/bolt sensor combination.
- Standard + closed + locked -> `LockedClosed`.
- Standard + closed + unlocked -> `Locking`, unless a held exit request immediately releases it.
- Open Night + closed + locked -> `Unlocking`.
- Open Night + closed + unlocked -> `UnlockedClosed`.
- Enabled + open + unlocked -> `UnlockedOpen`.
- Enabled + open + locked -> `Error / DoorOpenBoltLocked`.
- Invalid mode -> `Error / InvalidMode`, unless the physical contradiction has higher priority.

RFID active at boot is deliberately disarmed until it first becomes stably inactive. A held exit button is honoured when electronic control is enabled.

## Release during an in-flight lock command

A release request received during `Locking` or `LockRetryWait` is queued rather than immediately changing to `Unlocking`. This avoids declaring the door released while a previously issued one-shot lock command may still complete. When the in-flight lock confirms `BoltLocked`, times out, or a pending retry wait expires, the pending request takes priority and the FSM enters `Unlocking` rather than starting/accepting another lock operation.

Unlock completion remains level-sensitive: after an event genuinely enters `Unlocking`, the controller checks whether the stable bolt level is already unlocked and, if so, immediately supplies `BoltUnlocked`.

## Error recovery

Present-tense observable faults have priority while `Error` is active:

1. door open + bolt locked -> `DoorOpenBoltLocked`;
2. invalid mode -> `InvalidMode`;
3. otherwise a historical operation failure such as `LockFailed` or `UnlockFailed` is retained until a valid recovery transition occurs.

Fault history is not persisted across reset.

## Relay startup and reversal

Relay outputs are set LOW before their pins become outputs. Whenever an energised lock/unlock relay is released, its direction and release time are remembered. The opposite relay may not energise until 250 ms later, even if one or more explicit `None` commands occur in between. Reasserting the same direction does not require dead time.
