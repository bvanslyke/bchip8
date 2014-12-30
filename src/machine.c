
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bchip.h"
#include "machine.h"


static int pixelxy(int x, int y) {
	return (y % PIXELS_HEIGHT) * PIXELS_WIDTH + (x % PIXELS_WIDTH);
}

static void machine_set_pixel(struct machine *machine, int x, int y, int val) {
	machine->_pixels[pixelxy(x,y)] = val;
}

static int machine_get_pixel(struct machine *machine, int x, int y) {
	return machine->_pixels[pixelxy(x,y)];
}

static int machine_get_memory(struct machine *machine, uint16_t loc) {
	assert(loc < 4096);
	return machine->memory[loc];
}

static void machine_set_memory(struct machine *machine, uint16_t loc, uint8_t value) {
	assert(loc < 4096);
	machine->memory[loc] = value;
}

static void machine_dump_registers(FILE *out, const struct machine *machine) {
	fprintf(out, "Registers:\n");

	fprintf(out, "\t(pc: 0x%x=%d)\n", machine->pc, machine->pc);

	int i;
	for (i = 0; i < REGISTERS_LEN; i++ ) {
		fprintf( out, "\t(reg v%x: 0x%x=%d)\n", i, machine->registers[i],
				 machine->registers[i] );
	}
}

static void machine_jump_to(struct machine *machine, uint16_t loc) {
	machine->pc = loc;
}

static void machine_call(struct machine *machine, uint16_t loc) {
	assert(machine->sp < STACK_SIZE);
	machine->stack[machine->sp] = machine->pc + 2;
	machine->sp++;
	machine->pc = loc;
}

static void machine_set_register(struct machine *machine, int reg, uint8_t value) {
	assert (reg >= 0 && reg <= 0xF);
	machine->registers[reg] = value;
}

static void machine_copy_register(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regY];
}

static void machine_regs_or(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] | machine->registers[regY];
}

static void machine_regs_and(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] & machine->registers[regY];
}

static void machine_regs_xor(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] ^ machine->registers[regY];
}

static void machine_regs_addto(struct machine *machine, int regX, int regY) {
	/* Add the value of register VY to register VX */
	/* Set VF to 01 if a carry occurs */
	/* Set VF to 00 if a carry does not occur */
	uint8_t sum = machine->registers[regX] + machine->registers[regY];
	machine->registers[0xF] = sum < machine->registers[regX] ? 1 : 0;
	machine->registers[regX] = sum;
}

static void machine_reg_subfrom(struct machine *machine, int regX, int regY) {
	/* Subtract the value of register VY from register VX */
	/* Set VF to 00 if a borrow occurs */
	/* Set VF to 01 if a borrow does not occur */
	uint8_t diff = machine->registers[regX] - machine->registers[regY];
	machine->registers[0xF] = diff > machine->registers[regX];
	machine->registers[regX] = diff;
}

static void machine_reg_rshift(struct machine *machine, int regX, int regY) {
	/* Store the value of register VY shifted right one bit in register VX */
	/* Set register VF to the least significant bit prior to the shift */
	int y = machine->registers[regY];
	machine->registers[0xF] = y & 1;
	machine->registers[regX] = y >> 1;
}

static void machine_reg_minus(struct machine *machine, int regX, int regY) {
	/* Set register VX to the value of VY minus VX */
	/* Set VF to 00 if a borrow occurs */
	/* Set VF to 01 if a borrow does not occur */
	uint8_t diff = machine->registers[regY] - machine->registers[regX];
	machine->registers[0xF] = diff > machine->registers[regY];
	machine->registers[regY] = diff;
}

static void machine_reg_lshift(struct machine *machine, int regX, int regY) {
	/* Store the value of register VY shifted left one bit in register VX */
	/* Set register VF to the most significant bit prior to the shift */
	int y = machine->registers[regY];
	machine->registers[0xF] = y & 0x80;
	machine->registers[regX] = y << 1;
}


static void machine_add_to_register(struct machine *machine, int reg, uint8_t value) {
	assert(reg >= 0 && reg <= 0xF);
	machine->registers[reg] += value;
}

static void machine_skip_if_eq(struct machine *machine, int regx, uint8_t value) {
	if (machine->registers[regx] == value) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

static void machine_skip_if_neq(struct machine *machine, int regx, uint8_t value) {
	if (machine->registers[regx] == value) {
		machine->pc += 2;
	} else {
		machine->pc += 4;
	}
}

static void machine_skip_if_regs_eq(struct machine *machine, int regx, int regy) {
	if (machine->registers[regx] == machine->registers[regy]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

static void machine_skip_if_regs_neq(struct machine *machine, int regx, int regy) {
	if (machine->registers[regx] == machine->registers[regy]) {
		machine->pc += 2;
	} else {
		machine->pc += 4;
	}
}

static void machine_return(struct machine *machine) {
	assert(machine->sp > 0);
	machine->sp--;
	machine->pc = machine->stack[machine->sp];
}

static void machine_set_address(struct machine *machine, uint16_t loc) {
	machine->_address = loc & 0x0FFF;
}

static void machine_jump_plus(struct machine *machine, uint16_t loc) {
	machine->pc = machine->registers[0x0] + loc;
}

static void machine_set_random(struct machine *machine, int regx, uint8_t mask) {
	uint8_t value = (uint8_t) (rand() % 256);
	machine->registers[regx] = value & mask;
}

static void machine_draw(struct machine *machine, int regx, int regy, int n) {
	/*
	  Draw a sprite at position VX, VY with N bytes of sprite data starting
	  at the address stored in I.
	  Set VF to 01 if any set pixels are changed to unset, and 00 otherwise
	*/
	int dx, dy;
	int x = machine->registers[regx];
	int y = machine->registers[regy];

	machine->registers[0xF] = 0;
	for (dy = 0; dy < n; dy++) {
		for (dx = 0; dx < 8; dx++) {
			if (machine_get_memory(machine, machine->_address + dy) & (0x80 >> dx)) {
				int prev = machine_get_pixel(machine, x + dx, y + dy);
				if (prev == 1) {
					machine_set_pixel(machine, x + dx, y + dy, 0);
					machine->registers[0xF] = 1;
				} else if (prev == 0) {
					machine_set_pixel(machine, x + dx, y + dy, 1);
				}
			}
		}
	}
}

static void machine_clear_display(struct machine *machine) {
	int i;
	for (i = 0; i < PIXELS; i++) {
		machine->_pixels[i] = 0;
	}
}

static void machine_skip_if_pressed(struct machine *machine, int regX) {
	if (machine->keys[machine->registers[regX]]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

static void machine_skip_if_unpressed(struct machine *machine, int regX) {
	if (! machine->keys[machine->registers[regX]]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

static void machine_store_delay(struct machine *machine, int regX) {
	machine->registers[regX] = machine->delay_timer;
}

static void machine_wait_for_key(struct machine *machine, int regX) {
	printf("v%x := key\n", regX);
	machine->waiting_for_key = regX;
}

static void machine_set_delay_timer(struct machine *machine, int regX) {
	machine->delay_timer = machine->registers[regX];
}

static void machine_set_sound_timer(struct machine *machine, int regX) {
	machine->sound_timer = machine->registers[regX];
}

static void machine_add_to_address(struct machine *machine, int regX) {
	machine_set_address(machine, machine->_address + machine->registers[regX]);
}

static void machine_font_address(struct machine *machine, int regX) {
	machine->_address = machine->registers[regX] * 5;
}

static void machine_store_bcd(struct machine *machine, int regX) {
}

static void machine_store_regs(struct machine *machine, int regX) {
	int i;
	for (i = 0; i <= regX; i++) {
		machine_set_memory(machine,
						   machine->_address + i,
						   machine->registers[i]);
	}
}

static void machine_load_regs(struct machine *machine, int regX) {
	int i;
	for (i = 0; i <= regX; i++) {
		machine->registers[i] =
			machine_get_memory(machine,
							   machine->_address + i);
	}
}

static int op_nyb0(uint16_t op) { return (op & 0xF000) >> 12; }
static int op_nyb1(uint16_t op) { return (op & 0x0F00) >> 8; }
static int op_nyb2(uint16_t op) { return (op & 0x00F0) >> 4; }
static int op_nyb3(uint16_t op) { return (op & 0x000F) >> 0; }
static int op_byte2(uint16_t op) { return op & 0x00FF; }
static int op_address(uint16_t op) { return op & 0x0FFF; }

#define PC_NEXT -1
#define PC_END -2
#define PC_JUMPED -3
int machine_dispatch(struct machine *mac, uint16_t op) {
	int x,y;
	if (op == 0x00E0) {
		machine_clear_display(mac);
		return PC_NEXT;
	}
	if (op == 0x00EE) {
		machine_return(mac);
		return PC_JUMPED;
	}
	switch (op_nyb0(op)) {
	case 0:
		return PC_END;
	case 1:
		machine_jump_to(mac, op_address(op));
		return PC_JUMPED;
	case 2:
		machine_call(mac, op_address(op));
		return PC_JUMPED;
	case 3:
		machine_skip_if_eq(mac, op_nyb1(op), op_byte2(op));
		return PC_JUMPED;
	case 4:
		machine_skip_if_neq(mac, op_nyb1(op), op_byte2(op));
		return PC_JUMPED;
	case 5:
		assert(op_nyb3(op) == 0);
		machine_skip_if_regs_eq(mac, op_nyb1(op), op_nyb2(op));
		return PC_JUMPED;
	case 6:
		machine_set_register(mac, op_nyb1(op), op_byte2(op));
		return PC_NEXT;
	case 7:
		machine_add_to_register(mac, op_nyb1(op), op_byte2(op));
		return PC_NEXT;
	case 8:
		x = op_nyb1(op);
		y = op_nyb2(op);
		switch (op_nyb3(op)) {
		case 0: machine_copy_register(mac, x, y); break;
		case 1:	machine_regs_or(mac, x, y); break;
		case 2:	machine_regs_and(mac, x, y); break;
		case 3:	machine_regs_xor(mac, x, y); break;
		case 4:	machine_regs_addto(mac, x, y); break;
		case 5:	machine_reg_subfrom(mac, x, y); break;
		case 6:	machine_reg_rshift(mac, x, y); break;
		case 7:	machine_reg_minus(mac, x, y); break;
		case 0xE: machine_reg_lshift(mac, x, y); break;
		default: assert(0);
		}
		return PC_NEXT;
	case 9:
		assert(op_nyb3(op) == 0);
		machine_skip_if_regs_neq(mac, op_nyb1(op), op_nyb2(op));
		return PC_JUMPED;
	case 0xA:
		machine_set_address(mac, op_address(op));
		return PC_NEXT;
	case 0xB:
		machine_jump_plus(mac, op_address(op));
		return PC_JUMPED;
	case 0xC:
		machine_set_random(mac, op_nyb1(op), op_byte2(op));
		return PC_NEXT;
	case 0xD:
		machine_draw(mac, op_nyb1(op), op_nyb2(op), op_nyb3(op));
		return PC_NEXT;
	case 0xE:
		if (op_byte2(op) == 0x9E) {
			machine_skip_if_pressed(mac, op_nyb1(op));
			return PC_JUMPED;
		} else if (op_byte2(op) == 0xA1) {
			machine_skip_if_unpressed(mac, op_nyb1(op));
			return PC_JUMPED;
		} else {
			assert(0);
		}
	case 0xF:
		x = op_nyb1(op);
		switch (op_byte2(op)) {
		case 0x07: machine_store_delay(mac, x); break;
		case 0x0A: machine_wait_for_key(mac, x); break;
		case 0x15: machine_set_delay_timer(mac, x); break;
		case 0x18: machine_set_sound_timer(mac, x); break;
		case 0x1E: machine_add_to_address(mac, x); break;
		case 0x29: machine_font_address(mac, x); break;
		case 0x33: machine_store_bcd(mac, x); break;
		case 0x55: machine_store_regs(mac, x); break;
		case 0x65: machine_load_regs(mac, x); break;
		}
		return PC_NEXT;
	default:
		fprintf(stderr, "Unmatched op 0x%x. PC=0x%x/%d\n", op, mac->pc, mac->pc);
		return 0;
	}
}

// Returns whether to continue (bool)
int machine_cycle(struct machine *machine) {
	uint16_t pc = machine->pc;
	uint16_t opcode = (machine->memory[pc] << 8) | machine->memory[pc + 1];
	int result = machine_dispatch(machine, opcode);

	if (!result) {
		return 0;
	}

	machine->cycles++;

	if (result == PC_NEXT) {
		machine->pc += 2;
		return 1;
	}

	if (result == PC_END) {
		fprintf(stderr, "0x0000 received, ending\n");
		machine_dump_registers(stderr, machine);
		return 0;
	}

	// Otherwise, jumping
	return 1;
}
