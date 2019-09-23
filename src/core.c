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

#include "core.h"
#include "util.h"
#include <stdlib.h>
#include <assert.h>

TM* TM_init(uint64_t n, uint64_t q){
	TM* machine = NEWSTR(TM);
	assert(machine);
	machine->n = n;
	machine->q = q;
	machine->ok = zalloc2(q);     // final states
	machine->s = zalloc64(n * q); // state transition table
	machine->a = zalloc64(n * q); // symbol transition table
	machine->m = zalloc2(n * q);  // motion transition table
	return machine;
}

void TM_free(TM* machine){
	free(machine->ok);
	free(machine->s);
	free(machine->a);
	free(machine->m);
	free(machine);
}

/*
 * Define a transition table entry.
 */
void TM_define(TM* machine, 
			   uint64_t s_from, // old state, shall be >0
			   uint64_t a_from, // old symbol
			   uint64_t s_to,   // new state
			   uint64_t a_to,   // new symbol
			   bool motion){    // motion direction (boolean, 1 is right)
	assert(s_from);
	machine->s[(s_from - 1) * machine->n + a_from] = s_to;
	machine->a[(s_from - 1) * machine->n + a_from] = a_to;
	machine->m[(s_from - 1) * machine->n + a_from] = motion;
}

/*
 * Define all transition table entries for a state.
 */
void TM_define_forall(TM* machine,
					  uint64_t s_from, // old state, shall be >0
					  uint64_t s_to,   // new state
					  uint64_t a_to,   // new symbol
					  bool motion){    // motion direction (boolean, 1 is right)
	for (uint64_t a = 0; a < machine->n; a++)
		TM_define(machine, s_from, a, s_to, a_to, motion);
}

/*
 * Define all transition table entries for a state
 * without modifying symbols.
 */
void TM_define_forall_readonly(TM* machine,
							   uint64_t s_from, // old state, shall be >0
							   uint64_t s_to,   // new state
							   bool motion){    // motion direction (boolean, 1 is right)
	for (uint64_t a = 0; a < machine->n; a++)
		TM_define(machine, s_from, a, s_to, a, motion);
}

/*
 *           block -1                block 0               block 1
 *                   bkmem<-|->fwmem
 * ~~  +—+—+—+—~  ~~  ~—+—+ | +—+—+—+—~  ~~  ~—+—+  +—+—+—+—~  ~~  ~—+—+  ~~
 * ~~  |0|0|0|0        0|0| | |0|0|0|0        0|0|  |0|0|0|0        0|0|  ~~
 * ~~  +—+—+—+—~  ~~  ~—+—+ | +—+—+—+—~  ~~  ~—+—+  +—+—+—+—~  ~~  ~—+—+  ~~
 *                    -2 -1 |  ^ 1 2 3              <——— BLOCK_SIZE ———>
 *                            pos          state=1
 */
TMTape* TMTape_init(bool fast){
	TMTape* tape = NEWSTR(TMTape);
	assert(tape);
	tape->bkmem = NULL;
	tape->fwmem = NULL;
	tape->bl = 0;
	tape->br = 0;
	tape->pos = 0;
	tape->state = 1;
	tape->fast = fast;
	tape->left = (TMTapePattern){ 0 };
	tape->right = (TMTapePattern){ 0 };
	return tape;
}

void TMTape_prepare(TMTape* tape){
	for (uint64_t i = 0; i < tape->bl; i++)
		free(tape->bkmem[i]);
	free(tape->bkmem);
	tape->bl = 0;
	for (uint64_t i = 0; i < tape->br; i++)
		free(tape->fwmem[i]);
	free(tape->fwmem);
	tape->br = 0;
	tape->pos = 0;
	tape->state = 1;
	TMTape_alloc(tape, true);
}

void TMTape_free(TMTape* tape){
	free(tape->left.data);
	free(tape->right.data);
	for (uint64_t i = 0; i < tape->bl; i++)
		free(tape->bkmem[i]);
	for (uint64_t i = 0; i < tape->br; i++)
		free(tape->fwmem[i]);
	free(tape);
}

/*
 * Write contents of tape to `mem` 
 * from positions [`pos`..`pos`+`n`-1].
 * `mem` shall be preallocated.
 */
void TMTape_readmem(TMTape* tape, int64_t pos, size_t n, uint64_t* mem){
	assert(mem);
	for (int64_t i = pos; i < pos + n; i++)
		mem[i - pos] = TMTape_read_at(tape, i);
}

/*
 * Write contents of `mem` to tape 
 * into positions [`pos`..`pos`+`n`-1].
 */
void TMTape_writemem(TMTape* tape, int64_t pos, size_t n, uint64_t* mem){
	for (int64_t i = pos; i < pos + n; i++)
		TMTape_write_at(tape, i, mem[i - pos]);
}

uint64_t TMTape_undefined(TMTape* tape, int64_t pos){
	if (tape->left.n && pos < tape->left.start)
		return tape->left.data[
			modulo(pos - tape->left.start, tape->left.n)
		];
	if (tape->right.n && pos >= tape->right.start)
		return tape->right.data[
			modulo(pos - tape->right.start, tape->right.n)
		];
	return 0;
}

/*
 * Read a symbol at the specified position.
 */
uint64_t TMTape_read_at(TMTape* tape, int64_t pos){
	if (pos < -tape->bl * TM_BLOCK_SIZE || pos >= tape->br * TM_BLOCK_SIZE)
		return TMTape_undefined(tape, pos);
	if (pos < 0)
		return tape->bkmem[(-1 - pos) / TM_BLOCK_SIZE][(-1 - pos) % TM_BLOCK_SIZE];
	else
		return tape->fwmem[pos / TM_BLOCK_SIZE][pos % TM_BLOCK_SIZE];
}

/*
 * Write a symbol at the specified position.
 */
void TMTape_write_at(TMTape* tape, int64_t pos, uint64_t sym){
	while (pos < -tape->bl * TM_BLOCK_SIZE)
		TMTape_alloc(tape, false);
	while (pos >= tape->br * TM_BLOCK_SIZE)
		TMTape_alloc(tape, true);
	if (pos < 0)
		tape->bkmem[(-1 - pos) / TM_BLOCK_SIZE][(-1 - pos) % TM_BLOCK_SIZE] = sym;
	else
		tape->fwmem[pos / TM_BLOCK_SIZE][pos % TM_BLOCK_SIZE] = sym;
}

/*
 * Read a symbol at the current position.
 */
uint64_t TMTape_read(TMTape* tape){
	if (tape->pos < 0)
		return tape->bkmem[(-1 - tape->pos) / TM_BLOCK_SIZE][(-1 - tape->pos) % TM_BLOCK_SIZE];
	else
		return tape->fwmem[tape->pos / TM_BLOCK_SIZE][tape->pos % TM_BLOCK_SIZE];
}

/*
 * Write a symbol at the current position.
 */
void TMTape_write(TMTape* tape, uint64_t sym){
	if (tape->pos < 0)
		tape->bkmem[(-1 - tape->pos) / TM_BLOCK_SIZE][(-1 - tape->pos) % TM_BLOCK_SIZE] = sym;
	else
		tape->fwmem[tape->pos / TM_BLOCK_SIZE][tape->pos % TM_BLOCK_SIZE] = sym;
}

/*
 * Allocate a memory block in the given direction.
 */
void TMTape_alloc(TMTape* tape, bool right){
	uint64_t *block = NEWARR(uint64_t, TM_BLOCK_SIZE);
	assert(block);
	for (uint64_t i = 0; i < TM_BLOCK_SIZE; i++)
		if (right)
			block[i] = TMTape_read_at(tape, tape->br * TM_BLOCK_SIZE + i);
		else
			block[i] = TMTape_read_at(tape, -(tape->bl + 1) * TM_BLOCK_SIZE + i);
	if (right){
		tape->fwmem = realloc(tape->fwmem, (tape->br + 1) * sizeof(uint64_t*));
		assert(tape->fwmem);
		tape->fwmem[tape->br] = block;
		tape->br++;
	} else {
		tape->bkmem = realloc(tape->bkmem, (tape->bl + 1) * sizeof(uint64_t*));
		assert(tape->bkmem);
		tape->bkmem[tape->bl] = block;
		tape->bl++;
	}
}

/*
 * Move the pointer in the given direction.
 */
void TMTape_step(TMTape* tape, bool right){
	if (right){
		if (tape->pos == tape->br * TM_BLOCK_SIZE - 1)
			TMTape_alloc(tape, 1);
		tape->pos++;
		if (!tape->fast && tape->bl > 0 && tape->pos >= -(tape->bl - 1) * TM_BLOCK_SIZE){
			for (int64_t i = -tape->bl * TM_BLOCK_SIZE; i < -(tape->bl - 1) * TM_BLOCK_SIZE; i++)
				if ((!tape->right.n && TMTape_read_at(tape, i)) ||
					(tape->right.n && TMTape_read_at(tape, i) != tape->right.data[
						modulo(i - tape->right.start, tape->right.n)
					]))
					return;
			free(tape->bkmem[tape->bl - 1]);
			tape->bl--;
			tape->bkmem = realloc(tape->bkmem, tape->bl * sizeof(uint64_t*));
			assert(tape->bl == 0 || tape->bkmem);
		}
	} else {
		if (-tape->pos == tape->bl * TM_BLOCK_SIZE)
			TMTape_alloc(tape, 0);
		tape->pos--;
		if (!tape->fast && tape->br > 0 && tape->pos <= (tape->br - 1) * TM_BLOCK_SIZE - 1){
			for (int64_t i = (tape->br - 1) * TM_BLOCK_SIZE; i < tape->br * TM_BLOCK_SIZE; i++)
				if ((!tape->left.n && TMTape_read_at(tape, i)) ||
					(tape->left.n && TMTape_read_at(tape, i) != tape->left.data[
						modulo(i - tape->left.start, tape->left.n)
					]))
					return;
			free(tape->fwmem[tape->br - 1]);
			tape->br--;
			tape->fwmem = realloc(tape->fwmem, tape->br * sizeof(uint64_t*));
			assert(tape->br == 0 || tape->fwmem);
		}
	}
}

/*
 * Returns state.
 */
uint64_t TM_step(TM* machine, TMTape* tape){
	if (!tape->state || machine->ok[tape->state - 1])
		return tape->state;
	uint64_t sym = TMTape_read(tape);
	uint64_t new = machine->a[(tape->state - 1) * machine->n + sym];
	uint64_t right = machine->m[(tape->state - 1) * machine->n + sym];
	tape->state = machine->s[(tape->state - 1) * machine->n + sym];
	TMTape_write(tape, new);
	TMTape_step(tape, right);
	return tape->state;
}

/*
 * Returns state (if finishes).
 */
uint64_t TM_run(TM* machine, TMTape* tape){
	while (tape->state && !machine->ok[tape->state - 1])
		TM_step(machine, tape);
	return tape->state;
}

/*
 * Returns state after <= `max` steps.
 */
uint64_t TM_run_restricted(TM* machine, TMTape* tape, uint64_t max){
	while (max-- && tape->state && !machine->ok[tape->state - 1])
		TM_step(machine, tape);
	return tape->state;
}
