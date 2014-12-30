
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

#include "machine.h"


struct machine init_machine = {
	.pc = PROGRAM_START,
	._address = 0,
	.stack = {0},
	.sp = 0,
	.memory = {0},
	.registers = {0},
	._pixels = {0},
	.keys = {0},
	.cycles = 0,
	.delay_timer = 0,
	.sound_timer = 0,
	.waiting_for_key = -1
};

uint8_t font[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, /* 0 */
	0x20, 0x60, 0x20, 0x20, 0x70, /* 1 */
	0xF0, 0x10, 0xF0, 0x80, 0xF0, /* 2 */
	0xF0, 0x10, 0xF0, 0x10, 0xF0, /* 3 */
	0x90, 0x90, 0xF0, 0x10, 0x10, /* 4 */
	0xF0, 0x80, 0xF0, 0x10, 0xF0, /* 5 */
	0xF0, 0x80, 0xF0, 0x90, 0xF0, /* 6 */
	0xF0, 0x10, 0x20, 0x40, 0x40, /* 7 */
	0xF0, 0x90, 0xF0, 0x90, 0xF0, /* 8 */
	0xF0, 0x90, 0xF0, 0x10, 0xF0, /* 9 */
	0xF0, 0x90, 0xF0, 0x90, 0x90, /* A */
	0xE0, 0x90, 0xE0, 0x90, 0xE0, /* B */
	0xF0, 0x80, 0x80, 0x80, 0xF0, /* C */
	0xE0, 0x90, 0x90, 0x90, 0xE0, /* D */
	0xF0, 0x80, 0xF0, 0x80, 0xF0, /* E */
	0xF0, 0x80, 0xF0, 0x80, 0x80  /* F */
};

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
	static SDL_Rect rect;
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

int machine_loop(struct machine *machine, SDL_Renderer *renderer) {
	int running = 1;
	int stepping = 1;

	SDL_Event e;

	while (running) {
		if (!stepping && machine->waiting_for_key < 0) {
			running = machine_cycle(machine);
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
					if (stepping && machine->waiting_for_key < 0) {
						running = machine_cycle(machine);
					}
					break;
				case SDLK_ESCAPE:
					running = 0;
					break;
				case SDLK_1: running &= on_key(machine, stepping, 0x1); break;
				case SDLK_2: running &= on_key(machine, stepping, 0x2); break;
				case SDLK_3: running &= on_key(machine, stepping, 0x3); break;
				case SDLK_4: running &= on_key(machine, stepping, 0xC); break;
				case SDLK_q: running &= on_key(machine, stepping, 0x4); break;
				case SDLK_w: running &= on_key(machine, stepping, 0x5); break;
				case SDLK_e: running &= on_key(machine, stepping, 0x6); break;
				case SDLK_r: running &= on_key(machine, stepping, 0xD); break;
				case SDLK_a: running &= on_key(machine, stepping, 0x7); break;
				case SDLK_s: running &= on_key(machine, stepping, 0x8); break;
				case SDLK_d: running &= on_key(machine, stepping, 0x9); break;
				case SDLK_f: running &= on_key(machine, stepping, 0xE); break;
				case SDLK_z: running &= on_key(machine, stepping, 0xA); break;
				case SDLK_x: running &= on_key(machine, stepping, 0x0); break;
				case SDLK_c: running &= on_key(machine, stepping, 0xB); break;
				case SDLK_v: running &= on_key(machine, stepping, 0xF); break;
				}
			} else if (e.type == SDL_KEYUP) {
				switch (e.key.keysym.sym) {
				case SDLK_1: machine->keys[0x1] = 0; break;
				case SDLK_2: machine->keys[0x2] = 0; break;
				case SDLK_3: machine->keys[0x3] = 0; break;
				case SDLK_4: machine->keys[0xC] = 0; break;
				case SDLK_q: machine->keys[0x4] = 0; break;
				case SDLK_w: machine->keys[0x5] = 0; break;
				case SDLK_e: machine->keys[0x6] = 0; break;
				case SDLK_r: machine->keys[0xD] = 0; break;
				case SDLK_a: machine->keys[0x7] = 0; break;
				case SDLK_s: machine->keys[0x8] = 0; break;
				case SDLK_d: machine->keys[0x9] = 0; break;
				case SDLK_f: machine->keys[0xE] = 0; break;
				case SDLK_z: machine->keys[0xA] = 0; break;
				case SDLK_x: machine->keys[0x0] = 0; break;
				case SDLK_c: machine->keys[0xB] = 0; break;
				case SDLK_v: machine->keys[0xF] = 0; break;
				}
			}
		}

		SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderer);
		draw_display(machine, renderer);

		if (stepping) draw_dbg(machine, renderer);

        SDL_RenderPresent(renderer);
		//SDL_Delay(1);
		//if (machine->cycles > max_cycles) running = 0;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Usage: bchip <program>\n");
		return 1;
	}

	struct machine machine = init_machine;

	srand(time(NULL));

	load_font(machine.memory);

	if (!load_program(argv[1], &machine)) {
		fprintf(stderr, "Couldn't load program %s\n", argv[1]);
		return 1;
	}

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;
	window_init(PIXELS_WIDTH * PIXEL_SIZE,
				PIXELS_HEIGHT * PIXEL_SIZE,
				&window, &renderer);

	machine_loop(&machine, renderer);

	printf("Cycles: %d\n", machine.cycles);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
