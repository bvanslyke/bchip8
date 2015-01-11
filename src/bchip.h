
#ifndef BCHIP_H_
#define BCHIP_H_

#include <stdint.h>

#define MEMORY_LEN 4096
#define REGISTERS_LEN 16
#define MAX_PROGRAM_LEN 4096
#define PROGRAM_START 0x200
#define STACK_SIZE 12

#define PIXELS_WIDTH 64
#define PIXELS_HEIGHT 32
#define PIXELS PIXELS_WIDTH * PIXELS_HEIGHT

#define PIXEL_SIZE 22
#define KEYS_LEN 16

#define CYCLES_PER_FRAME 7

struct machine {
	uint16_t pc;
	uint16_t _address;
	uint16_t stack[STACK_SIZE];
	int sp;
	uint8_t memory[MEMORY_LEN];
	uint8_t registers[REGISTERS_LEN];
	int _pixels[PIXELS];
	int keys[KEYS_LEN];
	int cycles;
	int delay_timer;
	int sound_timer;
	int waiting_for_key;
};

#endif
