
/*
TODO:
  - make waitkey work
  - fix bad performance
  - bcd instruction
  - font address instruction
  - delay and sound timers
*/

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL2/SDL.h>

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

uint8_t font[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0,
	0x20, 0x60, 0x20, 0x20, 0x70,
	0xF0, 0x10, 0xF0, 0x80, 0xF0,
	0xF0, 0x10, 0xF0, 0x10, 0xF0,
	0x90, 0x90, 0xF0, 0x10, 0x10,
	0xF0, 0x80, 0xF0, 0x10, 0xF0,
	0xF0, 0x80, 0xF0, 0x90, 0xF0,
	0xF0, 0x10, 0x20, 0x40, 0x40,
	0xF0, 0x90, 0xF0, 0x90, 0xF0,
	0xF0, 0x90, 0xF0, 0x10, 0xF0,
	0xF0, 0x90, 0xF0, 0x90, 0x90,
	0xE0, 0x90, 0xE0, 0x90, 0xE0,
	0xF0, 0x80, 0x80, 0x80, 0xF0,
	0xE0, 0x90, 0x90, 0x90, 0xE0,
	0xF0, 0x80, 0xF0, 0x80, 0xF0,
	0xF0, 0x80, 0xF0, 0x80, 0x80
};

int pixelxy(int x, int y) {
	return (y % PIXELS_HEIGHT) * PIXELS_WIDTH + (x % PIXELS_WIDTH);
}
void machine_set_pixel(struct machine *machine, int x, int y, int val) {
	machine->_pixels[pixelxy(x,y)] = val;
}

int machine_get_pixel(struct machine *machine, int x, int y) {
	return machine->_pixels[pixelxy(x,y)];
}

int machine_get_memory(struct machine *machine, uint16_t loc) {
	assert(loc < 4096);
	return machine->memory[loc];
}

void machine_set_memory(struct machine *machine, uint16_t loc, uint8_t value) {
	assert(loc < 4096);
	machine->memory[loc] = value;
}

void machine_dump_registers(FILE *out, const struct machine *machine) {
	fprintf(out, "Registers:\n");

	fprintf(out, "\t(pc: 0x%x=%d)\n", machine->pc, machine->pc);

	int i;
	for (i = 0; i < REGISTERS_LEN; i++ ) {
		fprintf( out, "\t(reg v%x: 0x%x=%d)\n", i, machine->registers[i],
				 machine->registers[i] );
	}
}

void machine_dump(FILE *out, const struct machine *machine) {
	int i;

	machine_dump_registers(out, machine);

	fprintf(out, "Memory:\n");

	for (i = 0; i < MEMORY_LEN; i++) {
		fprintf( out, "\t(mem 0x%x: 0x%x = %d)\n", i, machine->memory[i],
				 machine->memory[i] );
	}
}

void machine_jump_to(struct machine *machine, uint16_t loc) {
	machine->pc = loc;
}

void machine_call(struct machine *machine, uint16_t loc) {
	assert(machine->sp < STACK_SIZE);
	machine->stack[machine->sp] = machine->pc + 2;
	machine->sp++;
	machine->pc = loc;
}

void machine_set_register(struct machine *machine, int reg, uint8_t value) {
	assert (reg >= 0 && reg <= 0xF);
	machine->registers[reg] = value;
}

void machine_copy_register(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regY];
}

void machine_regs_or(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] | machine->registers[regY];
}

void machine_regs_and(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] & machine->registers[regY];
}

void machine_regs_xor(struct machine *machine, int regX, int regY) {
	machine->registers[regX] = machine->registers[regX] ^ machine->registers[regY];
}

void machine_regs_addto(struct machine *machine, int regX, int regY) {
	/* Add the value of register VY to register VX */
	/* Set VF to 01 if a carry occurs */
	/* Set VF to 00 if a carry does not occur */
	uint8_t sum = machine->registers[regX] + machine->registers[regY];
	machine->registers[0xF] = sum < machine->registers[regX] ? 1 : 0;
	machine->registers[regX] = sum;
}

void machine_reg_subfrom(struct machine *machine, int regX, int regY) {
	/* Subtract the value of register VY from register VX */
	/* Set VF to 00 if a borrow occurs */
	/* Set VF to 01 if a borrow does not occur */
	uint8_t diff = machine->registers[regX] - machine->registers[regY];
	machine->registers[0xF] = diff > machine->registers[regX];
	machine->registers[regX] = diff;
}

void machine_reg_rshift(struct machine *machine, int regX, int regY) {
	/* Store the value of register VY shifted right one bit in register VX */
	/* Set register VF to the least significant bit prior to the shift */
	int y = machine->registers[regY];
	machine->registers[0xF] = y & 1;
	machine->registers[regX] = y >> 1;
}

void machine_reg_minus(struct machine *machine, int regX, int regY) {
	/* Set register VX to the value of VY minus VX */
	/* Set VF to 00 if a borrow occurs */
	/* Set VF to 01 if a borrow does not occur */
	uint8_t diff = machine->registers[regY] - machine->registers[regX];
	machine->registers[0xF] = diff > machine->registers[regY];
	machine->registers[regY] = diff;
}

void machine_reg_lshift(struct machine *machine, int regX, int regY) {
	/* Store the value of register VY shifted left one bit in register VX */
	/* Set register VF to the most significant bit prior to the shift */
	int y = machine->registers[regY];
	machine->registers[0xF] = y & 0x80;
	machine->registers[regX] = y << 1;
}


void machine_add_to_register(struct machine *machine, int reg, uint8_t value) {
	assert(reg >= 0 && reg <= 0xF);
	machine->registers[reg] += value;
}

void machine_skip_if_eq(struct machine *machine, int regx, uint8_t value) {
	if (machine->registers[regx] == value) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

void machine_skip_if_neq(struct machine *machine, int regx, uint8_t value) {
	if (machine->registers[regx] == value) {
		machine->pc += 2;
	} else {
		machine->pc += 4;
	}
}

void machine_skip_if_regs_eq(struct machine *machine, int regx, int regy) {
	if (machine->registers[regx] == machine->registers[regy]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

void machine_skip_if_regs_neq(struct machine *machine, int regx, int regy) {
	if (machine->registers[regx] == machine->registers[regy]) {
		machine->pc += 2;
	} else {
		machine->pc += 4;
	}
}

void machine_return(struct machine *machine) {
	assert(machine->sp > 0);
	machine->sp--;
	machine->pc = machine->stack[machine->sp];
}

void machine_set_address(struct machine *machine, uint16_t loc) {
	machine->_address = loc & 0x0FFF;
}

void machine_jump_plus(struct machine *machine, uint16_t loc) {
	machine->pc = machine->registers[0x0] + loc;
}

void machine_set_random(struct machine *machine, int regx, uint8_t mask) {
	uint8_t value = (uint8_t) (rand() % 256);
	machine->registers[regx] = value & mask;
}

void machine_draw(struct machine *machine, int regx, int regy, int n) {
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

void machine_clear_display(struct machine *machine) {
	int i;
	for (i = 0; i < PIXELS; i++) {
		machine->_pixels[i] = 0;
	}
}

void machine_skip_if_pressed(struct machine *machine, int regX) {
	if (machine->keys[machine->registers[regX]]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

void machine_skip_if_unpressed(struct machine *machine, int regX) {
	if (! machine->keys[machine->registers[regX]]) {
		machine->pc += 4;
	} else {
		machine->pc += 2;
	}
}

void machine_store_delay(struct machine *machine, int regX) {
	machine->registers[regX] = machine->delay_timer;
}

void machine_wait_for_key(struct machine *machine, int regX) {
	printf("v%x := key\n", regX);
	machine->waiting_for_key = regX;
}

void machine_set_delay_timer(struct machine *machine, int regX) {
	machine->delay_timer = machine->registers[regX];
}

void machine_set_sound_timer(struct machine *machine, int regX) {
	machine->sound_timer = machine->registers[regX];
}

void machine_add_to_address(struct machine *machine, int regX) {
	machine_set_address(machine, machine->_address + machine->registers[regX]);
}

void machine_font_address(struct machine *machine, int regX) {
	machine->_address = machine->registers[regX] * 5;
}

void machine_store_bcd(struct machine *machine, int regX) {
}

void machine_store_regs(struct machine *machine, int regX) {
	int i;
	for (i = 0; i <= regX; i++) {
		machine_set_memory(machine,
						   machine->_address + i,
						   machine->registers[i]);
	}
}

void machine_load_regs(struct machine *machine, int regX) {
	int i;
	for (i = 0; i <= regX; i++) {
		machine->registers[i] =
			machine_get_memory(machine,
							   machine->_address + i);
	}
}

int op_nyb0(uint16_t op) { return (op & 0xF000) >> 12; }
int op_nyb1(uint16_t op) { return (op & 0x0F00) >> 8; }
int op_nyb2(uint16_t op) { return (op & 0x00F0) >> 4; }
int op_nyb3(uint16_t op) { return (op & 0x000F) >> 0; }
int op_byte2(uint16_t op) { return op & 0x00FF; }
int op_address(uint16_t op) { return op & 0x0FFF; }
// 0  -> error
// -1 -> next instruction
// -2 -> end
// -3 -> don't go to next
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

void load_font(uint8_t *dest) {
	memcpy(dest, font, sizeof(font));
}

int load_program(const char *filename, struct machine *machine) {
	uint8_t *dest = &machine->memory[PROGRAM_START];
	int ret = 0;
	long fsize;
	FILE *file = fopen(filename, "r");

	if (file == NULL) goto done;
	if (fseek(file, 0, SEEK_END)) goto done;

	fsize = ftell(file);
	if (fsize >= MAX_PROGRAM_LEN) goto done;

	if (fseek(file, 0, SEEK_SET)) goto done;
	if (fread(dest, 1, fsize, file) != fsize) goto done;

	machine->pc = PROGRAM_START;
	ret = 1;

 done:
	fclose(file);
	return ret;
}

void load_dbg(struct machine *machine) {
	uint8_t prog[] = { 0x00};
	uint8_t *dest = &machine->memory[PROGRAM_START];

	int i;
	for (i = 0; i < sizeof(prog); i++) {
		dest[i] = prog[i];
	}

	machine->pc = PROGRAM_START;
}


int window_init(int width, int height, SDL_Window **window_out,
				SDL_Renderer **renderer_out) {
	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) goto error;

	window = SDL_CreateWindow("Chip8", SDL_WINDOWPOS_UNDEFINED,
							  SDL_WINDOWPOS_UNDEFINED, width, height, 0);

	if (window == NULL) goto error;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == NULL) goto error;

	*window_out = window;
	*renderer_out = renderer;
	return 1;

 error:
	if (window) SDL_DestroyWindow(window);
	fprintf(stderr, "SDL Error: %s\n", SDL_GetError());
	SDL_Quit();
	return 0;
}

void draw_dbg_digit(SDL_Renderer *renderer, int x, int y, int digit) {
	int dx, dy;
	for (dy = 0; dy < 5; dy++) {
		for (dx = 0; dx < 4; dx++) {
			if (font[digit * 5 + dy] & (0x80 >> dx)) {
				SDL_RenderDrawPoint(renderer, x + dx, y + dy + 1);
			}
		}
	}
}

void draw_dbg(struct machine *machine, SDL_Renderer *renderer) {
	int pad = PIXEL_SIZE;

	int i;
	for (i = 0; i < 0x1000; i++) {
		if (i == machine->pc || i == machine->pc + 1) {
			SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, SDL_ALPHA_OPAQUE);
		} else if(i == machine->_address || i == machine->_address + 1) {
			SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, SDL_ALPHA_OPAQUE);
		} else {
			SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
		}
		int col = i / 0x0040;
		int row = i % 0x0040;
		uint8_t byte = machine->memory[i];
		draw_dbg_digit(renderer, 4 + pad * col,     pad + 8 * row, (byte & 0xF0) >> 4);
		draw_dbg_digit(renderer, 4 + pad * col + 6, pad + 8 * row, byte & 0x0F);
	}

	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
	for (i = 0; i < 16; i++) {
		uint8_t byte = machine->registers[i];
		draw_dbg_digit(renderer, 4 + i * pad,     550, (byte & 0xF0) >> 4);
		draw_dbg_digit(renderer, 4 + i * pad + 6, 550, byte & 0x0F);
	}

	for (i = 0; i < STACK_SIZE; i++) {
		uint16_t loc = machine->stack[i];
		if (i == machine->sp) {
			SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, SDL_ALPHA_OPAQUE);
		} else {
			SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE);
		}
		draw_dbg_digit(renderer, 4 + i * pad,      600, (loc & 0xF00) >> 8);
		draw_dbg_digit(renderer, 4 + i * pad + 6,  600, (loc & 0x0F0) >> 4);
		draw_dbg_digit(renderer, 4 + i * pad + 12, 600, (loc & 0x00F));
	}
}

void draw_display(struct machine *machine, SDL_Renderer *renderer) {
	SDL_Rect rect;
	rect.w = PIXEL_SIZE;
	rect.h = PIXEL_SIZE;

	int i;
	for (i = 0; i < PIXELS; i++) {
		rect.x = PIXEL_SIZE * (i % PIXELS_WIDTH);
		rect.y = PIXEL_SIZE * (i / PIXELS_WIDTH);
		if (machine->_pixels[i]) {
			SDL_SetRenderDrawColor(renderer, 0xFF, 0x5E, 0x5E, SDL_ALPHA_OPAQUE);
		} else {
			SDL_SetRenderDrawColor(renderer, 0xAD, 0x39, 0x39, SDL_ALPHA_OPAQUE);
		}
		SDL_RenderFillRect(renderer, &rect);
	}
}

int on_key(struct machine *machine, int stepping, int key) {
	printf("onkey\n");
	int wfk = machine->waiting_for_key;
	if (wfk > 0) {
		printf("wfk\n");
		machine->registers[wfk] = key;
		machine->waiting_for_key = -1;
		if (stepping) {
			return machine_cycle(machine);
		}
	}
	machine->keys[key] = 1;
	return 1;
}


int main(int argc, char *argv[]) {
	struct machine machine = {0};

	machine.pc = PROGRAM_START;
	machine.waiting_for_key = -1;

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	int running = 1;
	int stepping = 1;

	srand(time(NULL));

	load_font(&machine.memory[0]);

	if (argc == 2) {
		if (!load_program(argv[1], &machine)) {
			fprintf(stderr, "Couldn't load program %s\n", argv[1]);
			return 1;
		}
	} else {
		load_dbg(&machine);
	}

	window_init(PIXELS_WIDTH * PIXEL_SIZE,
				PIXELS_HEIGHT * PIXEL_SIZE,
				&window, &renderer);

	//int max_cycles = 0x7FFFFFFF;

	SDL_Event e;
	while (running) {
		if (!stepping && machine.waiting_for_key < 0) {
			running = machine_cycle(&machine);
		}

		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT){
				running = 0;
			} else if (e.type == SDL_KEYDOWN) {
				switch (e.key.keysym.sym) {
				case SDLK_TAB:
					stepping = !stepping;
					break;
				case SDLK_SPACE:
					if (stepping && machine.waiting_for_key < 0) {
						running = machine_cycle(&machine);
					}
					break;
				case SDLK_ESCAPE:
					running = 0;
					break;
				case SDLK_1: running &= on_key(&machine, stepping, 0x1); break;
				case SDLK_2: running &= on_key(&machine, stepping, 0x2); break;
				case SDLK_3: running &= on_key(&machine, stepping, 0x3); break;
				case SDLK_4: running &= on_key(&machine, stepping, 0xC); break;
				case SDLK_q: running &= on_key(&machine, stepping, 0x4); break;
				case SDLK_w: running &= on_key(&machine, stepping, 0x5); break;
				case SDLK_e: running &= on_key(&machine, stepping, 0x6); break;
				case SDLK_r: running &= on_key(&machine, stepping, 0xD); break;
				case SDLK_a: running &= on_key(&machine, stepping, 0x7); break;
				case SDLK_s: running &= on_key(&machine, stepping, 0x8); break;
				case SDLK_d: running &= on_key(&machine, stepping, 0x9); break;
				case SDLK_f: running &= on_key(&machine, stepping, 0xE); break;
				case SDLK_z: running &= on_key(&machine, stepping, 0xA); break;
				case SDLK_x: running &= on_key(&machine, stepping, 0x0); break;
				case SDLK_c: running &= on_key(&machine, stepping, 0xB); break;
				case SDLK_v: running &= on_key(&machine, stepping, 0xF); break;
				}
			} else if (e.type == SDL_KEYUP) {
				switch (e.key.keysym.sym) {
				case SDLK_1: machine.keys[0x1] = 0; break;
				case SDLK_2: machine.keys[0x2] = 0; break;
				case SDLK_3: machine.keys[0x3] = 0; break;
				case SDLK_4: machine.keys[0xC] = 0; break;
				case SDLK_q: machine.keys[0x4] = 0; break;
				case SDLK_w: machine.keys[0x5] = 0; break;
				case SDLK_e: machine.keys[0x6] = 0; break;
				case SDLK_r: machine.keys[0xD] = 0; break;
				case SDLK_a: machine.keys[0x7] = 0; break;
				case SDLK_s: machine.keys[0x8] = 0; break;
				case SDLK_d: machine.keys[0x9] = 0; break;
				case SDLK_f: machine.keys[0xE] = 0; break;
				case SDLK_z: machine.keys[0xA] = 0; break;
				case SDLK_x: machine.keys[0x0] = 0; break;
				case SDLK_c: machine.keys[0xB] = 0; break;
				case SDLK_v: machine.keys[0xF] = 0; break;
				}
			}
		}

		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		draw_display(&machine, renderer);

		if (stepping) draw_dbg(&machine, renderer);

        SDL_RenderPresent(renderer);
		//SDL_Delay(1);
		//if (machine.cycles > max_cycles) running = 0;
	}

	printf("Cycles: %d\n", machine.cycles);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
