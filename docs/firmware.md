---
layout: page
title: Firmware
permalink: /firmware/
---

By making my home-made motor have the same electrical interfaces as any standard
off-the-shelf BLDC motor, I was able to reuse [existing
firmware](https://github.com/a-sc/Flywheel/tree/main/firmware) to control it.
The main difference with standard BLDCs is the absence of bearings, thanks to
the [magnetic alignment](mechanics.md) of the axle, and therefore the
minimization of friction. This is a key goal in energy storage flywheels. I did
have to understand the code well enough to figure out a way to spin the motor
both clockwise and counter-clockwise, and also to tune the proportional gain of
the PI controller. Below you can find a summary description of the firmware
running in the STM32 MCU.

* Dummy list
{:toc}

## Development environment

The `Makefile` uses the `arm-none-eabi-gcc` compiler for ARM chips based on
Cortex-M and Cortex-R processors. Once you have an output binary, you must use
`arm-none-eabi-objcopy` to convert its format for use by `openocd`, the tool
which flashes the binary into the STM32 through the USB interface of the Nucleo
board. In Debian, you get these tools by installing the `gcc-arm-none-eabi`,
`binutils-arm-none-eabi` and `openocd` packages. In macOS, you can `brew install
--cask gcc-arm-embedded` and `brew install open-ocd`. Then you will need a
terminal emulator program to talk with the Nucleo board during operation. I use
`GTKTerm` in Linux and `minicom` (also available as a `brew` formula) in macOS.

## Program structure

The main program does the initial setup of all relevant peripherals and then
jumps to an infinite loop which prints diagnostics data to the serial (USB) port
every 500ms. The heavy lifting is done by the interrupt handlers.

The initial setup includes:

* Setting up the three pins interfacing to the Hall sensors, and declaring the
  three of them as interrupt sources, both when they go down (falling edge) and
  when they go up (rising edge).
* Setting up the six pins interfacing to the IFX007T motor shield (three PWM and
  three inhibit pins, outputs of the STM32, inputs of the motor shield).
* Configuring the TIM1 timer (the most versatile timer in the MCU) to provide
  signals to the PWM pins, with a programmable duty cycle.
* Configuring a standard timer to count clock ticks after the rising edge of one
  of the Hall sensor signals. This is used to measure the rotating speed four
  times per turn.
  
Whenever one of the Hall sensor signals undergoes a transition, the interrupt
handler is executed:

* It samples the state of the three Hall sensor signals to find in which phase
  the motor is within the turn.
* With this information, it calls `bldc_commutate()`, the function that controls
the outputs going to the motor shield (more on this later).
* It reads the count in the general-purpose timer to estimate rotation speed.
* It feeds this information to the PI controller, so it can derive from it, and
  from past history plus the speed requested by the user, a duty cycle of the
  signals to be sent to the motor shield. 

## Driving the coils

The three coils are spaced 120° apart from one another. Each coil has one end
connected to a half-bridge and the other end connected to the other coils. A
diagram will help:

![](../assets/images/coil_control.png)

It is customary to label the coils U, V and W when working with BLDC motors. Now
imagine that the Hall sensor sitting between coils U and V has just signaled the
passage of a north pole in front of it. If that rotor magnet showing its north
pole is headed towards U, then we want U to attract it and V to repel it. That
means U should become a south pole and V should become a north pole. I have
connected the windings in a way that makes them become north poles when current
comes from the half-bridge. So in this particular example, the firmware must
drive a PWM signal into the upper switch (MOSFET) of the V half-bridge and close
the lower switch (i.e. connect it to GND) in the U half-bridge. Current then
flows from the V<sub>power</sub> supply through the upper switch of the V
bridge, then the V winding, U winding and out of the U winding to GND, turning
the V coil into a north pole and the U coil into a south pole as desired. The
average current will depend on the duty cycle of the PWM signal, as provided by
the PI controller.

## Proportional Integral (PI) control

In PI control, the system continuously measures the controlled variable (in this
case rotational speed) and derives an error signal from it by subtracting the
measured speed from the set point (the speed the user wants). This error signal
is then fed to the controller, which produces a duty cycle for the PWM signals.

In steady state, the error is close to zero, so if we only used proportional
control (i.e. having the duty cycle be proportional to the error) the system
would oscillate back and forth around the set point. The duty cycle would go to
zero when the speed were reached, which would make the speed go down, then the
error go up, then the duty cycle go up and so forth. By having an integral
branch in the controller, added to the proportional branch to generate the duty
cycle output, we can have a non-zero output even when the system is locked at
the right speed, which is exactly what we want.

Both the proportional and the integral branch of the controller have their own
gains. These are multiplicative constants, customarily named k<sub>p</sub> and
k<sub>i</sub>, which the user can set to fine-tune the dynamics of the feedback
system.

I mention all this because one of the slight tweaks I had to do on the firmware
side was to adjust the value of k<sub>p</sub>. In the internal integer
representation of the error in the PI controller, one rpm corresponds to 68
counts. If I wanted the proportional part to provide a correction signal when
the error was 1 rpm, then I had to push k<sub>p</sub> to a value of 200, so its
contribution would not disappear due to some scaling (right shift) done inside
the code.
