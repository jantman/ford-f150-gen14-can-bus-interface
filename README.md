# Ford F150 Gen14 CAN bus Interface

Interface for doing various things in response to messages on the CAN bus of my 2025 (Generation 14) Ford F150, along with details of construction, initial investigation/experiments, wiring info, etc.

## What does it do?

* Trigger a relay for OEM-style aftermarket bed lights, when the CHMSL bed lights are on.
* Allow the truck bed toolbox to be unlocked via two buttons, only when the doors are unlocked and the vehicle is in park.
* ... whatever else I think of in the future? (there's substantial room for expansion built-in)

## Contents - What's Here?

* [experiments/](experiments/) - Initial investigation and experiments with the CAN bus as well as vehicle-specific wiring and connector information.
* [f150_wiring_notes.md](f150_wiring_notes.md) - Information for my 2025 F150 Lariat V8 on what circuits are in what connectors, etc.

## Software

TBD.

## Hardware

TBD.

## History of Project

Vehicles have more and more intelligent components over time and fewer and fewer simple circuits. It was hard enough for me to find a truck that met most of what I wanted, and I was in a bit of a rush to get rid of my previous vehicle, so I ended up buying a truck with the 501A Mid trim package (no interior ambient lighting) and without the Bed Utility Package (no factory bed lighting, and no wiring for it). Beyond that, I also wanted the truck bed toolbox to... not have a giant key, and not have to be manually re-locked after unlocking. WeatherGuard makes a lock actuator kit that fits the toolbox, but on Gen 14 F150s only the low trim lines have a separate lock circuit for the tailgate (the Remote Release and Power Tailgate have more complicated systems controlled by a module inside the tailgate).

So, I set out to try and use the vehicle's CAN busses to do what I want. I knew from the factory service manual a pretty good idea of what the various busses are and where their circuits are accessible, and a vague idea of which busses would carry the messages I'm interested in, but not much more than that. I tried tapping into the busses using a Raspberry Pi with a [Waveshare CAN-FD HAT](https://www.waveshare.com/wiki/2-CH_CAN_FD_HAT), and quickly determined that my first choice connection point at the rear of the vehicle near the trailer connector didn't seem to be broadcasting any useful messages (at least when the vehicle wasn't running), so I resorted to doing my initial experimentation by back-probing the inline connectors behind the glove box.

After doing a bunch of research and not being able to find any write-ups on what I wanted (and specifically nothing on the exact CAN messages for door locks, bed lights, etc.), I resorted to writing (or, having GitHub CoPilot / Claude Sonnet) write me a small Python program ([can_action_analyzer.py](experiments/can_analyzer/can_action_analyzer.py)) to run on the Pi that would capture CAN traffic while I repeatedly toggled the door locks and bed lights, and would attempt to analyze the results for patterns clearly related to the toggling of state. I wasted most of a weekend on this without any reliable results.

The real breakthrough came when someone at work (thanks, Wolfy!) mentioned to me that they were playing around with [OpenPilot](https://github.com/commaai/openpilot), that it's Python based and knows how to do _all sorts_ of things with supported vehicles, and that the current-generation (Gen14) F150 is very well supported. That led me to the [opendbc](https://github.com/commaai/opendbc) project and then to [ford_lincoln_base_pt.dbc](https://github.com/commaai/opendbc/blob/ca8673cbc8f4709700ad223914dc359ed63a4835/opendbc/dbc/ford_lincoln_base_pt.dbc) which contains message information for everything I needed and much, much more! After that it was just a matter of asking CoPilot to write a console-based program to display the values of the signals I was interested in ([can_dashboard.py](experiments/message_watcher/can_dashboard.py)) to let me confirm that they change as expected - and they did!

Since I bought this truck brand new and intended to buy an extended warranty and keep it for quite a while, I was rather obsessed with not modifying the factory wiring at all - everything had to be able to be un-done, which meant making adapter harnesses to fit factory connectors. I determined that HS-CAN3 was the bus that carried all of the signals I was interested in, so it was another trip back to the service manual to enumerate all of the connectors that carry HS-CAN3 (see [f150_wiring_notes.md](f150_wiring_notes.md)). I ended up determining that the best for my needs was C3154A, the main connector for the audio amplifier mounted on the rear wall of the cab behind the rear passenger seat. This not only carries HS-CAN3 but also has power and ground available on the nearby C3154B connector, and the area behind the rear seat provides more than enough space to locate an enclosure for a small microcontroller as well as easy access to run wiring to the bed via the rear cab vents.

With that done, it was just a matter of sourcing the proper connectors, programming a microcontroller, and wiring everything up. See the relevant hardware and software sections above for the details.
