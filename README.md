
# Chip8 Simulator

This is a work in progress. At this point some Chip8 programs run. You can toggle the debugger by
hitting tab and you can step forward through machine instructions with the spacebar.


To run a Chip8 program you can either

- Run `bin/bchip program.c8` where program.c8 is a binary file. Or
- Run `scripts/runhex.sh [0xDE, 0xAD, 0xBE, 0xEF]` to run a program in the form of a hex string (e.g. the output of http://johnearnest.github.io/Octo/). This requires Haskell (for the moment) because it was faster for me to write this part in Haskell.

The compile you need the development library for SDL2.

Some of the example programs are from here: http://johnearnest.github.io/Octo/


## Status

The simulator mostly works. But it's still missing implementation of waitkey, bcd, and the timer instructions.

## MIT License

Copyright (c) 2014 Brandon Van Slyke

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
