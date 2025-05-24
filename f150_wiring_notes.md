# 2025 Ford F150 Lariat V8 - Wiring and CAN Notes

**Note:** Sorry to any of my currently or historically British friends, but I use the term "Driver side" and "Passenger side" to to refer to left-hand-drive vehicles, as that's what I have and the only service manual that I have. I'm not sure what's different in right-hand-drive vehicles.

## CAN Messages

* Interior ambient lighting: FD-CAN1, HS-CAN3, and LIN.
* Lock system: FD-CAN1, FD-CAN3, HS-CAN3, MS-CAN1

## CAN Busses, Circuits, and Connectors

The easiest places to access the various busses for testing/experimentation are:

* FD-CAN1, HS-CAN2, MS-CAN1 - C210 and C212 inline connectors behind lower glove box
* HS-CAN3 - C3154A or C3155A, Audio DSP Module ("amplifier") on the rear wall of the cab behind the passenger seat, behind the insulation

### FD-CAN1

Flexible Data rate CAN bus; this _should_ be an arbitration bitrate of 1Mbps and a data bitrate of 2Mbps, though I never successfully connected.

Circuits `BDB03` “Diagnostic # CAN FD Bus High Speed High” and `BDB04` “Diagnostic # CAN FD Bus High Speed Low”

Connectors:

* C135 ABS Module
* C175B PCM
* C210 inline (behind glove box)
* C1026 inline
* C1035E BCMC
* C1232B PCM
* C1233B PCM
* C1381B PCM
* C1458A SOBDMC
* C1591 inline
* C2280F BCM
* C2371A TCCM
* C2431A GWM
* C4396 VDM

### FD-CAN2

Flexible Data rate CAN bus; this _should_ be an arbitration bitrate of 1Mbps and a data bitrate of 2Mbps, though I never successfully connected.

Circuits `BDB01` “Diagnostic # CAN FD Bus High Speed 2 High” and `BDB02` “Diagnostic # CAN FD Bus High Speed 2 Low”

Connectors: 

* C248 inline
* C2431A GWM
* C3221B Upfitter Customization Interface Module (UCIM) *(may not be present; isn't on my vehicle)*
* C4803A Telematics Control Unit (TCU) Module

### FD-CAN3

Flexible Data rate CAN bus; this _should_ be an arbitration bitrate of 1Mbps and a data bitrate of 2Mbps, though I never successfully connected.

Circuits `BDB05` “Diagnostic # CAN FD Bus High Speed 3 High” and `BDB06` “Diagnostic # CAN FD Bus High Speed 3 Low”

Connectors: 

* C145 inline
* C215 inline
* C226A Steering Column Control Module (SCCM)
* C238 inline
* C242B IPMA
* C265 inline
* C501A Driver Door Module (DDM)
* C511 inline
* C610 inline
* C652A Passenger Door Module (PDM)
* C1463A Power Steering Control Module (PSCM)
* C2129 Headlamp Control Module (HCM)
* C2431A GWM
* C2503 Gear Shift Module (GSM)
* C2826A Driver Status Monitor Camera Module (CMR)
* C3245 Gear Shift Module (GSM)

### Diagnostic bus 1

500Kbps bitrate HS-CAN, from the Gateway Module (GWM) to the OBD DLC.

Circuits `VDB33` “Diagnostic # CAN Bus High Speed Diag 1 High” and `VDB34` “Diagnostic # CAN Bus High Speed Diag 1 Low”

Connectors:

* C2122 DLC
* C2431A GWM

### Diagnostic bus 2

500Kbps bitrate HS-CAN, from the Gateway Module (GWM) to the OBD DLC.

Circuits `VDB35` “Diagnostic # CAN Bus High Speed Diag 2 High” and `VDB36` “Diagnostic # CAN Bus High Speed Diag 2 Low”

Connectors:

* C2122 DLC
* C2431A GWM

### HS-CAN2

500Kbps bitrate HS-CAN

Circuits `VDB25` “Diagnostic # CAN Bus High Speed 2 (Upper) High” and `VDB26` “Diagnostic # CAN Bus High Speed 2 (Upper) Low”

Connectors:

* C145 inline
* C146 inline
* C210 inline (behind glove box)
* C265 inline
* C310B Restraints Control Module (RCM)
* C312 inline
* C406 inline
* C1035G BCMC
* C1457B DC/DC Converter Control Module
* C1591 inline
* C1803A Air Conditioning Control Module (ACCM)
* C2071 All Terrain Control Module (ATCM)
* C2431A GWM
* C2828 Pedestrian Alert Control Module (PACM)
* C3285 Occupant Classification System Module (OCSM)
* C3501C DC/AC Inverter
* C4630A DC/AC Inverter

### HS-CAN3

500Kbps bitrate HS-CAN

Circuits `VDB29` “Diagnostic # CAN Bus High Speed 3 (Lower) High” and `VDB30` “Diagnostic # CAN Bus High Speed 3 (Lower) Low”

Connectors:

* C215 inline (far front of center console, right side near top)
* C220A IPC (behind touchscreen)
* C240A Audio Front Control Module (ACM) (behind touchscreen)
* C248 inline (underneath center console towards front; maybe under wireless charger)
* C390 Wireless Accessory Charging Module (WACM) (in center console under/near wireless charging pad)
* C2342B Accessory Protocol Interface Module (APIM) (plug-in electric (BEV) only)
* C2383A APIM (behind touchscreen)
* C2431A GWM (behind touchscreen)
* C2498C Trailer Module (TRM) (behind driver's side dash, forward of A pillar)
* C2877 Head Up Display (top of driver's side dash behind gauge cluster; where the HUD is or would be)
* C3154A Audio DSP Module (rear wall of cab behind rear passenger seat, just in front of cab vent)
* C3155A Audio DSP Module (rear wall of cab behind rear passenger seat, just in front of cab vent)

### MS-CAN1

125Kbps MS-CAN

Circuits `VDB06` “Diagnostic # CAN Bus Medium Speed High” and `VDB07` “Diagnostic # CAN Bus Medium Speed Low”

* C212 inline (behind glove box)
* C228A HVAC control mod (behind HVAC controls)
* C263 inline (driver's side A-pillar below dash)
* C311 inline (large connector under driver's seat)
* C312 inline (large connector under passenger seat)
* C341D Driver Front Seat Module (DSM) (under/inside driver's seat)
* C2431A GWM (behind touch screen)
* C3050 inline (under/inside driver's seat)
* C3052 inline (under/inside passenger seat)
* C3385 Driver Multi-Contour Seat Module (SCMG) (inside back of driver's seat, if equipped)
* C3386 Passenger Multi-Contour Seat Module (SCMH) (inside back of driver's seat, if equipped)
* C9026 RTM (above rear window, passenger side)

### MS-CAN2

125Kbps MS-CAN

Circuits `VDB27` “Diagnostic # CAN Bus Medium Speed 2 High” and `VDB28` “Diagnostic # CAN Bus Medium Speed 2 Low”

Connectors:

* C238 inline (driver's side A-pillar below dash)
* C316 inline (outside of firewall, above/behind driver's side front wheel)
* C423 inline (rear underside of bed; see note below)
* C431 trailer tow connector (rear underside of bed; see note below)
* C437 inline (inside tailgate; depending on tailgate style)
* C438 inline (inside tailgate; depending on tailgate style)
* C439 trailer tow connector (rear underside of bed; see note below)
* C2431A GWM (behind touchscreen)
* C4100 inline (rear underside of bed; black 12-pin connector)
* C4623 Rear Gate Trunk Module (RGTM) (inside tailgate; depending on tailgate style)
* C4724 Rear Gate Trunk Module (RGTM) (inside tailgate; depending on tailgate style)

## Connector Notes

* C210 - Second-from-the-left of the four inline connectors directly behind the glove box. Carries FD-CAN1 and HS-CAN2.
* C211 - Second-from-the-right (or rightmost of the group of 3) of the four inline connectors directly behind the glove box. Does not carry any CAN.
* C212 - Rightmost of the four inline connectors directly behind the glove box; noticeably separated to the right from closely-spaced group of C213/C210/C211. Carries MS-CAN1.
* C213 Leftmost of the four inline connectors directly behind the glove box. Does not carry any CAN.
* C423 - the huge (34-pin) locking black connector for the tailgate and trailer harnesses. Service pigtails are available (WPT-1606 / KU2Z-14S411-SA male and WPT-1607 / KU2Z-14S411-TA female) but very expensive.
* C431 - Early Production variation of C439; this is the 12-pin black "trailer tow connector" under the bed near the tailgate, that's generally plugged into an empty connector with an OBD DLC-like connector on the other side, as a weather cap. This is used for the trailer yaw sensor and trailer camera options, and carries `MS-CAN2` (`VDB27` / `VDB28`). On my vehicle, with the ignition off, this seems to just broadcast `0x227` all the time. The connector is a Molex 33472-6324.
* C439 - Late Production variation of C431; also carries `MS-CAN2`.
* C3154A - Audio DSP Module ("stereo amplifier") on the rear wall of the cab behind the passenger seat, behind the insulation. Carries HS-CAN3. This connector is a Molex 34728-0200 with a combination of 19, 20, and 22 gauge wires. HS-CAN3 High is on pin 1, 22ga Green-Blue wire; HS-CAN3 Low is on pin 11, 22ga White-Green wire. The Molex 34728-0200 is a proprietary connector.
  * C3154B also connects to the Audio DSP Module and in my vehicle (standard 8-speaker B&O stereo) just has four pins populated for ground and power. This is a Molex 34728-0080 with 19ga and possibly one 22ga wires. Pins 1 and 5 are circuit GD348 ground to the C pillar, 19ga Black-Yellow wire; pins 4 and 8 are circuit SBA40 to Fuse 40, always hot, 19ga Red wire.
