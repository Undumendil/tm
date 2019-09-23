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

#include <stdint.h>
#include <stdbool.h>

/*
 * A standard one-dimensional Turing machine.
 * Current state and tape are maintained by 
 * TMTape structure.
 */
typedef struct {
	uint64_t n,  // size of alphabet (0 is blank and is counted)
			 q;  // number of states (0 is undefined, 1 is the initial one)
	bool *ok;    // final states (boolean, 0..q-1 (undefined state is omitted))
	uint64_t *s; // transition table for states ([0<=i<=q-1, 0<=j<=n-1] = [i * n + j])
	uint64_t *a; // transition table for symbols (same as above)
	bool *m;     // transition table for motion (same as above)
} TM;

TM* TM_init(uint64_t n, uint64_t q);
void TM_free(TM*);

/*
 * Define a transition table entry.
 */
void TM_define(TM*,
			   uint64_t s_from, // old state, shall be >0
			   uint64_t a_from, // old symbol
			   uint64_t s_to,   // new state
			   uint64_t a_to,   // new symbol
			   bool motion);    // motion direction (boolean, 1 is right)

/*
 * Define all transition table entries for a state.
 */
void TM_define_forall(TM*,
					  uint64_t s_from, // old state, shall be >0
					  uint64_t s_to,   // new state
					  uint64_t a_to,   // new symbol
					  bool motion);    // motion direction (boolean, 1 is right)

/*
 * Define all transition table entries for a state
 * without modifying symbols.
 */
void TM_define_forall_readonly(TM*,
							   uint64_t s_from, // old state, shall be >0
							   uint64_t s_to,   // new state
							   bool motion); // motion direction (boolean, 1 is right)

/*
 * Memory allocation is done block-by-block.
 * This is the number of cells in a single block.
 * Shall be a power of 2.
 */
#define TM_BLOCK_SIZE 16
/*
 * Block size for rendering the tape.
 * Shall be a power of 2.
 */
#define TM_RENDER_BLOCK_SIZE 16

typedef struct {
	int64_t start;
	uint64_t n;
	uint64_t *data;
} TMTapePattern;

typedef struct {
	uint64_t **bkmem,			// negative (<0) blocks of memory
			 **fwmem;			// positive (>=0) blocks of memory
	int64_t bl,					// count of negative blocks
			br;					// count of positive blocks
	int64_t pos;				// current head position
	uint64_t state;				// current machine state
	bool fast;					// do not try to clear empty edge blocks
	TMTapePattern left, right;  // infinite patterns to the left/right
} TMTape;

/*
 *           block -1                block 0               block 1
 *                   bkmem<-|->fwmem
 * ~~  +—+—+—+—~  ~~  ~—+—+ | +—+—+—+—~  ~~  ~—+—+  +—+—+—+—~  ~~  ~—+—+  ~~
 * ~~  |0|0|0|0        0|0| | |0|0|0|0        0|0|  |0|0|0|0        0|0|  ~~
 * ~~  +—+—+—+—~  ~~  ~—+—+ | +—+—+—+—~  ~~  ~—+—+  +—+—+—+—~  ~~  ~—+—+  ~~
 *                    -2 -1 |  ^ 1 2 3              <——— BLOCK_SIZE ———>
 *                            pos          state=1
 */
TMTape* TMTape_init(bool fast);
void TMTape_prepare(TMTape*);
void TMTape_free(TMTape*);

/*
 * Write contents of tape to `mem` 
 * from positions [`pos`..`pos`+`n`-1].
 * `mem` shall be preallocated.
 */
void TMTape_readmem(TMTape*, int64_t pos, size_t n, uint64_t* mem);

/*
 * Write contents of `mem` to tape 
 * into positions [`pos`..`pos`+`n`-1].
 */
void TMTape_writemem(TMTape*, int64_t pos, size_t n, uint64_t* mem);

/*
 * Read a symbol at the specified position.
 */
uint64_t TMTape_read_at(TMTape*, int64_t pos);

/*
 * Write a symbol at the specified position.
 */
void TMTape_write_at(TMTape*, int64_t pos, uint64_t sym);

/*
 * Read a symbol at the current position.
 */
uint64_t TMTape_read(TMTape*);

/*
 * Write a symbol at the current position.
 */
void TMTape_write(TMTape*, uint64_t sym);

/*
 * Allocate a memory block in the given direction.
 */
void TMTape_alloc(TMTape*, bool right);

/*
 * Move the pointer in the given direction.
 */
void TMTape_step(TMTape*, bool right);

/*
 * Return state.
 */
uint64_t TM_step(TM*, TMTape*);

/*
 * Return state (if finishes).
 */
uint64_t TM_run(TM*, TMTape*);

/*
 * Return state after <= `max` steps.
 */
uint64_t TM_run_restricted(TM*, TMTape*, uint64_t max);
