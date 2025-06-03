# Ford F150 Gen14 CAN bus Interface - Experiments

Experiments with the CAN networks on my 2025 Ford F150 Lariat V8.

## What's here?

* [can_analyzer/](can_analyzer/) - A script for experimentally watching and logging CAN bus traffic, and also for recording traffic around events (such as toggling door locks or lights) and analyzing likely-related messages.
* [can_decoder/](can_decoder/) - A script using [a Ford DBC file from comma.ai OpenDBC](https://github.com/commaai/opendbc/blob/master/opendbc/dbc/ford_lincoln_base_pt.dbc) to watch for and display changes to specific CAN messages. Also includes a script to watch the value of certain signals in a dashboard-like view.

## Vehicle Specific Notes

I have a bunch of notes culled from the service manual, such as connector pinouts and locations, CAN information, etc. in [f150_wiring_notes.md](f150_wiring_notes.md).

## Connecting to CAN Bus

For testing I use a Raspberry Pi 4B with a [Waveshare 2-channel CAN FD HAT](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT).

Assuming we're plugged in to the `can0` interface:

**FD-CAN:**

```bash
sudo ip link set can0 type can bitrate 500000 listen-only on restart-ms 1000
sudo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536
```

**HS-CAN:**

```bash
sudo ip link set can0 type can bitrate 500000 listen-only on restart-ms 1000
sudo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536
```

**MS-CAN:**

```bash
sudo ip link set can0 type can bitrate 125000 listen-only on restart-ms 1000
sudo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536
```

### What Messages Are Where?

* ``BCM_Lamp_Stat_FD1`` (`0x3c3`) - **Present** on: MS-CAN1, HS-CAN2, HS-CAN3.
* ``Locking_Systems_2_FD1`` (``0x331``) - **Present** on: MS-CAN1, HS-CAN3; **Not Present** on: HS-CAN2.

### What Messages Am I Interested In?

* `BCM_Lamp_Stat_FD1`
  * `PudLamp_D_Rq` - Exterior puddle and bed lighting. Bed lights are on when value is `ON` or `RAMP_UP`. `3 "RAMP_DOWN" 2 "RAMP_UP" 1 "ON" 0 "OFF"`
  * `Dr_Courtesy_Light_Stat` - Driver courtesy light status. Should be on when Driver's door is open. `3 "Invalid" 2 "Unknown" 1 "On" 0 "Off"`
* `Locking_Systems_2_FD1`
  * `Veh_Lock_Status` - Door lock status. We're interested in all options: `3 "UNLOCK_DRV" 2 "UNLOCK_ALL" 1 "LOCK_ALL" 0 "LOCK_DBL"`
* `PowertrainData_10`
  * `TrnPrkSys_D_Actl` - In practice we should only see `Park` or `OutOfPark`. `15 "Faulty" 14 "NotUsed_5" 13 "NotUsed_4" 12 "NotUsed_3" 11 "NotUsed_2" 10 "NotUsed_1" 9 "FrequencyError" 8 "OutOfRangeHigh" 7 "OutOfRangeLow" 6 "Override" 5 "OutOfPark" 4 "TransitionCloseToOutOfPark" 3 "AtNoSpring" 2 "TransitionCloseToPark" 1 "Park" 0 "NotKnown"`
* `EngVehicleSpThrottle`
  * `EngAout_N_Actl` - Engine RPM.
* `Battery_Mgmt_3_FD1`
  * `BSBattSOC` - Battery state of charge percentage. According to the service manual for my truck, with the engine running, first stage load shedding begins at 62% SoC and second stage load shedding begins at 50%. With the engine off, the SoC threshold is 40% when battery temperature is above freezing, higher threshold at lower temperatures.
