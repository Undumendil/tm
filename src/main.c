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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <argp.h>
#include <sys/mman.h>
#include "util.h"
#include "core.h"
#include "interpreter.h"
#include "tui.h"


// TODO: improve doc.
static char doc[] = "A swift Turing machine simulator\n\n"
					"This program is free software; you can redistribute it and/or "
					"modify it under the terms of the GNU General Public License as "
					"published by the Free Software Foundation; version 2.\v"
					"Machine file format:\n"
					"\t[:] q0\n"
					"\t[.] f1 f2 [...]\n"
					"\tP a -> Q b DIR\n"
					"\t...\n"
					"\t=-=-=\n"
					"\tTAPE_ENTRY\n"
					"\t...\n"
					"`q0`, `f1`, ..., `P`, `Q` are state names. "
					"State names may be multi-character.\n"
					"`a`, `b` are symbol names. "
					"Symbol names may be multi-character.\n"
					"Reserved names for states/symbols are "
					"`null`, `(null)`, `*`, `_`.\n"
					"DIR is one of the following characters: "
					"`l`, `<`, `r`, `>`.\n"
					"All transitions are undefined by default.\n"
					"Entries may overwrite previous ones.\n"
					"Machine in undefined state halts with error code 1.\n"
					"You may define a transition from undefined "
					"(`null`) symbol.\n"
					"You may not define transition from undefined state.\n"
					"You may use wildcards for a pair of states/symbols "
					"in a transition:\n"
					"`<*> -> <s>` means transitions from all instances "
					"of that class to instance `s`.\n"
					"`<_> -> <_>` means transitions independent of that "
					"class.\n"
					"`<s> -> <_>` means transition that does not change "
					"class instance `s`.\n"
					"Examples:\n"
					"\t* a -> H b >\n"
					"\t\t(in any state, if symbol `a` is encountered, "
					"write `b` and move right into state `H`)\n"
					"\tP _ -> Q _ l\n"
					"\t\t(in state `P`, move left into state `Q` without "
					"overwriting the current symbol)\n"
					"\tP a -> _ b <\n"
					"\t\t(in state `P`, if symbol `a` is encountered, "
					"write `b` and move left into the same state)\n"
					"`=-=-=` line indicates start of tape description. "
					"You may omit tape description to get an empty tape.\n"
					"TAPE_ENTRY format is one of the following:\n"
					"\tpos: a b c ...\n"
					"\tpos~end: a b c ...\n"
					"\tpos~inf: a b c ...\n"
					"\tinf~pos: a b c ...\n"
					"`a`, `b`, `c`, ... are symbol names.\n"
					"1) Insert symbols from `pos` and forth (until "
					"all symbols are inserted)\n"
					"2) Insert pattern symbols from `pos` to `end`, "
					"including `end`\n"
					"3) Insert pattern symbols from `pos` to right infinity\n"
					"4) Insert pattern symbols from left infinity to `end`, "
					"including `end`. The last pattern symbol will be at the "
					"`end` position\n"
					"Tape entries overwrite previous ones if intersections "
					"occur.";
static char args_doc[] = "MACHINE_FILE";

#define OPT_TUI 1
#define OPT_TAPE 2
#define OPT_FRAME 3

static struct argp_option options[] = {
	{ "fast", 'f', 0, OPTION_ARG_OPTIONAL, 
					"Be as fast as possible: deactivate any delays, "
					"do not print tape state after each step, print "
					"step number rarely. Specify this option twice "
					"to disable printing step/tape information after "
					"machine halts" },

	{ "tape", OPT_TAPE,	"TAPE_FILE", OPTION_ARG_OPTIONAL, 
					"Use tape from the specified file; the tape from "
					"the machine file is ignored, if is" },

	{ "speed", 's', "SPEED", OPTION_ARG_OPTIONAL, 
					"Set speed of simulation (0 - slowest, 10 - no "
					"delays, default 7)" },

	{ "tui", OPT_TUI, 0, OPTION_ARG_OPTIONAL, 
					"Use ncurses-based interface" },

	{ "frame", OPT_FRAME, 0, OPTION_ARG_OPTIONAL, 
					"Draw frame around tape" },

	{ 0 }
};

struct arguments {
	bool fast, ultrafast, tui, frame;
	int8_t speed;
	char *in;
	char *tape;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state){
	struct arguments *args = state->input;
	switch(key){
		case 'f':
			if (args->fast)
				args->ultrafast = true;
			else
				args->fast = true;
			break;
		case OPT_TUI:
			args->tui = true;
			break;
		case OPT_FRAME:
			args->frame = true;
			break;
		case 's':
			if (arg){
				for (char *c = arg; *c != '\0'; c++)
					if (!isdigit(*c))
						argp_usage(state);
				int result = sscanf(arg, "%" SCNu8, &args->speed);
				if (!result || args->speed < 0 || args->speed > 10)
					argp_usage(state);
			} else
				argp_usage(state);
			break;
		case OPT_TAPE:
			args->tape = arg;
			break;
		case ARGP_KEY_ARG: 
			if (state->argc != state->next)
				argp_usage(state);
			args->in = arg;
			break;
		case ARGP_KEY_END:
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp parser = { options, parse_opt, args_doc, doc };
uint16_t delay[] = { 2000, 1400, 1000, 800, 600, 400, 200, 100, 30, 10, 0 };
struct timespec *wait;
bool *paused;
int64_t *block;

/*
 * Other processes can call this to change the speed of simulation.
 */
void set_speed(uint8_t speed){
	wait->tv_sec = delay[speed] / 1000;
	wait->tv_nsec = (delay[speed] % 1000) * 1000000;
}

int main(int argc, char **argv){
	setlocale(LC_ALL, "");

	struct arguments args = { 0 };
	args.speed = 7;
	// TODO: enable frame by default?
	argp_parse(&parser, argc, argv, 0, 0, &args);
	if (!args.in){
		fprintf(stderr, "No filename specified.\n");
		argp_help(&parser, stderr, ARGP_HELP_STD_ERR, "tm");
		return 1;
	}
	if (args.fast && args.tui){
		fprintf(stderr, "TUI is disabled in fast mode.\n");
		args.tui = false;
	}

	// Update sleep time between steps (shared).
	wait = SHALLOC(sizeof(struct timespec));
	assert(wait);
	set_speed(args.speed);

	// Paused flag (shared, TUI only).
	paused = SHAAALLOC(paused, sizeof(bool), false);

	// Update sleep time during pause (TUI only).
	struct timespec tick = { 0 };
	tick.tv_nsec = 50 * 1000000;
	
	// Tracked block offset (shared).
	block = SHAAALLOC(block, sizeof(int64_t), 0);

	if (args.tui)
		TUI_init(set_speed, args.speed, paused, block);

	uint64_t i = 0; // Step number.
	TMProgram* program = TMProgram_parse(args.in);
	if (args.tape)
		TMProgram_parse_tape(program, args.tape);
	TMExecutable* exec = TMProgram_compile(program, args.fast);
	TMProgram_free(program);

	global_set_frame(args.frame);
	global_set_draw_all(false);

	for (; exec->tape->state && !exec->machine->ok[exec->tape->state - 1]; i++){
		if (args.fast){
			// We do not want that much output while fast mode is enabled.
			if (i % 10000000 == 0)
				printf("Step:   %14lu\n", i);
		} else {
			if (args.tui)
				TUI_render(exec->tape, exec->states, exec->chars, i);
			else
				TMTape_print(exec->tape, exec->states, exec->chars, i, *block);

			nanosleep(wait, NULL);
			// A bit smarter tracked block transition.
			// It is assumed that 3*TM_RENDER_BLOCK_SIZE cells are drawn.
			if (exec->tape->pos - *block * TM_RENDER_BLOCK_SIZE >= TM_RENDER_BLOCK_SIZE * 3 / 2)
				(*block)++;
			else if (exec->tape->pos - *block * TM_RENDER_BLOCK_SIZE < -TM_RENDER_BLOCK_SIZE / 2)
				(*block)--;

			while (*paused){
				nanosleep(&tick, NULL);
				// Only TUI could have triggered a pause.
				TUI_render(exec->tape, exec->states, exec->chars, i);
			}
		}
		TM_step(exec->machine, exec->tape);
	}

	global_set_draw_all(true);
	
	if (args.tui){
		TUI_render(exec->tape, exec->states, exec->chars, i);
		TUI_deinit();
	} else {
		if (!args.ultrafast)
			TMTape_print(exec->tape, exec->states, exec->chars, i, *block);
	}
	// 0 if state is defined, 1 otherwise.
	int code = !exec->tape->state;

	// Free the memory.
	TM_free(exec->machine);
	TMTape_free(exec->tape);
	TMDict_free(exec->states);
	TMDict_free(exec->chars);
	free(exec);

	munmap(wait, sizeof(struct timespec));
	munmap(paused, sizeof(bool));
	munmap(block, sizeof(int64_t));

	exit(code);
}
