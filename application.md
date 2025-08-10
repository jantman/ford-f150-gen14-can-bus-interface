# Application Introduction

We will be writing a software application that is described below. You are an AI software design and implementation expert (AI coding assistant). Our task, collaboratively, is to perform the following steps:

1. Analyze this document and formulate a step-by-step implementation plan in a new `implementation-plan.md` document. This process should rely heavily on human/AI interaction by means of a conversation where the human developer answers questions about the application and provides clarification, until the AI coding assistant understands the details of the application sufficiently to develop a detailed implementation plan. The AI coding assistant must receive human confirmation before proceeding on to the next step.
2. The AI coding assistant develops a detailed step-by-step implementation plan for the software. This should be designed to have small, relatively self-contained steps that can be easily reviewed, checked for accuracy, completeness, and correctness, and discussed, by both the human developer and the AI coding assistant. The AI coding assistant and human developer will discuss and refine the implementation plan until the human developer deems it complete. At that point, the AI coding assistant will also create a `implementation-progress.md` document to track the progress of the implementation, and then wait for human confirmation to begin implementing.
3. The AI coding assistant will implement the steps of the plan, in order. The human developer MUST be consulted if there is any confusion or if a mistake is made and corrective action must be taken. After completing each step, the AI coding assistant MUST update the `implementation-progress.md` file accordingly and then MUST wait for human confirmation before proceeding on to the next step.

## Application Description

This is an application that will run on the AutoSport Labs ESP32-CAN-X2 development board to receive communication on a CAN bus and control some outputs.

- **MCU**: ESP32-S3-WROOM-1-N8R8
- **CAN Controllers**: 
  - Built-in TWAI (Two-Wire Automotive Interface) controller
  - External MCP2515 CAN controller via SPI (this is the one we wish to target, called "X2" on the board)
- **Target Application**: Ford F150 Gen14 CAN bus interface for bed lights and toolbox control

High-level functionality:

* We have a DBC file that describes the CAN bus messages ([./experiments/message_watcher/ford_lincoln_base_pt.dbc](./experiments/message_watcher/ford_lincoln_base_pt.dbc)) but it's too large for use on a microcontroller. We have a [minimal version of it](./experiments/message_watcher/minimal.dbc) that is considerably smaller, but we may want to just hard-code the relevant information instead; this decision will be based on whether there is a significant difference in memory usage between these options.
* Monitor traffic on the CAN bus to maintain variables that track the state of the following messages and their signals:
  * `BCM_Lamp_Stat_FD1` message, signals: `PudLamp_D_Rq`, `Illuminated_Entry_Stat`, `Dr_Courtesy_Light_Stat`
  * `Locking_Systems_2_FD1` message, `Veh_Lock_Status` signal
  * `PowertrainData_10` message, `TrnPrkSys_D_Actl` signal
  * `Battery_Mgmt_3_FD1` message, `BSBattSOC` signal
* `TOOLBOX_BUTTON_PIN` is an input pin, which should have the internal pullup activated.
* When `PudLamp_D_Rq` changes to `RAMP_UP` or `ON`, turn on the `BEDLIGHT_PIN` output.
* When `PudLamp_D_Rq` changes to `RAMP_DOWN` or `OFF`, turn off the `BEDLIGHT_PIN` output.
* **Manual Bed Light Override**: When the toolbox button is double-clicked (pressed twice within 300ms) AND the vehicle is unlocked, toggle the bed light manual override mode. In manual mode, the bed lights operate independently of CAN bus signals. Manual override is automatically cleared when the vehicle is locked. Button input is ignored when vehicle is locked for security.
* Read `TOOLBOX_BUTTON_PIN` as an input with debouncing, pulled high. When the button is held for 1000ms (configurable via `BUTTON_HOLD_THRESHOLD_MS`), if `Veh_Lock_Status` is `UNLOCK_DRV` or `UNLOCK_ALL` AND `TrnPrkSys_D_Actl` is `Park`, then turn the `TOOLBOX_OPENER_PIN` output on for 500ms (configurable duration via a constant). The button must be held for the full threshold time before activation occurs - brief presses are ignored.
* All changes to inputs and CAN signal state should be logged.

## Implementation Details

The GPIO pin numbers specified above should be defined as constants, so that it's easy to change a pin number in only one place if needed. Initially, we will have the following assignments:

* `BEDLIGHT_PIN` GPIO5
* `TOOLBOX_OPENER_PIN` GPIO4
* `TOOLBOX_BUTTON_PIN` GPIO17
* `SYSTEM_READY_PIN` GPIO18

**Available for Future Use:**
* GPIO15 (previously `UNLOCKED_LED_PIN` - removed as LED was for initial hardware testing only)
* GPIO16 (previously `PARKED_LED_PIN` - removed as LED was for initial hardware testing only)
