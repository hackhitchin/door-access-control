# Startup and recovery

Status: implemented behaviour, September 2026.

## Startup input qualification

After GPIO configuration, the Arduino adapter keeps all relay outputs off and waits until the complete input set has remained unchanged for 100 ms. Only then is `DoorController` constructed. This prevents a single power-up sample from commanding the lock.

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

## Level-sensitive completion

Unlock completion is not edge-only. After any event that leaves the FSM in `Unlocking`, the controller checks whether the stable bolt level is already unlocked and, if so, immediately supplies `BoltUnlocked`. This covers release requests received while a lock attempt is already in progress, where the unlocked level predates the transition. No equivalent synthetic `BoltLocked` path is currently needed because there is no reachable transition into `Locking` with the stable bolt level already locked.

## Error recovery

Present-tense observable faults have priority while `Error` is active:

1. door open + bolt locked -> `DoorOpenBoltLocked`;
2. invalid mode -> `InvalidMode`;
3. otherwise a historical operation failure such as `LockFailed` or `UnlockFailed` is retained until a valid recovery transition occurs.

Fault history is not persisted across reset.

## Relay startup and reversal

Relay outputs are set LOW before their pins become outputs. A direct Lock<->Unlock reversal releases both relays for 250 ms before the opposite relay may energise. A request for `None` cancels any pending reversal immediately.
