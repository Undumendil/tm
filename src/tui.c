/* 
 * Copyright (c) 2019 Daniil Fomichev <azathtoth@protonmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include "util.h"
#include "tui.h"

#define FRAME_COLOUR 1
#define STATS_COLOUR 2

void (*upd)(uint8_t);
WINDOW *wtape, *wstats;
uint64_t *tui_speed;
bool *state, *paused, *reset, rendered;
int64_t *block;
struct timespec tick;

void TMTape_printw(TMTape* tape, TMDict* chars, int64_t block);
void TUI_process_keypresses();

void TUI_init(void (*set_speed)(uint8_t), uint8_t speed, bool *pause, int64_t *bl){
	upd = set_speed;
	paused = pause;
	block = bl;

	// Whether there was an initial render.
	rendered = false;

	// A tiny pause to release CPU resources while waiting.
	tick.tv_sec = 0;
	tick.tv_nsec = 50 * 1000000;

	// Current simulation speed.
	tui_speed = SHAAALLOC(tui_speed, sizeof(uint64_t), speed);

	// state[0] - whether the process listening for keypresses should stop ASAP.
	// state[1] - whether the process listening for keypresses is still running.
	// state[2] - whether the process listening for keypresses has started.
	state = SHALLOC(3 * sizeof(bool));
	assert(state);
	state[0] = state[1] = state[2] = false;

	// Indicates that simulation is no more paused.
	reset = SHAAALLOC(reset, sizeof(bool), false);

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, true);
	curs_set(0);
	start_color();
	use_default_colors();
	init_pair(FRAME_COLOUR, COLOR_MAGENTA, -1);
	init_pair(STATS_COLOUR, COLOR_CYAN, -1);
	timeout(-1);

	// FIXME: define TM_RENDER_BLOCK_SIZE here
	// FIXME: handle SYGWINCH
	wtape = newwin(4, COLS, 10, COLS / 2 - TM_RENDER_BLOCK_SIZE * 3);
	wstats = newwin(9, 24, 0, 0);

	// The other process will now process keypresses
	// asynchronously.
	pid_t code = fork();
	if (code < 0){
		endwin();
		fprintf(stderr, "Could not fork(), aborting\n");
		exit(1);
	} else if (code == 0)
		TUI_process_keypresses();
}

void TUI_deinit(){
	state[0] = true;
	while (state[1]) 
		nanosleep(&tick, NULL);
	delwin(wtape);
	delwin(wstats);
	endwin();
}

void wprintwc(WINDOW* win, uint64_t colour, char *format, ...){
	wattron(win, COLOR_PAIR(colour));
	va_list args;
	va_start(args, format);
	vw_printw(win, format, args);
	va_end(args);
	wattroff(win, COLOR_PAIR(colour));
}

void TUI_render(TMTape* tape, TMDict* states, TMDict* chars, uint64_t i){
	while (!state[2])
		nanosleep(&tick, NULL);
	if (*reset){
		*block = tape->pos - modulo(tape->pos, TM_RENDER_BLOCK_SIZE);
		*reset = false;
	}
	if (!rendered || i % 12 == 0){
		wclear(wtape);
		wclear(wstats);
		rendered = true;
	} else {
		werase(wtape);
		werase(wstats);
	}
	wprintw(wstats, "\n Step:   ");
	wprintwc(wstats, STATS_COLOUR, "%14lu\n", i);
	wprintw(wstats, " State:  ");
	wprintwc(wstats, STATS_COLOUR, "%14s\n", TMDict_at(states, tape->state));
	wprintw(wstats, " Pos:    ");
	wprintwc(wstats, STATS_COLOUR, "%14ld\n", tape->pos);
	wprintw(wstats, " Speed:  ");
	wprintwc(wstats, STATS_COLOUR, "%14lu\n", *tui_speed);
	wprintw(wstats, " Offset: ");
	wprintwc(wstats, STATS_COLOUR, "%14ld\n", (*block - 1) * TM_RENDER_BLOCK_SIZE);
	wprintw(wstats, " BL:     ");
	wprintwc(wstats, STATS_COLOUR, "%14lu\n", tape->bl);
	wprintw(wstats, " BR:     ");
	wprintwc(wstats, STATS_COLOUR, "%14lu\n", tape->br);
	wattron(wstats, COLOR_PAIR(FRAME_COLOUR));
	wborder(wstats, ' ', '|', ' ', '-', ' ', '+', '+', '+');
	wattroff(wstats, COLOR_PAIR(FRAME_COLOUR));
	TMTape_printw(tape, chars, *block);
	wrefresh(wtape);
	wrefresh(wstats);
}

void TUI_process_keypresses(){
	state[1] = state[2] = true;
	while (!state[0]){
		int ch = wgetch(stdscr);
		switch (ch){
			case ERR: // timeout
				break;
			case KEY_UP:
				if (*tui_speed < 10)
					upd(++*tui_speed);
				break;
			case KEY_DOWN:
				if (*tui_speed)
					upd(--*tui_speed);
				break;
			case KEY_LEFT:
				if (paused){
					(*block)--;
				}
				break;
			case KEY_RIGHT:
				if (*paused){
					(*block)++;
				}
				break;
			case ' ':
				*paused = !*paused;
				if (!*paused)
					*reset = true;
				break;
			default:;
		}
	}
	state[1] = false;
	exit(0);
}

void TMTape_printw(TMTape* tape, TMDict* chars, int64_t block){
	uint64_t len = 3 * TM_RENDER_BLOCK_SIZE;
	int64_t offset = (block - 1) * TM_RENDER_BLOCK_SIZE;
	int64_t spaces = 2 * (tape->pos - offset);
	wattron(wtape, COLOR_PAIR(FRAME_COLOUR));
	for (uint64_t i = 0; i < len; i++){
		char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
		if (str){
			for (uint64_t j = 0; j < strlen(str); j++)
				if (i + j == 0 || (i == len - 1 && j == strlen(str) - 1))
					wprintw(wtape, "%s", "~");
				else
					wprintw(wtape, "%s", "—");
		} else
			if (i == 0 || i == len - 1)
				wprintw(wtape, "%s", "~");
			else
				wprintw(wtape, "%s", "—");
		if (i != len - 1)
			wprintw(wtape, "+");
	}
	wattroff(wtape, COLOR_PAIR(FRAME_COLOUR));
	wprintw(wtape, "\n");
	for (uint64_t i = 0; i < len; i++){
		char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
		if (str){
			if (offset + (int64_t)i < tape->pos)
				spaces += strlen(str) - 1;
			wprintw(wtape, "%s", str);
		} else
			wprintw(wtape, " ");
		if (i != len - 1)
			wprintwc(wtape, FRAME_COLOUR, "%s", "|");
	}
	wprintw(wtape, "\n");
	wattron(wtape, COLOR_PAIR(FRAME_COLOUR));
	for (uint64_t i = 0; i < len; i++){
		char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
		if (str){
			for (uint64_t j = 0; j < strlen(str); j++)
				if (i + j == 0 || (i == len - 1 && j == strlen(str) - 1))
					wprintw(wtape, "%s", "~");
				else
					wprintw(wtape, "%s", "—");
		} else
			if (i == 0 || i == len - 1)
				wprintw(wtape, "%s", "~");
			else
				wprintw(wtape, "%s", "—");
		if (i != len - 1)
			wprintw(wtape, "+");
	}
	wattroff(wtape, COLOR_PAIR(FRAME_COLOUR));
	wprintw(wtape, "\n");
	if (tape->pos >= offset && tape->pos < offset + len){
		for (int64_t i = 0; i < spaces; i++)
			wprintw(wtape, " ");
		wprintw(wtape, "^\n");
	}
}
