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

#include "interpreter.h"
#include "util.h"
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

TMDict* TMDict_init(){
	TMDict* dict = NEWSTR(TMDict);
	assert(dict);
	dict->n = 0;
	dict->tok = NULL;
	return dict;
}

void TMDict_free(TMDict* dict){
	for (uint64_t i = 0; i < dict->n; i++)
		free(dict->tok[i]);
	free(dict);
}

/*
 * Put a token into the dict, 
 * do nothing if it is already there.
 * Returns symbol/state code.
 */
uint64_t TMDict_put(TMDict* dict, char *str){
	for (uint64_t i = 0; i < dict->n; i++)
		if (strcmp(dict->tok[i], str) == 0)
			return i + 1;
	dict->tok = realloc(dict->tok, (dict->n + 1) * sizeof(char*));
	assert(dict->tok);
	dict->tok[dict->n] = str;
	return ++dict->n;
}

/*
 * Get symbol/state code for a token
 * Returns 0 on failure.
 */
uint64_t TMDict_get(TMDict* dict, char *str){
	if (strcmp(str, "null") == 0)
		return 0;
	for (uint64_t i = 0; i < dict->n; i++)
		if (strcmp(dict->tok[i], str) == 0)
			return i + 1;
	return 0;
}

/*
 * Get string representation of a symbol/state code
 * Returns NULL if not registered.
 */
char* TMDict_at(TMDict* dict, uint64_t k){
	if (k > 0 && k <= dict->n)
		if (dict->tok[k - 1])
			return dict->tok[k - 1];
	return NULL;
}

/*
 * Get string representation of an array of symbol/state codes
 * Allocates and returns a null-terminated string.
 */
char* TMDict_stringify(TMDict* dict, uint64_t n, uint64_t *mem){
	uint64_t len = 0;
	for (uint64_t i = 0; i < n; i++)
		len += strlen(TMDict_at(dict, mem[i]));
	char *str = NEWARR(char, len + 1);
	assert(str);
	str[0] = '\0';
	for (uint64_t i = 0; i < n; i++)
		strcat(str, TMDict_at(dict, mem[i]));
	return str;
}

TMRuleToken TMRuleToken_init(char *str){
	return (TMRuleToken){
		strcmp(str, "*") == 0 || strcmp(str, "_") == 0,  // any
		strcmp(str, "null") == 0,                        // null
		str                                              // str
	};
}

void TMProgram_parse_tape_file(TMProgram* program, FILE* file);

/*
 * Parse a program fron the specified file.
 */
TMProgram* TMProgram_parse(char *filename){
	FILE* file = fopen(filename, "r");
	assert(file);
	
	TMRuleToken start = {0, 0, NULL};
	
	TMRuleToken* final = NULL;
	uint64_t fn = 0;
	
	TMRule* rules = NULL;
	uint64_t n = 0;

	bool start_state_defined = false,
		 final_states_defined = false;
	
	while (!feof(file)){
		char *line = readline_trim(file);
		if (strlen(line) == 0){
			free(line);
			continue;
		}
		if (strcmp(line, "=-=-=") == 0){
			free(line);
			break;
		}
		if (!start_state_defined){
			char *start_state = NEWARR(char, strlen(line));
			assert(start_state);
			if (sscanf(line, "[:] %s", start_state)){
				if (!check_name(start_state)){
					fprintf(stderr,
							"This name is invalid: %s\n"
							"A correct name shall use only characters from "
							"[A-Za-z0-9\\-_.~+-^<>[]{}()] and cannot be "
							"equal to (null)\n", start_state);
					free(start_state);
					free(line);
					fclose(file);
					exit(1);
				}
				if (strcmp(start_state, "null") == 0){
					fprintf(stderr, "Undefined state cannot be the starting one.\n");
					free(start_state);
					free(line);
					fclose(file);
					exit(1);
				}
				if (strcmp(start_state, "*") == 0 || strcmp(start_state, "_") == 0){
					fprintf(stderr, "Wildcard state cannot be the starting one.\n");
					free(start_state);
					free(line);
					fclose(file);
					exit(1);
				}
				start.str = NEWARR(char, strlen(start_state) + 1);
				assert(start.str);
				strcpy(start.str, start_state);
				start.any = false;
				start.null = false;
				start_state_defined = true;
			} else
				fprintf(stderr, "Definition of starting state shall be in form:\n"
								"[:] q0\n"
								"Found:\n"
								"%s\n"
								"Skipping...\n", line);
			free(start_state);
		} else if (!final_states_defined){
			if (strncmp(line, "[.]", 3) != 0){
				fprintf(stderr, "Definition of final states shall be in form:\n"
								"[.] one two three\n"
								"Found:\n"
								"%s\n"
								"Skipping...\n", line);
				free(line);
				continue;
			}
			char *final_state = NEWARR(char, strlen(line));
			assert(final_state);
			for (char *state = strtok(line + 3, " \t"); 
					state != NULL; 
					state = strtok(NULL, " \t")){
				strcpy(final_state, state);
				if (strlen(final_state) == 0)
					continue;
				if (!check_name(final_state)){
					fprintf(stderr, "This name is invalid: %s\n"
									"A correct name shall use only characters from "
									"[A-Za-z0-9\\-_.~+-^<>[]{}()] and cannot be "
									"equal to (null)\n", final_state);
					free(final_state);
					free(line);
					free(final);
					fclose(file);
					exit(1);
				}
				if (strcmp(final_state, "null") == 0){
					fprintf(stderr, "Undefined state cannot be final.\n");
					free(final_state);
					free(line);
					free(final);
					fclose(file);
					exit(1);
				}
				if (strcmp(final_state, "*") == 0 || strcmp(final_state, "_") == 0){
					fprintf(stderr, "Wildcard state cannot be final.\n");
					free(final_state);
					free(line);
					free(final);
					fclose(file);
					exit(1);
				}
				final = realloc(final, (fn + 1) * sizeof(TMRuleToken));
				assert(final);
				final[fn].str = NEWARR(char, strlen(final_state) + 1);
				assert(final[fn].str);
				strcpy(final[fn].str, final_state);
				final[fn].any = false;
				final[fn].null = false;
				fn++;
			}
			free(final_state);
			final_states_defined = true;
		} else {
			uint64_t l = strlen(line);
			char *s_from_s = NEWARR(char, l),
				 *a_from_s = NEWARR(char, l),
				 *s_to_s = NEWARR(char, l),
				 *a_to_s = NEWARR(char, l),
				 *motion_s = NEWARR(char, l);
			assert(s_from_s);
			assert(a_from_s);
			assert(s_to_s);
			assert(a_to_s);
			assert(motion_s);
			if (sscanf(line, "%s %s -> %s %s %s", 
						s_from_s, 
						a_from_s, 
						s_to_s, 
						a_to_s, 
						motion_s) == 5){
				if (strcmp(s_to_s, "*") == 0 
						|| strcmp(s_from_s, "null") == 0
						|| (strcmp(s_from_s, "*") == 0 && strcmp(s_to_s, "_") == 0)
						|| (strcmp(s_from_s, "_") == 0 && strcmp(s_to_s, "_") != 0)
						|| strcmp(a_to_s, "*") == 0
						|| (strcmp(a_from_s, "*") == 0 && strcmp(a_to_s, "_") == 0)
						|| (strcmp(a_from_s, "_") == 0 && strcmp(a_to_s, "_") != 0)
						|| (strcmp(motion_s, "l") != 0 && strcmp(motion_s, "r") != 0 
								&& strcmp(motion_s, "<") != 0 && strcmp(motion_s, ">") != 0)){
					fprintf(stderr, "Illegal rule:\n%s\n", line);
					free(s_from_s);
					free(a_from_s);
					free(s_to_s);
					free(a_to_s);
					free(motion_s);
					free(line);
					free(final);
					free(rules);
					fclose(file);
					exit(1);
				}
				if (!check_name(s_from_s) 
						|| !check_name(s_to_s)){
					fprintf(stderr, "Invalid name detected:\n%s\n"
									"A correct name shall use only characters from "
									"[A-Za-z0-9\\-_.~+-^<>[]{}()] and cannot be "
									"equal to (null)\n", line);
					free(s_from_s);
					free(a_from_s);
					free(s_to_s);
					free(a_to_s);
					free(motion_s);
					free(line);
					free(rules);
					free(final);
					fclose(file);
					exit(1);
				}
				rules = realloc(rules, (n + 1) * sizeof(TMRule));
				assert(rules);
				rules[n].s_from = TMRuleToken_init(s_from_s);
				rules[n].a_from = TMRuleToken_init(a_from_s);
				rules[n].s_to = TMRuleToken_init(s_to_s);
				rules[n].a_to = TMRuleToken_init(a_to_s);
				rules[n].motion = strcmp(motion_s, "r") == 0 || strcmp(motion_s, ">") == 0;
				n++;
			} else {
				if (strlen(line) > 0)
					fprintf(stderr, "Definition of rules shall be in form:\n"
									"q0 a0 -> q a [<|l|>|r|]\n"
									"Found:\n"
									"%s\n"
									"Skipping...\n", line);
				free(s_from_s);
				free(a_from_s);
				free(s_to_s);
				free(a_to_s);
				free(motion_s);
			}
		}
		free(line);
	}
	TMProgram* prog = NEWSTR(TMProgram);
	assert(prog);
	prog->n = n;
	prog->rules = rules;
	prog->start_state = start;
	prog->fn = fn;
	prog->final_states = final;
	TMProgram_parse_tape_file(prog, file);
	fclose(file);
	return prog;
}

void TMProgram_free(TMProgram* program){
	free(program->rules);
	free(program->start_state.str);
	for (uint64_t i = 0; i < program->fn; i++)
		free(program->final_states[i].str);
	free(program->final_states);
	for (uint64_t i = 0; i < program->tn; i++){
		for (uint64_t j = 0; j < program->entries[i].n; j++)
			free(program->entries[i].data[j]);
		free(program->entries[i].data);
	}
	free(program->entries);
	free(program);
}

/*
 * Parse tape data from the specified file.
 */
void TMProgram_parse_tape(TMProgram* program, char *filename){
	FILE* file = fopen(filename, "r");
	assert(file);
	TMProgram_parse_tape_file(program, file);
	fclose(file);
}

void TMProgram_parse_tape_file(TMProgram* program, FILE* file){
	list_t *entry_list = list_init();

	while (!feof(file)){
		char *line = readline_trim(file);
		if (strlen(line) == 0){
			free(line);
			continue;
		}
		int64_t pos = 0, end = 0;
		bool pattern, l_inf = false, r_inf = false;
		char c;
		if (sscanf(line, "%ld%c", &pos, &c) == 2 && c == ':'){
			pattern = false;
		} else if (sscanf(line, "%ld~%ld%c", &pos, &end, &c) == 3 && c == ':'){
			pattern = true;
			if (end < pos){
				fprintf(stderr, "Starting index cannot be larger than ending one.\n"
								"Could not parse entry:\n%s\n", line);
				free(line);
				fclose(file);
				exit(1);
			}
		} else if (sscanf(line, "%ld~inf%c", &pos, &c) == 2 && c == ':'){
			pattern = true;
			r_inf = true;
		} else if (sscanf(line, "inf~%ld%c", &end, &c) == 2 && c == ':'){
			pattern = true;
			l_inf = true;
		} else {
			fprintf(stderr, "Tape entry shall be in form:\n"
							"pos: a1 a2 a3\n"
							"Or:\n"
							"pos~end: a1 a2 a3\n"
							"\t(one of [pos, end] may be inf)\n"
							"Found:\n%s\n"
							"Skipping...\n", line);
			free(line);
			fclose(file);
			continue;
		}
		list_push(entry_list, NEWSTR(TMProgramTapeEntry));
		TMProgramTapeEntry* entry = entry_list->tail->val;
		assert(entry);
		entry->pos = pos;
		entry->n = 0;
		entry->data = NULL;
		entry->l_inf = l_inf;
		entry->r_inf = r_inf;
		entry->shift = 0;
		if (!pattern)
			end = pos - 1;
		char *ch = strtok(line, ":");
		for (ch = strtok(NULL, " \t"); ch != NULL; ch = strtok(NULL, " \t")){
			entry->data = realloc(entry->data, (entry->n + 1) * sizeof(char*));
			assert(entry->data);
			entry->data[entry->n] = NEWARR(char, strlen(ch) + 1);
			assert(entry->data[entry->n]);
			strcpy(entry->data[entry->n], ch);
			entry->n++;
			if (!pattern)
				end++;
		}
		if (pattern && entry->n == 0){
			fprintf(stderr, "Empty patterns are not allowed.\n"
							"Could not parse entry:\n"
							"%s\n", line);
			free(line);
			fclose(file);
			exit(1);
		}
		entry->end = end;
		free(line);
		uint64_t i = 0;
		for (list_node_t *node = entry_list->head; node->val != entry; i++, node = node->next){
			TMProgramTapeEntry* that = node->val;
			if (l_inf){
				if (that->r_inf){
					if (that->pos <= end){
						that->shift = modulo(that->shift + end + 1 - that->pos, that->n);
						that->pos = end + 1;
					}
				} else if (that->end <= end){
					for (uint64_t j = 0; j < that->n; j++)
						free(that->data[j]);
					free(that->data);
					list_del(entry_list, i);
					i--;
				} else if (that->l_inf){
					if (that->end > end){
						that->shift = modulo(that->shift + end + 1 - that->end - 1, that->n);
						that->pos = end + 1;
						that->l_inf = false;
					}
				} else if (that->pos <= end){
					that->shift = modulo(that->shift + end + 1 - that->pos, that->n);
					that->pos = end + 1;
				}
			} else if (r_inf){
				if (that->l_inf){
					if (that->end >= pos){
						that->shift = modulo(that->shift + pos - 1 - that->end, that->n);
						that->end = pos - 1;
					}
				} else if (that->pos >= pos){
					for (uint64_t j = 0; j < that->n; j++)
						free(that->data[j]);
					free(that->data);
					list_del(entry_list, i);
					i--;
				} else if (that->r_inf){
					if (that->pos < pos){
						that->end = pos - 1;
						that->r_inf = false;
					}
				} else if (that->end >= pos){
					if (that->l_inf)
						that->shift = modulo(that->shift + pos - 1 - that->end, that->n);
					that->end = pos - 1;
				}
			} else if (!that->l_inf && !that->r_inf){
				if (that->end >= pos && that->end <= end){
					if (that->pos < pos){
						that->end = pos - 1;
					} else {
						for (uint64_t j = 0; j < that->n; j++)
							free(that->data[j]);
						free(that->data);
						list_del(entry_list, i);
						i--;
					}
				} else if (that->end > end){
					if (that->pos <= end && that->pos >= pos){
						that->shift = modulo(that->shift + end + 1 - that->pos, that->n);
						that->pos = end + 1;
					} else if (that->pos < pos){
						TMProgramTapeEntry* cut = NEWSTR(TMProgramTapeEntry);
						assert(cut);
						cut->shift = modulo(that->shift + end + 1 - that->pos, that->n);
						cut->pos = end + 1;
						cut->end = that->end;
						cut->n = that->n;
						cut->l_inf = cut->r_inf = false;
						cut->data = NEWARR(char*, that->n);
						assert(cut->data);
						for (uint64_t j = 0; j < that->n; j++){
							cut->data[j] = NEWARR(char, sizeof(that->data[j]));
							assert(cut->data[j]);
							strcpy(cut->data[j], that->data[j]);
						}
						list_insert(entry_list, node, NEWSTR(TMProgramTapeEntry));
						that->end = pos - 1;
					}
				}
			}
		}
	}
	program->tn = entry_list->n;
	program->entries = NEWARR(TMProgramTapeEntry, entry_list->n);
	assert(program->entries);
	uint64_t i = 0;
	for (list_node_t *node = entry_list->head; node; i++, node = node->next)
		program->entries[i] = *(TMProgramTapeEntry*)node->val;
	while (entry_list->n)
		list_del(entry_list, 0);
}

char* strcln(char *s){
	char *str = NEWARR(char, strlen(s) + 1);
	assert(str);
	strcpy(str, s);
	return str;
}

void TMDict_put_copy_if_unique(TMDict* dict, TMRuleToken tok){
	if (!tok.any && !tok.null)
		TMDict_put(dict, strcln(tok.str));
}

/*
 * Compile a parsed program.
 */
TMExecutable* TMProgram_compile(TMProgram* program, bool fast){
	TMDict* states = TMDict_init();
	TMDict* chars = TMDict_init();

	// Register all the mentioned states and symbols.

	TMDict_put_copy_if_unique(states, program->start_state);
	
	for (uint64_t i = 0; i < program->n; i++){
		TMDict_put_copy_if_unique(states, program->rules[i].s_from);
		TMDict_put_copy_if_unique(chars, program->rules[i].a_from);
		TMDict_put_copy_if_unique(states, program->rules[i].s_to);
		TMDict_put_copy_if_unique(chars, program->rules[i].a_to);
	}

	for (uint64_t i = 0; i < program->fn; i++)
		TMDict_put_copy_if_unique(states, program->final_states[i]);

	for (uint64_t i = 0; i < program->tn; i++)
		for (uint64_t j = 0; j < program->entries[i].n; j++)
			TMDict_put(chars, strcln(program->entries[i].data[j]));

	TM* machine = TM_init(chars->n + 1, states->n + 1);
	TMTape* tape = TMTape_init(fast);
	
	// Register infinite patterns, if any.
	for (uint64_t i = 0; i < program->tn; i++){
		TMProgramTapeEntry* entry = &program->entries[i];
		if (entry->l_inf){
			tape->left.start = entry->end + 1;
			tape->left.n = entry->n;
			tape->left.data = NEWARR(uint64_t, entry->n);
			assert(tape->left.data);
			for (uint64_t j = 0; j < entry->n; j++)
				tape->left.data[j] = TMDict_get(chars, entry->data[(j + entry->shift) % entry->n]);
		} else if (entry->r_inf){
			tape->right.start = entry->pos;
			tape->right.n = entry->n;
			tape->right.data = NEWARR(uint64_t, entry->n);
			assert(tape->right.data);
			for (uint64_t j = 0; j < entry->n; j++)
				tape->right.data[j] = TMDict_get(chars, entry->data[(j + entry->shift) % entry->n]);
		}
	}
	// Now, since infinite patterns are registered, 
	// we can write to tape. If it was not done earlier,
	// allocated blocks would be zero-filled.
	TMTape_prepare(tape);
	for (uint64_t i = 0; i < program->tn; i++){
		TMProgramTapeEntry* entry = &program->entries[i];
		if (!entry->l_inf && !entry->r_inf) {
			for (int64_t j = entry->pos; j <= entry->end; j++)
				TMTape_write_at(tape, j, TMDict_get(chars, 
							entry->data[(j - entry->pos + entry->shift) % entry->n]));
		}
	}
	// Mark final states as final.
	for (uint64_t i = 0; i < program->fn; i++){
		uint64_t id = TMDict_get(states, program->final_states[i].str);
		if (id)
			machine->ok[id - 1] = true;
	}
	// Register transition rules.
	for (uint64_t i = 0; i < program->n; i++){
		TMRuleToken s_from = program->rules[i].s_from,
					 a_from = program->rules[i].a_from,
					 s_to = program->rules[i].s_to,
					 a_to = program->rules[i].a_to;
		bool motion = program->rules[i].motion;
		if (a_from.any){
			if (a_to.any){
				if (s_from.any){
					if (s_to.any){
						// _ _ -> _ _ [l|r|<|>]
						for (uint64_t i = 0; i < states->n; i++)
							TM_define_forall_readonly(machine, 
													   i + 1, 
													   i + 1, 
													   motion);
					} else {
						// * _ -> s _ [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						for (uint64_t i = 0; i < states->n; i++)
							TM_define_forall_readonly(machine, 
													   i + 1, 
													   s, 
													   motion);
					}
				} else {
					uint64_t s0 = TMDict_get(states, s_from.str);
					if (s_to.any){
						// s0 _ -> _ _ [l|r|<|>]
						TM_define_forall_readonly(machine,
												   s0,
												   s0,
												   motion);
					} else {
						// s0 _ -> s _ [l|r]
						uint64_t s = TMDict_get(states, s_to.str);
						TM_define_forall_readonly(machine,
												   s0,
												   s,
												   motion);
					}
				}
			} else {
				uint64_t a = TMDict_get(chars, a_to.str);
				if (s_from.any){
					if (s_to.any){
						// _ * -> _ a [l|r|<|>]
						for (uint64_t i = 0; i < states->n; i++)
							TM_define_forall(machine, 
											  i + 1, 
											  i + 1, 
											  a,
											  motion);
					} else {
						// * * -> s a [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						for (uint64_t i = 0; i < states->n; i++)
							TM_define_forall(machine, 
											  i + 1, 
											  s, 
											  a,
											  motion);
					}
				} else {
					uint64_t s0 = TMDict_get(states, s_from.str);
					if (s_to.any){
						// s0 * -> _ a [l|r|<|>]
						TM_define_forall(machine,
										  s0,
										  s0,
										  a,
										  motion);
					} else {
						// s0 * -> s a [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						TM_define_forall(machine,
										  s0,
										  s,
										  a,
										  motion);
					}
				}
			}
		} else {
			uint64_t a0 = TMDict_get(chars, a_from.str);
			if (a_to.any){
				if (s_from.any){
					if (s_to.any){
						// _ a0 -> _ _ [l|r|<|>]
						for (uint64_t i = 0; i < states->n; i++)
							TM_define(machine, 
									   i + 1,
									   a0,
									   i + 1,
									   a0,
									   motion);
					} else {
						// * a0 -> s _ [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						for (uint64_t i = 0; i < states->n; i++)
							TM_define(machine,
									   i + 1,
									   a0,
									   s,
									   a0,
									   motion);
					}
				} else {
					uint64_t s0 = TMDict_get(states, s_from.str);
					if (s_to.any){
						// s0 a0 -> _ _ [l|r|<|>]
						TM_define(machine,
								   s0,
								   a0,
								   s0,
								   a0,
								   motion);
					} else {
						// s0 a0 -> s _ [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						TM_define(machine,
								   s0,
								   a0,
								   s,
								   a0,
								   motion);
					}
				}
			} else {
				uint64_t a = TMDict_get(chars, a_to.str);
				if (s_from.any){
					if (s_to.any){
						// _ a0 -> _ a [l|r|<|>]
						for (uint64_t i = 0; i < states->n; i++)
							TM_define(machine,
									   i + 1,
									   a0,
									   i + 1,
									   a,
									   motion);
					} else {
						// * a0 -> s a [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						for (uint64_t i = 0; i < states->n; i++)
							TM_define(machine,
									   i + 1,
									   a0,
									   s,
									   a,
									   motion);
					}
				} else {
					uint64_t s0 = TMDict_get(states, s_from.str);
					if (s_to.any){
						// s0 a0 -> _ a [l|r|<|>]
						TM_define(machine,
								   s0,
								   a0,
								   s0,
								   a,
								   motion);
					} else {
						// s0 a0 -> s a [l|r|<|>]
						uint64_t s = TMDict_get(states, s_to.str);
						TM_define(machine,
								   s0,
								   a0,
								   s,
								   a,
								   motion);
					}
				}
			}
		}
	}
	TMExecutable* exec = NEWSTR(TMExecutable);
	assert(exec);
	exec->machine = machine;
	exec->tape = tape;
	exec->states = states;
	exec->chars = chars;
	return exec;
}

static bool frame = false;
static bool all = false;

/*
 * Whether a frame will be drawn.
 */
void global_set_frame(bool frm){
	frame = frm;
}

/*
 * Whether all the tape will be drawn.
 */
void global_set_draw_all(bool a){
	all = a;
}

/*
 * Pretty-print contents of tape to stdout.
 */
void TMTape_print(TMTape* tape, TMDict* states, TMDict* chars, uint64_t i, int64_t block){
	// Default: print all the tape.
	int64_t offset = -tape->bl * TM_BLOCK_SIZE;
	uint64_t len = (tape->bl + tape->br) * TM_BLOCK_SIZE;
	// Print only 3 blocks around the target one.
	if (!all){
		offset = (block - 1) * TM_RENDER_BLOCK_SIZE;
		len = 3 * TM_RENDER_BLOCK_SIZE;
	} else {
		while (!TMTape_read_at(tape, offset))
			offset++;
		while (len > 0 && !TMTape_read_at(tape, offset + (int64_t)len - 1))
			len--;
	}
	printf("Step:   %14lu\n", i);
	printf("State:  %14s\n", TMDict_at(states, tape->state));
	printf("Pos:    %14ld\n", tape->pos);
	printf("Offset: %14ld\n", offset);
	printf("BL:     %14lu\n", tape->bl);
	printf("BR:     %14lu\n", tape->br);
	uint64_t spaces = 2 * (tape->pos - offset);
	// Upper frame part:
	if (frame){
		for (uint64_t i = 0; i < len; i++){
			char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
			if (str){
				for (uint64_t j = 0; j < strlen(str); j++)
					if (i + j == 0 || (i == len - 1 && j == strlen(str) - 1))
						printf("~");
					else
						printf("-");
			} else
				if (i == 0 || i == len - 1)
					printf("~");
				else
					printf("-");
			if (i != len - 1)
				printf("+");
		}
		printf("\n");
	}
	for (uint64_t i = 0; i < len; i++){
		char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
		if (str){
			if (offset + (int64_t)i < tape->pos)
				spaces += strlen(str) - 1;
			printf("%s", str);
		} else {
			if (frame)
				printf(" ");
			else
				printf("_");
		}
		if (!frame)
			printf(" ");
		else if (i != len - 1)
			printf("|");
	}
	printf("\n");
	// Lower frame part:
	if (frame){
		for (uint64_t i = 0; i < len; i++){
			char *str = TMDict_at(chars, TMTape_read_at(tape, offset + (int64_t)i));
			if (str){
				for (uint64_t j = 0; j < strlen(str); j++)
					if (i + j == 0 || (i == len - 1 && j == strlen(str) - 1))
						printf("~");
					else
						printf("-");
			} else
				if (i == 0 || i == len - 1)
					printf("~");
				else
					printf("-");
			if (i != len - 1)
				printf("+");
		}
		printf("\n");
	}
	if (!all){
		for (uint64_t i = 0; i < spaces; i++)
			printf(" ");
		printf("^\n");
	}
}

/*
 * Pretty-print Turing machine.
 */
void TM_print(TM* machine, TMDict* states, TMDict* chars){
	printf("Total states: %lu\n"
		   "Total chars: %lu\n"
		   "Rules:\n", 
		   states->n, chars->n);
	for (uint64_t s = 1; s <= states->n; s++){
		for (uint64_t c = 0; c <= chars->n; c++){
			printf("%lu:%s %lu:%s -> ", s, TMDict_at(states, s), c, TMDict_at(chars, c));
			printf("%lu:%s %lu:%s %s\n", machine->s[(s - 1) * machine->n + c],
										 TMDict_at(states, machine->s[(s - 1) * machine->n + c]), 
										 machine->a[(s - 1) * machine->n + c],
										 TMDict_at(chars, machine->a[(s - 1) * machine->n + c]), 
										 machine->m[(s - 1) * machine->n + c] ? ">" : "<");

		}
		printf("\n");
	}
}
