---
layout: page
title: Electronics
permalink: /electronics/
---

The control of the home-made motor is pretty much like the control of any
brushless DC motor (BLDC). The main hardware components on the electronics side
are a [NUCLEO-F401RE](https://www.st.com/en/evaluation-tools/nucleo-f401re.html)
development board from ST, a [motor control
shield](https://www.infineon.com/cms/en/product/evaluation-boards/bldc-shield_ifx007t/)
by Infineon and a basic comparator circuit I put together to interface to the
Hall sensors.

* Dummy list
{:toc}

## STM32 Nucleo-64 development board

I selected this board because I could easily get hold of [BLDC control
code](https://github.com/a-sc/Flywheel/tree/main/firmware) for it. It features
an [STM32F401RE
MCU](https://www.st.com/en/microcontrollers-microprocessors/stm32f401re.html)
with 512 Kbytes of Flash memory and an 84 MHz CPU. The MCU contains all the
peripherals I need. In particular, a powerful set of timers to send pulse width
modulation (PWM) signals to the coils of the motor and measure its speed, plus
external general-purpose input/output (GPIO) pins I use to read Hall sensor
signals and have them produce interrupts.

## IFX007T motor shield

This PCB features three
[IFX007T](https://www.infineon.com/cms/en/product/power/motor-control-ics/brushed-dc-motor-driver-ics/single-half-bridge-ics/ifx007t/)
half-bridges. A half-bridge is half of what you need to make an
[H-bridge](https://en.wikipedia.org/wiki/H-bridge), which is what is typically
used to control motors. In essence, a half-bridge is made of two MOSFET
transistors connected in series. In my case, I connect the higher one to the
supply voltage (9V most of the time) and the lower one to ground. The output of
the half-bridge is the mid-point connection between the MOSFETs, so by
controlling them with the STM32 CPU I can connect the output either to ground or
to 9V.

During operation, I can drive a variable amount of current through a coil by
sending a PWM signal to the higher transistor of the corresponding H-bridge and
connecting the lower transistor (of the other half-bridge in the H-bridge) to
ground. The duty cycle of the PWM signal sets the average current through the
coil. The sense of the current depends on which half-bridge is used for,
respectively, PWM and ground, so can be easily changed by the MCU. See the
[firmware description](firmware.md) for more details.


## Interfacing with the Hall sensors

I use the [AH49E](https://www.diodes.com/part/view/AH49E/) Linear Hall-effect IC
from Diodes Incorporated for sensing the instantaneous position of the motor.
This is needed by the control firmware. The chips are conveniently available in a
TO-92S package which allowed me to solder their pins directly to flying wires and
keep full freedom in their physical placement. There are three Hall sensors in
the motor, interleaved at regular intervals with the three windings in the
stator. This means an angular separation of 120° between Hall sensors and 60°
between any Hall sensor and its closest coil either side.

![](../assets/images/Stator.jpg)
*A close-up view of the placement of the Hall sensors in the stator*

The orientation of the Hall sensors is chosen to produce a negative-going signal
whenever the north pole of a permanent magnet in the rotor goes past them. The
output of the Hall sensor sits around half-way between ground and the positive
supply when not exposed to any field (the Earth field is much weaker than the
one produced by the magnets). In order to get a proper 0V or 3.3V signal I can
send to the MCU, I condition the signals coming from the sensors using a simple
LM339 comparator with a bit of hysteresis.

![](../assets/images/Comparator.jpg)
*Picture of the comparator circuit (Todo: post full schematics)*

This short video shows the signals I get in the oscilloscope as I turn the rotor
manually, making each of the Hall sensors see alternate north and south poles:

<iframe width="560" height="315" src="https://www.youtube.com/embed/Yc3aZQofmLY" title="YouTube video player" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" allowfullscreen></iframe>
*Testing Hall sensor output (you can see the signal stretch in time as the rotor slows down)*

## Connection tables

They say a picture is sometimes worth more than a thousand words. Well, not in
this case! As you can see in the picture below, it is quite hard to discern the
connections in the unholy mess of wires:

![](../assets/images/NucleoSTM32MotorBoard.jpg)
*The Nucleo STM32 board connected to the IFX007T motor shield*

Here's a connection table to make sense of the wiring. Please refer to the
Nucleo board [user
manual](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf)
and the motor shield [user manual](https://www.infineon.com/dgdl/Infineon-Motor_Control_Shield_with_IFX007T_for_Arduino-UserManual-v03_00-EN.pdf?fileId=5546d462694c98b401696d2026783556) as appropriate, In particular, connectors in the
motor shield do not have proper designators, so you really need to look at the
user manual to make sense of the table below.

| Pin on Nucleo board | Pin on IFX007T shield |
| ------------------- | --------------------- |
| CN11 (any pin)      | GND                   |
| CN10 pin 23 (PA8)   | INU                   |
| CN10 pin 21 (PA9)   | INV                   |
| CN10 pin 33 (PA10)  | INW                   | 
| CN10 pin 15 (PA7)   | INHU                  |
| CN7  pin 34 (PB0)   | INHV                  |
| CN10 pin 24 (PB1)   | INHW                  |

In addition to these Nucleo-shield connections there are also three wires to
connect the Hall sensors (actually the outputs of the sensor conditioning board)
to the Nucleo board:

| Pin on Nucleo board | Sensor signal |
| ------------------- | ------------- |
| CN10 pin 3 (PB8)    | U             |
| CN10 pin 5 (PB9)    | V             |
| CN10 pin 17 (PB6)   | W             |

Bear in mind the following naming convention for the Hall sensors: their name is
the same as that of the coil they are facing in the stator. For example, sensor
U is the one surrounded by coils V and W, and so on.

## Bill of materials

I live in France. The following is a list of items I purchased and convenient
places for me to buy them from. If you want to reproduce and build upon this
work, hopefully you can easily figure out where best to buy from in your
location.

| Item         | Provider |
| ------------ | -------- |
| Nucleo board | [Amazon](https://amzn.eu/d/cLdppPl) |
| Motor shield | [Farnell](https://fr.farnell.com/en-FR/infineon/bldcshieldifx007ttobo1/carte-de-d-mo-driver-de-moteur/dp/3051941) |
| Hall sensors | [Farnell](https://fr.farnell.com/en-FR/diodes-inc/ah49ez3-g1/hall-effect-sensor-40-to-85deg/dp/3373778)|
| Comparator   | [Farnell](https://fr.farnell.com/en-FR/texas-instruments/lm339an/ic-comparator-quad-differential/dp/3118450) |

You will also need resistors, capacitors and jumper wires (female-female and
some female-male for easily probing with the oscilloscope). I trust if you have
read this far and are interested in building this, you will be able to find
these commodity items easily!
