
/*
TODO:
  - test delay timer
  - implement sound timer
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
	.waiting_for_key = -1,
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

void dispatch_keys(SDL_Event *e, struct machine *machine, int *stepping,
				   int *running) {
	if (e->type == SDL_KEYDOWN) {
		switch (e->key.keysym.sym) {
		case SDLK_TAB: *stepping = !*stepping; break;
		case SDLK_ESCAPE: *running = 0; break;
		case SDLK_SPACE:
			if (*stepping && machine->waiting_for_key < 0) {
				*running = machine_cycle(machine);
			}
			break;
#define MAP_DOWN(key,val) case key: *running &= on_key(machine, *stepping, val); break;
		MAP_DOWN(SDLK_1, 0x1); MAP_DOWN(SDLK_2, 0x2); MAP_DOWN(SDLK_3, 0x3);
		MAP_DOWN(SDLK_4, 0xC); MAP_DOWN(SDLK_q, 0x4); MAP_DOWN(SDLK_w, 0x5);
		MAP_DOWN(SDLK_e, 0x6); MAP_DOWN(SDLK_r, 0xD); MAP_DOWN(SDLK_a, 0x7);
		MAP_DOWN(SDLK_s, 0x8); MAP_DOWN(SDLK_d, 0x9); MAP_DOWN(SDLK_f, 0xE);
		MAP_DOWN(SDLK_z, 0xA); MAP_DOWN(SDLK_x, 0x0); MAP_DOWN(SDLK_c, 0xB);
		MAP_DOWN(SDLK_v, 0xF);
		}
	} else if (e->type == SDL_KEYUP) {
		switch (e->key.keysym.sym) {
#define MAP_UP(key,val) case key: machine->keys[val] = 0; break;
		MAP_UP(SDLK_1, 0x1); MAP_UP(SDLK_2, 0x2); MAP_UP(SDLK_3, 0x3);
		MAP_UP(SDLK_4, 0xC); MAP_UP(SDLK_q, 0x4); MAP_UP(SDLK_w, 0x5);
		MAP_UP(SDLK_e, 0x6); MAP_UP(SDLK_r, 0xD); MAP_UP(SDLK_a, 0x7);
		MAP_UP(SDLK_s, 0x8); MAP_UP(SDLK_d, 0x9); MAP_UP(SDLK_f, 0xE);
		MAP_UP(SDLK_z, 0xA); MAP_UP(SDLK_x, 0x0); MAP_UP(SDLK_c, 0xB);
		MAP_UP(SDLK_v, 0xF);
		}
	}
}

int machine_loop(struct machine *machine, SDL_Renderer *renderer) {
	int running = 1;
	int stepping = 1;

	Uint32 ticks;
	SDL_Event e;

	while (running) {
		if (!stepping && machine->waiting_for_key < 0) {
			if (ticks > TICK_MS) {
				int new_delay = machine->delay_timer - (ticks / TICK_MS);
				machine->delay_timer = new_delay > 0 ? new_delay : 0;
				ticks = 0;
			}
			running = machine_cycle(machine);
		}

		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT){
				running = 0;
			} else {
				dispatch_keys(&e, machine, &stepping, &running);
			}
		}

		int refresh = 0;
		if (machine->cycles % CYCLES_PER_FRAME == 0) {
			refresh = 1;
			draw_display(machine, renderer);
		}

		if (stepping) {
			refresh = 1;
			draw_dbg(machine, renderer);
		}

		if (refresh) {
			SDL_RenderPresent(renderer);
		}

		ticks += SDL_GetTicks();
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

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
