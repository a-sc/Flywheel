---
layout: post
title:  "Interactive control of the motor from a laptop"
date:   2023-02-26 21:35:00 +0100
---

As I write this, the project is at commit c9b6b35. You can use [this
link](https://github.com/a-sc/Flywheel/tree/c9b6b351d2ad0e428fc1421b7ee499edfa6beab9)
to browse the code at that particular commit, in case it changes in the future
and some of the information in this post no longer applies. The [initial
code](https://github.com/a-sc/Flywheel/tree/7137e67b5240492d49ae9100eec4e01b828223a7)
I got from Tom had the motor start immediately after power up, with a speed set
point which was hard coded. Statistics were continuously printed on the screen
of the laptop using a simplified version of `printf()`, but there was nothing
resembling `scanf()`, which would make life much simpler to make the control of
the motor more interactive.

### Using newlib-nano as C standard library

In the
[`Makefile`](https://github.com/a-sc/Flywheel/blob/c9b6b351d2ad0e428fc1421b7ee499edfa6beab9/firmware/Makefile),
option `--specs=nano.specs` is passed to the linker. This means my object code
links to newlib-nano, a space-conscious version of newlib.
[Newlib](https://sourceware.org/newlib/) is an implementation of the C standard
library intended for embedded systems. Since both `printf()` and `scanf()` live
in the standard library, I should be in business, right? Well, it's not so
simple. `printf()` needs to be told how to write in your particular setup (in
this case I want it to write to the UART) and similarly `scanf()` needs
low-level read support. Luckily, I found [this
post](https://interrupt.memfault.com/blog/boostrapping-libc-with-newlib) by
François Baldassari very clearly explaining how to add this kind of support in a
particular system. I needed to add code in
[`usart.c`](https://github.com/a-sc/Flywheel/blob/c9b6b351d2ad0e428fc1421b7ee499edfa6beab9/firmware/src/usart.c)
for 7 low-level functions: `_fstat()`, `_close()`, `_lseek()`, `_isatty()`,
`_sbrk()`, `_read()` and `_write()`. The first 4 are fairly trivial. I just
copied/pasted his code.

`_sbrk()` is used by `printf()` to dynamically allocate memory. It receives as
an argument the number of chars to allocate and internally manages a pointer to
the top of the heap, which grows every time new memory is allocated. This means
you need to know were your heap starts before you can write `_sbrk()`.

### An aside: linker scripts

In the STM32F401RE, there are 512KB of flash starting at address 0x08000000 and
96KB of RAM starting at 0x20000000. The [linker
script](https://github.com/a-sc/Flywheel/blob/c9b6b351d2ad0e428fc1421b7ee499edfa6beab9/firmware/Device/gcc.ld)
is where you tell the linker where each part of your program goes. It uses a
relatively obscure syntax, but as luck would have it, François comes again to
the rescue with another clear
[post](https://interrupt.memfault.com/blog/how-to-write-linker-scripts-for-firmware).
Space in RAM is used for the `.data` and `.bss` sections successively. The
`.data` section, starting at address 0x20000000 in my case, contains all
initialized data (i.e. variables I initialized after declaring them in the
code). The `.bss` section contains all uninitialized data, which is cleared
(i.e. set to zero) by the [startup
code](https://github.com/a-sc/Flywheel/blob/c9b6b351d2ad0e428fc1421b7ee499edfa6beab9/firmware/Device/startup_stm32f401xe.s)
written in ARM assembler. The rest of RAM is free to use. In my linker script,
the heap starts at the end of the `.bss` section and grows upwards (i.e. towards
increasing addresses) whenever new memory is dynamically allocated. The stack
starts at the end of RAM (address 0x20017FFF) and grows downwards. I don't think
there is any rock-solid way to make sure your heap and stack don't step on each
other at some point, at least not at compile time. Your compiler cannot know in
advance which paths your code will take, and how many times you will allocate
dynamic memory (on the heap) or call functions which will need space for their
arguments and automatic variables in the stack. In this particular application,
the code is relatively simple and most of the 96KB is available for heap and
stack, so we are safe.

### The intricacies of `_read()` and `_write()`

The UART code is interrupt-based. This ensures you don't overflow the UART TX
path by writing another character to it before it has finished sending the
previous one. Conversely, in the RX path, it is good to be awakened by an
interrupt every time a character is received from the serial line so that it can
be read before another incoming character overwrites the RX register.

On the read side, the UART generates an interrupt every time a character is
received (from the laptop through USB in my case). The interrupt handler,
`USART2_IRQHandler()`, reads the character and does two things with it: send it
to the TX of the UART (so the user typing in the laptop can see the character
(s)he typed) and push it to an RX FIFO. The `_read()` function just reads from
the FIFO in a busy loop until the number of characters requested by `scanf()` is
received. If a `\r` character is received, the busy waiting stops and the
function returns with the number of characters read until then.

On the write side, the UART generates an interrupt when its TX buffer is empty.
This means that, after it is enabled in the `usart_send()` routine, it will
generate an interrupt. Then, in the `USART2_IRQHandler()` function, the
character will be read from the TX FIFO and sent out to the serial line.

### Status and plans

With `printf()` and `scanf()` fully working now, I have implemented three simple
commands: `start`, `stop` and `set speed XXXX`. Next I plan to add some simple
telemetry so that I can use the data to better tune the PI controller and study
in detail the deceleration of the flywheel after external power is switched off.
