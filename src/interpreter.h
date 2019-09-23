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

#pragma once

#include "core.h"

/*
 * Maps printable tokens to symbol/state codes.
 * Getting a string value is O(1).
 */
typedef struct {
	uint64_t n;    // count of registered tokens
	char **tok;    // registered tokens; index is symbol/state code + 1
} TMDict;

TMDict* TMDict_init();
void TMDict_free(TMDict*);

/*
 * Put a token into the dict, 
 * do nothing if it is already there.
 * Returns symbol/state code.
 */
uint64_t TMDict_put(TMDict*, char *str);

/*
 * Get symbol/state code for a token.
 * Returns 0 on failure.
 */
uint64_t TMDict_get(TMDict*, char *str);

/*
 * Get string representation of a symbol/state code.
 * Returns NULL if not registered.
 */
char* TMDict_at(TMDict*, uint64_t k);

/*
 * Get string representation of an array of symbol/state codes.
 * Allocates and returns a null-terminated string.
 */
char* TMDict_stringify(TMDict*, uint64_t n, uint64_t *mem);

/*
 * Token representation of symbols and states.
 */
typedef struct {
	bool any;  // `*` or `_`
	bool null; // `null` token
	char *str; // string representation
} TMRuleToken;

/*
 * <state> <char> -> <state> <char> [l|r|<|>]
 *
 * `_` may be used in the second part to 
 * indicate no change of state or character.
 *
 * `*` may be used in the first part to
 * indicate any state or character.
 *
 * `_` may be used in both parts for
 * states or characters to indicate
 * any state or character, which
 * will not change.
 *
 * Examples:
 *         * a -> s3 b l
 *         s0 _ -> s1 _ r
 *	       s5 t -> _ r r
 *	       very_long_state_name this_character -> another_long_state_name that_character l
 */
typedef struct {
	TMRuleToken s_from;
	TMRuleToken a_from;
	TMRuleToken s_to;
	TMRuleToken a_to;
	bool motion;
} TMRule;

/*
 * A tape entry. The following types are supported:
 * Insertion entry:
 *			pos: <data>
 * Pattern repeat entry:
 *			pos~end: <data>
 * Infinite pattern repeat entry:
 *			inf~pos: <data>
 *			pos~inf: <data>
 *
 * <data> shall be a non-empty space-separated symbol sequence.
 */
typedef struct {
	int64_t pos, end;
	bool l_inf, r_inf;
	uint64_t n, shift;
	char **data;
} TMProgramTapeEntry;

/*
 * Everything that user provides to create a machine.
 */
typedef struct {
	uint64_t n; // rules count
	TMRule* rules;
	TMRuleToken start_state;
	uint64_t fn; // final states count
	TMRuleToken* final_states;
	uint64_t tn; // tape entries count
	TMProgramTapeEntry* entries;
} TMProgram;

/*
 * Parse a program fron the specified file.
 */
TMProgram* TMProgram_parse(char *filename);

void TMProgram_free(TMProgram*);

/*
 * Parse tape data from the specified file.
 */
void TMProgram_parse_tape(TMProgram*, char *filename);

/*
 * A machine, a tape, some string representations of
 * symbols and states.
 */
typedef struct {
	TM* machine;
	TMTape* tape;
	TMDict* states;
	TMDict* chars;
} TMExecutable;

/*
 * Compile a parsed program.
 */
TMExecutable* TMProgram_compile(TMProgram*, bool fast);

/*
 * Whether a frame will be drawn.
 */
void global_set_frame(bool);
/*
 * Whether all the tape will be drawn.
 */
void global_set_draw_all(bool);

/*
 * Pretty-print contents of tape to stdout.
 */
void TMTape_print(TMTape*,
				TMDict* states,
				TMDict* chars,
				uint64_t i,
				int64_t block);

/*
 * Pretty-print Turing machine.
 */
void TM_print(TM* machine, TMDict* states, TMDict* chars);
