# Flywheel plate mechanics

The flywheel plate design I am using is an earlier version of the one contained
in this directory, unfortunately not available anymore because it was done
before the design went into version control. The original author of the design
is Tomasz Włostowski, and he not only allowed me to post the files here but also
gave me a prototype of the plate made with his CNC machine. 

Although not a PCB, the mechanical design is actually a
[KiCad](https://www.kicad.org/) project, using only the PCB layout part of the
tool (i.e. no schematics). You will need KiCad 7 or later to open it. If you are
curious about its shape but not enough to install KiCad, here it is for your
viewing pleasure:

![](../../docs/assets/images/flywheel_plate.png)

The holes in the periphery allow increasing the moment of inertia (and hence the
stored energy for a given rotational speed) by adding nuts and bolts all around
the flywheel.
