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

#include "util.h"
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

bool* zalloc2(uint64_t n){
	bool* mem = NEWARR(bool, n);
	assert(mem);
	for (uint64_t i = 0; i < n; i++)
		mem[i] = false;
	return mem;
}

uint64_t* zalloc64(uint64_t n){
	uint64_t* mem = NEWARR(uint64_t, n);
	assert(mem);
	for (uint64_t i = 0; i < n; i++)
		mem[i] = 0;
	return mem;
}

uint64_t** wrap64(uint64_t* p){
	uint64_t **mem = NEWSTR(uint64_t*);
	assert(mem);
	*mem = p;
	return mem;
}

int64_t modulo(int64_t a, int64_t n){
	return ((a % n) + n) % n;
}

char* readline_trim(FILE* file){
	assert(file);
	uint64_t reserved = 80, len = 0;
	char *str = NEWARR(char, reserved + 1);
	assert(str);
	str[0] = '\0';
	bool only_spaces = true;
	char c = ' ';
	while (c != '\n'){
		if (feof(file))
			break;
		int ci = fgetc(file);
		if (ci == EOF)
			break;
		c = ci;
		if (only_spaces && isspace(c))
			continue;
		else
			only_spaces = false;
		str[len] = c;
		if (len == reserved){
			reserved += 20;
			str = realloc(str, (reserved + 1) * sizeof(char));
			assert(str);
		}
		str[len + 1] = '\0';
		len++;
	}
	while (len >= 1 && isspace(str[len - 1])){
		str[len - 1] = '\0';
		len--;
	}
	if (len >= 2 && str[0] == '/' && str[1] == '/')
		str[0] = '\0';
	return str;
}

static char allowed[] = "-_.~+-^<>[]{}()";

bool check_name(char *name){
	if (strcmp(name, "*") == 0 || strcmp(name, "_") == 0 || strcmp(name, "null") == 0)
		return true;
	if (strcmp(name, "(null)") == 0)
		return false;
	for (; *name != '\0'; name++)
		if (!isalnum(*name) && !strchr(allowed, *name))
			return false;
	return true;
}

list_t* list_init(){
	list_t *list = NEWSTR(list_t);
	assert(list);
	list->head = list->tail = NULL;
	list->n = 0;
	return list;
}

void list_push(list_t *list, void *val){
	list_node_t *node = NEWSTR(list_node_t);
	node->val = val;
	node->next = NULL;
	assert(node);
	if (list->n){
		list->tail->next = node;
		list->tail = node;
	} else
		list->head = list->tail = node;
	list->n++;
}

void list_del(list_t *list, uint64_t index){
	if (index >= list->n)
		return;
	if (index == 0){
		free(list->head->val);
		list_node_t *head = list->head->next;
		free(list->head);
		list->head = head;
		list->n--;
		if (!head)
			list->tail = NULL;
		return;
	}
	list_node_t *cur = list->head;
	for (uint64_t i = 1; i < index; i++)
		cur = cur->next;
	free(cur->next->val);
	list_node_t *end = cur->next->next;
	free(cur->next);
	cur->next = end;
	list->n--;
	if (!end)
		list->tail = cur;
}

void list_insert(list_t *list, list_node_t *where, void *val){
	list_node_t *node = NEWSTR(list_node_t);
	assert(node);
	node->val = val;
	node->next = where->next;
	where->next = node;
	list->n++;
	if (!node->next)
		list->tail = node;
}
