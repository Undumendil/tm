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

#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#define NEWSTR(type) (type*) malloc(sizeof(type))
#define NEWARR(type,n) (type*) malloc((n) * sizeof(type))
// Shared allocation.
#define SHALLOC(size) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
// Allocate-Assert-Assign (AAA) version of SHALLOC.
#define SHAAALLOC(var,size,val) SHALLOC(size); assert(var); *(var) = val;

// Allocate zero-filled arrays of size n.
bool* zalloc2(uint64_t n);
uint64_t* zalloc64(uint64_t n);
uint64_t** wrap64(uint64_t* p);

int64_t modulo(int64_t a, int64_t n);

// Read a line, ignoring comments and removing leading and ending spaces.
char* readline_trim(FILE*);

// Checks whether a string contains only letters, digits and
// some whitelisted special symbols.
bool check_name(char *name);

// A simple list implementation for non-critical internal needs.
typedef struct list_node {
	void *val;
	struct list_node *next;
} list_node_t;

typedef struct {
	list_node_t *head, *tail;
	uint64_t n;
} list_t;

list_t* list_init();

void list_push(list_t *list, void *val);

void list_del(list_t *list, uint64_t index);

void list_insert(list_t *list, list_node_t *where, void *val);
