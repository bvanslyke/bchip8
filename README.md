
# Chip8 Simulator

This is a work in progress, but the example Chip8 programs I find seem to work with it. I think I only need to implement the sound timer.
There is no rate limiting so your programs might blaze.
You can enable/disable the debugger by hitting tab, where you can step forward through machine instructions with the spacebar.

To run a Chip8 program you can either

- Run `bin/bchip program.c8` where program.c8 is a binary file. Or
- Run `scripts/runhex.sh [0xDE, 0xAD, 0xBE, 0xEF]` to run a program in the form of a hex string (e.g. the output of http://johnearnest.github.io/Octo/).

To compile you need the development library for SDL2.

You can find some example programs here: http://johnearnest.github.io/Octo/

## MIT License

Copyright (c) 2014 Brandon Van Slyke

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
