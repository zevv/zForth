
![zForth](/zforth.png)

zForth
======

zForth is yet another Forth, but with some special features not found in most
other forths. zForth was written as a lightweight scripting language for
extending embedded applications on small microprocessors. It's not particulary
fast, but should be easy to integrate on any platform.

Some of zForth's properties:

- Small dictionary: instead of relying on a fixed cell size, the dictionary is
  written in variable length cells: small and common numbers take less space
  then larger, resulting in 30% to 50% space saving

- Portable: zForth is written in 100% ANSI C, and runs on virtually all
  architectures. Tested on x86, x86_64, ARM, ARM thumb, Mips, Atmel AVR and 8051

- Small footprint: the kernel C code compiles to about 3 or 4 kB of machine
  code, depending on the architecture and chosen cell data types.

- Implemented as a small virtual machine: not the fastest, but safe and
  flexible. Instead of having direct access to host memory, the forth VM memory
  is abstracted, allowing proper boundary checking on memory accesses and stack
  operations.

- Flexible data types: at compile time the user is free to choose what C data
  type should be used for the dictionary and the stacks. zForth supports signed
  integer sizes from 16 to 128 bit, but also works seamlessly with floating point
  types like float and double.

- Easy extendibility: calling C code from forth is easy through a host system
  call primitive, and code has access to the stack for exchanging data between
  Forth and C.



Usage
=====

zForth consists of only two files: zforth.c and zforth.h. Add both to your
project and call `zf_init()` and `zf_bootstrap()` during initialisation. Read
forth statements from a file or terminal and pass the strings to `zf_eval()` to
interpret, compile and run the code.

A demo application for running zForth in linux is provided here, simply run `make`
to build.

To start zforth and load the core forth code, run:

````
./zfort forth/core.zf
````

And zForth will welcome you with the startup message:

````
Welcome to zForth, 786 bytes used
````

zForth is now ready to use. Try some of the following:

Adding one and one or calculate the 144 squared:

````
1 1 + .
144 dup * .
````

Print the sine of 10 numbers between 0 and PI

````
: pi 3.141592654 ;
: demo 0 begin dup sin . pi 10 / + dup 10 > until ;
demo
````

Load and run the demo Mandelbrot fractal:

````
include forth/mandel.zf
````

