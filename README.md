# ford-f150-can-experiments

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
udo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536
```

**MS-CAN:**

```bash
sudo ip link set can0 type can bitrate 125000 listen-only on restart-ms 1000
udo ip link set can0 up
sudo ifconfig can0 txqueuelen 65536
```

### What Messages Are Where?

* ``BCM_Lamp_Stat_FD1`` (`0x3c3`) - **Present** on: MS-CAN1, HS-CAN2; **Not Present** on: .
* ``Locking_Systems_2_FD1`` (``0x331``) - **Present** on: MS-CAN1; **Not Present** on: HS-CAN2.
